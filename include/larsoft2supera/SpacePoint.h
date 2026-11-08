#ifndef LARSOFT2SUPERA_SPACEPOINT_H
#define LARSOFT2SUPERA_SPACEPOINT_H

#include "larsoft2supera/Normalization.h"
#include "supera/data/SpacePoint.h"

#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/SpacePoint.h"

namespace larsoft2supera {

/// Never infer the meaning of ErrXYZ from the detector name or values.
enum class SpacePointProfile { Covariance, IcarusCluster3D };

supera::SpacePointCollection convert_spacepoints(const std::vector<recob::SpacePoint> &points,
                                                 SpacePointProfile profile,
                                                 NormalizationReport *report = nullptr);
supera::WireHit convert_wire_hit(const recob::Hit &hit, const Normalization &normalize = {});

} // namespace larsoft2supera
#endif
