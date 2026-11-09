#include "larsoft2supera/AuxDet.h"
#include "larsoft2supera/Generator.h"
#include "larsoft2supera/Optical.h"
#include "larsoft2supera/ReadDeposit.h"
#include "larsoft2supera/ReadSpacePoint.h"
#include "larsoft2supera/ReadTransport.h"
#include "supera/process/AttributeDeposits.h"
#include "supera/process/ResolveTransport.h"
#include "supera/process/SelectGenerator.h"
#include "supera/process/SelectSpacePoints.h"
#include <chrono>
#include <fstream>

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "supera/io/larcv2/Writer.h"

#include "messagefacility/MessageLogger/MessageLogger.h"
#include <sstream>

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace larsoft2supera {
namespace {
void report_normalization(const NormalizationReport &report, const art::Event &event,
                          const std::string &source) {
  if (report.examples.empty())
    return;
  std::ostringstream message;
  message << "event=" << event.id() << " source=" << source << " sentinels=" << report.counts[0]
          << " nan=" << report.counts[1] << " infinity=" << report.counts[2];
  for (const auto &example : report.examples)
    message << "\nrecord=" << example.record << " particle=" << example.particle
            << " step=" << example.step << " field=" << example.field << " reason="
            << (example.reason == MissingReason::SourceSentinel ? "source-sentinel"
                : example.reason == MissingReason::NotANumber   ? "nan"
                                                                : "infinity");
  mf::LogInfo("SuperaNormalization") << message.str();
}
} // namespace

// Serial analyzer: owns one LArCV2 writer, whose mutable state is not thread-safe.
// This temporary writer keeps the conversion independent of art event access.
class SuperaWriter : public art::EDAnalyzer {
public:
  explicit SuperaWriter(const fhicl::ParameterSet &config);
  void beginJob() override;
  void analyze(const art::Event &event) override;
  void endJob() override;

private:
  std::vector<art::InputTag> inputs_;
  std::vector<std::string> outputs_;
  std::string output_file_;
  std::vector<supera::OpticalConventions> conventions_;
  std::vector<art::InputTag> auxiliary_inputs_;
  std::vector<std::string> auxiliary_outputs_;
  std::vector<supera::AuxDetConventions> auxiliary_conventions_;

  struct GeneratorInput {
    art::InputTag tag;
    bool required;
  };

  struct GeneratorCollection {
    std::vector<GeneratorInput> inputs;
    std::optional<int> origin;
  };

  std::vector<GeneratorCollection> generator_collections_;
  std::vector<std::string> generator_outputs_;

  struct TransportDefinition {
    std::vector<TransportInput> sources;
    std::vector<art::InputTag> maps;
    std::set<std::int32_t> root_mothers;
  };

  std::vector<TransportDefinition> transport_collections_;
  std::vector<std::string> transport_outputs_;

  struct DepositDefinition {
    art::InputTag tag;
    supera::DepositKind kind;
    std::size_t transport;
    std::optional<supera::Grid3D> grid;
  };

  std::vector<DepositDefinition> deposit_collections_;
  std::vector<supera::io::larcv2::DepositOutput> deposit_outputs_;
  std::ofstream deposit_timings_;

  struct PointInput {
    art::InputTag tag;
    SpacePointProfile profile;
    std::optional<SpacePointHitInput> hits;
  };

  struct PointCollection {
    std::vector<PointInput> inputs;
    supera::FixedGrid grid;
    supera::SpacePointSelection selection;
  };

  std::vector<PointCollection> point_collections_;
  std::vector<supera::io::larcv2::SpacePointOutput> point_outputs_;
  std::ofstream timings_;
  std::ofstream transport_timings_;
  std::unique_ptr<supera::EventWriter> writer_;
};

SuperaWriter::SuperaWriter(const fhicl::ParameterSet &config) : art::EDAnalyzer(config) {
  // Read the requested products and require a usable output destination.
  const auto collections = config.get<std::vector<fhicl::ParameterSet>>("Collections", {});
  const auto auxiliary = config.get<std::vector<fhicl::ParameterSet>>("AuxiliaryCollections", {});
  const auto generator = config.get<std::vector<fhicl::ParameterSet>>("GeneratorCollections", {});
  const auto transport = config.get<std::vector<fhicl::ParameterSet>>("TransportCollections", {});
  const auto points = config.get<std::vector<fhicl::ParameterSet>>("SpacePointCollections", {});
  const auto output_file = config.get<std::string>("OutputFile");
  if ((collections.empty() && auxiliary.empty() && generator.empty() && points.empty() &&
       transport.empty()) ||
      output_file.empty()) {
    throw std::invalid_argument("Collections and OutputFile must not be empty");
  }

  for (const auto &collection : transport) {
    TransportDefinition definition;
    const auto name = collection.get<std::string>("OutputLabel");
    if (name.empty() || std::find(transport_outputs_.begin(), transport_outputs_.end(), name) !=
                            transport_outputs_.end())
      throw std::invalid_argument("Transport outputs must be nonempty and unique");
    for (const auto &source : collection.get<std::vector<fhicl::ParameterSet>>("Sources")) {
      const auto storage = source.get<std::string>("Storage");
      if (storage != "Retained" && storage != "Dropped")
        throw std::invalid_argument("Transport Storage must be Retained or Dropped");
      TransportInput input{art::InputTag(source.get<std::string>("InputTag")),
                           storage == "Retained" ? supera::TransportStorage::Retained
                                                 : supera::TransportStorage::Dropped,
                           {}};
      if (input.particles.label().empty())
        throw std::invalid_argument("Empty transport input tag");
      const auto associations = source.get<std::string>("TruthAssociations", "");
      if (!associations.empty())
        input.truth_associations = art::InputTag(associations);
      consumes<std::vector<simb::MCParticle>>(input.particles);
      definition.sources.push_back(std::move(input));
    }
    if (definition.sources.empty())
      throw std::invalid_argument("Transport Sources must not be empty");
    for (const auto &tag : collection.get<std::vector<std::string>>("AncestryMaps", {})) {
      if (art::InputTag(tag).label().empty())
        throw std::invalid_argument("Empty ancestry map tag");
      definition.maps.emplace_back(tag);
      consumes<sim::ParticleAncestryMap>(definition.maps.back());
    }
    for (auto id : collection.get<std::vector<int>>("RootMotherIDs", {0}))
      definition.root_mothers.insert(id);
    if (definition.root_mothers.empty())
      throw std::invalid_argument("RootMotherIDs must not be empty");
    transport_collections_.push_back(std::move(definition));
    transport_outputs_.push_back(name);
  }

  for (const auto &collection :
       config.get<std::vector<fhicl::ParameterSet>>("DepositCollections", {})) {
    const auto name = collection.get<std::string>("OutputLabel");
    const art::InputTag tag(collection.get<std::string>("InputTag"));
    const auto transport_name = collection.get<std::string>("Transport");
    const auto target =
        std::find(transport_outputs_.begin(), transport_outputs_.end(), transport_name);
    if (tag.label().empty() || target == transport_outputs_.end())
      throw std::invalid_argument("Deposit source and transport namespace must be explicit");
    const auto kind_name = collection.get<std::string>("Reader");
    if (kind_name != "SEDLite" && kind_name != "SED" && kind_name != "SimChannel")
      throw std::invalid_argument("Unsupported deposit Reader");
    DepositDefinition definition{tag,
                                 kind_name == "SEDLite" ? supera::DepositKind::EnergyPoint
                                 : kind_name == "SED"   ? supera::DepositKind::EnergyStep
                                                        : supera::DepositKind::ReadoutIDE,
                                 static_cast<std::size_t>(target - transport_outputs_.begin()),
                                 {}};
    if (collection.has_key("MinimumCM")) {
      const auto triple = [&](const std::string &key) {
        const auto values = collection.get<std::vector<double>>(key);
        if (values.size() != 3)
          throw std::invalid_argument("Deposit grid requires three values per axis");
        return std::array<double, 3>{values[0], values[1], values[2]};
      };
      if (definition.kind == supera::DepositKind::ReadoutIDE)
        throw std::invalid_argument(
            "SimChannel energy cannot use physical-energy voxel projection");
      definition.grid = supera::FixedGrid::from_lengths(triple("MinimumCM"), triple("LengthCM"),
                                                        triple("VoxelSizeCM"))
                            .metadata();
    }
    if (definition.kind == supera::DepositKind::EnergyPoint)
      consumes<std::vector<sim::SimEnergyDepositLite>>(tag);
    else if (definition.kind == supera::DepositKind::EnergyStep)
      consumes<std::vector<sim::SimEnergyDeposit>>(tag);
    else
      consumes<std::vector<sim::SimChannel>>(tag);
    deposit_outputs_.push_back({name, definition.grid.has_value()});
    deposit_collections_.push_back(std::move(definition));
  }
  const auto deposit_timing_file = config.get<std::string>("DepositTimingFile", "");
  if (!deposit_timing_file.empty()) {
    deposit_timings_.open(deposit_timing_file);
    if (!deposit_timings_)
      throw std::runtime_error("Cannot open deposit timing report");
    deposit_timings_ << "run,subrun,event,collection,read_s,attribute_s,records\n";
  }

  // Register optical inputs and their collection-level conventions.
  std::set<std::string> output_names;
  for (const auto &collection : collections) {
    const art::InputTag input(collection.get<std::string>("InputTag"));
    const auto output = collection.get<std::string>("OutputLabel");
    if (input.label().empty() || output.empty() || !output_names.insert(output).second) {
      throw std::invalid_argument(
          "InputTag and OutputLabel must be nonempty; outputs must be unique");
    }

    consumes<std::vector<recob::OpFlash>>(input);
    supera::OpticalConventions conventions;
    conventions.detector = collection.get<std::string>("Detector", "");
    conventions.region = collection.get<std::string>("Region", "");
    conventions.coordinate_frame = collection.get<std::string>("CoordinateFrame", "");

    conventions_.push_back(std::move(conventions));
    inputs_.push_back(input);
    outputs_.push_back(output);
  }

  // Auxiliary names are unique within their own product type.
  output_names.clear();
  for (const auto &collection : auxiliary) {
    const art::InputTag input(collection.get<std::string>("InputTag"));
    const auto output = collection.get<std::string>("OutputLabel");
    if (input.label().empty() || output.empty() || !output_names.insert(output).second)
      throw std::invalid_argument(
          "Auxiliary inputs/outputs must be nonempty; outputs must be unique");
    if (collection.get<std::string>("Reader") != "SbnCrtHit")
      throw std::invalid_argument("Unsupported auxiliary Reader");

    consumes<std::vector<sbn::crt::CRTHit>>(input);
    supera::AuxDetConventions conventions;
    conventions.detector = collection.get<std::string>("Detector", "");
    conventions.coordinate_frame = collection.get<std::string>("CoordinateFrame", "");
    conventions.technology = supera::AuxDetTechnology::Scintillator;
    conventions.source_schema = supera::AuxDetSourceSchema::SbnCrtHit;

    // Role describes the subsystem, never the origin of an individual particle.
    conventions.roles = {supera::AuxDetRole::CosmicTagging};

    auxiliary_conventions_.push_back(std::move(conventions));
    auxiliary_inputs_.push_back(input);
    auxiliary_outputs_.push_back(output);
  }

  // Source order is stable even when explicitly optional products are absent.
  output_names.clear();
  for (const auto &collection : generator) {
    const auto output = collection.get<std::string>("OutputLabel");
    if (output.empty() || !output_names.insert(output).second)
      throw std::invalid_argument("Generator outputs must be nonempty and unique");
    if (collection.get<std::string>("Projection") != "LegacySuperaMCTruth")
      throw std::invalid_argument("Unsupported generator Projection");

    GeneratorCollection definition;
    if (collection.has_key("Origin"))
      definition.origin = collection.get<int>("Origin");
    std::set<std::string> sources;
    for (const auto &source : collection.get<std::vector<fhicl::ParameterSet>>("Sources")) {
      const art::InputTag tag(source.get<std::string>("InputTag"));
      if (tag.label().empty() || !sources.insert(tag.encode()).second)
        throw std::invalid_argument("Generator sources must be nonempty and unique");
      const bool required = source.get<bool>("Required", true);
      consumes<std::vector<simb::MCTruth>>(tag);
      definition.inputs.push_back({tag, required});
    }
    if (definition.inputs.empty())
      throw std::invalid_argument("Generator Sources must not be empty");
    generator_collections_.push_back(std::move(definition));
    generator_outputs_.push_back(output);
  }

  // Configure shared grid/selection separately from native product access.
  for (const auto &collection : points) {
    const auto triple = [&](const std::string &key) {
      const auto values = collection.get<std::vector<double>>(key);
      if (values.size() != 3)
        throw std::invalid_argument(key + " requires three values");
      return std::array<double, 3>{values[0], values[1], values[2]};
    };
    auto grid = supera::FixedGrid::from_lengths(triple("MinimumCM"), triple("LengthCM"),
                                                triple("VoxelSizeCM"));
    supera::SpacePointSelection selection;
    const auto range = collection.get<std::vector<double>>("ChargeRange");
    if (range.size() != 2)
      throw std::invalid_argument("ChargeRange requires two values");
    selection.minimum_charge = range[0];
    selection.maximum_charge = range[1];
    selection.shift_cm = triple("ShiftCM");
    selection.float_charge = collection.get<bool>("FloatCharge", false);
    selection.prefer_more_hits = collection.get<bool>("PreferMoreHits", true);
    selection.planes = collection.get<unsigned int>("Planes", 3);
    PointCollection definition{{}, std::move(grid), selection};
    for (const auto &source : collection.get<std::vector<fhicl::ParameterSet>>("Sources")) {
      const art::InputTag tag(source.get<std::string>("InputTag"));
      const auto profile_name = source.get<std::string>("Profile");
      if (tag.label().empty() || profile_name != "IcarusCluster3D")
        throw std::invalid_argument("Voxel output requires an explicit supported charge profile");
      PointInput input{tag, SpacePointProfile::IcarusCluster3D, {}};
      consumes<std::vector<recob::SpacePoint>>(tag);
      if (source.get<bool>("WithHits", true)) {
        input.hits =
            SpacePointHitInput{art::InputTag(source.get<std::string>("AssociationTag")), {}};
        consumes<art::Assns<recob::SpacePoint, recob::Hit>>(input.hits->associations);
        for (const auto &hit : source.get<std::vector<std::string>>("HitTags")) {
          input.hits->products.emplace_back(hit);
          consumes<std::vector<recob::Hit>>(input.hits->products.back());
        }
      }
      definition.inputs.push_back(std::move(input));
    }
    if (definition.inputs.empty())
      throw std::invalid_argument("Space-point Sources must not be empty");
    point_collections_.push_back(std::move(definition));
    point_outputs_.push_back({collection.get<std::string>("OutputLabel"), selection.planes,
                              collection.get<std::vector<std::string>>("Fields")});
  }
  const auto timing_file = config.get<std::string>("TimingFile", "");
  if (!timing_file.empty()) {
    timings_.open(timing_file);
    if (!timings_)
      throw std::runtime_error("Cannot open timing report");
    timings_ << "event,collection,read_s,select_s,write_s,voxels,nonfinite,charge_cut,outside,"
                "collisions,replacements\n";
  }
  const auto transport_timing_file = config.get<std::string>("TransportTimingFile", "");
  if (!transport_timing_file.empty()) {
    transport_timings_.open(transport_timing_file);
    if (!transport_timings_)
      throw std::runtime_error("Cannot open transport timing report");
    transport_timings_ << "run,subrun,event,read_s,resolve_s,write_s,particles\n";
  }
  output_file_ = output_file;
}

void SuperaWriter::beginJob() {
  writer_ = std::make_unique<supera::io::larcv2::Writer>(output_file_, outputs_, auxiliary_outputs_,
                                                         generator_outputs_, point_outputs_,
                                                         transport_outputs_, deposit_outputs_);
}

void SuperaWriter::analyze(const art::Event &event) {
  // All records are event-local. Missing inputs throw before the sink sees data.
  supera::EventRecord record;
  record.id = {event.run(), event.subRun(), event.event()};

  // Resolve and translate the optical collections first.
  record.optical.reserve(inputs_.size());
  for (std::size_t i = 0; i < inputs_.size(); ++i) {
    NormalizationReport report;
    const auto flashes = event.getValidHandle<std::vector<recob::OpFlash>>(inputs_[i]);
    record.optical.push_back(
        {outputs_[i], inputs_[i].encode(), conventions_[i], convert_flashes(*flashes, &report)});
    report_normalization(report, event, inputs_[i].encode());
  }

  // Resolve auxiliary inputs before handing any part of the event to the sink.
  for (std::size_t i = 0; i < auxiliary_inputs_.size(); ++i) {
    NormalizationReport report;
    const auto hits = event.getValidHandle<std::vector<sbn::crt::CRTHit>>(auxiliary_inputs_[i]);
    record.auxiliary.push_back({auxiliary_outputs_[i], auxiliary_inputs_[i].encode(),
                                auxiliary_conventions_[i], convert_crt_hits(*hits, &report)});
    report_normalization(report, event, auxiliary_inputs_[i].encode());
  }

  // Build all source records before applying the detector-independent selection.
  for (std::size_t index = 0; index < generator_collections_.size(); ++index) {
    supera::GeneratorCollection collection;
    collection.name = generator_outputs_[index];
    const auto &definition = generator_collections_[index];
    for (std::size_t source_index = 0; source_index < definition.inputs.size(); ++source_index) {
      const auto &input = definition.inputs[source_index];
      supera::GeneratorSource source;
      source.source_index = supera::record_index(source_index);
      source.source = input.tag.encode();
      const auto handle = event.getHandle<std::vector<simb::MCTruth>>(input.tag);
      source.present = handle.isValid();
      if (!source.present && input.required)
        throw std::runtime_error("Required generator product missing: " + source.source);
      if (source.present) {
        NormalizationReport report;
        source.records = convert_generator(*handle, &report);
        report_normalization(report, event, source.source);
      }
      collection.sources.push_back(std::move(source));
    }
    record.generator.push_back(supera::select_generator(std::move(collection), definition.origin));
  }

  // Native extraction preserves all points; shared processing owns rasterization.
  using Clock = std::chrono::steady_clock;
  const auto now = [&] { return timings_.is_open() ? Clock::now() : Clock::time_point{}; };
  for (std::size_t index = 0; index < point_collections_.size(); ++index) {
    const auto &definition = point_collections_[index];
    std::vector<supera::SpacePointCollection> sources;
    const auto start = now();
    for (const auto &input : definition.inputs) {
      NormalizationReport report;
      sources.push_back(read_spacepoints(event, input.tag, input.profile, input.hits, &report));
      report_normalization(report, event, input.tag.encode());
    }
    const auto loaded = now();
    record.spacepoints.push_back(supera::select_spacepoints(
        point_outputs_[index].name, std::move(sources), definition.grid, definition.selection));
    const auto selected = now();
    if (timings_.is_open()) {
      const auto &output = record.spacepoints.back();
      timings_ << event.event() << ',' << output.name << ','
               << std::chrono::duration<double>(loaded - start).count() << ','
               << std::chrono::duration<double>(selected - loaded).count() << ",0,"
               << output.voxels.size() << ',' << output.report.nonfinite << ','
               << output.report.charge_cut << ',' << output.report.outside_grid << ','
               << output.report.collisions << ',' << output.report.replacements << '\n';
    }
  }
  const auto start = now();
  double transport_read = 0., transport_resolve = 0.;
  std::size_t transport_count = 0;
  for (std::size_t i = 0; i < transport_collections_.size(); ++i) {
    const auto start = std::chrono::steady_clock::now();
    const auto &definition = transport_collections_[i];
    NormalizationReport report;
    auto collection = read_transport(event, definition.sources, definition.maps, &report);
    collection.name = transport_outputs_[i];
    collection.root_mother_ids = definition.root_mothers;
    report_normalization(report, event, collection.name);
    const auto converted = std::chrono::steady_clock::now();
    supera::resolve_transport(collection);
    const auto resolved = std::chrono::steady_clock::now();
    transport_read += std::chrono::duration<double>(converted - start).count();
    transport_resolve += std::chrono::duration<double>(resolved - converted).count();
    transport_count += collection.particles.size();
    record.transport.push_back(std::move(collection));
  }
  std::vector<double> deposit_reads;
  for (std::size_t i = 0; i < deposit_collections_.size(); ++i) {
    const auto &definition = deposit_collections_[i];
    const auto start = std::chrono::steady_clock::now();
    NormalizationReport report;
    auto collection = read_deposits(event, definition.tag, definition.kind, &report);
    collection.name = deposit_outputs_[i].name;
    collection.transport = record.transport.at(definition.transport).name;
    collection.grid = definition.grid;
    report_normalization(report, event, collection.product);
    deposit_reads.push_back(
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    record.deposits.push_back(std::move(collection));
  }

  // Build evidence only after all requested products are available, independent of input order.
  const auto evidence_start = std::chrono::steady_clock::now();
  std::vector<supera::ReassignmentEvidence> readout;
  for (const auto &transport : record.transport)
    readout.push_back(supera::reassignment_evidence(record.deposits, transport.name));
  if (deposit_timings_)
    deposit_timings_
        << event.run() << ',' << event.subRun() << ',' << event.event()
        << ",reassignment_evidence,0,"
        << std::chrono::duration<double>(std::chrono::steady_clock::now() - evidence_start).count()
        << ",0\n";
  for (std::size_t i = 0; i < record.deposits.size(); ++i) {
    const auto start = std::chrono::steady_clock::now();
    auto &collection = record.deposits[i];
    const auto target = deposit_collections_[i].transport;
    supera::attribute_deposits(collection, record.transport.at(target), readout.at(target));
    if (deposit_timings_)
      deposit_timings_
          << event.run() << ',' << event.subRun() << ',' << event.event() << ',' << collection.name
          << ',' << deposit_reads[i] << ','
          << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << ','
          << collection.deposits.size() << '\n';
  }
  const auto transport_write_start = std::chrono::steady_clock::now();
  writer_->write(record);
  if (transport_timings_)
    transport_timings_ << event.run() << ',' << event.subRun() << ',' << event.event() << ','
                       << transport_read << ',' << transport_resolve << ','
                       << std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                                        transport_write_start)
                              .count()
                       << ',' << transport_count << '\n';
  if (timings_.is_open()) {
    timings_ << event.event() << ",writer,0,0,"
             << std::chrono::duration<double>(now() - start).count() << ",0,0,0,0,0,0\n";
    timings_.flush();
  }
}

void SuperaWriter::endJob() {
  writer_->close();
}

} // namespace larsoft2supera

DEFINE_ART_MODULE(larsoft2supera::SuperaWriter)
