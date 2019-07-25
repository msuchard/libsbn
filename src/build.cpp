// Copyright 2019 Matsen group.
// libsbn is free software under the GPLv3; see LICENSE file for details.

#include "build.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

// Using insert and at avoids needing to make a default constructor.
// https://stackoverflow.com/questions/17172080/insert-vs-emplace-vs-operator-in-c-map

TagBitsetMap TagBitsetMapOf(Node::NodePtr t) {
  TagBitsetMap m;
  auto leaf_count = t->LeafCount();
  t->PostOrder([&m, leaf_count](const Node* n) {
    Bitset x((size_t)leaf_count);
    if (n->IsLeaf()) {
      x.set(n->MaxLeafID());
    } else {
      // Take the union of the children below.
      for (const auto& child : n->Children()) {
        x |= m.at(child->Tag());
      }
    }
    assert(m.insert({n->Tag(), std::move(x)}).second);
  });
  return m;
}

void PrintTagBitsetMap(TagBitsetMap m) {
  for (const auto& iter : m) {
    std::cout << StringOfPackedInt(iter.first) << " " << iter.second.ToString()
              << std::endl;
  }
}

BitsetUInt32Dict RootsplitCounterOf(const Node::TopologyCounter& topologies) {
  BitsetUInt32Dict rootsplit_counter(0);
  for (const auto& iter : topologies) {
    auto topology = iter.first;
    auto count = iter.second;
    auto tag_to_bitset = TagBitsetMapOf(topology);
    auto Aux = [&rootsplit_counter, &tag_to_bitset, &count](const Node* n) {
      auto split = tag_to_bitset.at(n->Tag()).copy();
      split.Minorize();
      rootsplit_counter.increment(std::move(split), count);
    };
    for (const auto& child : topology->Children()) {
      child->PreOrder(Aux);
    }
  }
  return rootsplit_counter;
}

PCSSDict PCSSCounterOf(const Node::TopologyCounter& topologies) {
  PCSSDict pcss_dict;
  for (const auto& iter : topologies) {
    auto topology = iter.first;
    auto count = iter.second;
    auto tag_to_bitset = TagBitsetMapOf(topology);
    auto leaf_count = topology->LeafCount();
    if (topology->Children().size() != 3) {
      std::cerr << "PCSSCounterOf was expecting a tree with a trifurcation at "
                   "the root!";
      abort();
    }
    topology->PCSSPreOrder([&pcss_dict, &tag_to_bitset, &count, &leaf_count](
                               const Node* sister_node, bool sister_direction,
                               const Node* focal_node, bool focal_direction,  //
                               const Node* child0_node,
                               bool child0_direction,  //
                               const Node* child1_node, bool child1_direction) {
      Bitset parent(2 * leaf_count, false);
      // The first chunk is for the sister node.
      parent.CopyFrom(tag_to_bitset.at(sister_node->Tag()), 0,
                      sister_direction);
      // The second chunk is for the focal node.
      parent.CopyFrom(tag_to_bitset.at(focal_node->Tag()), leaf_count,
                      focal_direction);
      // Now we build the child bitset.
      auto child0 = tag_to_bitset.at(child0_node->Tag());
      if (child0_direction) {
        child0.flip();
      }
      auto child1 = tag_to_bitset.at(child1_node->Tag());
      if (child1_direction) {
        child1.flip();
      }
      auto child = std::min(child0, child1);
      // Insert the parent-child pair into the map.
      auto search = pcss_dict.find(parent);
      if (search == pcss_dict.end()) {
        // The first time we have seen this parent.
        BitsetUInt32Dict child_singleton(0);
        child_singleton.increment(std::move(child), count);
        assert(pcss_dict.insert({parent, child_singleton}).second);
      } else {
        search->second.increment(std::move(child), count);
      }
    });
  }
  return pcss_dict;
}