// Copyright 2019-2020 libsbn project contributors.
// libsbn is free software under the GPLv3; see LICENSE file for details.
//
// This is a shared parent class between the rooted and unrooted SBN instances.
//
// The idea is to have as much in here as possible, which in practice means everything
// that doesn't require explicit reference to a specific tree type. Note that this
// excludes SBN training, which is different based on if we are thinking of trees as
// rooted or unrooted.
//
// We can't make this a pure virtual class because we want to have some shared
// functionality on the Python side (see pylibsbn.cpp).

#ifndef SRC_SBN_INSTANCE_HPP_
#define SRC_SBN_INSTANCE_HPP_

#include <random>
#include "ProgressBar.hpp"
#include "alignment.hpp"
#include "engine.hpp"
#include "numerical_utils.hpp"
#include "psp_indexer.hpp"
#include "sbn_maps.hpp"
#include "sbn_probability.hpp"

class SBNInstance {
 public:
  // A Range is a range of values of our vector using 0-indexed Python/Numpy slice
  // notation, such that if we have the range (1, 3), that refers to the items with
  // 0-index 1 and 2. Said another way, these are considered half-open intervals [start,
  // end).
  using Range = std::pair<size_t, size_t>;
  using RangeVector = std::vector<Range>;

  // The Primary Split Pair indexer.
  PSPIndexer psp_indexer_;
  // A vector that contains all of the SBN-related probabilities.
  EigenVectorXd sbn_parameters_;
  // The master indexer for SBN parameters.
  BitsetSizeMap indexer_;
  // A vector of the taxon names.
  StringVector taxon_names_;

  // ** Initialization and status

  explicit SBNInstance(const std::string &name) : name_(name), rescaling_{false} {}
  void PrintStatus();

  // ** Dummy functions corresponding to an empty tree_collection (see above).
  virtual size_t TaxonCount() const { return 0; }
  virtual StringVector TaxonNames() const { return {}; }
  virtual size_t TreeCount() const { return 0; }
  virtual TagStringMap TagTaxonMap() const { return {}; }
  virtual Node::TopologyCounter TopologyCounter() const { return {}; }
  virtual BitsetSizeDict RootsplitCounterOf(
      const Node::TopologyCounter &topologies) const {
    return BitsetSizeDict(0);
  }
  virtual PCSSDict PCSSCounterOf(const Node::TopologyCounter &topologies) const {
    return {};
  }

  // ** SBN-related items

  // Use the loaded trees to get the SBN maps, set taxon_names_, and prepare the
  // sbn_parameters_ vector.
  void ProcessLoadedTrees();

  void CheckTopologyCounter();

  // Define "SBN maps" to be the collection of maps associated with the
  // SBNInstance, such as indexer_, index_to_child_, parent_to_range_, and rootsplits_.
  void CheckSBNMapsAvailable();

  // "Pretty" string representation of the indexer.
  StringVector PrettyIndexer();
  void PrettyPrintIndexer();

  // Return indexer_ and parent_to_range_ converted into string-keyed maps.
  std::tuple<StringSizeMap, StringSizePairMap> GetIndexers() const;

  // Get the indexer, but reversed and with bitsets appropriately converted to
  // strings.
  StringVector StringReversedIndexer() const;

  void NormalizeSBNParametersInLog(EigenVectorXdRef sbn_parameters);

  // ** Phylogenetic likelihood

  // Get the phylogenetic model parameters as a big matrix.
  Eigen::Ref<EigenMatrixXd> GetPhyloModelParams();

  // The phylogenetic model parameters broken down into blocks according to
  // model structure. See test_libsbn.py for an example of what this does.
  BlockSpecification::ParameterBlockMap GetPhyloModelParamBlockMap();

  // Set whether we use rescaling for phylogenetic likelihood computation.
  void SetRescaling(bool use_rescaling) { rescaling_ = use_rescaling; }

  void CheckSequencesAndTreesLoaded() const;

  // Prepare for phylogenetic likelihood calculation. If we get a nullopt
  // argument, it just uses the number of trees currently in the SBNInstance.
  void PrepareForPhyloLikelihood(
      const PhyloModelSpecification &model_specification, size_t thread_count,
      const std::vector<BeagleFlags> &beagle_flag_vector = {},
      const bool use_tip_states = true,
      std::optional<size_t> tree_count_option = std::nullopt);

  // Make the number of phylogentic model parameters fit the number of trees and
  // the speficied model. If we get a nullopt argument, it just uses the number
  // of trees currently in the SBNInstance.
  void ResizePhyloModelParams(std::optional<size_t> tree_count_option);

  // ** I/O

  void ReadFastaFile(std::string fname);

 protected:
  // The name of our libsbn instance.
  std::string name_;
  // Our phylogenetic likelihood computation engine.
  std::unique_ptr<Engine> engine_;
  // Whether we use likelihood vector rescaling.
  bool rescaling_;
  // The multiple sequence alignment.
  Alignment alignment_;
  // A map that indexes these probabilities: rootsplits are at the beginning,
  // and PCSS bitsets are at the end.
  // The collection of rootsplits, with the same indexing as in the indexer_.
  BitsetVector rootsplits_;
  // A map going from the index of a PCSS to its child.
  SizeBitsetMap index_to_child_;
  // A map going from a parent subsplit to the range of indices in
  // sbn_parameters_ with its children. See the definition of Range for the indexing
  // convention.
  BitsetSizePairMap parent_to_range_;
  // The phylogenetic model parameterization. This has as many rows as there are
  // trees, and holds the parameters before likelihood computation, where they
  // will be processed across threads.
  EigenMatrixXd phylo_model_params_;
  // A counter for the currently loaded set of topologies.
  Node::TopologyCounter topology_counter_;

  // Random bits.
  static std::random_device random_device_;
  static std::mt19937 random_generator_;
  inline void SetSeed(unsigned long seed) { random_generator_.seed(seed); }

  // Make a likelihood engine with the given specification.
  void MakeEngine(const EngineSpecification &engine_specification,
                  const PhyloModelSpecification &model_specification);

  // Return a raw pointer to the engine if it's available.
  Engine *GetEngine() const;

  // Sample an integer index in [range.first, range.second) according to
  // sbn_parameters_.
  size_t SampleIndex(Range range) const;
  Node::NodePtr SampleTopology(bool rooted) const;

  // The input to this function is a parent subsplit (of length 2n).
  Node::NodePtr SampleTopology(const Bitset &parent_subsplit) const;

  // Clear all of the state that depends on the current tree collection.
  void ClearTreeCollectionAssociatedState();

  void PushBackRangeForParentIfAvailable(const Bitset &parent,
                                         SBNInstance::RangeVector &range_vector);
  RangeVector GetSubsplitRanges(const SizeVector &rooted_representation);

  static EigenVectorXd CalculateMultiplicativeFactors(const EigenVectorXdRef log_f);
  static EigenVectorXd CalculateVIMCOMultiplicativeFactors(
      const EigenVectorXdRef log_f);
};

#ifdef DOCTEST_LIBRARY_INCLUDED

#endif  // DOCTEST_LIBRARY_INCLUDED
#endif  // SRC_SBN_INSTANCE_HPP_
