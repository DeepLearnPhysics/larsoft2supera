#include "larsoft2supera/AuxDet.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace

int main() try {
  using namespace supera;
  require(larsoft2supera::convert_crt_hits({}).empty(), "Empty input");

  // Arrange a source with distinct values, duplicate IDs, and an exact large clock.
  sbn::crt::CRTHit hit;
  hit.feb_id = {255, 0, 255};
  hit.pesmap = {{0, {}}, {255, {{-1, -0.F}, {31, 2.5F}, {31, 7.5F}}}};
  hit.peshit = 42.5F;
  hit.ts0_s = (std::uint64_t{1} << 53) + 1;
  hit.ts0_s_corr = 1.25;
  hit.ts0_ns = -2.5;
  hit.ts0_ns_corr = 3.75;
  hit.ts1_ns = -4.5;
  hit.plane = -1;
  hit.x_pos = -10.F;
  hit.y_pos = 20.F;
  hit.z_pos = 30.F;
  hit.x_err = 1.F;
  hit.y_err = 2.F;
  hit.z_err = 3.F;
  hit.tagger = "test panel";

  // Convert, then mutate the input to verify that the result owns its values.
  auto records = larsoft2supera::convert_crt_hits({hit, hit});
  hit.feb_id.clear();
  hit.pesmap.clear();
  hit.tagger.clear();

  // Check identity and reconstructed quantities independently.
  const auto &point = records.front();
  require(records.size() == 2 && point.id == 0 && records[1].id == 1, "Order and IDs");
  require(point.position && point.position->cm == std::array<double, 3>{-10., 20., 30.},
          "Position");
  require(point.position->uncertainty_cm == std::array<double, 3>{1., 2., 3.}, "Uncertainty");
  require(point.signal && point.signal->value == 42.5 &&
              point.signal->unit == SignalUnit::Photoelectron,
          "Signal");
  require(point.plane == -1 && point.region == "test panel" && !point.complete,
          "Source annotations");

  // Check hardware ordering, empty groups, and channel contributions.
  require(point.readout_ids == std::vector<std::uint64_t>{255, 0, 255},
          "Board order, duplicates and ownership");
  require(point.readouts.size() == 2 && point.readouts[0].id == 0 &&
              point.readouts[0].channels.empty(),
          "Empty map entry");
  const auto &channels = point.readouts[1].channels;
  require(point.readouts[1].id == 255 && channels.size() == 3 && channels[0].channel == -1 &&
              std::signbit(channels[0].signal.value) && channels[1].channel == 31 &&
              channels[1].signal.value == 2.5 && channels[2].channel == 31 &&
              channels[2].signal.value == 7.5,
          "Channel order and values");
  for (const auto &channel : channels)
    require(channel.signal.unit == SignalUnit::Photoelectron, "Channel units");

  // Check all source clocks without deriving one reading from another.
  require(point.times.size() == 5 && point.times[0].name == "ts0_s" &&
              std::get<std::uint64_t>(point.times[0].value) == (std::uint64_t{1} << 53) + 1,
          "Exact integer clock");
  const std::vector<std::string> names{"ts0_s_corr", "ts0_ns", "ts0_ns_corr", "ts1_ns"};
  const std::vector<double> values{1.25, -2.5, 3.75, -4.5};
  for (std::size_t i = 0; i < point.times.size(); ++i) {
    const auto &time = point.times[i];
    require(time.reference == TimeReference::SourceDefined && time.clock == "sbn_crt_source" &&
                !time.uncertainty,
            "Clock convention");
    require(time.unit == (i < 2 ? TimeUnit::Second : TimeUnit::Nanosecond), "Time units");
    if (i)
      require(time.name == names[i - 1] && std::get<double>(time.value) == values[i - 1],
              "Time field");
  }

  // Preserve sentinels and special values instead of introducing validity cuts.
  sbn::crt::CRTHit defaults;
  auto missing = larsoft2supera::convert_crt_hits({defaults}).front();
  require(missing.id == 0 && missing.signal->value == sbn::crt::CRTHit::kInvalidF &&
              std::get<std::uint64_t>(missing.times[0].value) ==
                  std::numeric_limits<std::uint64_t>::max(),
          "Raw sentinels and ID reset");

  defaults.x_pos = std::numeric_limits<float>::infinity();
  defaults.peshit = std::numeric_limits<float>::quiet_NaN();
  auto special = larsoft2supera::convert_crt_hits({defaults}).front();
  require(std::isinf(special.position->cm[0]) && std::isnan(special.signal->value),
          "No physical filtering");

  return 0;

} catch (const std::exception &error) {

  std::cerr << error.what() << '\n';
  return 1;
}
