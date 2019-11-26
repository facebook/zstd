import argparse
import glob
import os
import re

import numpy as np


GITHUB_URL = "https://github.com/facebook/zstd"
WORKING_DIR = "zstd-automated-benchmarking"
LEVELS = [1, 2, 3, 4, 5]
BUILDS = [{"compiler": "clang", "mode": "64"}, {"compiler": "gcc", "mode": "64"}]

# Not sure what the threshold for triggering alarms should be
# 1% regression sounds like a little too sensitive but the desktop
# that I'm running it on is pretty stable so I think this is fine
CSPEED_REGRESSION_TOLERANCE = 0.01
DSPEED_REGRESSION_TOLERANCE = 0.01
N_BENCHMARK_ITERATIONS = 5


def clean():
    os.system("rm -rf {}".format(WORKING_DIR))
    print("Cleaned directories")


def clone_repo():
    os.system("git clone {} {}".format(GITHUB_URL, WORKING_DIR))
    print("Git repo ({}) cloned in {}".format(GITHUB_URL, WORKING_DIR))


def build(version, compiler="gcc", mode="64"):
    os.system(
        """
        cd {working_dir} &&
        make -C programs clean zstd {mode_str} CC={compiler} MOREFLAGS="-DZSTD_GIT_COMMIT={version}" &&
        cp programs/zstd programs/zstd-{version}-{compiler}-{mode}
    """.format(
            version=version,
            compiler=compiler,
            mode=mode,
            working_dir=WORKING_DIR,
            mode_str="zstd32" if mode == "32" else "",
        )
    )
    os.system("cd ../")
    print("Built (version={} compiler={} mode={})".format(version, compiler, mode))
    return "{}/programs/zstd-{}-{}-{}".format(WORKING_DIR, version, compiler, mode)


def bench(executable, level, filename):
    os.system("{} -qb{} {} &> tmp".format(executable, level, filename))
    with open("tmp", "r") as f:
        output = f.read()
        floats = sorted(set([float(f) for f in re.findall("\d+\.\d+", output)]))
        dspeed = floats[-1]
        cspeed = next(x for x in reversed(floats) if x < dspeed * 0.75)
    os.system("rm -rf tmp")
    return [cspeed, dspeed]


def bench_n(executable, level, filename, n=N_BENCHMARK_ITERATIONS):
    speeds = np.max([bench(executable, level, filename) for _ in range(n)], axis=0)
    print(
        "Bench (executable={} level={} filename={}, iterations={}):\n\t[cspeed: {} MB/s, dspeed: {} MB/s]".format(
            os.path.basename(executable),
            level,
            os.path.basename(filename),
            n,
            speeds[0],
            speeds[1],
        )
    )
    return speeds


def get_latest_git_hash():
    os.system("cd {}".format(WORKING_DIR))
    os.system("git rev-parse HEAD &> tmp")
    with open("tmp", "r") as f:
        latest_hash = f.read()
    os.system("rm -rf tmp; cd ../")
    return latest_hash


def bench_cycle(version, filenames):
    print("version={}".format(version))
    clean()
    clone_repo()
    res = []
    for b in BUILDS:
        executable = build(version, b["compiler"], b["mode"])
        res.append([[bench_n(executable, l, f) for f in filenames] for l in LEVELS])
    print("Benchmarked version {}".format(version))
    return np.array(res)


def compare_versions(old_version, new_version, filenames):
    old = bench_cycle(old_version, filenames)
    new = bench_cycle(new_version, filenames)
    regressions = []
    for i, b in enumerate(BUILDS):
        for j, l in enumerate(LEVELS):
            for k, f in enumerate(filenames):
                old_cspeed, old_dspeed = old[i][j][k]
                new_cspeed, new_dspeed = new[i][j][k]
                cspeed_reg = (old_cspeed - new_cspeed) / old_cspeed
                dspeed_reg = (old_dspeed - new_dspeed) / old_dspeed
                if cspeed_reg > -CSPEED_REGRESSION_TOLERANCE:
                    regressions.append(
                        "[COMPRESSION REGRESSION] (compiler={} mode={} level={} filename={})\n\t{} -> {}\n\t{} -> {} ({:0.2f}%)".format(
                            b["compiler"],
                            b["mode"],
                            l,
                            f,
                            old_version,
                            new_version,
                            old_cspeed,
                            new_cspeed,
                            cspeed_reg * 100.0,
                        )
                    )
                if dspeed_reg > -DSPEED_REGRESSION_TOLERANCE:
                    regressions.append(
                        "[DECOMPRESSION REGRESSION] (compiler={} mode={} level={} filename={})\n\t{} -> {}\n\t{} -> {} ({:0.2f}%)".format(
                            b["compiler"],
                            b["mode"],
                            l,
                            f,
                            old_version,
                            new_version,
                            old_dspeed,
                            new_dspeed,
                            dspeed_reg * 100.0,
                        )
                    )
    return regressions


def get_nth_version_from_head(n=0):
    os.system('git log -{} --format="%H" &> tmp'.format(n))
    with open("tmp", "r") as f:
        hash_val = f.read()
    os.system("rm -rf tmp")
    return hash_val


def compare_current_version_with_latest(filenames):
    current_version = get_nth_version_from_head()
    os.system("git pull")
    latest_version = get_nth_version_from_head()
    if current_version == latest_version:
        print("No new version to tests")
        return []
    return compare_versions(current_version, latest_version, filenames)


def start_benchmarking(filenames, emails):
    print("Started benchmarking loop")
    while True:
        regressions = compare_current_version_with_latest(filenames)
        if len(regressions) > 0:
            body = "\n".join(regressions)
            os.system(
                """
                echo "{}" | mutt -s "[zstd regression] caused by latest commit" {}
            """.format(
                    body, emails
                )
            )
            print("Emailed {} about new regressions".format(emails))
            print(body)
        else:
            print("No regressions")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("filenames", help="Directory with files to benchmark")
    parser.add_argument(
        "emails", help="Email addresses of people who will be alerted upon regression"
    )
    parser.add_argument("github_url", help="Url of the git repo", default=GITHUB_URL)
    parser.add_argument(
        "working_dir",
        help="Name of directory where everything will be checkout out and built",
        default=WORKING_DIR,
    )
    parser.add_argument(
        "levels", help="Which levels to test eg ('1,2,3,4,5')", default="1,2,3,4,5"
    )
    parser.add_argument(
        "builds",
        help="format: 'compiler|mode,compiler|mode'. eg ('gcc|64,clang|64')",
        default="gcc|64,clang|64",
    )
    args = parser.parse_args()
    filenames = glob.glob("{}/**".format(args.filenames))
    emails = args.emails
    GITHUB_URL = args.github_url
    WORKING_DIR = args.working_dir
    LEVELS = [int(l) for l in args.levels.split(",")]
    BUILDS = [
        {"compiler": b.split("|")[0], "mode": b.split("|")[1]}
        for b in args.builds.split(",")
    ]
    start_benchmarking(filenames, emails)
