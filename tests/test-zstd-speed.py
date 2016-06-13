#! /usr/bin/env python
# execute(), fetch(), notify() are based on https://github.com/getlantern/build-automation/blob/master/build.py

import argparse
import os
import string
import time
import traceback
from subprocess import Popen, PIPE

repo_url = 'https://github.com/Cyan4973/zstd.git'
test_dir_name = 'speedTest'


def log(text):
    print time.strftime("%Y/%m/%d %H:%M:%S") + ' - ' + text

def execute(command, print_output=False, print_error=True):
    log("> " + command)
    popen = Popen(command, stdout=PIPE, stderr=PIPE, shell=True, cwd=execute.cwd)
    itout = iter(popen.stdout.readline, b"")
    iterr = iter(popen.stderr.readline, b"")
    stdout_lines = list(itout)
    if print_output:
        print ''.join(stdout_lines)
    stderr_lines = list(iterr)
    if print_output:
        print ''.join(stderr_lines)
    popen.communicate()
    if popen.returncode is not None and popen.returncode != 0:
        if not print_output and print_error:
            print ''.join(stderr_lines)    
        raise RuntimeError(''.join(stderr_lines))
    return stdout_lines + stderr_lines
execute.cwd = None


def does_command_exist(command):
    try:
        execute(command, False, False);
    except Exception as e:
        return False
    return True


def fetch():
    execute('git fetch -p')
    output = execute('git branch -rl')
    for line in output:
        if "HEAD" in line: 
            output.remove(line) # remove "origin/HEAD -> origin/dev"
    branches = map(lambda l: l.strip(), output)
    return map(lambda b: (b, execute('git show -s --format=%h ' + b)[0].strip()), branches)


def notify(branch, commit, last_commit):
    text_tmpl = string.Template('Changes since $last_commit:\r\n$commits')
    branch = branch.split('/')[1]
    fmt = '--format="%h: (%an) %s, %ar"'
    if last_commit is None:
        commits = execute('git log -n 10 %s %s' % (fmt, commit))
    else:
        commits = execute('git --no-pager log %s %s..%s' % (fmt, last_commit, commit))

    text = text_tmpl.substitute({'last_commit': last_commit, 'commits': ''.join(commits)})
    print str("commits for %s: %s" % (commit, text))


def compile(branch, commit, dry_run):
    local_branch = string.split(branch, '/')[1]
    version = local_branch.rpartition('-')[2]
    version = version + '_' + commit
    execute('git checkout -- . && git checkout ' + branch)
    if not dry_run:
        execute('VERSION=' + version + '; make clean zstdprogram')


def get_last_commit(resultsFileName):
    if not os.path.isfile(resultsFileName):
        return None, None, None
    commit = None
    cspeed = []
    dspeed = []
    with open(resultsFileName,'r') as f:
        for line in f:
            words = line.split()
            if len(words) == 2: # branch + commit
                commit = words[1];
                cspeed = []
                dspeed = []
            if (len(words) == 8): 
                cspeed.append(float(words[3]))
                dspeed.append(float(words[5]))
        #if commit != None:
        #    print "commit=%s cspeed=%s dspeed=%s" % (commit, cspeed, dspeed)
    return commit, cspeed, dspeed


def benchmark_and_compare(branch, commit, resultsFileName, lastCLevel, testFilePath, fileName, last_cspeed, last_dspeed, lower_limit, maxLoadAvg, message):
    sleepTime = 30
    while os.getloadavg()[0] > maxLoadAvg:
        log("WARNING: bench loadavg=%.2f is higher than %s, sleeping for %s seconds" % (os.getloadavg()[0], maxLoadAvg, sleepTime))
        time.sleep(sleepTime)
    start_load = str(os.getloadavg())
    result = execute('programs/zstd -qi5b1e' + str(lastCLevel) + ' ' + testFilePath)
    end_load = str(os.getloadavg())
    linesExpected = lastCLevel + 2;
    if len(result) != linesExpected:
        log("ERROR: number of result lines=%d is different that expected %d" % (len(result), linesExpected))
        return ""
    with open(resultsFileName, "a") as myfile:
        myfile.write(branch + " " + commit + "\n")
        myfile.writelines(result)
        myfile.close()
        if (last_cspeed == None):
            return ""
        commit, cspeed, dspeed = get_last_commit(resultsFileName)
        text = ""
        for i in range(0, min(len(cspeed), len(last_cspeed))):
            if (cspeed[i]/last_cspeed[i] < lower_limit):
                text += "WARNING: File=%s level=%d cspeed=%s last=%s diff=%s\n" % (fileName, i+1, cspeed[i], last_cspeed[i], cspeed[i]/last_cspeed[i])
            if (dspeed[i]/last_dspeed[i] < lower_limit):
                text += "WARNING: File=%s level=%d dspeed=%s last=%s diff=%s\n" % (fileName, i+1, dspeed[i], last_dspeed[i], dspeed[i]/last_dspeed[i])
        if text:
            text = message + ("\nmaxLoadAvg=%s  load average at start=%s end=%s\n" % (maxLoadAvg, start_load, end_load)) + text
        return text


