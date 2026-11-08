#ifndef LARSOFT2SUPERA_READSPACEPOINT_H
#define LARSOFT2SUPERA_READSPACEPOINT_H

#include "larsoft2supera/SpacePoint.h"

#include "canvas/Persistency/Common/Assns.h"
#include "canvas/Utilities/InputTag.h"

#include <chrono>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace larsoft2supera {

struct SpacePointHitInput {
  art::InputTag associations;
  std::vector<art::InputTag> products; // Explicit allowlist of target Hit products.
};

struct SpacePointTimings {
  double point_access_seconds{};
  double point_conversion_seconds{};
  double hit_access_seconds{};
  double association_access_seconds{};
  double hit_conversion_and_links_seconds{};
};

/// Event-like reader, validated with art::Event. No associations are touched
/// when hit_input is absent. Returned records own their data.
template <class Event>
supera::SpacePointCollection
read_spacepoints(const Event &event, const art::InputTag &tag, SpacePointProfile profile,
                 const std::optional<SpacePointHitInput> &hit_input = {},
                 NormalizationReport *report = nullptr, SpacePointTimings *timings = nullptr) {
  using Clock = std::chrono::steady_clock;
  const auto now = [&] { return timings ? Clock::now() : Clock::time_point{}; };
  auto start = now();
  const auto points = event.template getValidHandle<std::vector<recob::SpacePoint>>(tag);
  const auto point_access = now();
  auto output = convert_spacepoints(*points, profile, report);
  output.source = tag.encode();
  const auto point_conversion = now();
  if (timings) {
    *timings = {};
    timings->point_access_seconds = std::chrono::duration<double>(point_access - start).count();
    timings->point_conversion_seconds =
        std::chrono::duration<double>(point_conversion - point_access).count();
  }
  if (!hit_input)
    return output;

  // Load each allowed target once. Validate keys against its actual vector size,
  // never by dereferencing an unchecked art::Ptr.
  using Handle = decltype(event.template getValidHandle<std::vector<recob::Hit>>(tag));
  std::vector<Handle> handles;
  std::map<art::ProductID, std::size_t> products;
  std::vector<std::vector<supera::RecordIndex>> dense_indices;
  auto &associated = output.associated_hits.emplace();
  for (const auto &source : hit_input->products) {
    auto handle = event.template getValidHandle<std::vector<recob::Hit>>(source);
    if (!products.emplace(handle.id(), handles.size()).second)
      throw std::invalid_argument("Duplicate SpacePoint hit product: " + source.encode());
    dense_indices.emplace_back(handle->size(), supera::missing_index);
    std::ostringstream identity;
    identity << handle.id();
    associated.collections.push_back({identity.str(), source.encode(), handle->size(), {}, {}});
    handles.push_back(std::move(handle));
  }
  const auto hit_access = now();

  // Read the native table directly. Its partner representation handles reversed
  // Assns<Hit, SpacePoint> products without an extra vector for every point.
  const auto associations =
      event.template getValidHandle<art::Assns<recob::SpacePoint, recob::Hit>>(
          hit_input->associations);
  const auto association_access = now();
  if (associations->size() >
      static_cast<std::uint64_t>(std::numeric_limits<supera::RecordIndex>::max()))
    throw std::overflow_error("SpacePoint association count exceeds supported range");
  associated.offsets.assign(points->size() + 1, 0);
  for (const auto &link : *associations) {
    if (link.first.isNull() || link.first.id() != points.id())
      throw std::invalid_argument("Null or foreign SpacePoint reference: " + tag.encode());
    if (link.first.key() >= points->size())
      throw std::out_of_range("SpacePoint key exceeds source collection: " + tag.encode());
    ++associated.offsets[link.first.key() + 1];
  }
  for (std::size_t index = 1; index < associated.offsets.size(); ++index)
    associated.offsets[index] += associated.offsets[index - 1];
  associated.links.resize(associations->size());
  auto next_link = associated.offsets;

  // Stable scatter preserves each point's native link order, including duplicates.
  // Only referenced hits are converted, once per product/key in this read call.
  for (const auto &link : *associations) {
    const auto index = link.first.key();
    const auto &pointer = link.second;
    const auto source = products.find(pointer.id());
    if (pointer.isNull() || source == products.end())
      throw std::invalid_argument("Null or unconfigured hit reference at point " +
                                  std::to_string(index));
    const auto collection = source->second;
    const auto key = pointer.key();
    if (key >= handles[collection]->size())
      throw std::out_of_range("Hit key exceeds source collection at point " +
                              std::to_string(index));
    auto &dense_index = dense_indices[collection][key];
    auto &hits = associated.collections[collection];
    if (dense_index == supera::missing_index) {
      dense_index = supera::record_index(hits.hits.size());
      hits.source_keys.push_back(supera::record_index(key));
      hits.hits.push_back(convert_wire_hit(handles[collection]->at(key),
                                           Normalization{report, supera::record_index(index)}));
    }
    associated.links[next_link[index]++] = {supera::record_index(collection), dense_index};
  }
  if (timings) {
    timings->hit_access_seconds =
        std::chrono::duration<double>(hit_access - point_conversion).count();
    timings->association_access_seconds =
        std::chrono::duration<double>(association_access - hit_access).count();
    timings->hit_conversion_and_links_seconds =
        std::chrono::duration<double>(now() - association_access).count();
  }
  return output;
}

} // namespace larsoft2supera
#endif
