#ifndef LARSOFT2SUPERA_GENERATOR_H
#define LARSOFT2SUPERA_GENERATOR_H

#include "larsoft2supera/Normalization.h"

#include "nusimdata/SimulationBase/MCTruth.h"
#include "supera/data/Generator.h"

#include <vector>

namespace larsoft2supera {

/// Copy generator records without origin/status selection or legacy output policy.
/// Preserve original indices and signed track IDs; returned records own all values.
/// Missing trajectories stay absent for the selection step to handle explicitly.
std::vector<supera::GeneratorRecord> convert_generator(const std::vector<simb::MCTruth> &truths,
                                                       NormalizationReport *report = nullptr);

} // namespace larsoft2supera

#endif
