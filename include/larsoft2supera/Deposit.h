#ifndef LARSOFT2SUPERA_DEPOSIT_H
#define LARSOFT2SUPERA_DEPOSIT_H

#include "larsoft2supera/Normalization.h"

#include "lardataobj/Simulation/SimChannel.h"
#include "lardataobj/Simulation/SimEnergyDeposit.h"
#include "lardataobj/Simulation/SimEnergyDepositLite.h"
#include "supera/data/Deposit.h"

namespace larsoft2supera {
/// LArG4 negative track IDs indicate upstream reassignment. Raw IDs are also retained.
std::vector<supera::Deposit> convert_deposits(const std::vector<sim::SimEnergyDepositLite> &input,
                                              NormalizationReport *report = nullptr);
std::vector<supera::Deposit> convert_deposits(const std::vector<sim::SimEnergyDeposit> &input,
                                              NormalizationReport *report = nullptr);
std::vector<supera::Deposit> convert_deposits(const std::vector<sim::SimChannel> &input,
                                              NormalizationReport *report = nullptr);
} // namespace larsoft2supera
#endif
