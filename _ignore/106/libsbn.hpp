// Copyright 2019 libsbn project contributors.
// libsbn is free software under the GPLv3; see LICENSE file for details.

#ifndef SRC_LIBSBN_HPP_
#define SRC_LIBSBN_HPP_

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include "ProgressBar.hpp"
#include "alignment.hpp"
#include "driver.hpp"
#include "engine.hpp"
#include "numerical_utils.hpp"
#include "psp_indexer.hpp"
#include "sbn_maps.hpp"
#include "sbn_probability.hpp"
#include "sugar.hpp"
#include "tree.hpp"

class SBNInstance {
 public:
  // Trees get loaded in from a file or sampled from SBNs.
  TreeCollection tree_collection_;
  // The Primary Split Pair indexer.
  PSPIndexer psp_indexer_;
  // A vector that contains all of the SBN-related probabilities.
  EigenVectorXd sbn_parameters_;
  // The master indexer for SBN parameters.
  BitsetSizeMap indexer_;
  // A vector of the taxon names.
  std::vector<std::string> taxon_names_;

  // ** Initialization and status

  explicit SBNInstance(const std::string &name) : name_(name), rescaling_{false} {}

  size_t TreeCount() const { return tree_collection_.TreeCount(); }
  void PrintStatus();

  // ** SBN-related items

  // Define "SBN maps" to be the collection of maps associated with the
  // SBNInstance, such as indexer_, index_to_child_, parent_to_range_, and
  // rootsplits_.

  // Use the loaded trees to get the SBN maps, set taxon_names_, and prepare the
  // sbn_parameters_ vector.
  void ProcessLoadedTrees();
  void CheckSBNMapsAvailable();
  // "Pretty" string representation of the indexer.
  StringVector PrettyIndexer();
  void PrettyPrintIndexer();

  // SBN training. See sbn_probability.hpp for details.
  void TrainSimpleAverage();
  // max_iter is the maximum number of EM iterations to do, while score_epsilon is the
  // cutoff for score improvement.
  EigenVectorXd TrainExpectationMaximization(double alpha, size_t max_iter,
                                             double score_epsilon = 0.);
  EigenVectorXd CalculateSBNProbabilities();

  // Sample an integer index in [range.first, range.second) according to
  // sbn_parameters_.
  size_t SampleIndex(std::pair<size_t, size_t> range) const;

  // Sample a topology from the SBN.
  Node::NodePtr SampleTopology(bool rooted = false) const;

  // Sample trees and store them internally
  void SampleTrees(size_t count);

  // Get indexer representations of the trees in tree_collection_.
  // See the documentation of IndexerRepresentationOf in sbn_maps.hpp for an
  // explanation of what these are. This version uses the length of sbn_parameters_ as a
  // sentinel value for all rootsplits/PCSSs that aren't present in the indexer.
  std::vector<IndexerRepresentation> MakeIndexerRepresentations() const;

  // Get PSP indexer representations of the trees in tree_collection_.
  std::vector<SizeVectorVector> MakePSPIndexerRepresentations() const;

  // Return indexer_ and parent_to_range_ converted into string-keyed maps.
  std::tuple<StringSizeMap, StringSizePairMap> GetIndexers() const;

  // Get the indexer, but reversed and with bitsets appropriately converted to
  // strings.
  StringVector StringReversedIndexer() const;

  // Turn an IndexerRepresentation into a string representation of the underying
  // bitsets. This is really just so that we can make a test of indexer
  // representations.
  StringSetVector StringIndexerRepresentationOf(
      IndexerRepresentation indexer_representation) const;

  // Return a ragged vector of vectors such that the ith vector is the
  // collection of branch lengths in the current tree collection for the ith
  // split.
  DoubleVectorVector SplitLengths() const;

  // This function is really just for testing-- it recomputes counters from
  // scratch.
  std::pair<StringSizeMap, StringPCSSMap> SplitCounters() const;

  // ** Phylogenetic likelihood

  Eigen::Ref<EigenMatrixXd> GetPhyloModelParams();
  // The phylogenetic model parameters broken down into blocks according to
  // model structure. See test_libsbn.py for an example of what this does.
  BlockSpecification::ParameterBlockMap GetPhyloModelParamBlockMap();

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

