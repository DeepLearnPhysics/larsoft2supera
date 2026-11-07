#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "fhiclcpp/ParameterSet.h"
#include "lardataobj/RecoBase/OpFlash.h"
#include "sbnobj/Common/CRT/CRTHit.hh"

#include <memory>
#include <vector>

namespace larsoft2supera {
// Three consecutive events exercise nonempty -> empty -> nonempty in art itself.
class OpticalFixture : public art::EDProducer {
public:
  explicit OpticalFixture(const fhicl::ParameterSet &config) : art::EDProducer(config) {
    produces<std::vector<recob::OpFlash>>("east");
    produces<std::vector<recob::OpFlash>>("west");
    produces<std::vector<sbn::crt::CRTHit>>("auxiliary");
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
    auto auxiliary = std::make_unique<std::vector<sbn::crt::CRTHit>>();
    if (event.event() != 2) {
      sbn::crt::CRTHit hit;
      hit.peshit = 12.5F;
      hit.ts0_s = (std::uint64_t{1} << 53) + 1;
      hit.feb_id = {255, 0};
      hit.pesmap = {{255, {{-1, 3.5F}}}, {0, {}}};
      auxiliary->push_back(hit);
    }
    event.put(std::move(auxiliary), "auxiliary");
    event.put(std::move(east), "east");
    event.put(std::move(west), "west");
  }
};
} // namespace larsoft2supera

DEFINE_ART_MODULE(larsoft2supera::OpticalFixture)
