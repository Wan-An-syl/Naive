#include <cassert>

#include "refined_itmcd_algorithm.h"

int main() {
  mbes::GraphSnapshot g;
  g.AddEdge(1, 2);
  g.AddEdge(2, 3);
  g.AddEdge(1, 3);

  const auto cliques = mbes::FindMaximalCliques(g);
  assert(!cliques.empty());

  mbes::TemporalGraphSequence seq;
  seq.snapshots.push_back(g);
  seq.snapshots.push_back(g);

  mbes::RefinedIncrementalTopK solver(3);
  const auto res = solver.Run(seq);
  assert(!res.empty());
  assert(res.front().score >= 3);

  return 0;
}
