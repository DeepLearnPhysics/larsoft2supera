#include "larsoft2supera/Optical.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string &description) {
  if (!condition)
    throw std::runtime_error(description);
}

bool same_bits(double left, double right) {
  // This test deliberately distinguishes signed zero and preserves NaN bits.
  // NOLINTNEXTLINE(bugprone-suspicious-memory-comparison)
  return std::memcmp(&left, &right, sizeof(double)) == 0;
}

recob::OpFlash example(double time = -12.5) {
  return {time,  0.25, 123456.5, 77,  {0.0, 2.5, 7.5}, true,      -3, 0.125, -10.0, 1.0,
          -20.0, 2.0,  30.0,     3.0, {4.0, 5.0},      {0.4, 0.5}};
}

void fields_and_ownership() {
  auto input = std::vector<recob::OpFlash>{example(), example(22.5)};
  const auto output = larsoft2supera::convert_flashes(input);
  input.clear();
  require(output.size() == 2, "collection size and owned lifetime");
  const auto &flash = output.front();
  require(flash.id == 0 && output.back().id == 1, "collection-local IDs");
  require(flash.time_us == -12.5 && output.back().time_us == 22.5, "time and ordering");
  require(flash.time_width_us == 0.25, "time width");
  require(flash.absolute_time == 123456.5, "absolute time");
  require(flash.frame == 77, "frame");
  require(flash.pe == std::vector<double>({0.0, 2.5, 7.5}), "PE vector");
  require(flash.in_beam_frame && flash.on_beam_time == -3, "beam flags preserve integer");
  require(flash.fast_to_total == 0.125, "fast fraction");
  require(flash.x_cm == -10.0 && flash.x_width_cm == 1.0, "x fields");
  require(flash.y_cm == -20.0 && flash.y_width_cm == 2.0, "y fields");
  require(flash.z_cm == 30.0 && flash.z_width_cm == 3.0, "z fields");
  require(flash.wire_centers == std::vector<double>({4.0, 5.0}), "wire centers");
  require(flash.wire_widths == std::vector<double>({0.4, 0.5}), "wire widths");
}

void empty_and_event_isolation() {
  require(larsoft2supera::convert_flashes({}).empty(), "first empty event");
  const auto first = larsoft2supera::convert_flashes({example(), example()});
  require(larsoft2supera::convert_flashes({}).empty(), "empty after nonempty");
  const auto next = larsoft2supera::convert_flashes({example(4.0)});
  require(next.size() == 1 && next.front().id == 0, "IDs restart in next collection");
  require(first.size() == 2 && first.front().time_us == -12.5, "previous output unchanged");
}

void numerical_values_are_not_filtered() {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  const auto infinity = std::numeric_limits<double>::infinity();
  const recob::OpFlash input(-0.0, -1.0, infinity, std::numeric_limits<unsigned int>::max(),
                             {nan, -0.0, -2.0}, false, 0, nan, recob::OpFlash::NoCenter,
                             recob::OpFlash::NoCenter, -infinity, 0.0, 0.0, -0.0, {}, {});
  const auto output = larsoft2supera::convert_flashes({input});
  const auto &flash = output.front();
  require(same_bits(flash.time_us, -0.0), "negative zero time");
  require(flash.time_width_us == -1.0, "negative widths are not silently corrected");
  require(flash.absolute_time == infinity && flash.y_cm == -infinity, "infinities");
  require(flash.frame == std::numeric_limits<unsigned int>::max(), "maximum frame");
  require(!flash.in_beam_frame && flash.on_beam_time == 0, "false beam flags");
  require(same_bits(flash.fast_to_total, nan), "NaN representation");
  require(flash.x_cm == recob::OpFlash::NoCenter && flash.x_width_cm == recob::OpFlash::NoCenter,
          "missing x sentinel");
  require(same_bits(flash.z_width_cm, -0.0), "negative zero width");
  require(same_bits(flash.pe[0], nan) && same_bits(flash.pe[1], -0.0) && flash.pe[2] == -2.0,
          "PE values are copied without cleaning");
  require(flash.wire_centers.empty() && flash.wire_widths.empty(), "empty wire vectors");
  const recob::OpFlash no_vectors(0., 0., 0., 0, {}, false, 0, 1., 0., 0., 0., 0., 0., 0.);
  require(larsoft2supera::convert_flashes({no_vectors}).front().pe.empty(), "empty PE vector");
}
} // namespace

int main() {
  try {
    fields_and_ownership();
    empty_and_event_isolation();
    numerical_values_are_not_filtered();
    std::cout
        << "PASS: fields, ownership, ordering, empty collections, event isolation, numeric edges\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
