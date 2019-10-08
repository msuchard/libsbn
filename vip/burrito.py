import click
import numpy as np

import libsbn
import vip.scalar_models
import vip.optimizers


class Burrito:
    """A class to wrap an instance and relevant model data.

    The current division of labor is that the optimizer handles everything after we have
    sampled a topology, while the burrito can sample topologies and then ask the
    optimizer to update model parameters accordingly.
    """

    def __init__(
        self,
        *,
        mcmc_nexus_path,
        fasta_path,
        model_name,
        optimizer_name,
        step_count,
        particle_count
    ):
        self.inst = libsbn.instance("burrito")

        # Read MCMC run to get tree structure.
        self.inst.read_nexus_file(mcmc_nexus_path)
        self.inst.process_loaded_trees()

        # Set up tree likelihood calculation.
        self.inst.read_fasta_file(fasta_path)
        self.inst.make_beagle_instances(1)
        # It's important to do tree sampling here so that self.branch_lengths
        # etc gets set up.
        self.sample_topology()

        sbn_model = vip.sbn_model.SBNModel(self.inst)
        scalar_model = vip.scalar_models.of_name(
            model_name,
            variable_count=len(self.branch_lengths),
            particle_count=particle_count,
        )
        self.opt = vip.optimizers.of_name(optimizer_name, sbn_model, scalar_model)

    @staticmethod
    def log_exp_prior(x, rate=10):
        return np.log(rate) - np.sum(rate * x, axis=1)

    @staticmethod
    def grad_log_exp_prior(x, rate=10):
        return -rate

    def sample_topology(self):
        """Sample a tree, then set up branch length vector and the translation
        from splits to branches and back again."""
        self.inst.sample_trees(1)
        tree = self.inst.tree_collection.trees[0]
        branch_lengths_extended = np.array(tree.branch_lengths, copy=False)
        # Here we are getting a slice that excludes the last (fake) element.
        # Thus we can just deal with the actual branch lengths.
        self.branch_lengths = branch_lengths_extended[:-1]

        # Note: in the following arrays, the particles are laid out along axis 0 and the
        # splits are laid out along axis 1.
        # Now we need to set things up to translate between split indexing and branch
        # indexing.
        # The ith entry of this array gives the index of the split
        # corresponding to the ith branch.
        self.branch_to_split = np.array(
            self.inst.get_psp_indexer_representations()[0][0]
        )
        # The ith entry of this array gives the index of the branch
        # corresponding to the ith split.
        self.split_to_branch = np.empty_like(self.branch_to_split)
        for branch in range(len(self.branch_to_split)):
            self.split_to_branch[self.branch_to_split[branch]] = branch

    def translate_branches_to_splits(self, branch_vector):
        """The ith entry of the array returned by this function is the entry of
        branch_vector corresponding to the ith split."""
        return branch_vector[self.split_to_branch]

    def translate_splits_to_branches(self, split_vector):
        """The ith entry of the array returned by this function is the entry of
        split_vector corresponding to the ith branch."""
        return split_vector[self.branch_to_split]

    def log_like_or_grad_with(self, split_lengths, grad=False):
        """Calculate log likelihood or the gradient with given split
        lengths."""
        self.branch_lengths[:] = self.translate_splits_to_branches(split_lengths)
        if grad:
            _, log_grad = self.inst.branch_gradients()[0]
            # This :-2 is because of the two trailing zeroes that appear at the end of
            # the gradient.
            result = self.translate_branches_to_splits(np.array(log_grad)[:-2])
        else:
            result = np.array(self.inst.log_likelihoods())[0]
        return result

    def phylo_log_like(self, split_lengths_arr):
        """Calculate phylogenetic log likelihood for each of the split length
        assignments laid out along axis 1."""
        return np.apply_along_axis(self.log_like_or_grad_with, 1, split_lengths_arr)

    def grad_phylo_log_like(self, split_lengths_arr):
        return np.apply_along_axis(
            lambda x: self.log_like_or_grad_with(x, grad=True), 1, split_lengths_arr
        )

    def phylo_log_upost(self, split_lengths_arr):
        """The unnormalized phylogenetic posterior with an Exp(10) prior."""
        return self.phylo_log_like(split_lengths_arr) + Burrito.log_exp_prior(
            split_lengths_arr
        )

    def grad_phylo_log_upost(self, split_lengths_arr):
        """The unnormalized phylogenetic posterior with an Exp(10) prior."""
        return self.grad_phylo_log_like(split_lengths_arr) + Burrito.grad_log_exp_prior(
            split_lengths_arr
        )

    def gradient_steps(self, step_count):
        with click.progressbar(range(step_count), label="Gradient descent") as bar:
            for step in bar:
                # TODO self.sample_topology()
                if not self.opt.gradient_step(
                    self.phylo_log_upost, self.grad_phylo_log_upost
                ):
                    raise Exception("ELBO is not finite. Stopping.")
