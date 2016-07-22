#! /usr/bin/env python

import argparse
import os
import string
import time
import traceback
import subprocess
import signal


default_repo_url = 'https://github.com/Cyan4973/zstd.git'
working_dir_name = 'speedTest'
working_path = os.getcwd() + '/' + working_dir_name     # /path/to/zstd/tests/speedTest
clone_path = working_path + '/' + 'zstd'                # /path/to/zstd/tests/speedTest/zstd
email_header = '[ZSTD_speedTest]'
pid = str(os.getpid())
verbose = False


def log(text):
    print(time.strftime("%Y/%m/%d %H:%M:%S") + ' - ' + text)


def execute(command, print_command=True, print_output=False, print_error=True, param_shell=True):
    if print_command:
        log("> " + command)
    popen = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=param_shell, cwd=execute.cwd)
    stdout = popen.communicate()[0]
    stdout_lines = stdout.splitlines()
    if print_output:
        print('\n'.join(stdout_lines))
    if popen.returncode is not None and popen.returncode != 0:
        if not print_output and print_error:
            print('\n'.join(stdout_lines))
        raise RuntimeError('\n'.join(stdout_lines))
    return stdout_lines
execute.cwd = None


def does_command_exist(command):
    try:
        execute(command, verbose, False, False);
    except Exception as e:
        return False
    return True


def send_email(emails, topic, text, have_mutt, have_mail):
    logFileName = working_path + '/' + 'tmpEmailContent'
    with open(logFileName, "w") as myfile:
        myfile.writelines(text)
        myfile.close()
        if have_mutt:
            execute('mutt -s "' + topic + '" ' + emails + ' < ' + logFileName, verbose)
        elif have_mail:
            execute('mail -s "' + topic + '" ' + emails + ' < ' + logFileName, verbose)
        else:
            log("e-mail cannot be sent (mail or mutt not found)")


def send_email_with_attachments(branch, commit, last_commit, args, text, results_files, logFileName, have_mutt, have_mail):
    with open(logFileName, "w") as myfile:
        myfile.writelines(text)
        myfile.close()
        email_topic = '%s:%s Warning for %s:%s last_commit=%s speed<%s ratio<%s' % (email_header, pid, branch, commit, last_commit, args.lowerLimit, args.ratioLimit)
        if have_mutt:
            execute('mutt -s "' + email_topic + '" ' + args.emails + ' -a ' + results_files + ' < ' + logFileName)
        elif have_mail:
            execute('mail -s "' + email_topic + '" ' + args.emails + ' < ' + logFileName)
        else:
            log("e-mail cannot be sent (mail or mutt not found)")


def git_get_branches():
    execute('git fetch -p', verbose)
    branches = execute('git branch -rl', verbose)
    output = []
    for line in branches:
        if ("HEAD" not in line) and ("coverity_scan" not in line) and ("gh-pages" not in line):
            output.append(line.strip())
    return output


def git_get_changes(branch, commit, last_commit):
    fmt = '--format="%h: (%an) %s, %ar"'
    if last_commit is None:
        commits = execute('git log -n 10 %s %s' % (fmt, commit))
    else:
        commits = execute('git --no-pager log %s %s..%s' % (fmt, last_commit, commit))
    return str('Changes in %s since %s:\n' % (branch, last_commit)) + '\n'.join(commits)


def get_last_results(resultsFileName):
    if not os.path.isfile(resultsFileName):
        return None, None, None, None
    commit = None
    csize = []
    cspeed = []
    dspeed = []
    with open(resultsFileName,'r') as f:
        for line in f:
            words = line.split()
            if len(words) == 2:   # branch + commit
                commit = words[1];
                csize = []
                cspeed = []
                dspeed = []
            if (len(words) == 8):  # results
                csize.append(int(words[1]))
                cspeed.append(float(words[3]))
                dspeed.append(float(words[5]))
    return commit, csize, cspeed, dspeed


