#ifndef LARSOFT2SUPERA_READTRANSPORT_H
#define LARSOFT2SUPERA_READTRANSPORT_H

#include "larsoft2supera/Transport.h"

#include "canvas/Persistency/Common/FindOneP.h"
#include "canvas/Utilities/InputTag.h"
#include "lardataobj/Simulation/GeneratedParticleInfo.h"
#include "lardataobj/Simulation/ParticleAncestryMap.h"
#include "nusimdata/SimulationBase/MCTruth.h"

#include <sstream>

namespace larsoft2supera {
struct TransportInput {
  art::InputTag particles;
  supera::TransportStorage storage;
  std::optional<art::InputTag> truth_associations;
};

/// Read explicit products in one track-ID namespace. Missing configured products fail.
/// Associations remain explicit source evidence; inferred affiliations are separate.
template <class Event>
supera::TransportCollection read_transport(const Event &event,
                                           const std::vector<TransportInput> &sources,
                                           const std::vector<art::InputTag> &ancestry_maps,
                                           NormalizationReport *report = nullptr) {
  supera::TransportCollection result;
  std::set<art::ProductID> products;
  for (const auto &source : sources) {
    const auto handle =
        event.template getValidHandle<std::vector<simb::MCParticle>>(source.particles);
    if (!products.insert(handle.id()).second)
      throw std::invalid_argument("Repeated transport product");
    auto particles = convert_transport(*handle, source.storage, report);
    std::ostringstream identity;
    identity << handle.id();
    for (auto &particle : particles)
      particle.product = source.particles.encode() + "@" + identity.str();

    // FindOneP rejects multiplicity; an unavailable link is not silently inferred.
    if (source.truth_associations) {
      art::FindOneP<simb::MCTruth, sim::GeneratedParticleInfo> links(handle, event,
                                                                     *source.truth_associations);
      if (!links.isValid())
        throw std::runtime_error("Required transport truth associations are missing");
      for (std::size_t i = 0; i < particles.size(); ++i) {
        const auto &truth = links.at(i);
        const auto info = links.data(i);
        if (truth.isNull())
          continue;
        if (!truth.isAvailable() || !info.isValid())
          throw std::runtime_error("Unavailable transport truth association");
        std::ostringstream product;
        product << truth.id();
        supera::TruthLink link{product.str(), supera::record_index(truth.key()),
                               supera::missing_index, int(truth->Origin())};
        if (info.ref().hasGeneratedParticleIndex()) {
          const auto index = info.ref().generatedParticleIndex();
          if (index >= static_cast<std::size_t>(truth->NParticles()))
            throw std::out_of_range("Generated particle index exceeds MCTruth");
          link.generated_index = supera::record_index(index);
        }
        particles[i].truth = std::move(link);
      }
    }
    result.particles.insert(result.particles.end(), std::make_move_iterator(particles.begin()),
                            std::make_move_iterator(particles.end()));
  }

  // Retain all map entries, including links to particles absent from the inventory.
  for (const auto &tag : ancestry_maps) {
    const auto map = event.template getValidHandle<sim::ParticleAncestryMap>(tag);
    for (const auto &[ancestor, descendants] : map->GetMap())
      result.suppression[ancestor].insert(descendants.begin(), descendants.end());
  }
  return result;
}
} // namespace larsoft2supera
#endif
