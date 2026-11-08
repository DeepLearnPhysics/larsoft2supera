#include "TBranch.h"
#include "TBufferFile.h"
#include "TClass.h"
#include "TFile.h"
#include "TKey.h"
#include "TTree.h"
#include "larcv/core/DataFormat/EventBase.h"
#include "larcv/core/DataFormat/EventCRTHit.h"
#include "larcv/core/DataFormat/EventFlash.h"
#include "larcv/core/DataFormat/EventNeutrino.h"
#include "larcv/core/DataFormat/EventParticle.h"
#include "larcv/core/DataFormat/EventVoxel3D.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace supera_audit {
struct Row {
  std::string tree, branch, type, left_id, right_id;
  Long64_t entry;
  int left_bytes, right_bytes, first_difference;
  bool equal;
  std::vector<std::string> normalized_fields;
};

std::string event_id(const larcv::EventBase *p) {
  if (!p || !p->valid())
    throw std::runtime_error("Missing/invalid LArCV event identity");
  return std::to_string(p->run()) + ":" + std::to_string(p->subrun()) + ":" +
         std::to_string(p->event());
}

std::map<std::string, std::string> tree_keys(TFile &f) {
  std::map<std::string, std::string> result;
  for (auto *object : *f.GetListOfKeys()) {
    auto *key = dynamic_cast<TKey *>(object);
    if (!key || std::string(key->GetClassName()) != "TTree")
      throw std::runtime_error("Unexpected non-tree key; comparison must be extended explicitly");
    if (!result.emplace(key->GetName(), key->GetClassName()).second)
      throw std::runtime_error("Multiple key cycles; refusing ambiguous comparison");
  }
  if (result.empty())
    throw std::runtime_error("No product trees");
  return result;
}

std::map<std::string, std::string> branches(TTree &t) {
  std::map<std::string, std::string> result;
  for (auto *object : *t.GetListOfBranches()) {
    auto *b = dynamic_cast<TBranch *>(object);
    if (!b || !*b->GetClassName())
      throw std::runtime_error("Unexpected non-object branch");
    result.emplace(b->GetName(), b->GetClassName());
  }
  if (result.empty())
    throw std::runtime_error("Empty branch inventory");
  return result;
}

std::vector<Row> compare(const char *left, const char *right, Long64_t expected_entries,
                         bool auxiliary = false, bool generator = false, bool normalize_crt = false,
                         bool spacepoints = false) {
  std::unique_ptr<TFile> a(TFile::Open(left)), b(TFile::Open(right));
  if (!a || !b || a->IsZombie() || b->IsZombie())
    throw std::runtime_error("Cannot open input files");
  std::map<std::string, std::string> keys = {{"opflash_cryoE_tree", "TTree"},
                                             {"opflash_cryoW_tree", "TTree"}};
  if (auxiliary)
    keys.emplace("crthit_crthit_tree", "TTree");
  if (generator) {
    keys.emplace("particle_mpv_tree", "TTree");
    keys.emplace("neutrino_mpv_tree", "TTree");
  }
  if (spacepoints) {
    keys.clear();
    for (const auto &side : {std::string("E"), std::string("W")})
      for (const auto &suffix : {"", "_chi2", "_cryo", "_tpc", "_hit_charge0", "_hit_charge1",
                                 "_hit_charge2", "_hit_key0", "_hit_key1", "_hit_key2"})
        keys.emplace("sparse3d_reco_cryo" + side + suffix + "_tree", "TTree");
  }
  const auto reference_keys = tree_keys(*a);
  for (const auto &key : keys)
    if (!reference_keys.count(key.first))
      throw std::runtime_error("Reference product missing");
  if (keys != tree_keys(*b))
    throw std::runtime_error("Product tree inventories differ");
  std::vector<Row> rows;
  auto *base = TClass::GetClass("larcv::EventBase");
  for (auto const &key : keys) {
    auto *ta = dynamic_cast<TTree *>(a->Get(key.first.c_str()));
    auto *tb = dynamic_cast<TTree *>(b->Get(key.first.c_str()));
    if (ta->GetEntries() != expected_entries || tb->GetEntries() != expected_entries)
      throw std::runtime_error("Wrong event count in " + key.first);
    auto inventory = branches(*ta);
    if (inventory != branches(*tb))
      throw std::runtime_error("Branch schema differs in " + key.first);
    for (auto const &item : inventory) {
      auto *cl = TClass::GetClass(item.second.c_str());
      if (!cl || !cl->IsLoaded() || !cl->InheritsFrom(base))
        throw std::runtime_error("Missing compiled LArCV dictionary for " + item.second);
      auto *ba = ta->GetBranch(item.first.c_str());
      auto *bb = tb->GetBranch(item.first.c_str());
      void *oa = cl->New();
      void *ob = cl->New();
      ba->SetAddress(&oa);
      bb->SetAddress(&ob);
      for (Long64_t i = 0; i < expected_entries; ++i) {
        if (ba->GetEntry(i) <= 0 || bb->GetEntry(i) <= 0)
          throw std::runtime_error("Failed reading " + key.first);
        // Normalize only the reference, and expose every changed field for an
        // exact fixture allowlist check. The original file is never modified.
        std::vector<std::string> normalized;
        if (normalize_crt && item.second == "larcv::EventCRTHit") {
          auto *record = static_cast<larcv::EventCRTHit *>(oa);
          auto hits = record->as_vector();
          for (std::size_t index = 0; index < hits.size(); ++index) {
            auto &hit = hits[index];
            if (hit.x_err() == std::numeric_limits<float>::infinity()) {
              hit.x_err(std::numeric_limits<float>::lowest());
              normalized.push_back(event_id(record) + ":" + std::to_string(index) + ":x_err");
            }
            if (hit.z_err() == std::numeric_limits<float>::infinity()) {
              hit.z_err(std::numeric_limits<float>::lowest());
              normalized.push_back(event_id(record) + ":" + std::to_string(index) + ":z_err");
            }
          }
          record->set(hits);
        }
        TBufferFile sa(TBuffer::kWrite), sb(TBuffer::kWrite);
        sa.WriteObjectAny(oa, cl);
        sb.WriteObjectAny(ob, cl);
        int first = -1;
        for (int j = 0; j < std::min(sa.Length(), sb.Length()); ++j)
          if (sa.Buffer()[j] != sb.Buffer()[j]) {
            first = j;
            break;
          }
        bool equal = first == -1 && sa.Length() == sb.Length();
        auto *ea = static_cast<larcv::EventBase *>(cl->DynamicCast(base, oa));
        auto *eb = static_cast<larcv::EventBase *>(cl->DynamicCast(base, ob));
        rows.push_back({key.first, item.first, item.second, event_id(ea), event_id(eb), i,
                        sa.Length(), sb.Length(), first, equal, normalized});
      }
      ta->ResetBranchAddresses();
      tb->ResetBranchAddresses();
      cl->Destructor(oa);
      cl->Destructor(ob);
    }
  }
  return rows;
}
} // namespace supera_audit
