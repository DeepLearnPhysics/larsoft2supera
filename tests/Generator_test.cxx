#include "TLorentzVector.h"
#include "larsoft2supera/Generator.h"
#include "supera/process/SelectGenerator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

template <class F> void rejects(F action) {
  try {
    action();
  } catch (const std::invalid_argument &) {
    return;
  }
  throw std::runtime_error("Expected invalid_argument");
}

} // namespace

int main() try {
  // Construct a neutrino interaction with known, orthogonal lepton momentum.
  simb::MCTruth truth;
  truth.SetOrigin(simb::kBeamNeutrino);
  simb::MCParticle incoming(-7, 14, "primary", -1, 0., 0);
  incoming.AddTrajectoryPoint({1., 2., 3., 4.}, {0., 0., 2., 2.});
  incoming.AddTrajectoryPoint({5., 6., 7., 8.}, {0., 0., 1., 1.});
  simb::MCParticle lepton(9, 13, "primary", -7, 0.105, 1);
  lepton.AddTrajectoryPoint({11., 12., 13., 14.}, {1., 0., 0., 1.25});
  truth.Add(incoming);
  truth.Add(lepton);
  truth.SetNeutrino(0, 1, 1001, 1000180400, 2212, 2, 1.5, 0.25, 0.5, 0.75);

  // A second interaction exercises absence of neutrino metadata and signed IDs.
  simb::MCTruth other;
  other.SetOrigin(simb::kCosmicRay);
  simb::MCParticle positron(-9, -11, "test", -1, 0.0005, 1);
  positron.AddTrajectoryPoint({-1., -2., -3., -4.}, {0.25, 0.5, 0.75, 1.});
  other.Add(positron);
  other.Add(simb::MCParticle(20, 22, "unselected", -1, 0., 0));

  auto output = larsoft2supera::convert_generator({truth, other});
  truth = simb::MCTruth{};
  other = simb::MCTruth{};

  // Check source identity and owning values independently of LArCV serialization.
  require(output.size() == 2 && output[0].truth_index == 0 && output[1].truth_index == 1,
          "Original interaction indices");
  require(output[0].origin == 1 && output[1].origin == 2 && !output[1].neutrino,
          "Origin and optional neutrino");
  const auto &particle = output[1].particles[0];
  require(particle.particle_index == 0 && particle.track_id == -9 && particle.pdg == -11 &&
              particle.status == 1 && particle.process == "test",
          "Particle fields");
  require(particle.initial->position_cm == std::array<double, 3>{-1., -2., -3.} &&
              particle.initial->time_ns == -4. &&
              particle.initial->momentum_gev_c == std::array<double, 3>{0.25, 0.5, 0.75} &&
              particle.initial->energy_gev == 1.,
          "Native units");
  require(!output[1].particles[1].initial, "Absent unselected trajectory");

  // Verify interaction metadata and every retained incoming trajectory component.
  const auto &nu = *output[0].neutrino;
  require(nu.incoming.track_id == -7 && nu.incoming.particle_index == supera::missing_index &&
              nu.incoming.pdg == 14 && nu.incoming.process == "primary" && nu.lepton_track_id == 9,
          "Neutrino identity");
  require(nu.current == 0 && nu.mode == 1 && nu.interaction_type == 1001 &&
              nu.target == 1000180400 && nu.nucleon == 2212 && nu.quark == 2,
          "Interaction codes");
  require(nu.hadronic_mass_gev == 1.5 && nu.bjorken_x == 0.25 && nu.inelasticity == 0.5 &&
              nu.momentum_transfer_gev2 == 0.75 && nu.theta_rad &&
              std::abs(nu.theta_rad - std::acos(0.)) < 1.e-14,
          "Interaction kinematics");
  require(nu.trajectory.size() == 2 &&
              nu.trajectory[1].position_cm == std::array<double, 3>{5., 6., 7.} &&
              nu.trajectory[1].time_ns == 8. &&
              nu.trajectory[1].momentum_gev_c == std::array<double, 3>{0., 0., 1.} &&
              nu.trajectory[1].energy_gev == 1.,
          "Incoming trajectory");

  // Selection preserves source indices and removes only the specified origins/statuses.
  supera::GeneratorCollection collection{
      "mpv", {{0, "absent", false, {}}, {1, "generator", true, output}}};
  auto selected = supera::select_generator(collection, 1);
  require(selected.sources[0].records.empty() && !selected.sources[0].present &&
              selected.sources[1].source_index == 1 && selected.sources[1].records.size() == 1 &&
              selected.sources[1].records[0].particles[0].particle_index == 1,
          "Selection identity");
  require(collection.sources[1].records.size() == 2, "Selection owns its copy");
  require(supera::select_generator(collection, 0).sources[1].records.empty(),
          "Explicit unknown origin");
  require(larsoft2supera::convert_generator({}).empty(), "Empty collection");

  // Reject selected missing kinematics, while filtered-out records remain harmless.
  auto malformed = collection;
  malformed.sources[1].records[1].particles[0].initial.reset();
  rejects([&] { supera::select_generator(malformed); });
  require(supera::select_generator(malformed, 1).sources[1].records.size() == 1,
          "Filter before validation");

  // A malformed neutrino stays representable until selection decides it is needed.
  simb::MCTruth no_lepton_trajectory;
  no_lepton_trajectory.SetOrigin(simb::kBeamNeutrino);
  no_lepton_trajectory.Add(incoming);
  no_lepton_trajectory.Add(simb::MCParticle(9, 13, "primary", -7, 0.105, 1));
  no_lepton_trajectory.SetNeutrino(0, 0, 1001, 1000180400, 2212, 0, 0., 0., 0., 0.);
  auto no_theta = larsoft2supera::convert_generator({no_lepton_trajectory});
  require(no_theta[0].neutrino && std::isnan(no_theta[0].neutrino->theta_rad), "Absent theta");
  supera::GeneratorCollection malformed_neutrino{"mpv", {{0, "test", true, no_theta}}};
  rejects([&] { supera::select_generator(malformed_neutrino); });
  require(supera::select_generator(malformed_neutrino, 2).sources[0].records.empty(),
          "Filtered malformed interaction");

  // Native default codes are absent; legitimate negative IDs remain present.
  simb::MCTruth defaults;
  defaults.Add(simb::MCParticle{});
  larsoft2supera::NormalizationReport report;
  const auto normalized = larsoft2supera::convert_generator({defaults}, &report);
  const auto &unknown = normalized[0].particles[0];
  require(!unknown.track_id && !unknown.status && unknown.pdg == supera::unspecified_pdg &&
              report.counts[0] == 3,
          "Native default particle codes");

  // The neutrino constructor uses positive double::min(), not lowest().
  simb::MCTruth unknown_nu;
  unknown_nu.Add(incoming);
  unknown_nu.Add(lepton);
  const auto missing_code = std::numeric_limits<int>::min();
  const auto missing_number = std::numeric_limits<double>::min();
  unknown_nu.SetNeutrino(missing_code, missing_code, missing_code, missing_code, missing_code,
                         missing_code, missing_number, missing_number, missing_number,
                         missing_number);
  const auto unknown_interaction = larsoft2supera::convert_generator({unknown_nu});
  const auto &unknown_fields = *unknown_interaction[0].neutrino;
  require(!unknown_fields.current && !unknown_fields.mode && !unknown_fields.target &&
              std::isnan(unknown_fields.hadronic_mass_gev) &&
              std::isnan(unknown_fields.bjorken_x) && std::isnan(unknown_fields.inelasticity) &&
              std::isnan(unknown_fields.momentum_transfer_gev2),
          "Neutrino codes and kinematics normalize independently");

  simb::MCTruth edges;
  simb::MCParticle signed_id(-1, -11, "test", -1, 0., 1);
  signed_id.AddTrajectoryPoint({std::numeric_limits<double>::infinity(), 2., 3., -1.},
                               {0., 0., 1., 1.});
  edges.Add(signed_id);
  const auto edge_records = larsoft2supera::convert_generator({edges});
  require(edge_records[0].particles[0].track_id == -1 &&
              std::isnan(edge_records[0].particles[0].initial->position_cm[0]) &&
              edge_records[0].particles[0].initial->position_cm[1] == 2.,
          "Negative identity and partial position");
  // ROOT's zero-vector angle is not a measured direction. Keep it missing.
  simb::MCTruth zero_direction;
  simb::MCParticle zero_incoming(1, 14, "primary", -1, 0., 0);
  zero_incoming.AddTrajectoryPoint({0., 0., 0., 0.}, {0., 0., 0., 1.});
  zero_direction.Add(zero_incoming);
  zero_direction.Add(lepton);
  zero_direction.SetNeutrino(0, 0, 0, 0, 0, 0, 0., 0., 0., 0.);
  const auto zero_records = larsoft2supera::convert_generator({zero_direction});
  require(std::isnan(zero_records[0].neutrino->theta_rad), "Zero direction is unavailable");
  return 0;

} catch (const std::exception &error) {

  std::cerr << error.what() << '\n';
  return 1;
}
