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

  // 关键场景：同一条边拥有多个不连续时间区间。
  mbes::TemporalEdgeIntervalGraph interval_graph;
  interval_graph.AddEdgeInterval(1, 2, 0, 1);
  interval_graph.AddEdgeInterval(1, 2, 4, 6);
  interval_graph.AddEdgeInterval(2, 3, 2, 3);

  assert(interval_graph.HasEdgeAt(1, 2, 0));
  assert(interval_graph.HasEdgeAt(1, 2, 5));
  assert(!interval_graph.HasEdgeAt(1, 2, 3));

  const auto edge_intervals = interval_graph.GetEdgeIntervals(2, 1);
  assert(edge_intervals.size() == 2);

  const auto seq_from_edges = interval_graph.BuildSequence(0, 6);
  assert(seq_from_edges.Size() == 7);
  assert(!seq_from_edges[3].HasEdge(1, 2));
  assert(seq_from_edges[5].HasEdge(1, 2));

  const auto res_from_edges = solver.Run(interval_graph, 0, 6);
  assert(!res_from_edges.empty());

  return 0;
}
