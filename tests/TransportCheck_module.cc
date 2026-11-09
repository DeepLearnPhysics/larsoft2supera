#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "canvas/Persistency/Common/FindOneP.h"
#include "fhiclcpp/ParameterSet.h"
#include "lardataobj/Simulation/GeneratedParticleInfo.h"
#include "lardataobj/Simulation/ParticleAncestryMap.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nusimdata/SimulationBase/MCTruth.h"

#include "TFile.h"
#include "TTree.h"

#include <cmath>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {
void check(bool value) {
  if (!value)
    throw std::runtime_error("Serialized transport archive differs from native source");
}

bool equal(double left, double right) {
  return left == right || (std::isnan(left) && !std::isfinite(right));
}
} // namespace

/// Independently compare serialized output with native input; do not call the converter.
class TransportCheck : public art::EDAnalyzer {
public:
  explicit TransportCheck(const fhicl::ParameterSet &p)
      : art::EDAnalyzer(p), file_(TFile::Open(p.get<std::string>("Archive").c_str())) {
    check(file_ && !file_->IsZombie());
    tree_ = file_->Get<TTree>("transport");
    check(tree_);
    events_ = file_->Get<TTree>("events");
    check(events_);
    events_->SetBranchAddress("map_ancestor", &map_ancestor_);
    events_->SetBranchAddress("map_descendant", &map_descendant_);
#define FIELD(name) tree_->SetBranchAddress(#name, &name##_)
    FIELD(product);
    FIELD(truth_product);
    FIELD(process_names);
    FIELD(run);
    FIELD(subrun);
    FIELD(event);
    FIELD(source_index);
    FIELD(track_id);
    FIELD(mother_id);
    FIELD(pdg);
    FIELD(status);
    FIELD(process);
    FIELD(end_process);
    FIELD(mass);
    FIELD(weight);
    FIELD(daughters);
    FIELD(trajectory);
    FIELD(polarization);
    FIELD(generator_vertex);
    FIELD(has_truth);
    FIELD(truth_index);
    FIELD(generated_index);
    FIELD(origin);
    FIELD(rescatter);
    FIELD(has_rescatter);
    FIELD(process_steps);
    FIELD(process_codes);
#undef FIELD
  }

  void analyze(const art::Event &event) override {
    check(events_->GetEntry(event_entry_++) > 0);
    check(map_ancestor_->size() == map_descendant_->size());
    std::map<int, std::set<int>> serialized_map;
    for (std::size_t i = 0; i < map_ancestor_->size(); ++i)
      serialized_map[map_ancestor_->at(i)].insert(map_descendant_->at(i));
    const auto map = event.getValidHandle<sim::ParticleAncestryMap>(art::InputTag("largeant::G4"));
    check(serialized_map == map->GetMap());
    for (const auto &tag :
         {art::InputTag("largeant::G4"), art::InputTag("largeant:droppedMCParticles:G4")}) {
      const auto handle = event.getValidHandle<std::vector<simb::MCParticle>>(tag);
      for (std::size_t i = 0; i < handle->size(); ++i) {
        check(tree_->GetEntry(entry_++) > 0);
        const auto &p = handle->at(i);
        check(run_ == event.run() && subrun_ == event.subRun() && event_ == event.event());
        check(source_index_ == static_cast<long long>(i));
        std::ostringstream source_product;
        source_product << tag.encode() << '@' << handle.id();
        check(*product_ == source_product.str());
        check(track_id_ == p.TrackId() && mother_id_ == p.Mother());
        check(pdg_ == p.PdgCode() && status_ == p.StatusCode());
        check(*process_ == p.Process() && *end_process_ == p.EndProcess());
        check(equal(mass_, p.Mass()) && equal(weight_, p.Weight()));
        check(daughters_->size() == static_cast<std::size_t>(p.NumberDaughters()));
        for (int j = 0; j < p.NumberDaughters(); ++j)
          check(daughters_->at(j) == p.Daughter(j));
        check(trajectory_->size() == 8 * p.NumberTrajectoryPoints());
        for (std::size_t j = 0; j < p.NumberTrajectoryPoints(); ++j) {
          const auto &x = p.Position(j);
          const auto &momentum = p.Momentum(j);
          const double expected[]{x.X(),        x.Y(),        x.Z(),        x.T(),
                                  momentum.X(), momentum.Y(), momentum.Z(), momentum.T()};
          for (std::size_t k = 0; k < 8; ++k)
            check(equal(trajectory_->at(8 * j + k), expected[k]));
        }
        const auto &polarization = p.Polarization();
        check(equal(polarization_->at(0), polarization.X()));
        check(equal(polarization_->at(1), polarization.Y()));
        check(equal(polarization_->at(2), polarization.Z()));
        const auto vertex = p.GetGvtx();
        check(equal(generator_vertex_->at(0), vertex.X()));
        check(equal(generator_vertex_->at(1), vertex.Y()));
        check(equal(generator_vertex_->at(2), vertex.Z()));
        check(equal(generator_vertex_->at(3), vertex.T()));
        check(has_rescatter_ == (p.Rescatter() != std::numeric_limits<int>::min()));
        if (has_rescatter_)
          check(rescatter_ == p.Rescatter());
        const auto &processes = p.Trajectory().TrajectoryProcesses();
        check(process_steps_->size() == processes.size() &&
              process_codes_->size() == processes.size());
        for (std::size_t j = 0; j < processes.size(); ++j) {
          check(process_steps_->at(j) == static_cast<long long>(processes[j].first));
          check(process_codes_->at(j) == processes[j].second);
          check(process_names_->at(j) == p.Trajectory().KeyToProcess(processes[j].second));
        }
        if (tag.instance().empty()) {
          art::FindOneP<simb::MCTruth, sim::GeneratedParticleInfo> links(handle, event, tag);
          const auto truth = links.at(i);
          check(has_truth_ && truth_index_ == static_cast<long long>(truth.key()));
          check(origin_ == int(truth->Origin()));
          std::ostringstream truth_product;
          truth_product << truth.id();
          check(*truth_product_ == truth_product.str());
          const auto info = links.data(i);
          const auto index = info.ref().hasGeneratedParticleIndex()
                                 ? static_cast<long long>(info.ref().generatedParticleIndex())
                                 : -1;
          check(generated_index_ == index);
        } else {
          check(!has_truth_);
        }
      }
    }
  }

  void endJob() override {
    check(entry_ == tree_->GetEntries() && event_entry_ == events_->GetEntries());
  }

private:
  std::unique_ptr<TFile> file_;
  TTree *tree_{}, *events_{};
  long long event_entry_{};
  std::vector<int> *map_ancestor_{}, *map_descendant_{};
  std::vector<std::string> *process_names_{};
  std::string *product_{}, *truth_product_{};
  long long entry_{}, source_index_{}, truth_index_{}, generated_index_{};
  unsigned long long run_{}, subrun_{}, event_{};
  int track_id_{}, mother_id_{}, pdg_{}, status_{}, origin_{}, rescatter_{};
  double mass_{}, weight_{};
  bool has_truth_{}, has_rescatter_{};
  std::string *process_{}, *end_process_{};
  std::vector<int> *daughters_{};
  std::vector<double> *trajectory_{}, *polarization_{}, *generator_vertex_{};
  std::vector<long long> *process_steps_{};
  std::vector<unsigned char> *process_codes_{};
};
DEFINE_ART_MODULE(TransportCheck)