def send_email(branch, commit, last_commit, emails, text, results_files, logFileName, lower_limit, have_mutt, have_mail):
    with open(logFileName, "w") as myfile:
        myfile.writelines(text)
        myfile.close()
        if have_mutt:
            execute("mutt -s \"[ZSTD_speedTest] Warning for branch=" + branch + " commit=" + commit + " last_commit=" + last_commit + " speed<" + str(lower_limit) + "\" " + emails + " -a " + results_files + " < " + logFileName)
        elif have_mail:
            execute("mail -s \"[ZSTD_speedTest] Warning for branch=" + branch + " commit=" + commit + " last_commit=" + last_commit + " speed<" + str(lower_limit) + "\" " + emails + " < " + logFileName)
        else:
            log("e-mail cannot be sent (mail and mutt not found)")


def check_branches(args, test_path, testFilePaths, have_mutt, have_mail):
    for branch, commit in fetch():
        try:
            commitFileName = test_path + "/commit_" + branch.replace("/", "_")
            if os.path.isfile(commitFileName):
                last_commit = file(commitFileName, 'r').read()
            else:
                last_commit = None
            file(commitFileName, 'w').write(commit)

            if commit == last_commit:
                log("skipping branch %s: head %s already processed" % (branch, commit))
            else:
                log("build branch %s: head %s is different from prev %s" % (branch, commit, last_commit))
                compile(branch, commit, args.dry_run)

                logFileName = test_path + "/log_" + branch.replace("/", "_")
                text_to_send = []
                results_files = ""
                for filePath in testFilePaths:
                    fileName = filePath.rpartition('/')[2]
                    resultsFileName = test_path + "/results_" + branch.replace("/", "_") + "_" + fileName
                    last_commit, cspeed, dspeed = get_last_commit(resultsFileName)

                    if not args.dry_run:
                        text = benchmark_and_compare(branch, commit, resultsFileName, args.lastCLevel, filePath, fileName, cspeed, dspeed, args.lowerLimit, args.maxLoadAvg, args.message)
                        if text:
                            text = benchmark_and_compare(branch, commit, resultsFileName, args.lastCLevel, filePath, fileName, cspeed, dspeed, args.lowerLimit, args.maxLoadAvg, args.message)
                            if text:
                                text_to_send.append(text)
                                results_files += resultsFileName + " "
                if text_to_send:
                    send_email(branch, commit, last_commit, args.emails, text_to_send, results_files, logFileName, args.lowerLimit, have_mutt, have_mail)
                notify(branch, commit, last_commit)
        except Exception as e:
            stack = traceback.format_exc()
            log("ERROR: build %s, error %s" % (branch, str(e)) )
            print stack


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('testFileNames', help='file names list for speed benchmark')
    parser.add_argument('emails', help='list of e-mail addresses to send warnings')
    parser.add_argument('--message', help='attach an additional message to e-mail', default="")
    parser.add_argument('--lowerLimit', type=float, help='send email if speed is lower than given limit', default=0.98)
    parser.add_argument('--maxLoadAvg', type=float, help='maximum load average to start testing', default=0.75)
    parser.add_argument('--lastCLevel', type=int, help='last compression level for testing', default=5)
    parser.add_argument('--sleepTime', type=int, help='frequency of repository checking in seconds', default=300)
    parser.add_argument('--dry-run', dest='dry_run', action='store_true', help='not build', default=False)
    args = parser.parse_args()

    # check if test files are accessible
    testFileNames = args.testFileNames.split()
    testFilePaths = []
    for fileName in testFileNames:
        if os.path.isfile(fileName):
            testFilePaths.append(os.path.abspath(fileName))
        else:
            raise RuntimeError("File not found: " + fileName)

    test_path = os.getcwd() + '/' + test_dir_name     # /path/to/zstd/tests/speedTest 
    clone_path = test_path + '/' + 'zstd'             # /path/to/zstd/tests/speedTest/zstd 

    # check availability of e-mail senders
    have_mutt = does_command_exist("mutt --help");
    have_mail = does_command_exist("mail -V");
    if not have_mutt and not have_mail:
        log("WARNING: e-mail senders mail and mutt not found")

    # clone ZSTD repo if needed
    if not os.path.isdir(test_path):
        os.mkdir(test_path)
    if not os.path.isdir(clone_path):
        execute.cwd = test_path
        execute('git clone ' + repo_url)
    if not os.path.isdir(clone_path):
        raise RuntimeError("ZSTD clone not found: " + clone_path)
    execute.cwd = clone_path

    print "PARAMETERS:\ntest_path=%s" % test_path
    print "clone_path=%s" % clone_path
    print "testFilePath(%s)=%s" % (len(testFilePaths), testFilePaths)
    print "message=%s" % args.message
    print "emails=%s" % args.emails
    print "maxLoadAvg=%s" % args.maxLoadAvg
    print "lowerLimit=%s" % args.lowerLimit
    print "lastCLevel=%s" % args.lastCLevel
    print "sleepTime=%s" % args.sleepTime
    print "dry_run=%s" % args.dry_run
    print "have_mutt=%s have_mail=%s" % (have_mutt, have_mail)

    while True:
        pid = str(os.getpid())
        pidfile = "./speedTest.pid"
        if os.path.isfile(pidfile):
            log("%s already exists, exiting" % pidfile)
        else:
            file(pidfile, 'w').write(pid)
            try:
                loadavg = os.getloadavg()[0]
                if (loadavg <= args.maxLoadAvg):
                    check_branches(args, test_path, testFilePaths, have_mutt, have_mail)
                else:
                    log("WARNING: main loadavg=%.2f is higher than %s" % (loadavg, args.maxLoadAvg))
            finally:
                os.unlink(pidfile)
        log("sleep for %s seconds" % args.sleepTime)
        time.sleep(args.sleepTime)