def benchmark_and_compare(branch, commit, args, resultsFileName, testFilePath, fileName, last_csize, last_cspeed, last_dspeed):
    sleepTime = 30
    while os.getloadavg()[0] > args.maxLoadAvg:
        log("WARNING: bench loadavg=%.2f is higher than %s, sleeping for %s seconds" % (os.getloadavg()[0], args.maxLoadAvg, sleepTime))
        time.sleep(sleepTime)
    start_load = str(os.getloadavg())
    result = execute('programs/zstd -qi5b1e%s %s' % (args.lastCLevel, testFilePath), print_output=True)
    end_load = str(os.getloadavg())
    linesExpected = args.lastCLevel + 2;
    if len(result) != linesExpected:
        raise RuntimeError("ERROR: number of result lines=%d is different that expected %d\n%s" % (len(result), linesExpected, '\n'.join(result)))
    with open(resultsFileName, "a") as myfile:
        myfile.write(branch + " " + commit + "\n")
        myfile.write('\n'.join(result) + '\n')
        myfile.close()
        if (last_cspeed == None):
            log("WARNING: No data for comparison for branch=%s file=%s " % (branch, fileName))
            return ""
        commit, csize, cspeed, dspeed = get_last_results(resultsFileName)
        text = ""
        for i in range(0, min(len(cspeed), len(last_cspeed))):
            print("%s:%s -%d cSpeed=%6.2f cLast=%6.2f cDiff=%1.4f dSpeed=%6.2f dLast=%6.2f dDiff=%1.4f ratioDiff=%1.4f %s" % (branch, commit, i+1, cspeed[i], last_cspeed[i], cspeed[i]/last_cspeed[i], dspeed[i], last_dspeed[i], dspeed[i]/last_dspeed[i], float(csize[i])/last_csize[i], fileName))
            if (cspeed[i]/last_cspeed[i] < args.lowerLimit):
                text += "WARNING: -%d cSpeed=%.2f cLast=%.2f cDiff=%.4f %s\n" % (i+1, cspeed[i], last_cspeed[i], cspeed[i]/last_cspeed[i], fileName)
            if (dspeed[i]/last_dspeed[i] < args.lowerLimit):
                text += "WARNING: -%d dSpeed=%.2f dLast=%.2f dDiff=%.4f %s\n" % (i+1, dspeed[i], last_dspeed[i], dspeed[i]/last_dspeed[i], fileName)
            if (float(csize[i])/last_csize[i] < args.ratioLimit):
                text += "WARNING: -%d cSize=%d last_cSize=%d diff=%.4f %s\n" % (i+1, csize[i], last_csize[i], float(csize[i])/last_csize[i], fileName)
        if text:
            text = args.message + ("\nmaxLoadAvg=%s  load average at start=%s end=%s\n" % (args.maxLoadAvg, start_load, end_load)) + text
        return text


def update_config_file(branch, commit):
    last_commit = None
    commitFileName = working_path + "/commit_" + branch.replace("/", "_") + ".txt"
    if os.path.isfile(commitFileName):
        last_commit = file(commitFileName, 'r').read()
    file(commitFileName, 'w').write(commit)
    return last_commit


