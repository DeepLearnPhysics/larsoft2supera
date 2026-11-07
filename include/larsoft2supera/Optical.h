#ifndef LARSOFT2SUPERA_OPTICAL_H
#define LARSOFT2SUPERA_OPTICAL_H

#include "larsoft2supera/Normalization.h"

#include "lardataobj/RecoBase/OpFlash.h"
#include "supera/data/Optical.h"

#include <vector>

namespace larsoft2supera {

/// Copy all stored flash fields with canonical missingness and unchanged units/order.
/// IDs restart at zero for each collection. Returned records own their vectors.
/// Inputs must have initialized fields; no physical validity cuts are applied.
std::vector<supera::OpticalFlash> convert_flashes(const std::vector<recob::OpFlash> &flashes,
                                                  NormalizationReport *report = nullptr);

} // namespace larsoft2supera

#endif
