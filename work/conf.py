#!/usr/bin/env python

from sys import exit
from os import listdir
from os.path import abspath, join
from platform import system
from multiprocessing import cpu_count

# platforms
HOST = system()
assert HOST in ("Linux", "Darwin")

# paths
PATH_ROOT = abspath(join(__file__, "..", ".."))

PATH_APPS = join(PATH_ROOT, "apps")
PATH_DEPS = join(PATH_ROOT, "deps")

PATH_LLVM = join(PATH_ROOT, "llvm")
PATH_PASS = join(PATH_ROOT, "pass")
PATH_TEST = join(PATH_ROOT, "unit")

PATH_CODE = join(PATH_ROOT, "code")
PATH_LOGS = join(PATH_CODE, "logs")
PATH_OUTS = join(PATH_CODE, "outs")
PATH_SRCS = join(PATH_CODE, "srcs")
PATH_OBJS = join(PATH_CODE, "objs")
PATH_BINS = join(PATH_CODE, "bins")
PATH_EXTS = join(PATH_CODE, "exts")
PATH_BCFS = join(PATH_CODE, "bcfs")
PATH_MODS = join(PATH_CODE, "mods")
PATH_TRAS = join(PATH_CODE, "tras")
PATH_SYMS = join(PATH_CODE, "syms")

PATH_WORK = join(PATH_ROOT, "work")

# apps
LIST_APPS = listdir(PATH_APPS)

# deps
DEPS_Z3 = join(PATH_DEPS, "z3")
DEPS_UTIL = join(PATH_DEPS, "util")

# pass
PASS_BDIR = join(PATH_PASS, "Build")
PASS_KSYM = join(PASS_BDIR, "KSym", "KSym.so")

# llvm
LLVM_PREP = join(PATH_LLVM, "bins")

LLVM_BINS = join(LLVM_PREP, "bin")
LLVM_BIN_CPP = join(LLVM_BINS, "clang-cpp")
LLVM_BIN_CLA = join(LLVM_BINS, "clang")
LLVM_BIN_CXX = join(LLVM_BINS, "clang++")
LLVM_BIN_LLD = join(LLVM_BINS, "ld.lld")
LLVM_BIN_BLD = join(LLVM_BINS, "llvm-link")
LLVM_BIN_OPT = join(LLVM_BINS, "opt")

LLVM_SYMS = join(PATH_LLVM, "syms")

# opts
OPTS_NCPU = cpu_count()
OPTS_TIME = 43200
