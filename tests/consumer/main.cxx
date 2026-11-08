#include "larsoft2supera/AuxDet.h"
#include "larsoft2supera/Generator.h"
#include "larsoft2supera/Optical.h"
#include "larsoft2supera/SpacePoint.h"

int main() {
  return larsoft2supera::convert_flashes({}).empty() &&
                 larsoft2supera::convert_crt_hits({}).empty() &&
                 larsoft2supera::convert_generator({}).empty() &&
                 larsoft2supera::convert_spacepoints({},
                                                     larsoft2supera::SpacePointProfile::Covariance)
                     .points.empty()
             ? 0
             : 1;
}