  std::vector<double> LogLikelihoods();
  // For each loaded tree, returns a pair of (likelihood, gradient).
  std::vector<std::pair<double, std::vector<double>>> BranchGradients();

  // ** I/O

  void ParseDates();
  void ReadNewickFile(std::string fname);
  void ReadNexusFile(std::string fname);
  void ReadFastaFile(std::string fname);

 private:
  // The name of our libsbn instance.
  std::string name_;
  // Our phylogenetic likelihood computation engine.
  std::unique_ptr<Engine> engine_;
  // The multiple sequence alignment.
  Alignment alignment_;
  // A map that indexes these probabilities: rootsplits are at the beginning,
  // and PCSS bitsets are at the end.
  // The collection of rootsplits, with the same indexing as in the indexer_.
  BitsetVector rootsplits_;
  // A map going from the index of a PCSS to its child.
  SizeBitsetMap index_to_child_;
  // A map going from a parent subsplit to the range of indices in
  // sbn_parameters_ with its children.
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
  bool rescaling_;

  // Make a likelihood engine with the given specification.
  void MakeEngine(const EngineSpecification &engine_specification,
                  const PhyloModelSpecification &model_specification);
  // Return a raw pointer to the engine if it's available.
  Engine *GetEngine() const;

  // The input to this function is a parent subsplit (of length 2n).
  Node::NodePtr SampleTopology(const Bitset &parent_subsplit) const;

  // Clear all of the state that depends on the current tree collection.
  void ClearTreeCollectionAssociatedState();
};

