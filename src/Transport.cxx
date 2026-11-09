#include "larsoft2supera/Transport.h"

namespace larsoft2supera {
std::vector<supera::TransportParticle> convert_transport(const std::vector<simb::MCParticle> &input,
                                                         supera::TransportStorage storage,
                                                         NormalizationReport *report) {
  std::vector<supera::TransportParticle> result;
  result.reserve(input.size());
  const auto invalid = std::numeric_limits<int>::min();

  for (const auto &source : input) {
    auto &particle = result.emplace_back();
    particle.source_index = supera::record_index(result.size() - 1);
    Normalization normalize{report, particle.source_index};

    // Preserve signed identities and source processes before resolving ancestry.
    particle.track_id = normalize.code(source.TrackId(), invalid, "TrackId");
    particle.mother_id = normalize.code(source.Mother(), invalid, "Mother");
    particle.pdg = normalize.code(source.PdgCode(), invalid, "PdgCode").value_or(0);
    particle.status = normalize.code(source.StatusCode(), invalid, "StatusCode");
    particle.process = source.Process();
    particle.end_process = source.EndProcess();
    particle.mass_gev = normalize.measure(source.Mass(), "Mass", double(invalid));
    particle.weight = normalize.measure(source.Weight(), "Weight");
    const auto &polarization = source.Polarization();
    particle.polarization = {normalize.measure(polarization.X(), "polarization.x"),
                             normalize.measure(polarization.Y(), "polarization.y"),
                             normalize.measure(polarization.Z(), "polarization.z")};
    const auto vertex = source.GetGvtx();
    particle.generator_vertex = {
        normalize.measure(vertex.X(), "Gvx"), normalize.measure(vertex.Y(), "Gvy"),
        normalize.measure(vertex.Z(), "Gvz"), normalize.measure(vertex.T(), "Gvt")};
    particle.rescatter = normalize.code(source.Rescatter(), invalid, "Rescatter");
    for (const auto &[index, code] : source.Trajectory().TrajectoryProcesses()) {
      if (index >= source.NumberTrajectoryPoints())
        throw std::out_of_range("Trajectory process index exceeds stored trajectory");
      particle.trajectory_processes.push_back(
          {supera::record_index(index), code, source.Trajectory().KeyToProcess(code)});
    }
    particle.storage = storage;
    for (int i = 0; i < source.NumberDaughters(); ++i)
      particle.daughter_ids.push_back(source.Daughter(i));

    // Endpoints-only storage remains distinguishable from the retained collection.
    particle.trajectory.reserve(source.NumberTrajectoryPoints());
    for (std::size_t i = 0; i < source.NumberTrajectoryPoints(); ++i) {
      normalize.step = supera::record_index(i);
      const auto &x = source.Position(i);
      const auto &p = source.Momentum(i);
      particle.trajectory.push_back(
          {{normalize.measure(x.X(), "x"), normalize.measure(x.Y(), "y"),
            normalize.measure(x.Z(), "z")},
           normalize.measure(x.T(), "t"),
           {normalize.measure(p.X(), "px"), normalize.measure(p.Y(), "py"),
            normalize.measure(p.Z(), "pz")},
           normalize.measure(p.T(), "energy")});
    }
  }
  return result;
}
} // namespace larsoft2supera
