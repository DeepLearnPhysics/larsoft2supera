#include "larsoft2supera/Optical.h"

namespace larsoft2supera {
std::vector<supera::OpticalFlash> convert_flashes(const std::vector<recob::OpFlash> &flashes,
                                                  NormalizationReport *report) {
  // Allocate an owning collection in the original input order.
  std::vector<supera::OpticalFlash> result;
  result.reserve(flashes.size());

  for (const auto &input : flashes) {
    auto &output = result.emplace_back();
    output.id = supera::record_index(result.size() - 1);
    const Normalization normalize{report, output.id};

    // Preserve the source clock readings and frame.
    output.time_us = normalize.measure(input.Time(), "Time");
    output.time_width_us = normalize.measure(input.TimeWidth(), "TimeWidth");
    output.absolute_time = normalize.measure(input.AbsTime(), "AbsTime");
    output.frame = input.Frame();

    // Copy light yield and the stored beam/timing summaries.
    output.pe = input.PEs();
    for (auto &value : output.pe)
      value = normalize.measure(value, "pe");
    output.in_beam_frame = input.InBeamFrame();
    output.on_beam_time = input.OnBeamTime();
    output.fast_to_total = normalize.measure(input.FastToTotal(), "FastToTotal");

    // Keep spatial estimates and their widths in source coordinates.
    output.x_cm = normalize.measure(input.XCenter(), "XCenter", recob::OpFlash::NoCenter);
    output.x_width_cm = normalize.measure(input.XWidth(), "XWidth", recob::OpFlash::NoCenter);
    output.y_cm = normalize.measure(input.YCenter(), "YCenter");
    output.y_width_cm = normalize.measure(input.YWidth(), "YWidth");
    output.z_cm = normalize.measure(input.ZCenter(), "ZCenter");
    output.z_width_cm = normalize.measure(input.ZWidth(), "ZWidth");

    // Retain the source view ordering for wire estimates.
    output.wire_centers = input.WireCenters();
    for (auto &value : output.wire_centers)
      value = normalize.measure(value, "wire_centers");
    output.wire_widths = input.WireWidths();
    for (auto &value : output.wire_widths)
      value = normalize.measure(value, "wire_widths");
  }

  return result;
}
} // namespace larsoft2supera
