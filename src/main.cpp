#include <iostream>

#include "refined_itmcd_algorithm.h"

int main() {
  mbes::TemporalGraphSequence seq;
  seq.snapshots.resize(3);

  seq[0].AddEdge(1, 2);
  seq[0].AddEdge(2, 3);
  seq[0].AddEdge(1, 3);

  seq[1] = seq[0];
  seq[1].AddEdge(3, 4);
  seq[1].AddEdge(2, 4);

  seq[2] = seq[1];
  seq[2].AddEdge(1, 4);

  mbes::RefinedIncrementalTopK solver(5);
  const auto top = solver.Run(seq);

  std::cout << "Top cliques:\n";
  for (const auto& item : top) {
    std::cout << "score=" << item.score << " lifetime=" << item.lifetime << " clique={";
    for (std::size_t i = 0; i < item.clique.vertices.size(); ++i) {
      std::cout << item.clique.vertices[i] << (i + 1 == item.clique.vertices.size() ? "" : ",");
    }
    std::cout << "}\n";
  }

  return 0;
}
