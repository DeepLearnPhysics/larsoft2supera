#include "larsoft2supera/AuxDet.h"
#include "larsoft2supera/Optical.h"

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "supera/io/larcv2/Writer.h"

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace larsoft2supera {

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
  std::unique_ptr<supera::EventWriter> writer_;
};

SuperaWriter::SuperaWriter(const fhicl::ParameterSet &config) : art::EDAnalyzer(config) {
  // Read the requested products and require a usable output destination.
  const auto collections = config.get<std::vector<fhicl::ParameterSet>>("Collections", {});
  const auto auxiliary = config.get<std::vector<fhicl::ParameterSet>>("AuxiliaryCollections", {});
  const auto output_file = config.get<std::string>("OutputFile");
  if ((collections.empty() && auxiliary.empty()) || output_file.empty()) {
    throw std::invalid_argument("Collections and OutputFile must not be empty");
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

  output_file_ = output_file;
}

void SuperaWriter::beginJob() {
  writer_ =
      std::make_unique<supera::io::larcv2::Writer>(output_file_, outputs_, auxiliary_outputs_);
}

void SuperaWriter::analyze(const art::Event &event) {
  // All records are event-local. Missing inputs throw before the sink sees data.
  supera::EventRecord record;
  record.id = {event.run(), event.subRun(), event.event()};

  // Resolve and translate the optical collections first.
  record.optical.reserve(inputs_.size());
  for (std::size_t i = 0; i < inputs_.size(); ++i) {
    const auto flashes = event.getValidHandle<std::vector<recob::OpFlash>>(inputs_[i]);
    record.optical.push_back(
        {outputs_[i], inputs_[i].encode(), conventions_[i], convert_flashes(*flashes)});
  }

  // Resolve auxiliary inputs before handing any part of the event to the sink.
  for (std::size_t i = 0; i < auxiliary_inputs_.size(); ++i) {
    const auto hits = event.getValidHandle<std::vector<sbn::crt::CRTHit>>(auxiliary_inputs_[i]);
    record.auxiliary.push_back({auxiliary_outputs_[i], auxiliary_inputs_[i].encode(),
                                auxiliary_conventions_[i], convert_crt_hits(*hits)});
  }

  writer_->write(record);
}

void SuperaWriter::endJob() {
  writer_->close();
}

} // namespace larsoft2supera

DEFINE_ART_MODULE(larsoft2supera::SuperaWriter)
