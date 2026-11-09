#include "larsoft2supera/Deposit.h"

#include "lardataobj/Simulation/sim.h"

#include <stdexcept>

namespace {
void check(bool value) {
  if (!value)
    throw std::runtime_error("Native deposit conversion contract failed");
}
} // namespace

int main() {
  using namespace supera;
  std::vector<sim::SimEnergyDepositLite> steps{
      {2., {1., 2., 3.}, 5., 7}, {3., {4., 5., 6.}, 8., -7}, {0., {}, 0., sim::NoParticleId}};
  larsoft2supera::NormalizationReport report;
  auto output = larsoft2supera::convert_deposits(steps, &report);
  check(output.size() == 3 && output[0].original_id == 7 && !output[0].reassigned);
  check(output[0].position_cm[2] == 3. && output[0].energy_mev == 2. && output[0].time_ns == 5.);
  check(output[1].track_id == 7 && output[1].raw_track_id == -7 && output[1].reassigned);
  check(!output[1].original_id && !output[1].raw_original_id && !output[2].track_id);
  check(report.counts[0] == 1 && std::isnan(output[0].electrons));

  // Full steps retain both IDs and every persisted segment field.
  std::vector<sim::SimEnergyDeposit> full{
      {12, 34, .25, 5., {1., 2., 3.}, {3., 4., 5.}, 6., 8., -7, 11, 9}};
  output = larsoft2supera::convert_deposits(full);
  check(output[0].original_id == 9 && output[0].track_id == 7 && output[0].reassigned);
  check(output[0].raw_original_id == 9 && output[0].raw_track_id == -7);
  check(output[0].time_ns == 7. && output[0].position_cm[0] == 2.);
  const auto &segment = *output[0].segment;
  check(segment.start_cm[1] == 2. && segment.end_cm[2] == 5.);
  check(segment.start_ns == 6. && segment.end_ns == 8. && segment.pdg == 11);
  check(segment.photons == 12. && segment.scintillation_yield_ratio == .25);
  check(output[0].electrons == 34. && output[0].energy_mev == 5.);
  check(larsoft2supera::convert_deposits(std::vector<sim::SimEnergyDeposit>{}).empty());

  // Native SimChannel storage can merge contributions; copy its persisted IDEs exactly.
  sim::SimChannel channel(42);
  const double xyz[]{1., 2., 3.};
  channel.AddIonizationElectrons(-7, 9, 15., xyz, 4., 8);
  channel.AddIonizationElectrons(7, 10, 20., xyz, 5.);
  output = larsoft2supera::convert_deposits(std::vector<sim::SimChannel>{channel});
  check(output.size() == 2 && output[0].channel == 42 && output[0].tdc == 9);
  check(output[0].original_id == 8 && output[0].track_id == 7 && output[0].reassigned);
  check(output[0].raw_original_id == 8 && output[0].raw_track_id == -7);
  check(output[0].energy_mev == 4. && output[0].electrons == 15. && std::isnan(output[0].time_ns));
  check(output[0].channel_index == 0 && output[0].ide_index == 0 && output[1].source_index == 1);
  check(!output[1].original_id);
  check(larsoft2supera::convert_deposits(std::vector<sim::SimChannel>{}).empty());
  check(larsoft2supera::convert_deposits(std::vector<sim::SimEnergyDepositLite>{}).empty());
}
