#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Persistency/Common/PtrMaker.h"
#include "canvas/Persistency/Common/Assns.h"
#include "fhiclcpp/ParameterSet.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/SpacePoint.h"

namespace larsoft2supera {
class SpacePointFixture : public art::EDProducer {
public:
  explicit SpacePointFixture(const fhicl::ParameterSet &config)
      : art::EDProducer(config), mode_(config.get<std::string>("Mode", "valid")) {
    produces<std::vector<recob::SpacePoint>>("points");
    produces<std::vector<recob::Hit>>("hits");
    produces<std::vector<recob::Hit>>("other");
    produces<art::Assns<recob::SpacePoint, recob::Hit>>("points");
  }

  void produce(art::Event &event) override {
    auto points = std::make_unique<std::vector<recob::SpacePoint>>();
    auto hits = std::make_unique<std::vector<recob::Hit>>(1);
    auto other = std::make_unique<std::vector<recob::Hit>>(1);
    auto associations = std::make_unique<art::Assns<recob::SpacePoint, recob::Hit>>();
    if (event.event() != 2) {
      double xyz[]{1., 2., 3.}, fields[]{1., 42., 0., .25, 0., 1.};
      points->emplace_back(xyz, fields, 4., 5);
      points->emplace_back(xyz, fields, 4., 5);
      art::PtrMaker<recob::SpacePoint> point(event, "points");
      art::PtrMaker<recob::Hit> hit(event, "hits"), second(event, "other");
      if (mode_ == "bad-point-key")
        associations->addSingle(point(99), hit(0));
      else if (mode_ == "null-point")
        associations->addSingle(art::Ptr<recob::SpacePoint>{}, hit(0));
      else if (mode_ == "null")
        associations->addSingle(point(0), art::Ptr<recob::Hit>{});
      else if (mode_ == "bad-key")
        associations->addSingle(point(0), hit(99));
      else {
        associations->addSingle(point(0), hit(0));
        associations->addSingle(point(1), hit(0));
        associations->addSingle(point(0), second(0));
        associations->addSingle(point(0), second(0));
      }
    }
    event.put(std::move(points), "points");
    event.put(std::move(hits), "hits");
    event.put(std::move(other), "other");
    event.put(std::move(associations), "points");
  }

private:
  std::string mode_;
};
} // namespace larsoft2supera
DEFINE_ART_MODULE(larsoft2supera::SpacePointFixture)
