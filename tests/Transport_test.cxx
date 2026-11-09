#include "larsoft2supera/Transport.h"

#include <stdexcept>

namespace {
void check(bool value) {
  if (!value)
    throw std::runtime_error("Transport extraction contract failed");
}
} // namespace

int main() {
  using namespace supera;
  simb::MCParticle primary(5, -2212, "primary", 0, 0.938, 1);
  primary.AddDaughter(9);
  primary.AddTrajectoryPoint({1., 2., 3., 4.}, {5., 6., 7., 8.});
  primary.AddTrajectoryPoint({11., 12., 13., 14.}, {15., 16., 17., 18.}, "Transportation", true);
  primary.SetWeight(2.5);
  primary.SetPolarization({0.1, 0.2, 0.3});
  primary.SetGvtx(1., 2., 3., 4.);
  primary.SetRescatter(5);
  auto records = larsoft2supera::convert_transport({primary}, TransportStorage::Retained);
  primary = simb::MCParticle{};
  const auto &p = records.at(0);
  check(p.track_id == 5 && p.mother_id == 0 && p.pdg == -2212 && p.status == 1);
  check(p.source_index == 0 && p.process == "primary" && p.mass_gev == 0.938);
  check(p.daughter_ids == std::vector<std::int32_t>{9});
  check(p.trajectory.size() == 2 && p.trajectory[1].time_ns == 14.);
  check(p.trajectory[1].momentum_gev_c[2] == 17. && p.trajectory[1].energy_gev == 18.);
  check(p.weight == 2.5 && p.polarization[1] == 0.2 && p.generator_vertex[3] == 4.);
  check(p.rescatter == 5 && p.trajectory_processes.size() == 1);
  check(p.trajectory_processes[0].step == 1 && p.trajectory_processes[0].name == "Transportation");
  check(!p.primary && p.direct_parent == missing_index && !p.truth);

  // Missing trajectory and nonfinite measurements survive as explicit invalidity.
  simb::MCParticle empty(-7, 22, "secondary", 99, 0., 0);
  records = larsoft2supera::convert_transport({empty}, TransportStorage::Dropped);
  check(records[0].trajectory.empty() && records[0].track_id == -7);
  check(records[0].storage == TransportStorage::Dropped);
  empty.AddTrajectoryPoint({NAN, INFINITY, 3., 4.}, {1., 2., 3., INFINITY});
  larsoft2supera::NormalizationReport report;
  records = larsoft2supera::convert_transport({empty}, TransportStorage::Dropped, &report);
  check(std::isnan(records[0].trajectory[0].position_cm[0]));
  check(std::isnan(records[0].trajectory[0].position_cm[1]));
  check(std::isnan(records[0].trajectory[0].energy_gev));
  check(report.counts[1] == 1 && report.counts[2] == 2);
  check(larsoft2supera::convert_transport({}, TransportStorage::Retained).empty());
}
