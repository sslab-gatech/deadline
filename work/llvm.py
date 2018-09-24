#!/usr/bin/env python

from os import symlink
from os.path import exists
from shutil import rmtree
from argparse import ArgumentParser

from conf import *
from util import *

# functions
def build(clean = False, debug = None):
    psrc = PATH_PASS
    if not exists(psrc):
        LOG_ERR("Src path %s does not exist" % psrc)
        return

    pobj = PASS_BDIR
    if clean:
        mkdirs(pobj, force = True)
    else:
        if not exists(pobj):
            LOG_ERR("Obj path %s does not exist" % pobj)
            return

    with cd(pobj):
        if clean:
            if debug is not None:
                btype = "Profile"
                ditem = debug.upper()
            else:
                btype = "Release"
                ditem = "NONE"
            
            cmd = ("cmake " + \
                    "-G 'Unix Makefiles' " + \
                    "-DCMAKE_BUILD_TYPE=%s " % btype + \
                    "-DCMAKE_PREFIX_PATH=%s " % LLVM_PREP + \
                    "-DDEPS_Z3_DIR=%s " % DEPS_Z3 + \
                    "-DDEPS_UTIL_DIR=%s " % DEPS_UTIL + \
                    "-DKSYM_DEBUG_ITEM=%s " % ditem + \
                    psrc)

            if shell(cmd) == 0:
                LOG_INF("Config done")
            else:
                LOG_ERR("Config failed")
                return

        cmd = "make -j%d" % OPTS_NCPU

        if shell(cmd) == 0:
            LOG_INF("Build done")
        else:
            LOG_ERR("Build failed")
            return

def work(plain = False):
    pbin = LLVM_BINS
    if not exists(pbin):
        LOG_ERR("Bin path %sd does not exist" % pbin)
        return

    passthrough(resolve(LLVM_SYMS, "cpp"),          LLVM_BIN_CPP, [])
    passthrough(resolve(LLVM_SYMS, "cc"),           LLVM_BIN_CLA, [])
    passthrough(resolve(LLVM_SYMS, "c++"),          LLVM_BIN_CXX, [])
    passthrough(resolve(LLVM_SYMS, "clang-cpp"),    LLVM_BIN_CPP, [])
    passthrough(resolve(LLVM_SYMS, "clang"),        LLVM_BIN_CLA, [])
    passthrough(resolve(LLVM_SYMS, "clang++"),      LLVM_BIN_CXX, [])

    if plain:
        args = ["-S", "-emit-llvm"]
    else:
        args = ["-load", PASS_KSYM, "-KSym"]
    
    passthrough(resolve(LLVM_SYMS, "opt"),          LLVM_BIN_OPT, args)
    
    with envpath("PATH", LLVM_SYMS):
        with envpath("LD_LIBRARY_PATH", resolve(DEPS_Z3, "bins", "lib")):
            with cd(resolve(PATH_TEST, HOST)):
                shell("bash")

# main
if __name__ == "__main__":
    # init
    parser = ArgumentParser()
    subs = parser.add_subparsers(dest = "cmd")

    sub_build = subs.add_parser("build")
    sub_build.add_argument("-c", "--clean", action="store_true")
    sub_build.add_argument("-d", "--debug", type=str, default=None)
    
    sub_work = subs.add_parser("work")
    sub_work.add_argument("-p", "--plain", action="store_true")

    # parse
    args = parser.parse_args()

    # exec
    if args.cmd == "build":
        build(args.clean, args.debug)
    elif args.cmd == "work":
        work(args.plain)
    else:
        parser.print_usage()
