#pragma once

#include "refined_itmcd_data_structures.h"

namespace mbes {

NodeDelta GetNodeChange(const GraphSnapshot& curr, const GraphSnapshot& prev);
CliqueSet FindMaximalCliques(const GraphSnapshot& graph);
TemporalGraphSequence BuildTemporalGraphSequence(const TemporalEdgeIntervalGraph& graph,
                                                 Timestamp begin,
                                                 Timestamp end);

class RefinedIncrementalTopK {
 public:
  explicit RefinedIncrementalTopK(std::size_t k) : k_(k) {}

  std::vector<RankedClique> Run(const TemporalGraphSequence& sequence);
  std::vector<RankedClique> Run(const TemporalEdgeIntervalGraph& graph,
                                Timestamp begin,
                                Timestamp end);

 private:
  std::size_t k_;
};

}  // namespace mbes
