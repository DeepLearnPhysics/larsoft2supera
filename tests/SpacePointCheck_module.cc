#include "larsoft2supera/AuxDet.h"
#include "larsoft2supera/Generator.h"
#include "larsoft2supera/Optical.h"
#include "larsoft2supera/ReadSpacePoint.h"

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "fhiclcpp/ParameterSet.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace larsoft2supera {
namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

bool same(double actual, double expected) {
  return actual == expected || (std::isnan(actual) && !std::isfinite(expected));
}
} // namespace

class SpacePointCheck : public art::EDAnalyzer {
public:
  explicit SpacePointCheck(const fhicl::ParameterSet &config)
      : art::EDAnalyzer(config), inputs_(config.get<std::vector<std::string>>("Inputs")),
        hits_(config.get<std::vector<std::string>>("HitProducts", {})),
        with_hits_(config.get<bool>("WithHits", true)), verify_(config.get<bool>("Verify", true)),
        fixture_(config.get<bool>("Fixture", false)),
        association_(config.get<std::string>("Association", "")),
        csv_(config.get<std::string>("Report")), other_(config.get<bool>("OtherProducts", false)) {
    if (other_) {
      product_csv_.open(config.get<std::string>("ProductReport"));
      if (!product_csv_)
        throw std::runtime_error("Cannot open product timing report");
      product_csv_ << "event,source,objects,access_s,conversion_s\n";
    }
    if (!csv_)
      throw std::runtime_error("Cannot open timing report");
    csv_ << "event,source,points,links,unique_hits,owned_capacity_bytes,point_access_s,point_"
            "convert_s,hit_access_s,association_access_s,hit_convert_links_s\n";
  }

  template <class Product, class Convert>
  void benchmark(const art::Event &event, const std::string &source, Convert convert) {
    const auto start = std::chrono::steady_clock::now();
    const auto native = event.getValidHandle<std::vector<Product>>(art::InputTag(source));
    const auto loaded = std::chrono::steady_clock::now();
    NormalizationReport report;
    const auto output = convert(*native, &report);
    const auto converted = std::chrono::steady_clock::now();
    require(output.size() == native->size(), "Benchmark converter count");
    product_csv_ << event.event() << ',' << source << ',' << native->size() << ','
                 << std::chrono::duration<double>(loaded - start).count() << ','
                 << std::chrono::duration<double>(converted - loaded).count() << '\n';
    product_csv_.flush();
  }

