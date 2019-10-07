import pprint

import click


@click.group()
def cli():
    pass


@cli.command()
@click.option(
    "--model",
    type=click.Choice(
        ["lognormal", "tf_lognormal", "tf_truncated_lognormal", "tf_gamma"]
    ),
    default="lognormal",
    show_default=True,
)
@click.option(
    "--optimizer",
    type=click.Choice(["simple", "bump"]),
    default="simple",
    show_default=True,
)
@click.option(
    "--step-count",
    default=5,
    help="Number of gradient descent steps to take.",
    show_default=True,
)
@click.option(
    "--particle-count",
    default=100,
    help="Number of particles to use for stochastic gradient estimation.",
    show_default=True,
)
@click.option(
    "--out-prefix", default=None, help="Path prefix to which output should be saved."
)
@click.argument("data-path")
def benchmark(model, optimizer, step_count, particle_count, out_prefix, data_path):
    """Do a benchmarking comparison to an MCMC run.

    DATA_PATH is a path to a directory, which say is named X.

    We assume that X contains:

    X_out.t: an MCMC run on a fixed tree topology, and
    X.fasta: a FASTA file with the same sequence data as used for MCMC.
    """
    # This splendid non-Pythonic import in a function means that the CLI is fast unless
    # we do something.
    import vip.benchmark

    print("Starting validation:")
    pprint.pprint(locals())

    run_details, opt_trace, fitting_results = vip.benchmark.fixed(
        data_path,
        model_name=model,
        optimizer_name=optimizer,
        step_count=step_count,
        particle_count=particle_count,
    )
    if out_prefix is not None:
        opt_trace.to_csv(out_prefix + "_opt_trace.csv")
        fitting_results.to_csv(out_prefix + "_fitting_results.csv")
    pprint.pprint(run_details)