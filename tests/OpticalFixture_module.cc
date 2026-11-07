#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "fhiclcpp/ParameterSet.h"
#include "lardataobj/RecoBase/OpFlash.h"

#include <memory>
#include <vector>

namespace larsoft2supera {
// Three consecutive events exercise nonempty -> empty -> nonempty in art itself.
class OpticalFixture : public art::EDProducer {
public:
  explicit OpticalFixture(const fhicl::ParameterSet &config) : art::EDProducer(config) {
    produces<std::vector<recob::OpFlash>>("east");
    produces<std::vector<recob::OpFlash>>("west");
  }

  void produce(art::Event &event) override {
    auto east = std::make_unique<std::vector<recob::OpFlash>>();
    auto west = std::make_unique<std::vector<recob::OpFlash>>();
    const recob::OpFlash flash(-12.5, 0.25, 100., 5, {1., 2.}, true, 1, 0.5, 1., 2., 3., 4., 5., 6.,
                               {7.}, {8.});
    if (event.event() == 1)
      east->assign(2, flash);
    if (event.event() == 2)
      west->push_back(flash);
    if (event.event() == 3)
      east->push_back(flash);
    event.put(std::move(east), "east");
    event.put(std::move(west), "west");
  }
};
} // namespace larsoft2supera

DEFINE_ART_MODULE(larsoft2supera::OpticalFixture)