  void analyze(const art::Event &event) override {
    if (other_) {
      benchmark<recob::OpFlash>(event, "opflashCryoE::MCstage0", convert_flashes);
      benchmark<recob::OpFlash>(event, "opflashCryoW::MCstage0", convert_flashes);
      benchmark<sbn::crt::CRTHit>(event, "overlayCRTHit::Overlay", convert_crt_hits);
      benchmark<simb::MCTruth>(event, "generator::WireModMuon", convert_generator);
    }
    // Extract each point collection independently; timing stops before verification.
    for (const auto &source : inputs_) {
      const art::InputTag tag(source);
      std::optional<SpacePointHitInput> config;
      if (with_hits_) {
        config.emplace();
        config->associations = association_.empty() ? tag : art::InputTag(association_);
        if (hits_.empty())
          config->products.push_back(tag);
        else
          for (const auto &hit : hits_)
            config->products.emplace_back(hit);
      }
      SpacePointTimings times;
      NormalizationReport report;
      const auto result =
          read_spacepoints(event, tag, SpacePointProfile::IcarusCluster3D, config, &report, &times);
      std::size_t links = 0, unique = 0;
      std::size_t bytes =
          result.points.capacity() * sizeof(supera::SpacePoint) +
          (result.charge->capacity() + result.charge_asymmetry->capacity()) * sizeof(double);
      if (result.associated_hits) {
        const auto &data = *result.associated_hits;
        links = data.links.size();
        bytes += data.links.capacity() * sizeof(supera::HitReference) +
                 data.offsets.capacity() * sizeof(std::uint64_t);
        for (const auto &collection : data.collections) {
          unique += collection.hits.size();
          bytes += collection.hits.capacity() * sizeof(supera::WireHit) +
                   collection.source_keys.capacity() * sizeof(supera::RecordIndex);
        }
      }
      csv_ << event.event() << ',' << source << ',' << result.points.size() << ',' << links << ','
           << unique << ',' << bytes << ',' << times.point_access_seconds << ','
           << times.point_conversion_seconds << ',' << times.hit_access_seconds << ','
           << times.association_access_seconds << ',' << times.hit_conversion_and_links_seconds
           << '\n';
      csv_.flush();

      // Verification is outside the timed regions and can be disabled for repetitions.
      if (verify_) {
        const auto native = event.getValidHandle<std::vector<recob::SpacePoint>>(tag);
        require(result.points.size() == native->size(), "Point count");
        for (std::size_t index = 0; index < native->size(); ++index) {
          const auto &point = native->at(index);
          const bool missing_id = point.ID() == -1 || point.ID() == recob::SpacePoint::InvalidID;
          require(missing_id ? !result.points[index].source_id
                             : result.points[index].source_id == point.ID(),
                  "Native point identity");
          for (std::size_t axis = 0; axis < 3; ++axis)
            require(same(result.points[index].position_cm[axis], point.XYZ()[axis]),
                    "Point coordinate");
          require(same(result.points[index].chi_square, point.Chisq()) &&
                      same(result.charge->at(index), point.ErrXYZ()[1]) &&
                      same(result.charge_asymmetry->at(index), point.ErrXYZ()[3]),
                  "Point measurements");
        }
        if (result.associated_hits) {
          art::FindManyP<recob::Hit> original(native, event, config->associations);
          const auto &data = *result.associated_hits;
          require(data.offsets.size() == native->size() + 1 && data.offsets.front() == 0 &&
                      data.offsets.back() == data.links.size(),
                  "CSR boundaries");
          std::vector<decltype(event.getValidHandle<std::vector<recob::Hit>>(tag))> hit_products;
          for (const auto &collection : data.collections)
            hit_products.push_back(
                event.getValidHandle<std::vector<recob::Hit>>(art::InputTag(collection.source)));
          std::vector<std::pair<std::string, supera::RecordIndex>> expected_keys, actual_keys;
          for (std::size_t index = 0; index < native->size(); ++index) {
            expected_keys.clear();
            actual_keys.clear();
            const auto &pointers = original.at(index);
            require(data.offsets[index + 1] - data.offsets[index] == pointers.size(),
                    "Association count");
            for (const auto &pointer : pointers) {
              std::ostringstream id;
              id << pointer.id();
              expected_keys.emplace_back(id.str(), supera::record_index(pointer.key()));
            }
            for (std::size_t link = 0; link < pointers.size(); ++link) {
              const auto &reference = data.links.at(data.offsets[index] + link);
              const auto &collection = data.collections.at(reference.collection);
              const auto &hit = collection.hits.at(reference.index);
              const auto key = collection.source_keys.at(reference.index);
              actual_keys.emplace_back(collection.source_product, key);
              // FindManyP's order is hash-dependent. Locate the corresponding native hit.
              const auto &product = hit_products.at(reference.collection);
              require(collection.source_size == product->size(), "Full source hit count");
              const auto &raw = product->at(key);
              require(same(hit.integral_adc_tick, raw.Integral()) &&
                          same(hit.amplitude_adc, raw.PeakAmplitude()) &&
                          same(hit.peak_tick, raw.PeakTime()) && same(hit.rms_tick, raw.RMS()) &&
                          hit.multiplicity == raw.Multiplicity(),
                      "Associated hit measurements");
              if (raw.WireID().isValid)
                require(hit.wire && hit.wire->plane == raw.WireID().Plane &&
                            hit.wire->cryostat == raw.WireID().Cryostat &&
                            hit.wire->tpc == raw.WireID().TPC &&
                            hit.wire->wire == raw.WireID().Wire,
                        "Wire address");
            }
            std::sort(expected_keys.begin(), expected_keys.end());
            std::sort(actual_keys.begin(), actual_keys.end());
            require(expected_keys == actual_keys, "Exact product/key association multiset");
          }
        }
      }
      if (fixture_) {
        // Exercise the default API without diagnostics, associations, or timers.
        const auto plain = read_spacepoints(event, tag, SpacePointProfile::IcarusCluster3D);
        require(plain.points.size() == result.points.size() && !plain.associated_hits,
                "Default read is points only");
        const bool empty = event.event() == 2;
        require(result.points.size() == (empty ? 0 : 2), "Fixture point order/clearing");
        if (with_hits_) {
          require(links == (empty ? 0 : 4) && unique == (empty ? 0 : 2),
                  "Shared and duplicate hits across products");
          if (!empty) {
            const auto &data = *result.associated_hits;
            require(data.offsets == std::vector<std::uint64_t>{0, 3, 4} &&
                        data.links[0].collection == 0 && data.links[1].collection == 1 &&
                        data.links[2].collection == 1 && data.links[3].collection == 0,
                    "Serialized within-point order survives interleaved rows");
          }
        } else
          require(!result.associated_hits, "Points-only mode must omit hit information");
      }
    }
  }

private:
  std::vector<std::string> inputs_, hits_;
  bool with_hits_, verify_, fixture_;
  std::string association_;
  std::ofstream csv_;
  bool other_;
  std::ofstream product_csv_;
};
} // namespace larsoft2supera
DEFINE_ART_MODULE(larsoft2supera::SpacePointCheck)
