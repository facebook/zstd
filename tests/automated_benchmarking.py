import argparse
import glob
import json
import os
import time
import pickle as pk
import subprocess
import urllib.request


GITHUB_API_PR_URL = "https://api.github.com/repos/facebook/zstd/pulls?state=open"
GITHUB_URL_TEMPLATE = "https://github.com/{}/zstd"
MASTER_BUILD = {"user": "facebook", "branch": "dev", "hash": None}

# check to see if there are any new PRs every minute
DEFAULT_MAX_API_CALL_FREQUENCY_SEC = 60
PREVIOUS_PRS_FILENAME = "prev_prs.pk"

# Not sure what the threshold for triggering alarms should be
# 1% regression sounds like a little too sensitive but the desktop
# that I'm running it on is pretty stable so I think this is fine
CSPEED_REGRESSION_TOLERANCE = 0.01
DSPEED_REGRESSION_TOLERANCE = 0.01


def get_new_open_pr_builds(prev_state=True):
    prev_prs = None
    if os.path.exists(PREVIOUS_PRS_FILENAME):
        with open(PREVIOUS_PRS_FILENAME, "rb") as f:
            prev_prs = pk.load(f)
    data = json.loads(urllib.request.urlopen(GITHUB_API_PR_URL).read().decode("utf-8"))
    prs = {
        d["url"]: {
            "user": d["user"]["login"],
            "branch": d["head"]["ref"],
            "hash": d["head"]["sha"].strip(),
        }
        for d in data
    }
    with open(PREVIOUS_PRS_FILENAME, "wb") as f:
        pk.dump(prs, f)
    if not prev_state or prev_prs == None:
        return list(prs.values())
    return [pr for url, pr in prs.items() if url not in prev_prs or prev_prs[url] != pr]


def get_latest_hashes():
    tmp = subprocess.run(["git", "log", "-1"], stdout=subprocess.PIPE).stdout.decode(
        "utf-8"
    )
    sha1 = tmp.split("\n")[0].split(" ")[1]
    tmp = subprocess.run(
        ["git", "show", "{}^1".format(sha1)], stdout=subprocess.PIPE
    ).stdout.decode("utf-8")
    sha2 = tmp.split("\n")[0].split(" ")[1]
    tmp = subprocess.run(
        ["git", "show", "{}^2".format(sha1)], stdout=subprocess.PIPE
    ).stdout.decode("utf-8")
    sha3 = "" if len(tmp) == 0 else tmp.split("\n")[0].split(" ")[1]
    return [sha1.strip(), sha2.strip(), sha3.strip()]


def get_builds_for_latest_hash():
    hashes = get_latest_hashes()
    for b in get_new_open_pr_builds(False):
        if b["hash"] in hashes:
            return [b]
    return []


def clone_and_build(build):
    if build["user"] != None:
        github_url = GITHUB_URL_TEMPLATE.format(build["user"])
        os.system(
            """
            rm -rf zstd-{user}-{sha} &&
            git clone {github_url} zstd-{user}-{sha} &&
            cd zstd-{user}-{sha} &&
            {checkout_command}
            make &&
            cd ../
        """.format(
                user=build["user"],
                github_url=github_url,
                sha=build["hash"],
                checkout_command="git checkout {} &&".format(build["hash"])
                if build["hash"] != None
                else "",
            )
        )
        return "zstd-{user}-{sha}/zstd".format(user=build["user"], sha=build["hash"])
    else:
        os.system("cd ../ && make && cd tests")
        return "../zstd"


def benchmark_single(executable, level, filename):
    tmp = (
        subprocess.run(
            [executable, "-qb{}".format(level), filename], stderr=subprocess.PIPE
        )
        .stderr.decode("utf-8")
        .split(" ")
    )
    idx = [i for i, d in enumerate(tmp) if d == "MB/s"]
    return [float(tmp[idx[0] - 1]), float(tmp[idx[1] - 1])]


def benchmark_n(executable, level, filename, n):
    speeds_arr = [benchmark_single(executable, level, filename) for _ in range(n)]
    cspeed, dspeed = max(b[0] for b in speeds_arr), max(b[1] for b in speeds_arr)
    print(
        "Bench (executable={} level={} filename={}, iterations={}):\n\t[cspeed: {} MB/s, dspeed: {} MB/s]".format(
            os.path.basename(executable),
            level,
            os.path.basename(filename),
            n,
            cspeed,
            dspeed,
        )
    )
    return (cspeed, dspeed)


