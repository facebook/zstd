#!/usr/bin/env python3

# Based on LZ4 version test script, by Takayuki Matsuoka

import glob
import subprocess
import filecmp
import os
import shutil
import sys
import hashlib

repo_url = 'https://github.com/Cyan4973/zstd.git'
tmp_dir_name = 'versionsTest/zstdtest'
make_cmd = 'make'
git_cmd = 'git'
test_dat_src = 'README.md'
test_dat = 'test_dat'
head = 'vdevel'

def proc(cmd_args, pipe=True, dummy=False):
    if dummy:
        return
    if pipe:
        subproc = subprocess.Popen(cmd_args,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
    else:
        subproc = subprocess.Popen(cmd_args)
    return subproc.communicate()

def make(args, pipe=True):
    return proc([make_cmd] + args, pipe)

def git(args, pipe=True):
    return proc([git_cmd] + args, pipe)

def get_git_tags():
    stdout, stderr = git(['tag', '-l', 'v[0-9].[0-9].[0-9]'])
    tags = stdout.decode('utf-8').split()
    return tags

def compress_sample(tag, sample):
    try:
        from subprocess import DEVNULL # py3k
    except ImportError:
        DEVNULL = open(os.devnull, 'wb')
    if subprocess.call(['./zstd.' + tag, '-f'  ,  sample], stderr=DEVNULL)==0:
        os.rename(sample + '.zst', sample + '_01_64_' + tag + '.zst')
    if subprocess.call(['./zstd.' + tag, '-5f' ,  sample], stderr=DEVNULL)==0:
        os.rename(sample + '.zst', sample + '_05_64_' + tag + '.zst')
    if subprocess.call(['./zstd.' + tag, '-9f' ,  sample], stderr=DEVNULL)==0 :
        os.rename(sample + '.zst', sample + '_09_64_' + tag + '.zst')
    if subprocess.call(['./zstd.' + tag, '-15f',  sample], stderr=DEVNULL)==0 :
        os.rename(sample + '.zst', sample + '_15_64_' + tag + '.zst')
    if subprocess.call(['./zstd.' + tag, '-18f',  sample], stderr=DEVNULL)==0:
        os.rename(sample + '.zst', sample + '_18_64_' + tag + '.zst')
    # zstdFiles = glob.glob("*.zst*")
    # print(zstdFiles)

# http://stackoverflow.com/a/19711609/2132223
def sha1_of_file(filepath):
    with open(filepath, 'rb') as f:
        return hashlib.sha1(f.read()).hexdigest()

def remove_duplicates():
    list_of_zst = sorted(glob.glob('*.zst'))
    for i, ref_zst in enumerate(list_of_zst):
        if not os.path.isfile(ref_zst):
            continue
        for j in range(i+1, len(list_of_zst)):
            compared_zst = list_of_zst[j]
            if not os.path.isfile(compared_zst):
                continue
            if filecmp.cmp(ref_zst, compared_zst):
                os.remove(compared_zst)
                print('duplicated : {} == {}'.format(ref_zst, compared_zst))

def decompress_zst(tag):
    dec_error = 0
    list_zst = sorted(glob.glob('*.zst'))
    try:
        from subprocess import DEVNULL # py3k
    except ImportError:
        DEVNULL = open(os.devnull, 'wb')
    for file_zst in list_zst:
        print(file_zst, end=" ")
        print(tag, end=" ")
        file_dec = file_zst + '_d64_' + tag + '.dec'
        if subprocess.call(['./zstd.'   + tag, '-df', file_zst, '-o', file_dec], stderr=DEVNULL)==0:
            if not filecmp.cmp(file_dec, test_dat):
                print('ERR !! ')
                dec_error = 1
            else:
                print('OK     ')
        else:
            print('command does not work')
    return dec_error


if __name__ == '__main__':
    error_code = 0
    base_dir = os.getcwd() + '/..'           # /path/to/zstd
    tmp_dir = base_dir + '/' + tmp_dir_name  # /path/to/zstd/versionsTest/zstdtest
    clone_dir = tmp_dir + '/' + 'zstd'       # /path/to/zstd/versionsTest/zstdtest/zstd
    programs_dir = base_dir + '/programs'    # /path/to/zstd/programs
    os.makedirs(tmp_dir, exist_ok=True)

    # since Travis clones limited depth, we should clone full repository
    if not os.path.isdir(clone_dir):
        git(['clone', repo_url, clone_dir])

    shutil.copy2(base_dir + '/' + test_dat_src, tmp_dir + '/' + test_dat)

    # Retrieve all release tags
    print('Retrieve all release tags :')
    os.chdir(clone_dir)
    tags = get_git_tags() + [head]
    print(tags);

    # Build all release zstd
    for tag in tags:
        os.chdir(base_dir)
        dst_zstd   = '{}/zstd.{}'  .format(tmp_dir, tag) # /path/to/zstd/test/zstdtest/zstd.<TAG>
        if not os.path.isfile(dst_zstd) or tag == head:
            if tag != head:
                r_dir = '{}/{}'.format(tmp_dir, tag)  # /path/to/zstd/test/zstdtest/<TAG>
                os.makedirs(r_dir, exist_ok=True)
                os.chdir(clone_dir)
                git(['--work-tree=' + r_dir, 'checkout', tag, '--', '.'], False)
                os.chdir(r_dir + '/programs')  # /path/to/zstd/zstdtest/<TAG>/programs
                make(['clean', 'zstd'], False)
            else:
                os.chdir(programs_dir)
                make(['zstd'], False)
            shutil.copy2('zstd',   dst_zstd)

    # remove any remaining *.zst and *.dec from previous test
    os.chdir(tmp_dir)
    for compressed in glob.glob("*.zst"):
        os.remove(compressed)
    for dec in glob.glob("*.dec"):   
        os.remove(dec)

    print('Compress test.dat by all released zstd')

    error_code = 0;
    for tag in tags:
        print(tag)
        compress_sample(tag, test_dat)
        remove_duplicates()
        error_code += decompress_zst(tag)

    print('')
    print('Enumerate different compressed files')
    zstds = sorted(glob.glob('*.zst'))
    for zstd in zstds:
        print(zstd + ' : ' + repr(os.path.getsize(zstd)) + ', ' + sha1_of_file(zstd))

    if error_code != 0:
        print('==== ERROR !!! =====')

    sys.exit(error_code)
