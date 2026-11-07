#include "larsoft2supera/Generator.h"

namespace larsoft2supera {
namespace {
bool has_direction(const simb::MCParticle &input) {
  if (!input.NumberTrajectoryPoints())
    return false;
  const auto &p = input.Momentum(0);
  return std::isfinite(p.X()) && std::isfinite(p.Y()) && std::isfinite(p.Z()) &&
         (p.X() != 0. || p.Y() != 0. || p.Z() != 0.);
}

supera::GeneratorStep step(const simb::MCParticle &input, std::size_t index,
                           Normalization normalize) {
  normalize.step = supera::record_index(index);
  const auto &position = input.Position(index);
  const auto &momentum = input.Momentum(index);

  return {{normalize.measure(position.X(), "position.X()"),
           normalize.measure(position.Y(), "position.Y()"),
           normalize.measure(position.Z(), "position.Z()")},
          normalize.measure(position.T(), "position.T()"),
          {normalize.measure(momentum.X(), "momentum.X()"),
           normalize.measure(momentum.Y(), "momentum.Y()"),
           normalize.measure(momentum.Z(), "momentum.Z()")},
          normalize.measure(momentum.T(), "momentum.T()")};
}

supera::GeneratorParticle particle(const simb::MCParticle &input, supera::RecordIndex index,
                                   Normalization normalize) {
  // Preserve native identity and generator status independently of output labels.
  supera::GeneratorParticle result;
  result.particle_index = index;
  normalize.particle = index;
  result.track_id = normalize.code(input.TrackId(), std::numeric_limits<int>::min(), "TrackId");
  result.pdg = normalize.code(input.PdgCode(), std::numeric_limits<int>::min(), "PdgCode")
                   .value_or(supera::unspecified_pdg);
  result.status = normalize.code(input.StatusCode(), std::numeric_limits<int>::min(), "StatusCode");
  result.process = input.Process();

  // An absent trajectory is meaningful until origin/status selection is applied.
  if (input.NumberTrajectoryPoints())
    result.initial = step(input, 0, normalize);

  return result;
}

} // namespace

std::vector<supera::GeneratorRecord> convert_generator(const std::vector<simb::MCTruth> &truths,
                                                       NormalizationReport *report) {
  std::vector<supera::GeneratorRecord> result;
  result.reserve(truths.size());

  for (const auto &input : truths) {
    // Keep the original MCTruth index, even if a later selection removes entries.
    auto &output = result.emplace_back();
    output.truth_index = supera::record_index(result.size() - 1);
    output.origin = input.Origin();
    const Normalization normalize{report, output.truth_index};

    // Copy every particle's fields before applying the shared status policy.
    for (int index = 0; index < input.NParticles(); ++index)
      output.particles.push_back(particle(input.GetParticle(index), index, normalize));

    // Neutrino metadata and its trajectory form a separate interaction record.
    if (!input.NeutrinoSet())
      continue;

    const auto &interaction = input.GetNeutrino();
    const auto &incoming = interaction.Nu();
    auto &neutrino = output.neutrino.emplace();
    neutrino.incoming = particle(incoming, supera::missing_index, normalize);
    neutrino.lepton_track_id = normalize.code(interaction.Lepton().TrackId(),
                                              std::numeric_limits<int>::min(), "lepton_track_id");
    neutrino.current = normalize.code(interaction.CCNC(), std::numeric_limits<int>::min(), "CCNC");
    neutrino.mode = normalize.code(interaction.Mode(), std::numeric_limits<int>::min(), "Mode");
    neutrino.interaction_type = normalize.code(interaction.InteractionType(),
                                               std::numeric_limits<int>::min(), "InteractionType");
    neutrino.target =
        normalize.code(interaction.Target(), std::numeric_limits<int>::min(), "Target");
    neutrino.nucleon =
        normalize.code(interaction.HitNuc(), std::numeric_limits<int>::min(), "HitNuc");
    neutrino.quark =
        normalize.code(interaction.HitQuark(), std::numeric_limits<int>::min(), "HitQuark");

    // Retain source units and the native theta calculation without recomputation.
    neutrino.hadronic_mass_gev =
        normalize.measure(interaction.W(), "W", std::numeric_limits<double>::min());
    neutrino.bjorken_x =
        normalize.measure(interaction.X(), "X", std::numeric_limits<double>::min());
    neutrino.inelasticity =
        normalize.measure(interaction.Y(), "Y", std::numeric_limits<double>::min());
    neutrino.momentum_transfer_gev2 =
        normalize.measure(interaction.QSqr(), "QSqr", std::numeric_limits<double>::min());

    // Theta() dereferences momentum point zero on both particles.
    if (has_direction(incoming) && has_direction(interaction.Lepton()))
      neutrino.theta_rad = normalize.measure(interaction.Theta(), "theta");

    // Preserve every incoming-neutrino trajectory point in source order.
    for (std::size_t index = 0; index < incoming.NumberTrajectoryPoints(); ++index)
      neutrino.trajectory.push_back(step(incoming, index, normalize));
  }

  return result;
}

} // namespace larsoft2supera
