#pragma once

#include "refined_itmcd_data_structures.h"

namespace mbes {

NodeDelta GetNodeChange(const GraphSnapshot& curr, const GraphSnapshot& prev);
CliqueSet FindMaximalCliques(const GraphSnapshot& graph);

class RefinedIncrementalTopK {
 public:
  explicit RefinedIncrementalTopK(std::size_t k) : k_(k) {}

  std::vector<RankedClique> Run(const TemporalGraphSequence& sequence);

 private:
  std::size_t k_;
};

}  // namespace mbes
