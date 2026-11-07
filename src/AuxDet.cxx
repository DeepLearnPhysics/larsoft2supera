#include "larsoft2supera/AuxDet.h"

namespace larsoft2supera {
std::vector<supera::AuxDetSpacePoint> convert_crt_hits(const std::vector<sbn::crt::CRTHit> &hits) {
  // Allocate owning records and retain the source collection order.
  std::vector<supera::AuxDetSpacePoint> result;
  result.reserve(hits.size());

  for (const auto &input : hits) {
    auto &output = result.emplace_back();
    output.id = result.size() - 1;

    // Copy reconstructed position, uncertainty, and total signal.
    output.position =
        supera::AuxDetPosition{{input.x_pos, input.y_pos, input.z_pos},
                               std::array<double, 3>{input.x_err, input.y_err, input.z_err}};
    output.signal = supera::AuxDetSignal{input.peshit, supera::SignalUnit::Photoelectron};

    // These fields are not interchangeable clock estimates. Keep their source
    // names and numeric types, including the exact 64-bit seconds counter.
    using supera::TimeReference;
    using supera::TimeUnit;
    output.times = {{"ts0_s",
                     input.ts0_s,
                     TimeUnit::Second,
                     TimeReference::SourceDefined,
                     "sbn_crt_source",
                     {}},
                    {"ts0_s_corr",
                     input.ts0_s_corr,
                     TimeUnit::Second,
                     TimeReference::SourceDefined,
                     "sbn_crt_source",
                     {}},
                    {"ts0_ns",
                     input.ts0_ns,
                     TimeUnit::Nanosecond,
                     TimeReference::SourceDefined,
                     "sbn_crt_source",
                     {}},
                    {"ts0_ns_corr",
                     input.ts0_ns_corr,
                     TimeUnit::Nanosecond,
                     TimeReference::SourceDefined,
                     "sbn_crt_source",
                     {}},
                    {"ts1_ns",
                     input.ts1_ns,
                     TimeUnit::Nanosecond,
                     TimeReference::SourceDefined,
                     "sbn_crt_source",
                     {}}};

    // Keep hardware identities and channel contributions, including empty groups.
    output.readout_ids.assign(input.feb_id.begin(), input.feb_id.end());
    for (const auto &[id, channels] : input.pesmap) {
      auto &readout = output.readouts.emplace_back();
      readout.id = id;
      for (const auto &[channel, value] : channels)
        readout.channels.push_back({channel, {value, supera::SignalUnit::Photoelectron}});
    }

    // Preserve source annotations without inferring reconstruction completeness.
    output.plane = input.plane;
    output.region = input.tagger;
  }

  return result;
}
} // namespace larsoft2supera
