#ifndef LARSOFT2SUPERA_OPTICAL_H
#define LARSOFT2SUPERA_OPTICAL_H

#include "lardataobj/RecoBase/OpFlash.h"
#include "supera/data/Optical.h"

#include <vector>

namespace larsoft2supera {

/// Copy all stored flash fields without changing units, sentinels, or ordering.
/// IDs restart at zero for each collection. Returned records own their vectors.
/// Inputs must have initialized fields; no physical validity cuts are applied.
std::vector<supera::OpticalFlash> convert_flashes(const std::vector<recob::OpFlash> &flashes);

} // namespace larsoft2supera

#endif
