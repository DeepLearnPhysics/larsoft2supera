#ifndef LARSOFT2SUPERA_READDEPOSIT_H
#define LARSOFT2SUPERA_READDEPOSIT_H

#include "larsoft2supera/Deposit.h"

#include "canvas/Utilities/InputTag.h"

#include <sstream>

namespace larsoft2supera {
/// Required source access, with an owning copy and explicit native product identity.
template <class Event>
supera::DepositCollection read_deposits(const Event &event, const art::InputTag &tag,
                                        supera::DepositKind kind,
                                        NormalizationReport *report = nullptr) {
  supera::DepositCollection result;
  result.kind = kind;
  std::ostringstream product;
  product << tag.encode() << '@';
  if (kind == supera::DepositKind::EnergyPoint) {
    const auto handle = event.template getValidHandle<std::vector<sim::SimEnergyDepositLite>>(tag);
    result.deposits = convert_deposits(*handle, report);
    product << handle.id();
  } else if (kind == supera::DepositKind::EnergyStep) {
    const auto handle = event.template getValidHandle<std::vector<sim::SimEnergyDeposit>>(tag);
    result.deposits = convert_deposits(*handle, report);
    product << handle.id();
  } else {
    const auto handle = event.template getValidHandle<std::vector<sim::SimChannel>>(tag);
    result.deposits = convert_deposits(*handle, report);
    product << handle.id();
    for (const auto &channel : *handle)
      result.channel_inventory.push_back(channel.Channel());
  }
  result.product = product.str();
  return result;
}
} // namespace larsoft2supera
#endif