def benchmark(build, filenames, levels, iterations):
    executable = clone_and_build(build)
    return [
        [benchmark_n(executable, l, f, iterations) for f in filenames] for l in levels
    ]


def get_regressions(baseline_build, test_build, iterations, filenames, levels):
    old = benchmark(baseline_build, filenames, levels, iterations)
    new = benchmark(test_build, filenames, levels, iterations)
    regressions = []
    for j, level in enumerate(levels):
        for k, filename in enumerate(filenames):
            old_cspeed, old_dspeed = old[j][k]
            new_cspeed, new_dspeed = new[j][k]
            cspeed_reg = (old_cspeed - new_cspeed) / old_cspeed
            dspeed_reg = (old_dspeed - new_dspeed) / old_dspeed
            baseline_label = "{}:{} ({})".format(
                baseline_build["user"], baseline_build["branch"], baseline_build["hash"]
            )
            test_label = "{}:{} ({})".format(
                test_build["user"], test_build["branch"], test_build["hash"]
            )
            if cspeed_reg > CSPEED_REGRESSION_TOLERANCE:
                regressions.append(
                    "[COMPRESSION REGRESSION] (level={} filename={})\n\t{} -> {}\n\t{} -> {} ({:0.2f}%)".format(
                        level,
                        filename,
                        baseline_label,
                        test_label,
                        old_cspeed,
                        new_cspeed,
                        cspeed_reg * 100.0,
                    )
                )
            if dspeed_reg > DSPEED_REGRESSION_TOLERANCE:
                regressions.append(
                    "[DECOMPRESSION REGRESSION] (level={} filename={})\n\t{} -> {}\n\t{} -> {} ({:0.2f}%)".format(
                        level,
                        filename,
                        baseline_label,
                        test_label,
                        old_dspeed,
                        new_dspeed,
                        dspeed_reg * 100.0,
                    )
                )
    return regressions

def main(filenames, levels, iterations, builds=None, emails=None, continuous=False, frequency=DEFAULT_MAX_API_CALL_FREQUENCY_SEC):
    if builds == None:
        builds = get_new_open_pr_builds()
    while True:
        for test_build in builds:
            regressions = get_regressions(
                MASTER_BUILD, test_build, iterations, filenames, levels
            )
            body = "\n".join(regressions)
            if len(regressions) > 0:
                if emails != None:
                    os.system(
                        """
                        echo "{}" | mutt -s "[zstd regression] caused by new pr" {}
                    """.format(
                            body, emails
                        )
                    )
                    print("Emails sent to {}".format(emails))
                print(body)
        if not continuous:
            break
        time.sleep(frequency)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "directory", help="directory with files to benchmark", default="fuzz"
    )
    parser.add_argument("levels", help="levels to test eg ('1,2,3')", default="1,2,3")
    parser.add_argument(
        "mode", help="'fastmode', 'onetime', 'current' or 'continuous'", default="onetime"
    )
    parser.add_argument(
        "iterations", help="number of benchmark iterations to run", default=5
    )
    parser.add_argument(
        "emails",
        help="email addresses of people who will be alerted upon regression. Only for continuous mode",
        default=None,
    )
    parser.add_argument(
        "frequency",
        help="specifies the number of seconds to wait before each successive check for new PRs in continuous mode",
        default=DEFAULT_MAX_API_CALL_FREQUENCY_SEC
    )

    args = parser.parse_args()
    filenames = glob.glob("{}/**".format(args.directory))
    levels = [int(l) for l in args.levels.split(",")]
    mode = args.mode
    iterations = int(args.iterations)
    emails = args.emails
    frequency = int(args.frequency)

    if mode == "onetime":
        main(filenames, levels, iterations, frequency=frequency)
    elif mode == "current":
        builds = [{"user": None, "branch": "None", "hash": None}]
        main(filenames, levels, iterations, builds, frequency=frequency)
    elif mode == "fastmode":
        builds = [{"user": "facebook", "branch": "master", "hash": None}]
        main(filenames, levels, iterations, builds, frequency=frequency)
    else:
        main(filenames, levels, iterations, None, emails, True, frequency=frequency)
