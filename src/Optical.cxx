#include "larsoft2supera/Optical.h"

namespace larsoft2supera {
std::vector<supera::OpticalFlash> convert_flashes(const std::vector<recob::OpFlash> &flashes) {
  // Allocate an owning collection in the original input order.
  std::vector<supera::OpticalFlash> result;
  result.reserve(flashes.size());

  for (const auto &input : flashes) {
    auto &output = result.emplace_back();
    output.id = result.size() - 1;

    // Preserve the source clock readings and frame.
    output.time_us = input.Time();
    output.time_width_us = input.TimeWidth();
    output.absolute_time = input.AbsTime();
    output.frame = input.Frame();

    // Copy light yield and the stored beam/timing summaries.
    output.pe = input.PEs();
    output.in_beam_frame = input.InBeamFrame();
    output.on_beam_time = input.OnBeamTime();
    output.fast_to_total = input.FastToTotal();

    // Keep spatial estimates and their widths in source coordinates.
    output.x_cm = input.XCenter();
    output.x_width_cm = input.XWidth();
    output.y_cm = input.YCenter();
    output.y_width_cm = input.YWidth();
    output.z_cm = input.ZCenter();
    output.z_width_cm = input.ZWidth();

    // Retain the source view ordering for wire estimates.
    output.wire_centers = input.WireCenters();
    output.wire_widths = input.WireWidths();
  }

  return result;
}
} // namespace larsoft2supera
