// Copyright 2019 libsbn project contributors.
// libsbn is free software under the GPLv3; see LICENSE file for details.

#ifndef SRC_TREE_COLLECTION_HPP_
#define SRC_TREE_COLLECTION_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "tree.hpp"

class TreeCollection {
 public:
  explicit TreeCollection(Tree::TreeVector trees);

  TreeCollection() = default;
  TreeCollection(Tree::TreeVector trees, TagStringMap tag_taxon_map);
  TreeCollection(Tree::TreeVector trees, const std::vector<std::string> &taxon_labels);

  size_t TreeCount() const { return trees_.size(); }
  const Tree::TreeVector &Trees() const { return trees_; }
  const Tree &GetTree(size_t i) const { return *trees_.at(i); }
  const TagStringMap &TagTaxonMap() const { return tag_taxon_map_; }
  size_t TaxonCount() const { return tag_taxon_map_.size(); }
  const std::unordered_map<size_t, double> &TaxonDateMap() const {
    return taxon_date_map_;
  }
  void SetTaxonDateMap(std::unordered_map<size_t, double> taxon_date_map) {
    taxon_date_map_ = taxon_date_map;
  }

  bool operator==(const TreeCollection &other) const;

  // Remove trees from begin_idx to just before end_idx.
  void Erase(size_t begin_idx, size_t end_idx);

  std::string Newick() const;

  Node::TopologyCounter TopologyCounter() const;
  std::vector<std::string> TaxonNames() const;

  static TagStringMap TagStringMapOf(const std::vector<std::string> &taxon_labels);

  Tree::TreeVector trees_;

 private:
  TagStringMap tag_taxon_map_;
  std::unordered_map<size_t, double> taxon_date_map_;
};

#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("TreeCollection") {
  TreeCollection collection(Tree::ExampleTrees());
  auto counter = collection.TopologyCounter();
  std::unordered_map<std::string, uint32_t> counted;
  for (const auto &iter : counter) {
    SafeInsert(counted, iter.first->Newick(std::nullopt, std::nullopt, true),
               iter.second);
  }
  std::unordered_map<std::string, uint32_t> counted_correct(
      {{"(0_1,1_1,(2_1,3_1)3_2)3_4;", 2},
       {"(0_1,2_1,(1_1,3_1)3_2)3_4;", 1},
       {"(0_1,(1_1,(2_1,3_1)3_2)3_3)3_4;", 1}});
  CHECK_EQ(counted, counted_correct);
}
#endif  // DOCTEST_LIBRARY_INCLUDED

#endif  // SRC_TREE_COLLECTION_HPP_
