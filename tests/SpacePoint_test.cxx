#include "larsoft2supera/SpacePoint.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
}

int main() try {
  using namespace larsoft2supera;
  double xyz[]{-1., 2., 3.};
  double values[]{1., 12.5, 3., -.25, 5., 6.};
  std::vector<recob::SpacePoint> input{{xyz, values, 4.5, 77}};
  auto generic = convert_spacepoints(input, SpacePointProfile::Covariance);
  auto cluster = convert_spacepoints(input, SpacePointProfile::IcarusCluster3D);
  input.clear();

  require(generic.points.size() == 1 && generic.points[0].source_id == 77 &&
              generic.points[0].position_cm == std::array<double,3>{-1.,2.,3.} &&
              generic.points[0].chi_square == 4.5, "Owning point fields");
  require(generic.covariance_cm2 && generic.covariance_cm2->front() ==
              std::array<double,6>{1.,12.5,3.,-.25,5.,6.} && !generic.charge && !generic.associated_hits,
          "Generic profile preserves true covariance");
  require(!cluster.covariance_cm2 && cluster.charge->front() == 12.5 &&
              cluster.charge_asymmetry->front() == -.25 && !cluster.associated_hits,
          "ICARUS profile is explicitly different");
  require(convert_spacepoints({}, SpacePointProfile::IcarusCluster3D).charge->empty(),
          "Present empty column");

  bool rejected = false;
  try {
    convert_spacepoints({}, static_cast<SpacePointProfile>(99));
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "Unknown profiles are rejected");

  // Native absence, signed identity, and partial nonfinite measurements.
  xyz[0] = std::numeric_limits<double>::infinity();
  values[1] = std::numeric_limits<double>::quiet_NaN();
  input = {{xyz, values, 0., -7}, {xyz, values, 0., recob::SpacePoint::InvalidID}, {}};
  NormalizationReport report;
  const auto normalized = convert_spacepoints(input, SpacePointProfile::IcarusCluster3D, &report);
  require(normalized.points[0].source_id == -7 && !normalized.points[1].source_id &&
              !normalized.points[2].source_id && std::isnan(normalized.points[0].position_cm[0]) &&
              normalized.points[0].position_cm[1] == 2. && std::isnan(normalized.charge->front()),
          "Canonical missingness");
  require(report.counts[0] == 2 && report.counts[1] == 2 && report.counts[2] == 2, "Diagnostic reasons");

  // Default hit fields and a fully populated hit are checked independently.
  const auto missing = convert_wire_hit(recob::Hit{});
  require(!missing.channel && !missing.wire && !missing.local_index && !missing.degrees_of_freedom &&
              std::isnan(missing.peak_error_tick) && std::isnan(missing.amplitude_error_adc) &&
              std::isnan(missing.integral_error_adc_tick) && missing.integral_adc_tick == 0.,
          "Field-specific Hit defaults");
  const recob::Hit hit(12, 3, 8, 5.F, .5F, 1.F, -2.F, .25F, 10.F, 11.F, 12.F, .75F,
                       2, 1, 1.5F, 4, geo::kU, geo::kInduction, geo::WireID{1,2,0,123});
  const auto saved = convert_wire_hit(hit);
  require(saved.channel == 12 && saved.wire && saved.wire->cryostat == 1 && saved.wire->tpc == 2 &&
              saved.wire->plane == 0 && saved.wire->wire == 123 && saved.start_tick == 3. &&
              saved.end_tick == 8. && saved.peak_tick == 5. && saved.peak_error_tick == .5 &&
              saved.rms_tick == 1. && saved.amplitude_adc == -2. && saved.amplitude_error_adc == .25 &&
              saved.roi_sum_adc == 10. && saved.hit_sum_adc == 11. && saved.integral_adc_tick == 12. &&
              saved.integral_error_adc_tick == .75 && saved.goodness_of_fit == 1.5 &&
              saved.multiplicity == 2 && saved.local_index == 1 && saved.degrees_of_freedom == 4 &&
              saved.view_code == geo::kU && saved.signal_type_code == geo::kInduction,
          "All Hit fields and units");
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