def test_commit(branch, commit, last_commit, args, testFilePaths, have_mutt, have_mail):
    local_branch = string.split(branch, '/')[1]
    version = local_branch.rpartition('-')[2] + '_' + commit
    if not args.dry_run:
        execute('make clean zstd MOREFLAGS="-DZSTD_GIT_COMMIT=%s"' % version)
    logFileName = working_path + "/log_" + branch.replace("/", "_") + ".txt"
    text_to_send = []
    results_files = ""
    for filePath in testFilePaths:
        fileName = filePath.rpartition('/')[2]
        resultsFileName = working_path + "/results_" + branch.replace("/", "_") + "_" + fileName.replace(".", "_") + ".txt"
        last_commit, csize, cspeed, dspeed = get_last_results(resultsFileName)
        if not args.dry_run:
            text = benchmark_and_compare(branch, commit, args, resultsFileName, filePath, fileName, csize, cspeed, dspeed)
            if text:
                log("WARNING: redoing tests for branch %s: commit %s" % (branch, commit))
                text = benchmark_and_compare(branch, commit, args, resultsFileName, filePath, fileName, csize, cspeed, dspeed)
                if text:
                    text_to_send.append(text)
                    results_files += resultsFileName + " "
    if text_to_send:
        send_email_with_attachments(branch, commit, last_commit, args, text_to_send, results_files, logFileName, have_mutt, have_mail)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('testFileNames', help='file names list for speed benchmark')
    parser.add_argument('emails', help='list of e-mail addresses to send warnings')
    parser.add_argument('--message', help='attach an additional message to e-mail', default="")
    parser.add_argument('--repoURL', help='changes default repository URL', default=default_repo_url)
    parser.add_argument('--lowerLimit', type=float, help='send email if speed is lower than given limit', default=0.98)
    parser.add_argument('--ratioLimit', type=float, help='send email if ratio is lower than given limit', default=0.999)
    parser.add_argument('--maxLoadAvg', type=float, help='maximum load average to start testing', default=0.75)
    parser.add_argument('--lastCLevel', type=int, help='last compression level for testing', default=5)
    parser.add_argument('--sleepTime', type=int, help='frequency of repository checking in seconds', default=300)
    parser.add_argument('--dry-run', dest='dry_run', action='store_true', help='not build', default=False)
    parser.add_argument('--verbose', action='store_true', help='more verbose logs', default=False)
    args = parser.parse_args()
    verbose = args.verbose

    # check if test files are accessible
    testFileNames = args.testFileNames.split()
    testFilePaths = []
    for fileName in testFileNames:
        fileName = os.path.expanduser(fileName)
        if os.path.isfile(fileName):
            testFilePaths.append(os.path.abspath(fileName))
        else:
            log("ERROR: File not found: " + fileName)
            exit(1)

    # check availability of e-mail senders
    have_mutt = does_command_exist("mutt -h");
    have_mail = does_command_exist("mail -V");
    if not have_mutt and not have_mail:
        log("ERROR: e-mail senders 'mail' or 'mutt' not found")
        exit(1)

    if verbose:
        print("PARAMETERS:\nrepoURL=%s" % args.repoURL)
        print("working_path=%s" % working_path)
        print("clone_path=%s" % clone_path)
        print("testFilePath(%s)=%s" % (len(testFilePaths), testFilePaths))
        print("message=%s" % args.message)
        print("emails=%s" % args.emails)
        print("maxLoadAvg=%s" % args.maxLoadAvg)
        print("lowerLimit=%s" % args.lowerLimit)
        print("ratioLimit=%s" % args.ratioLimit)
        print("lastCLevel=%s" % args.lastCLevel)
        print("sleepTime=%s" % args.sleepTime)
        print("dry_run=%s" % args.dry_run)
        print("verbose=%s" % args.verbose)
        print("have_mutt=%s have_mail=%s" % (have_mutt, have_mail))

    # clone ZSTD repo if needed
    if not os.path.isdir(working_path):
        os.mkdir(working_path)
    if not os.path.isdir(clone_path):
        execute.cwd = working_path
        execute('git clone ' + args.repoURL)
    if not os.path.isdir(clone_path):
        log("ERROR: ZSTD clone not found: " + clone_path)
        exit(1)
    execute.cwd = clone_path

    # check if speedTest.pid already exists
    pidfile = "./speedTest.pid"
    if os.path.isfile(pidfile):
        log("ERROR: %s already exists, exiting" % pidfile)
        exit(1)

    send_email(args.emails, email_header + ':%s test-zstd-speed.py has been started' % pid, args.message, have_mutt, have_mail)
    file(pidfile, 'w').write(pid)

    while True:
        try:
            loadavg = os.getloadavg()[0]
            if (loadavg <= args.maxLoadAvg):
                branches = git_get_branches()
                for branch in branches:
                    commit = execute('git show -s --format=%h ' + branch, verbose)[0]
                    last_commit = update_config_file(branch, commit)
                    if commit == last_commit:
                        log("skipping branch %s: head %s already processed" % (branch, commit))
                    else:
                        log("build branch %s: head %s is different from prev %s" % (branch, commit, last_commit))
                        execute('git checkout -- . && git checkout ' + branch)
                        print(git_get_changes(branch, commit, last_commit))
                        test_commit(branch, commit, last_commit, args, testFilePaths, have_mutt, have_mail)
            else:
                log("WARNING: main loadavg=%.2f is higher than %s" % (loadavg, args.maxLoadAvg))
            if verbose:
                log("sleep for %s seconds" % args.sleepTime)
            time.sleep(args.sleepTime)
        except Exception as e:
            stack = traceback.format_exc()
            email_topic = '%s:%s ERROR in %s:%s' % (email_header, pid, branch, commit)
            send_email(args.emails, email_topic, stack, have_mutt, have_mail)
            print(stack)
        except KeyboardInterrupt:
            os.unlink(pidfile)
            send_email(args.emails, email_header + ':%s test-zstd-speed.py has been stopped' % pid, args.message, have_mutt, have_mail)
            exit(0)