#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("libsbn") {
  SBNInstance inst("charlie");
  inst.ReadNewickFile("data/hello.nwk");
  inst.ReadFastaFile("data/hello.fasta");
  PhyloModelSpecification simple_specification{"JC69", "constant", "strict"};
  inst.PrepareForPhyloLikelihood(simple_specification, 2);
  for (auto ll : inst.LogLikelihoods()) {
    CHECK_LT(fabs(ll - -84.852358), 0.000001);
  }
  // Reading one file after another checks that we've cleared out state.
  inst.ReadNewickFile("data/five_taxon.nwk");
  inst.ProcessLoadedTrees();
  auto pretty_indexer = inst.PrettyIndexer();
  // The indexer_ is to index the sbn_parameters_. Note that neither of these data
  // structures attempt to catalog the complete collection of rootsplits or PCSSs, but
  // just those that are present for some rooting of the input trees.
  //
  // The indexer_ and sbn_parameters_ are laid out as follows (I'll just call it the
  // "index" in what follows). Say there are rootsplit_count rootsplits in the support.
  // The first rootsplit_count entries of the index are assigned to the rootsplits
  // (again, those rootsplits that are present for some rooting of the unrooted input
  // trees). For the five_taxon example, this goes as follows:
  StringSet correct_pretty_rootsplits({"01110", "01010", "00101", "00111", "00001",
                                       "00011", "00010", "00100", "00110", "01000",
                                       "01111", "01001"});
  StringSet pretty_rootsplits(
      pretty_indexer.begin(),
      pretty_indexer.begin() + correct_pretty_rootsplits.size());
  CHECK(correct_pretty_rootsplits == pretty_rootsplits);
  // The rest of the entries of the index are laid out as blocks of parameters for
  // PCSSs that share the same parent. Take a look at the description of PCSS bitsets
  // (and the unit tests) in bitset.hpp to understand the notation used here.
  //
  // For example, here are four PCSSs that all share the parent 00001|11110:
  StringSet correct_pretty_pcss_block({"00001|11110|01110", "00001|11110|00010",
                                       "00001|11110|01000", "00001|11110|00100"});
  StringSet pretty_indexer_set(pretty_indexer.begin(), pretty_indexer.end());
  // It's true that this test doesn't show the block-ness, but it wasn't easy to show
  // off this feature in a way that wasn't compiler dependent.
  for (auto pretty_pcss : correct_pretty_pcss_block) {
    CHECK(pretty_indexer_set.find(pretty_pcss) != pretty_indexer_set.end());
  }
  // Now we can look at some tree representations. We get these by calling
  // IndexerRepresentationOf on a tree topology. This function "digests" the tree by
  // representing all of the PCSSs as bitsets which it can then look up in the indexer_.
  // It then spits them out as the rootsplit and PCSS indices.
  // The following tree is (2,(1,3),(0,4));, or with internal nodes (2,(1,3)5,(0,4)6)7
  auto indexer_test_topology_1 = Node::OfParentIdVector({6, 5, 7, 5, 6, 7, 7});
  // Here we look at the indexer representation of this tree. Rather than having the
  // indices themselves, which is what IndexerRepresentationOf actually outputs, we have
  // string representations of the features corresponding to those indices.
  // See sbn_maps.hpp for more description of these indexer representations.
  StringSetVector correct_representation_1(
      // The indexer representations for each of the possible virtual rootings.
      // For example, this first one is for rooting at the edge leading to leaf
      // 0, the second for rooting at leaf 1, etc.
      {{"01111", "10000|01111|00001", "00001|01110|00100", "00100|01010|00010"},
       {"01000", "01000|10111|00010", "00100|10001|00001", "00010|10101|00100"},
       {"00100", "10001|01010|00010", "01010|10001|00001", "00100|11011|01010"},
       {"00010", "00010|11101|01000", "00100|10001|00001", "01000|10101|00100"},
       {"00001", "00001|11110|01110", "10000|01110|00100", "00100|01010|00010"},
       {"01010", "10101|01010|00010", "00100|10001|00001", "01010|10101|00100"},
       {"01110", "00100|01010|00010", "10001|01110|00100", "01110|10001|00001"}});
  // Here 99999999 is the default value if a rootsplit or PCSS is missing.
  const size_t out_of_sample_index = 99999999;
  CHECK_EQ(inst.StringIndexerRepresentationOf(SBNMaps::IndexerRepresentationOf(
               inst.indexer_, indexer_test_topology_1, out_of_sample_index)),
           correct_representation_1);
  // See the "concepts" part of the online documentation to learn about PSP indexing.
  auto correct_psp_representation_1 = StringVectorVector(
      {{"01111", "01000", "00100", "00010", "00001", "01010", "01110"},
       {"", "", "", "", "", "01010|00010", "10001|00001"},
       {"01111|00001", "10111|00010", "11011|01010", "11101|01000", "11110|01110",
        "10101|00100", "01110|00100"}});
  CHECK_EQ(inst.psp_indexer_.StringRepresentationOf(indexer_test_topology_1),
           correct_psp_representation_1);
  // Same as above but for (((0,1),2),3,4);, or with internal nodes (((0,1)5,2)6,3,4)7;
  auto indexer_test_topology_2 = Node::OfParentIdVector({5, 5, 6, 7, 7, 6, 7});
  StringSetVector correct_representation_2(
      {{"01111", "10000|01111|00111", "00100|00011|00001", "01000|00111|00011"},
       {"01000", "01000|10111|00111", "00100|00011|00001", "10000|00111|00011"},
       {"00100", "00100|11011|00011", "11000|00011|00001", "00011|11000|01000"},
       {"00010", "00100|11000|01000", "00001|11100|00100", "00010|11101|00001"},
       {"00001", "00100|11000|01000", "00001|11110|00010", "00010|11100|00100"},
       {"00111", "00111|11000|01000", "00100|00011|00001", "11000|00111|00011"},
       {"00011", "00100|11000|01000", "11100|00011|00001", "00011|11100|00100"}});
  CHECK_EQ(inst.StringIndexerRepresentationOf(SBNMaps::IndexerRepresentationOf(
               inst.indexer_, indexer_test_topology_2, out_of_sample_index)),
           correct_representation_2);
  auto correct_psp_representation_2 = StringVectorVector(
      {{"01111", "01000", "00100", "00010", "00001", "00111", "00011"},
       {"", "", "", "", "", "11000|01000", "11100|00100"},
       {"01111|00111", "10111|00111", "11011|00011", "11101|00001", "11110|00010",
        "00111|00011", "00011|00001"}});
  CHECK_EQ(inst.psp_indexer_.StringRepresentationOf(indexer_test_topology_2),
           correct_psp_representation_2);

  // Test of RootedIndexerRepresentationOf.
  // Topology is ((((0,1),2),3),4);, or with internal nodes ((((0,1)5,2)6,3)7,4)8;
  auto indexer_test_rooted_topology_1 =
      Node::OfParentIdVector({5, 5, 6, 7, 8, 6, 7, 8});
  auto correct_rooted_indexer_representation_1 = StringSet(
      {"00001", "00001|11110|00010", "00010|11100|00100", "00100|11000|01000"});
  CHECK_EQ(inst.StringIndexerRepresentationOf({SBNMaps::RootedIndexerRepresentationOf(
               inst.indexer_, indexer_test_rooted_topology_1, out_of_sample_index)})[0],
           correct_rooted_indexer_representation_1);
  // Topology is (((0,1),2),(3,4));, or with internal nodes (((0,1)5,2)6,(3,4)7)8;
  auto indexer_test_rooted_topology_2 =
      Node::OfParentIdVector({5, 5, 6, 7, 7, 6, 8, 8});
  auto correct_rooted_indexer_representation_2 = StringSet(
      {"00011", "11100|00011|00001", "00011|11100|00100", "00100|11000|01000"});
  CHECK_EQ(inst.StringIndexerRepresentationOf({SBNMaps::RootedIndexerRepresentationOf(
               inst.indexer_, indexer_test_rooted_topology_2, out_of_sample_index)})[0],
           correct_rooted_indexer_representation_2);

  // Test likelihood and gradient computation.
  inst.ReadNexusFile("data/DS1.subsampled_10.t");
  inst.ReadFastaFile("data/DS1.fasta");
  std::vector<BeagleFlags> vector_flag_options{BEAGLE_FLAG_VECTOR_NONE,
                                               BEAGLE_FLAG_VECTOR_SSE};
  std::vector<bool> tip_state_options{false, true};
  for (const auto vector_flag : vector_flag_options) {
    for (const auto tip_state_option : tip_state_options) {
      inst.PrepareForPhyloLikelihood(simple_specification, 2, {vector_flag},
                                     tip_state_option);
      auto likelihoods = inst.LogLikelihoods();
      std::vector<double> pybeagle_likelihoods(
          {-14582.995273982739, -6911.294207416366, -6916.880235529542,
           -6904.016888831189, -6915.055570693576, -6915.50496696512,
           -6910.958836661867, -6909.02639968063, -6912.967861935749,
           -6910.7871105783515});
      for (size_t i = 0; i < likelihoods.size(); i++) {
        CHECK_LT(fabs(likelihoods[i] - pybeagle_likelihoods[i]), 0.00011);
      }

      auto gradients = inst.BranchGradients();
      // Test the log likelihoods.
      for (size_t i = 0; i < likelihoods.size(); i++) {
        CHECK_LT(fabs(gradients[i].first - pybeagle_likelihoods[i]), 0.00011);
      }
      // Test the gradients for the last tree.
      auto last = gradients.back();
      std::sort(last.second.begin(), last.second.end());
      // Zeros are for the root and one of the descendants of the root.
      std::vector<double> physher_gradients = {
          -904.18956, -607.70500, -562.36274, -553.63315, -542.26058, -539.64210,
          -463.36511, -445.32555, -414.27197, -412.84218, -399.15359, -342.68038,
          -306.23644, -277.05392, -258.73681, -175.07391, -171.59627, -168.57646,
          -150.57623, -145.38176, -115.15798, -94.86412,  -83.02880,  -80.09165,
          -69.00574,  -51.93337,  0.00000,    0.00000,    16.17497,   20.47784,
          58.06984,   131.18998,  137.10799,  225.73617,  233.92172,  253.49785,
          255.52967,  259.90378,  394.00504,  394.96619,  396.98933,  429.83873,
          450.71566,  462.75827,  471.57364,  472.83161,  514.59289,  650.72575,
          888.87834,  913.96566,  927.14730,  959.10746,  2296.55028};
      for (size_t i = 0; i < last.second.size(); i++) {
        CHECK_LT(fabs(last.second[i] - physher_gradients[i]), 0.0001);
      }

      // Test rescaling
      inst.SetRescaling(true);
      auto likelihoods_rescaling = inst.LogLikelihoods();
      // Likelihoods from LogLikelihoods()
      for (size_t i = 0; i < likelihoods_rescaling.size(); i++) {
        CHECK_LT(fabs(likelihoods_rescaling[i] - pybeagle_likelihoods[i]), 0.00011);
      }
      // Likelihoods from BranchGradients()
      inst.PrepareForPhyloLikelihood(simple_specification, 1, {}, tip_state_option);
      auto gradients_rescaling = inst.BranchGradients();
      for (size_t i = 0; i < gradients_rescaling.size(); i++) {
        CHECK_LT(fabs(gradients_rescaling[i].first - pybeagle_likelihoods[i]), 0.00011);
      }
      // Gradients
      auto last_rescaling = gradients_rescaling.back();
      std::sort(last_rescaling.second.begin(), last_rescaling.second.end());
      for (size_t i = 0; i < last_rescaling.second.size(); i++) {
        CHECK_LT(fabs(last_rescaling.second[i] - physher_gradients[i]), 0.0001);
      }
    }
  }

  // Test SBN training.
  inst.ReadNewickFile("data/DS1.100_topologies.nwk");
  inst.ProcessLoadedTrees();
  // These "Expected" functions are defined in sbn_probability.hpp.
  const auto expected_SA = ExpectedSAVector();
  inst.TrainSimpleAverage();
  CheckVectorXdEquality(inst.CalculateSBNProbabilities(), expected_SA, 1e-12);
  // Expected EM vectors with alpha = 0.
  const auto [expected_EM_0_1, expected_EM_0_23] = ExpectedEMVectorsAlpha0();
  // 1 iteration of EM with alpha = 0.
  inst.TrainExpectationMaximization(0., 1);
  CheckVectorXdEquality(inst.CalculateSBNProbabilities(), expected_EM_0_1, 1e-12);
  // 23 iterations of EM with alpha = 0.
  inst.TrainExpectationMaximization(0., 23);
  CheckVectorXdEquality(inst.CalculateSBNProbabilities(), expected_EM_0_23, 1e-12);
  // 100 iteration of EM with alpha = 0.5.
  const auto expected_EM_05_100 = ExpectedEMVectorAlpha05();
  inst.TrainExpectationMaximization(0.5, 100);
  CheckVectorXdEquality(inst.CalculateSBNProbabilities(), expected_EM_05_100, 1e-5);
  const auto expected_EM_00001_100 = ExpectedEMVectorAlpha05();

  // Test tree sampling.
  inst.ReadNewickFile("data/five_taxon.nwk");
  inst.ProcessLoadedTrees();
  inst.TrainSimpleAverage();
  // Count the frequencies of rooted trees in a file.
  size_t rooted_tree_count_from_file = 0;
  RootedIndexerRepresentationSizeDict counter_from_file(0);
  for (const auto &indexer_representation : inst.MakeIndexerRepresentations()) {
    SBNMaps::IncrementRootedIndexerRepresentationSizeDict(counter_from_file,
                                                          indexer_representation);
    rooted_tree_count_from_file += indexer_representation.size();
  }
  // Count the frequencies of trees when we sample after training with SimpleAverage.
  size_t sampled_tree_count = 1'000'000;
  RootedIndexerRepresentationSizeDict counter_from_sampling(0);
  ProgressBar progress_bar(sampled_tree_count / 1000);
  for (size_t sample_idx = 0; sample_idx < sampled_tree_count; ++sample_idx) {
    const auto rooted_topology = inst.SampleTopology(true);
    SBNMaps::IncrementRootedIndexerRepresentationSizeDict(
        counter_from_sampling,
        SBNMaps::RootedIndexerRepresentationOf(inst.indexer_, rooted_topology,
                                               out_of_sample_index));
    if (sample_idx % 1000 == 0) {
      ++progress_bar;
      progress_bar.display();
    }
  }
  // These should be equal in the limit when we're training with SA.
  for (const auto &[key, _] : counter_from_file) {
    double observed =
        static_cast<double>(counter_from_sampling.at(key)) / sampled_tree_count;
    double expected =
        static_cast<double>(counter_from_file.at(key)) / rooted_tree_count_from_file;
    CHECK_LT(fabs(observed - expected), 5e-3);
  }
  progress_bar.done();
}
#endif  // DOCTEST_LIBRARY_INCLUDED
#endif  // SRC_LIBSBN_HPP_
