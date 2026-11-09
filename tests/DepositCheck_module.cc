#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "fhiclcpp/ParameterSet.h"
#include "lardataobj/Simulation/SimChannel.h"
#include "lardataobj/Simulation/SimEnergyDepositLite.h"
#include "lardataobj/Simulation/sim.h"
#include "nusimdata/SimulationBase/MCParticle.h"

#include "TFile.h"
#include "TTree.h"
#include "larcore/CoreUtils/ServiceUtil.h"
#include "larcore/Geometry/Geometry.h"
#include <fstream>
#include <iomanip>

#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

namespace {
void check(bool value) {
  if (!value)
    throw std::runtime_error("Deposit archive differs from native source");
}

bool equal(double a, double b) {
  return a == b || (std::isnan(a) && (!std::isfinite(b) || b == float(util::kBogusD)));
}
} // namespace

/// Independent persisted-input comparison; does not call the reader or shared attribution.
class DepositCheck : public art::EDAnalyzer {
public:
  explicit DepositCheck(const fhicl::ParameterSet &p)
      : art::EDAnalyzer(p), file_(TFile::Open(p.get<std::string>("Archive").c_str())) {
    if (p.get<bool>("AuditGeometry", false)) {
      std::array<double, 3> low{1.e9, 1.e9, 1.e9}, high{-1.e9, -1.e9, -1.e9};
      const auto geometry = lar::providerFrom<geo::Geometry>();
      for (const auto &tpc : geometry->Iterate<geo::TPCGeo>()) {
        const auto &box = tpc.ActiveBoundingBox();
        low[0] = std::min(low[0], box.MinX());
        high[0] = std::max(high[0], box.MaxX());
        low[1] = std::min(low[1], box.MinY());
        high[1] = std::max(high[1], box.MaxY());
        low[2] = std::min(low[2], box.MinZ());
        high[2] = std::max(high[2], box.MaxZ());
      }
      std::ofstream bounds("legacy-active-bounds.json");
      bounds << std::setprecision(17) << "{\"minimum\":[" << low[0] << ',' << low[1] << ','
             << low[2] << "],\"maximum\":[" << high[0] << ',' << high[1] << ',' << high[2]
             << "]}\n";
    }
    check(file_ && !file_->IsZombie());
    events_ = file_->Get<TTree>("events");
    check(events_);
    events_->SetBranchAddress("channels", &channels_);
    rows_ = file_->Get<TTree>("deposits");
    check(rows_);
#define FIELD(name) rows_->SetBranchAddress(#name, &name##_)
    FIELD(run);
    FIELD(subrun);
    FIELD(event);
    FIELD(kind);
    FIELD(source_index);
    FIELD(channel_index);
    FIELD(ide_index);
    FIELD(channel);
    FIELD(tdc);
    FIELD(raw_track);
    FIELD(raw_original);
    FIELD(has_raw_original);
    FIELD(x);
    FIELD(y);
    FIELD(z);
    FIELD(time_ns);
    FIELD(energy_mev);
    FIELD(electrons);
    FIELD(has_segment);
    FIELD(has_pdg);
    FIELD(pdg);
    FIELD(start_x);
    FIELD(start_y);
    FIELD(start_z);
    FIELD(end_x);
    FIELD(end_y);
    FIELD(end_z);
    FIELD(start_ns);
    FIELD(end_ns);
    FIELD(photons);
    FIELD(yield_ratio);

    FIELD(original_particle);
    FIELD(assigned_particle);
    FIELD(attributed_particle);
    FIELD(attribution);
    FIELD(primary);
#undef FIELD
  }

