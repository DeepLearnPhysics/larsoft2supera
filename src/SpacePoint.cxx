#include "larsoft2supera/SpacePoint.h"

namespace larsoft2supera {

supera::SpacePointCollection convert_spacepoints(const std::vector<recob::SpacePoint> &points,
                                                 SpacePointProfile profile,
                                                 NormalizationReport *report) {
  // Allocate columns once. ICARUS's charge slots must never masquerade as covariance.
  supera::SpacePointCollection output;
  output.points.reserve(points.size());
  if (profile == SpacePointProfile::Covariance) {
    output.covariance_cm2.emplace();
    output.covariance_cm2->reserve(points.size());
  } else if (profile == SpacePointProfile::IcarusCluster3D) {
    output.charge.emplace();
    output.charge_asymmetry.emplace();
    output.charge->reserve(points.size());
    output.charge_asymmetry->reserve(points.size());
    output.charge_convention = "ICARUS cluster3D total charge, source units";
  } else {
    throw std::invalid_argument("Unknown SpacePoint input profile");
  }

  for (const auto &point : points) {
    const Normalization normalize{report, supera::record_index(output.points.size())};
    supera::SpacePoint record;
    for (std::size_t axis = 0; axis < 3; ++axis)
      record.position_cm[axis] = normalize.measure(point.XYZ()[axis], "position_cm");
    record.chi_square = normalize.measure(point.Chisq(), "chi_square");

    // The native default constructor and public InvalidID disagree. Recognize both.
    if (point.ID() == -1 || point.ID() == recob::SpacePoint::InvalidID)
      normalize.note("source_id", MissingReason::SourceSentinel);
    else
      record.source_id = point.ID();
    output.points.push_back(record);

    if (output.covariance_cm2) {
      std::array<double, 6> covariance;
      for (std::size_t index = 0; index < covariance.size(); ++index)
        covariance[index] = normalize.measure(point.ErrXYZ()[index], "covariance_cm2");
      output.covariance_cm2->push_back(covariance);
    } else {
      output.charge->push_back(normalize.measure(point.ErrXYZ()[1], "charge"));
      output.charge_asymmetry->push_back(normalize.measure(point.ErrXYZ()[3], "charge_asymmetry"));
    }
  }
  return output;
}

supera::WireHit convert_wire_hit(const recob::Hit &hit, const Normalization &normalize) {
  supera::WireHit output;
  output.channel = normalize.code(hit.Channel(), raw::InvalidChannelID, "channel");
  const auto &wire = hit.WireID();
  if (wire.isValid)
    output.wire = supera::WireAddress{wire.Cryostat, wire.TPC, wire.Plane, wire.Wire};
  else
    normalize.note("wire", MissingReason::SourceSentinel);
  output.view_code = hit.View();
  output.signal_type_code = hit.SignalType();

  // Normalize only documented field defaults, preserving finite negative measurements.
  output.start_tick = normalize.measure(hit.StartTick(), "start_tick");
  output.end_tick = normalize.measure(hit.EndTick(), "end_tick");
  output.peak_tick = normalize.measure(hit.PeakTime(), "peak_tick");
  output.peak_error_tick = normalize.measure(hit.SigmaPeakTime(), "peak_error_tick", -1.);
  output.rms_tick = normalize.measure(hit.RMS(), "rms_tick");
  output.amplitude_adc = normalize.measure(hit.PeakAmplitude(), "amplitude_adc");
  output.amplitude_error_adc =
      normalize.measure(hit.SigmaPeakAmplitude(), "amplitude_error_adc", -1.);
  output.roi_sum_adc = normalize.measure(hit.ROISummedADC(), "roi_sum_adc");
  output.hit_sum_adc = normalize.measure(hit.HitSummedADC(), "hit_sum_adc");
  output.integral_adc_tick = normalize.measure(hit.Integral(), "integral_adc_tick");
  output.integral_error_adc_tick =
      normalize.measure(hit.SigmaIntegral(), "integral_error_adc_tick", -1.);
  output.goodness_of_fit = normalize.measure(hit.GoodnessOfFit(), "goodness_of_fit");
  output.multiplicity = hit.Multiplicity();
  output.local_index = normalize.code(static_cast<int>(hit.LocalIndex()), -1, "local_index");
  output.degrees_of_freedom = normalize.code(hit.DegreesOfFreedom(), -1, "degrees_of_freedom");
  return output;
}

} // namespace larsoft2supera
