#ifndef LARSOFT2SUPERA_TRANSPORT_H
#define LARSOFT2SUPERA_TRANSPORT_H

#include "larsoft2supera/Normalization.h"

#include "nusimdata/SimulationBase/MCParticle.h"
#include "supera/data/Transport.h"

namespace larsoft2supera {

/// Copy every persisted particle and trajectory point, with no selection or grouping.
std::vector<supera::TransportParticle> convert_transport(const std::vector<simb::MCParticle> &input,
                                                         supera::TransportStorage storage,
                                                         NormalizationReport *report = nullptr);

} // namespace larsoft2supera
#endif
