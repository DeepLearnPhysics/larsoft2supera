#include "larsoft2supera/AuxDet.h"

namespace larsoft2supera {

std::vector<supera::AuxDetSpacePoint> convert_crt_hits(const std::vector<sbn::crt::CRTHit> &hits,
                                                       NormalizationReport *report) {
  // Allocate owning records and retain the source collection order.
  std::vector<supera::AuxDetSpacePoint> result;
  result.reserve(hits.size());

  for (const auto &input : hits) {
    auto &output = result.emplace_back();
    output.id = supera::record_index(result.size() - 1);
    const Normalization normalize{report, output.id};

    // Copy reconstructed position, uncertainty, and total signal.
    output.position = supera::AuxDetPosition{
        {normalize.measure(input.x_pos, "x_pos", sbn::crt::CRTHit::kInvalidF),
         normalize.measure(input.y_pos, "y_pos", sbn::crt::CRTHit::kInvalidF),
         normalize.measure(input.z_pos, "z_pos", sbn::crt::CRTHit::kInvalidF)},
        std::array<double, 3>{
            normalize.measure(input.x_err, "x_err", sbn::crt::CRTHit::kInvalidF),
            normalize.measure(input.y_err, "y_err", sbn::crt::CRTHit::kInvalidF),
            normalize.measure(input.z_err, "z_err", sbn::crt::CRTHit::kInvalidF)}};
    output.signal =
        supera::AuxDetSignal{normalize.measure(input.peshit, "peshit", sbn::crt::CRTHit::kInvalidF),
                             supera::SignalUnit::Photoelectron};

    // These fields are not interchangeable clock estimates. Keep their source
    // names and numeric types, including the exact 64-bit seconds counter.
    using supera::TimeReference;
    using supera::TimeUnit;
    output.times = {
        {"ts0_s",
         input.ts0_s,
         TimeUnit::Second,
         TimeReference::SourceDefined,
         "sbn_crt_source",
         {}},
        {"ts0_s_corr",
         normalize.measure(input.ts0_s_corr, "ts0_s_corr", sbn::crt::CRTHit::kInvalidD),
         TimeUnit::Second,
         TimeReference::SourceDefined,
         "sbn_crt_source",
         {}},
        {"ts0_ns",
         normalize.measure(input.ts0_ns, "ts0_ns", sbn::crt::CRTHit::kInvalidD),
         TimeUnit::Nanosecond,
         TimeReference::SourceDefined,
         "sbn_crt_source",
         {}},
        {"ts0_ns_corr",
         normalize.measure(input.ts0_ns_corr, "ts0_ns_corr", sbn::crt::CRTHit::kInvalidD),
         TimeUnit::Nanosecond,
         TimeReference::SourceDefined,
         "sbn_crt_source",
         {}},
        {"ts1_ns",
         normalize.measure(input.ts1_ns, "ts1_ns", sbn::crt::CRTHit::kInvalidD),
         TimeUnit::Nanosecond,
         TimeReference::SourceDefined,
         "sbn_crt_source",
         {}}};

    if (!normalize.code(input.ts0_s, std::numeric_limits<std::uint64_t>::max(), "ts0_s"))
      output.times.front().value = std::monostate{};

    // Keep hardware identities and channel contributions, including empty groups.
    output.readout_ids.assign(input.feb_id.begin(), input.feb_id.end());
    for (const auto &[id, channels] : input.pesmap) {
      auto &readout = output.readouts.emplace_back();
      readout.id = id;
      for (const auto &[channel, value] : channels)
        readout.channels.push_back(
            {channel,
             {normalize.measure(value, "channel_signal"), supera::SignalUnit::Photoelectron}});
    }

    // Preserve source annotations without inferring reconstruction completeness.
    output.plane = normalize.code(input.plane, -1, "plane");
    output.region = input.tagger;
  }

  return result;
}
} // namespace larsoft2supera
