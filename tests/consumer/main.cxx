#include "larsoft2supera/AuxDet.h"
#include "larsoft2supera/Generator.h"
#include "larsoft2supera/Optical.h"

int main() {
  return larsoft2supera::convert_flashes({}).empty() &&
                 larsoft2supera::convert_crt_hits({}).empty() &&
                 larsoft2supera::convert_generator({}).empty()
             ? 0
             : 1;
}
