#ifndef LARSOFT2SUPERA_AUXDET_H
#define LARSOFT2SUPERA_AUXDET_H

#include "sbnobj/Common/CRT/CRTHit.hh"
#include "supera/data/AuxDet.h"

#include <vector>

namespace larsoft2supera {

/// Copy SBN CRT measurements into owning, output-independent auxiliary records.
/// Preserve source units, sentinels and ordering; IDs restart per collection.
/// Timestamp names retain producer-specific semantics; no epoch is inferred.
std::vector<supera::AuxDetSpacePoint> convert_crt_hits(const std::vector<sbn::crt::CRTHit> &hits);

} // namespace larsoft2supera

#endif