  void analyze(const art::Event &event) override {
    std::map<int, long long> indices;
    long long particle_count = 0;
    for (const auto &tag :
         {art::InputTag("largeant::G4"), art::InputTag("largeant:droppedMCParticles:G4")}) {
      const auto particles = event.getValidHandle<std::vector<simb::MCParticle>>(tag);
      for (const auto &p : *particles)
        check(indices.emplace(p.TrackId(), particle_count++).second);
    }
    const auto lookup = [&](int id) {
      const auto found = indices.find(id);
      return found == indices.end() ? -1LL : found->second;
    };
    // Independently reconstruct the explicit native pairs used by this fixture.
    std::map<int, std::set<long long>> evidence;
    const auto pair = [&](int original, int assigned) {
      if (assigned < 0 && assigned != sim::NoParticleId && assigned != util::kBogusI &&
          original != sim::NoParticleId && original != util::kBogusI && lookup(-assigned) >= 0)
        evidence[original].insert(lookup(-assigned));
    };
    const auto native_steps = event.getValidHandle<std::vector<sim::SimEnergyDeposit>>(
        art::InputTag("filtersed::DetSim"));
    for (const auto &d : *native_steps)
      pair(d.OrigTrackID(), d.TrackID());
    const auto native_channels =
        event.getValidHandle<std::vector<sim::SimChannel>>(art::InputTag("merge::DetSim"));
    for (const auto &channel : *native_channels)
      for (const auto &entry : channel.TDCIDEMap())
        for (const auto &d : entry.second)
          pair(d.origTrackID, d.trackID);

    const auto identity = [&](int original, int assigned) {
      check(original_particle_ == lookup(original));
      const auto normalized = assigned == sim::NoParticleId || assigned == util::kBogusI
                                  ? assigned
                                  : std::abs(assigned);
      check(assigned_particle_ == lookup(normalized));
      check(attributed_particle_ >= -1 && attributed_particle_ < particle_count);
      check(primary_ >= -1 && primary_ < particle_count && attribution_ >= 0 && attribution_ <= 5);
      if (attribution_ == 0)
        check(attributed_particle_ == original_particle_ && original_particle_ >= 0);
      if (attribution_ == 1)
        check(attributed_particle_ == assigned_particle_ && original_particle_ == -1);
      if (attribution_ == 5) {
        const auto &candidates = evidence.at(original);
        // This pinned fixture has a unique explicit ancestor for every recovered ID.
        check(candidates.size() == 1 && attributed_particle_ == *candidates.begin());
      }
      if (attribution_ == 3 || attribution_ == 4)
        check(attributed_particle_ == -1);
    };
    const auto next = [&] {
      check(rows_->GetEntry(entry_++) > 0);
      check(run_ == event.run() && subrun_ == event.subRun() && event_ == event.event());
    };
    const auto steps = event.getValidHandle<std::vector<sim::SimEnergyDepositLite>>(
        art::InputTag("filtersed::DetSim"));
    for (std::size_t i = 0; i < steps->size(); ++i) {
      next();
      const auto &d = steps->at(i);
      check(!has_segment_);
      check(kind_ == 0 && source_index_ == static_cast<long long>(i));
      check(raw_track_ == d.TrackID() && !has_raw_original_ && channel_ == -1 && tdc_ == -1);
      check(equal(x_, d.X()) && equal(y_, d.Y()) && equal(z_, d.Z()));
      check(equal(time_ns_, d.Time()) && equal(energy_mev_, d.Energy()) && std::isnan(electrons_));
      identity(d.TrackID() < 0 ? sim::NoParticleId : d.TrackID(), d.TrackID());
    }
    const auto full = event.getValidHandle<std::vector<sim::SimEnergyDeposit>>(
        art::InputTag("filtersed::DetSim"));
    for (std::size_t i = 0; i < full->size(); ++i) {
      next();
      const auto &d = full->at(i);
      check(kind_ == 2 && source_index_ == static_cast<long long>(i) && has_segment_);
      check(raw_track_ == d.TrackID() && has_raw_original_ && raw_original_ == d.OrigTrackID());
      check(channel_ == -1 && tdc_ == -1 && has_pdg_ && pdg_ == d.PdgCode());
      check(equal(x_, d.X()) && equal(y_, d.Y()) && equal(z_, d.Z()));
      check(equal(time_ns_, d.Time()) && equal(energy_mev_, d.Energy()));
      check(equal(electrons_, d.NumElectrons()) && equal(photons_, d.NumPhotons()));
      check(equal(start_x_, d.StartX()) && equal(start_y_, d.StartY()) &&
            equal(start_z_, d.StartZ()));
      check(equal(end_x_, d.EndX()) && equal(end_y_, d.EndY()) && equal(end_z_, d.EndZ()));
      check(equal(start_ns_, d.StartT()) && equal(end_ns_, d.EndT()));
      check(equal(yield_ratio_, d.ScintYieldRatio()));
      identity(d.OrigTrackID(), d.TrackID());
    }
    const auto channels =
        event.getValidHandle<std::vector<sim::SimChannel>>(art::InputTag("merge::DetSim"));
    check(events_->GetEntry(event_entry_++) > 0 && channels_->empty());
    check(events_->GetEntry(event_entry_++) > 0 && channels_->empty());
    check(events_->GetEntry(event_entry_++) > 0 && channels_->size() == channels->size());
    for (std::size_t c = 0; c < channels->size(); ++c)
      check(channels_->at(c) == channels->at(c).Channel());
    long long index = 0;
    for (std::size_t c = 0; c < channels->size(); ++c) {
      for (const auto &[tdc, ides] : channels->at(c).TDCIDEMap()) {
        for (std::size_t i = 0; i < ides.size(); ++i) {
          next();
          const auto &d = ides[i];
          check(!has_segment_);
          check(kind_ == 1 && source_index_ == index++ &&
                channel_index_ == static_cast<long long>(c));
          check(ide_index_ == static_cast<long long>(i) && channel_ == channels->at(c).Channel() &&
                tdc_ == tdc);
          check(raw_track_ == d.trackID && has_raw_original_ && raw_original_ == d.origTrackID);
          check(equal(x_, d.x) && equal(y_, d.y) && equal(z_, d.z));
          check(equal(energy_mev_, d.energy) && equal(electrons_, d.numElectrons) &&
                std::isnan(time_ns_));
          identity(d.origTrackID, d.trackID);
        }
      }
    }
  }

  void endJob() override {
    check(entry_ == rows_->GetEntries());
  }

private:
  std::unique_ptr<TFile> file_;
  TTree *rows_{}, *events_{};
  std::vector<unsigned int> *channels_{};
  long long event_entry_{};
  long long entry_{}, source_index_{}, channel_index_{}, ide_index_{}, channel_{}, tdc_{};
  long long original_particle_{}, assigned_particle_{}, attributed_particle_{}, primary_{};
  unsigned long long run_{}, subrun_{}, event_{};
  int kind_{}, attribution_{}, raw_track_{}, raw_original_{};
  bool has_raw_original_{}, has_segment_{}, has_pdg_{};
  int pdg_{};
  double start_x_{}, start_y_{}, start_z_{}, end_x_{}, end_y_{}, end_z_{};
  double start_ns_{}, end_ns_{}, photons_{}, yield_ratio_{};
  double x_{}, y_{}, z_{}, time_ns_{}, energy_mev_{}, electrons_{};
};
DEFINE_ART_MODULE(DepositCheck)
