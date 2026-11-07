#include "larsoft2supera/Generator.h"

namespace larsoft2supera {
namespace {

supera::GeneratorStep step(const simb::MCParticle &input, std::size_t index) {
  const auto &position = input.Position(index);
  const auto &momentum = input.Momentum(index);

  return {{position.X(), position.Y(), position.Z()},
          position.T(),
          {momentum.X(), momentum.Y(), momentum.Z()},
          momentum.T()};
}

supera::GeneratorParticle particle(const simb::MCParticle &input,
                                   std::optional<std::uint64_t> index) {
  // Preserve native identity and generator status independently of output labels.
  supera::GeneratorParticle result;
  result.particle_index = index;
  result.track_id = input.TrackId();
  result.pdg = input.PdgCode();
  result.status = input.StatusCode();
  result.process = input.Process();

  // An absent trajectory is meaningful until origin/status selection is applied.
  if (input.NumberTrajectoryPoints())
    result.initial = step(input, 0);

  return result;
}

} // namespace

std::vector<supera::GeneratorRecord> convert_generator(const std::vector<simb::MCTruth> &truths) {
  std::vector<supera::GeneratorRecord> result;
  result.reserve(truths.size());

  for (const auto &input : truths) {
    // Keep the original MCTruth index, even if a later selection removes entries.
    auto &output = result.emplace_back();
    output.truth_index = result.size() - 1;
    output.origin = input.Origin();

    // Copy every particle's fields before applying the shared status policy.
    for (int index = 0; index < input.NParticles(); ++index)
      output.particles.push_back(particle(input.GetParticle(index), index));

    // Neutrino metadata and its trajectory form a separate interaction record.
    if (!input.NeutrinoSet())
      continue;

    const auto &interaction = input.GetNeutrino();
    const auto &incoming = interaction.Nu();
    auto &neutrino = output.neutrino.emplace();
    neutrino.incoming = particle(incoming, {});
    neutrino.lepton_track_id = interaction.Lepton().TrackId();
    neutrino.current = interaction.CCNC();
    neutrino.mode = interaction.Mode();
    neutrino.interaction_type = interaction.InteractionType();
    neutrino.target = interaction.Target();
    neutrino.nucleon = interaction.HitNuc();
    neutrino.quark = interaction.HitQuark();

    // Retain source units and the native theta calculation without recomputation.
    neutrino.hadronic_mass_gev = interaction.W();
    neutrino.bjorken_x = interaction.X();
    neutrino.inelasticity = interaction.Y();
    neutrino.momentum_transfer_gev2 = interaction.QSqr();

    // Theta() dereferences momentum point zero on both particles.
    if (incoming.NumberTrajectoryPoints() && interaction.Lepton().NumberTrajectoryPoints())
      neutrino.theta_rad = interaction.Theta();

    // Preserve every incoming-neutrino trajectory point in source order.
    for (std::size_t index = 0; index < incoming.NumberTrajectoryPoints(); ++index)
      neutrino.trajectory.push_back(step(incoming, index));
  }

  return result;
}

} // namespace larsoft2supera
