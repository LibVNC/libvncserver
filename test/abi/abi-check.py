#!/usr/bin/python3

import os
import sys
import argparse
import tempfile
import subprocess
from pathlib import Path


ORIGINAL_DIR = Path(os.getcwd())
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_DIR = SCRIPT_DIR.parent.parent  # Assuming this script is in 'test/abi'
WORK_DIR = tempfile.TemporaryDirectory(prefix='libvnc-abi-check')

DEFAULT_REV_FILE = Path(SCRIPT_DIR, 'published-abi-revision')  # May not exist
OUTPUT_DIR = Path(SCRIPT_DIR, 'abi-check-result')  # ABI dumps & compliance reports are generated here

LIB_CLIENT = 'vncclient'
LIB_SERVER = 'vncserver'
LABEL_OLD = 'old'
LABEL_NEW = 'new'


def run_cmd(cmd: str, check=True):
    return subprocess.run(cmd, shell=True, check=check)


def read_cmd_output(cmd: str):
    return subprocess.run(cmd, shell=True, check=True, stdout=subprocess.PIPE).stdout


def create_dump_file_path(library: str, label: str):
    return str(Path(OUTPUT_DIR, f"{library}-{label}.dump"))

# Builds given library (vncclient/vncserver), and stores it's ABI dump in output directory.
# Assumes we are in build directory.


def dump_library_abi(library: str, label: str):
    dump_file = create_dump_file_path(library, label)
    run_cmd(f"cmake --build . --target {library}")
    run_cmd(f"abi-dumper -lver {label} lib{library}.so -o {dump_file} -public-headers ../rfb")

# Dumps ABIs for given revision


def dump_abi(rev: str, label: str):
    tree_dir = Path(WORK_DIR.name, label)
    build_dir = Path(tree_dir, "build")
    run_cmd(f"git -C {str(REPO_DIR)} worktree add {tree_dir} {rev}")
    os.mkdir(build_dir)
    os.chdir(build_dir)
    run_cmd("env CFLAGS='-gdwarf-4 -Og' cmake ..")
    dump_library_abi(LIB_CLIENT, label)
    dump_library_abi(LIB_SERVER, label)


def compare_library_abi(library: str):
    old_abi = create_dump_file_path(library, LABEL_OLD)
    new_abi = create_dump_file_path(library, LABEL_NEW)
    report = str(Path(OUTPUT_DIR, f"{library}-report.html"))
    r = run_cmd(f"abi-compliance-checker -l {library}  -old {old_abi} -new {new_abi} -report-path {report}", False)
    if r.returncode != 0:
        print(f"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
        print(f"~ ERROR: ABI break detected in {library}")
        print(f"~ Please check the report at file://{report}")
        print(f"~ On GitHub Actions, this report is also available in workflow artifacts")
        print(f"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
        sys.exit(1)


def compare(old_rev: str, new_rev: str):
    dump_abi(old_rev, LABEL_OLD)
    dump_abi(new_rev, LABEL_NEW)
    compare_library_abi(LIB_CLIENT)
    compare_library_abi(LIB_SERVER)


def update():
    head = read_cmd_output(f"git -C {str(REPO_DIR)} rev-list HEAD --max-count=1")
    DEFAULT_REV_FILE.write_bytes(head)


def parse_args():
    rf_name = DEFAULT_REV_FILE.name
    parser = argparse.ArgumentParser(description="Check ABI compatibility between two Git revisions")
    parser.add_argument('-o', dest='old', help=f"Old revision; defaults to reading from '{rf_name}'")
    parser.add_argument('-n', dest='new', help="New revision; defaults to 'HEAD'")
    parser.add_argument('-u', dest='update', help=f"Update '{rf_name}' file with current 'HEAD'", action='store_true')
    args = parser.parse_args()

    if args.update:
        return args

    if args.old == None:
        if DEFAULT_REV_FILE.exists():
            with open(DEFAULT_REV_FILE) as f:
                args.old = f.readline()
        else:
            print(f"ERROR: Cannot detect old revision automatically, '{str(DEFAULT_REV_FILE)}' is missing")
            sys.exit(1)

    if args.new == None:
        args.new = read_cmd_output(f"git -C {str(REPO_DIR)} rev-list HEAD --max-count=1").decode().strip()
        if args.new == None:
            print("ERROR: Cannot detect new revision automatically from git repo")
            sys.exit(1)

    return args


def main():
    try:
        args = parse_args()
        if args.update:
            update()
        else:
            compare(args.old, args.new)
    finally:
        os.chdir(ORIGINAL_DIR)  # Restore
        WORK_DIR.cleanup()
        run_cmd(f"git -C {str(REPO_DIR)} worktree prune", check=False)


main()
