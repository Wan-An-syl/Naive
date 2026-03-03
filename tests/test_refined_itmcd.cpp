#include <algorithm>
#include <cassert>

#include "refined_itmcd_algorithm.h"

namespace {

bool IsClique(const mbes::RankedClique& item, std::initializer_list<mbes::NodeId> nodes) {
  mbes::Clique c{std::vector<mbes::NodeId>(nodes)};
  return item.clique == c;
}

std::int32_t MaxLifetimeForClique(const std::vector<mbes::RankedClique>& ranked,
                                  std::initializer_list<mbes::NodeId> nodes) {
  std::int32_t best = 0;
  for (const auto& item : ranked) {
    if (IsClique(item, nodes)) {
      best = std::max(best, item.lifetime);
    }
  }
  return best;
}

}  // namespace

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

  mbes::RefinedIncrementalTopK solver(16);
  const auto res = solver.Run(seq);
  assert(!res.empty());
  assert(res.front().score >= 3);

  // 同一条边多个不连续时间区间。
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

  const auto max_lifetime_12 = MaxLifetimeForClique(res_from_edges, {1, 2});
  assert(max_lifetime_12 <= 3);
  assert(max_lifetime_12 >= 1);

  // 覆盖伪代码分支：
  // t0: {1,2}
  // t1: {1,2}               -> c_new == c_prev
  // t2: {1,2,3}             -> c_prev ⊆ c_new
  // t3: {4,5}               -> not matched
  mbes::TemporalGraphSequence branch_seq;
  branch_seq.snapshots.resize(4);
  branch_seq[0].AddEdge(1, 2);
  branch_seq[1].AddEdge(1, 2);
  branch_seq[2].AddEdge(1, 2);
  branch_seq[2].AddEdge(1, 3);
  branch_seq[2].AddEdge(2, 3);
  branch_seq[3].AddEdge(4, 5);

  const auto branch_res = solver.Run(branch_seq);
  assert(!branch_res.empty());
  assert(MaxLifetimeForClique(branch_res, {1, 2}) >= 1);
  assert(MaxLifetimeForClique(branch_res, {1, 2, 3}) >= 1);
  assert(MaxLifetimeForClique(branch_res, {4, 5}) >= 1);

  return 0;
}
