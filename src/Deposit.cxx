#include "larsoft2supera/Deposit.h"

#include "lardataobj/Simulation/sim.h"

namespace larsoft2supera {
namespace {
std::optional<std::int32_t> identity(int raw, Normalization normalize) {
  if (raw == sim::NoParticleId || raw == util::kBogusI) {
    normalize.note("track_id", MissingReason::SourceSentinel);
    return {};
  }
  return raw;
}

void track(supera::Deposit &output, int raw, Normalization normalize) {
  output.raw_track_id = raw;
  output.track_id = identity(raw, normalize);
  if (output.track_id && *output.track_id < 0) {
    output.reassigned = true;
    output.track_id = -*output.track_id; // Native minimum sentinel was rejected before negation.
  }
}
} // namespace

std::vector<supera::Deposit> convert_deposits(const std::vector<sim::SimEnergyDepositLite> &input,
                                              NormalizationReport *report) {
  std::vector<supera::Deposit> result;
  result.reserve(input.size());
  for (const auto &source : input) {
    auto &output = result.emplace_back();
    output.source_index = supera::record_index(result.size() - 1);
    const Normalization normalize{report, output.source_index};
    track(output, source.TrackID(), normalize);
    if (!output.reassigned)
      output.original_id = output.track_id;

    // SEDLite has no original-ID field, electron count, or full step endpoints.
    output.position_cm = {normalize.measure(source.X(), "x"), normalize.measure(source.Y(), "y"),
                          normalize.measure(source.Z(), "z")};
    output.time_ns = normalize.measure(source.Time(), "time");
    output.energy_mev = normalize.measure(source.Energy(), "energy");
  }
  return result;
}

std::vector<supera::Deposit> convert_deposits(const std::vector<sim::SimEnergyDeposit> &input,
                                              NormalizationReport *report) {
  std::vector<supera::Deposit> result;
  result.reserve(input.size());
  for (const auto &source : input) {
    auto &output = result.emplace_back();
    output.source_index = supera::record_index(result.size() - 1);
    const Normalization normalize{report, output.source_index};
    track(output, source.TrackID(), normalize);
    output.raw_original_id = source.OrigTrackID();
    output.original_id = identity(source.OrigTrackID(), normalize);

    // Midpoints support the shared point projection; the full segment remains available.
    output.position_cm = {normalize.measure(source.X(), "x"), normalize.measure(source.Y(), "y"),
                          normalize.measure(source.Z(), "z")};
    output.time_ns = normalize.measure(source.Time(), "time");
    output.energy_mev = normalize.measure(source.Energy(), "energy");
    output.electrons = normalize.measure(source.NumElectrons(), "electrons", util::kBogusI);
    auto &segment = output.segment.emplace();
    segment.start_cm = {normalize.measure(source.StartX(), "start_x"),
                        normalize.measure(source.StartY(), "start_y"),
                        normalize.measure(source.StartZ(), "start_z")};
    segment.end_cm = {normalize.measure(source.EndX(), "end_x"),
                      normalize.measure(source.EndY(), "end_y"),
                      normalize.measure(source.EndZ(), "end_z")};
    segment.start_ns = normalize.measure(source.StartT(), "start_time");
    segment.end_ns = normalize.measure(source.EndT(), "end_time");
    segment.pdg = normalize.code(source.PdgCode(), util::kBogusI, "pdg");
    segment.photons = normalize.measure(source.NumPhotons(), "photons", util::kBogusI);
    segment.scintillation_yield_ratio = normalize.measure(source.ScintYieldRatio(), "yield_ratio");
  }
  return result;
}

std::vector<supera::Deposit> convert_deposits(const std::vector<sim::SimChannel> &input,
                                              NormalizationReport *report) {
  std::vector<supera::Deposit> result;
  for (std::size_t channel = 0; channel < input.size(); ++channel) {
    for (const auto &[tdc, ides] : input[channel].TDCIDEMap()) {
      for (std::size_t i = 0; i < ides.size(); ++i) {
        const auto &source = ides[i];
        auto &output = result.emplace_back();
        output.source_index = supera::record_index(result.size() - 1);
        output.channel_index = supera::record_index(channel);
        output.ide_index = supera::record_index(i);
        output.channel = input[channel].Channel();
        output.tdc = tdc;
        const Normalization normalize{report, output.source_index};
        track(output, source.trackID, normalize);
        output.raw_original_id = source.origTrackID;
        output.original_id = identity(source.origTrackID, normalize);

        // Keep each channel contribution intact. No division by plane count or clock conversion.
        output.position_cm = {normalize.measure(source.x, "x", float(util::kBogusD)),
                              normalize.measure(source.y, "y", float(util::kBogusD)),
                              normalize.measure(source.z, "z", float(util::kBogusD))};
        output.energy_mev = normalize.measure(source.energy, "energy", float(util::kBogusD));
        output.electrons =
            normalize.measure(source.numElectrons, "electrons", float(util::kBogusD));
      }
    }
  }
  return result;
}
} // namespace larsoft2supera
