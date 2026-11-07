#ifndef LARSOFT2SUPERA_NORMALIZATION_H
#define LARSOFT2SUPERA_NORMALIZATION_H

#include "supera/data/Validity.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace larsoft2supera {

enum class MissingReason { SourceSentinel, NotANumber, Infinity };

struct NormalizationExample {
  supera::RecordIndex record;
  supera::RecordIndex particle;
  supera::RecordIndex step;
  std::string field;
  MissingReason reason;
};

/// One report per source product and event, supplied by the caller.
/// Counts are complete; examples are bounded to avoid per-measurement storage.
struct NormalizationReport {
  std::array<std::uint64_t, 3> counts{};
  std::vector<NormalizationExample> examples;
};

struct Normalization {
  NormalizationReport *report{};
  supera::RecordIndex record{supera::missing_index};
  supera::RecordIndex particle{supera::missing_index};
  supera::RecordIndex step{supera::missing_index};

  void note(const char *field, MissingReason reason) const {
    if (!report)
      return;
    ++report->counts[static_cast<std::size_t>(reason)];
    if (report->examples.size() < 16)
      report->examples.push_back({record, particle, step, field, reason});
  }

  double measure(double value, const char *field) const {
    if (!std::isfinite(value))
      note(field, std::isnan(value) ? MissingReason::NotANumber : MissingReason::Infinity);
    return supera::finite_or_missing(value);
  }

  double measure(double value, const char *field, double sentinel) const {
    if (value == sentinel) {
      note(field, MissingReason::SourceSentinel);
      return supera::missing_value;
    }
    return measure(value, field);
  }

  template <class T> std::optional<T> code(T value, T sentinel, const char *field) const {
    if (value == sentinel) {
      note(field, MissingReason::SourceSentinel);
      return {};
    }
    return value;
  }
};

} // namespace larsoft2supera
#endif
