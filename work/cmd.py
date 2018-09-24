#!/usr/bin/env python

from collections import OrderedDict
from argparse import ArgumentParser

from conf import *
from util import *

# cmd parsing
def CMDParser():
    parser = ArgumentParser(add_help=False)

    # output
    parser.add_argument("-o", type=str, default=None)

    # cstd
    parser.add_argument("-std", type=str, default=None)

    # optimization
    parser.add_argument("-O", type=str, default=None)

    # operation mode
    parser.add_argument("-c", action="store_true")
    parser.add_argument("-E", action="store_true")
    parser.add_argument("-S", action="store_true")

    # various flags
    parser.add_argument("-arch", type=str, default=None)

    parser.add_argument("-g", action="store_true")
    parser.add_argument("-ggdb3", action="store_true")
    parser.add_argument("-gdwarf-2", action="store_true")

    parser.add_argument("-C", action="store_true")
    parser.add_argument("-P", action="store_true")

    parser.add_argument("-nostdinc", action="store_true")
    parser.add_argument("-nostdlib", action="store_true")
    parser.add_argument("-shared", action="store_true")

    parser.add_argument("-pipe", action="store_true")
    parser.add_argument("-pg", action="store_true")

    parser.add_argument("-MD", action="store_true")
    parser.add_argument("-MG", action="store_true")
    parser.add_argument("-MM", action="store_true")
    parser.add_argument("-MP", action="store_true")
    parser.add_argument("-MF", type=str, default=None)

    # defines
    parser.add_argument("-D", type=str, action="append", default=[])
    # undefs
    parser.add_argument("-U", type=str, action="append", default=[])

    # includes
    parser.add_argument("-I", type=str, action="append", default=[])
    # include headers
    parser.add_argument("-include", type=str, action="append", default=[])
    # include system
    parser.add_argument("-isystem", type=str, action="append", default=[])
    # include dirs
    parser.add_argument("-idirafter", type=str, action="append", default=[])

    # libs
    parser.add_argument("-l", type=str, action="append", default=[])
    # linked elfs
    parser.add_argument("-lelf", type=str, action="append", default=[])

    # warnings
    parser.add_argument("-W", type=str, action="append", default=[])
    # flags
    parser.add_argument("-f", type=str, action="append", default=[])
    # machine
    parser.add_argument("-m", type=str, action="append", default=[])

    # extra params
    parser.add_argument("--param", type=str, action="append", default=[])

    # src file
    parser.add_argument("inputs", type=str, nargs="+") 

    return parser

class CMDChange(object):
    def __init__(self, val, mod):
        self.val = val
        self.mod = mod

    def change(self, flag):
        # mod is None   ==> keep if in flags, do nothing otherwise
        # mod is True   ==> add if not in flags
        # mos is False  ==> drop if in flags
        if self.mod is None or self.mod:
            if self.val is None:
                return flag

            toks = flag.split("=", 1)
            return "%s-%s" % (toks[0], self.val)
        else:
            return None

class CMDChangeList(object):
    def __init__(self, white, filters):
        self.white = white
        self.filters = filters
        
    def __filter(self, flags):
        result = set() 

        for i in flags:
            if i not in self.filters:
                if not self.white:
                    result.add(i)
                continue

            res = self.filters[i].change(i)
            if res is None:
                continue

            result.add(res)

        return result

    def scan(self, flags):
        result = self.__filter(flags) 

        for i in self.filters:
            inst = self.filters[i]
            if inst.mod is not None and inst.mod:
                if inst.val is None:
                    result.add(i)
                else:
                    result.add("%s=%s" % (i, inst.val))

        return sorted(result)

class CMDBuilder(object):
    def __init__(self, defs=None, udef=None,
            incs=None, ihdr=None, isys=None, idir=None,
            libs=None, elfs=None,
            flag=None, fset=None, pars=None,
            wset=None, mset=None,):

        self.defs = defs
        self.udef = udef

        self.incs = incs
        self.ihdr = ihdr
        self.isys = isys

        self.libs = libs
        self.elfs = elfs

        self.wset = wset
        self.mset = mset

        self.flag = flag
        self.fset = fset
        self.pars = pars

    def build(self, opts):
        for k in self.__dict__:
            assert(k in opts.__dict__)

            filters = self.__dict__[k]
            if filters is None:
                # default to whitelist
                opts.__dict[k] = []
                continue

            flags = opts.__dict__[k]
            opts.__dict__[k] = filters.scan(flags)

class CMDResult(object):
    def __init__(self, parser, argv):
        # parse args
        args, rems = parser.parse_known_args(argv)

        if len(rems) != 0:
            for r in rems:
                print r

            LOG_ERR("Argument parse failed")
            sys.exit()

        # collect results
        self.srcs = args.inputs
        self.outs = args.o

        if args.c:
            self.mode = "c"
        elif args.S:
            self.mode = "S"
        elif args.E:
            self.mode = "E"
        else:
            self.mode = None

        self.arch = args.arch

        self.cstd = args.std 
        self.optl = args.O 

        self.flag = []
        if args.nostdinc:
            self.flag.append("nostdinc")

        if args.nostdlib:
            self.flag.append("nostdlib")

        if args.C:
            self.flag.append("C")

        if args.P:
            self.flag.append("P")

        self.defs = args.D 
        self.udef = args.U 

        self.incs = args.I 
        self.ihdr = args.include 
        self.isys = args.isystem 
        self.idir = args.idirafter

        self.libs = args.l 
        self.elfs = args.lelf

        self.wset = args.W
        self.mset = args.m
        self.fset = args.f 
        self.pars = args.param

    def organize(self):
        return " ".join([
            ("-%s" % self.mode if self.mode is not None else ""),
            ("-std=%s" % self.cstd if self.cstd is not None else ""),
            ("-O%s" % self.optl if self.optl is not None else ""),
            ("-arch%s" % self.arch if self.arch is not None else ""),
            " ".join(["-%s" % i for i in self.flag]),
            " ".join(["-f%s" % i for i in self.fset]),
            " ".join(["--param %s" % i for i in self.pars]),
            " ".join(["-isystem %s" % i for i in self.isys]),
            " ".join(["-I%s" % i for i in self.incs]),
            " ".join(["-include %s" % i for i in self.ihdr]),
            " ".join(["-idirafter %s" % i for i in self.idir]),
            " ".join(["-D%s" % i for i in self.defs]),
            " ".join(["-U%s" % i for i in self.udef]),
            " ".join(["-W%s" % i for i in self.wset]),
            " ".join(["-m%s" % i for i in self.mset])
            ])

class CMDCollect(object):
    def __init__(self):
        self.flag = set() 
        self.pars = set()
        self.fset = set()
        self.wset = set()
        self.mset = set()

    def update(self, opts):
        self.flag.update(opts.flag)
        self.pars.update(opts.pars)
        self.fset.update(opts.fset)
        self.wset.update(opts.wset)
        self.mset.update(opts.mset)

    def show(self):
        print "====== %s ======" % "flag"
        for i in self.flag:
            print i

        print "====== %s ======" % "pars"
        for i in self.pars:
            print i

        print "====== %s ======" % "fset"
        for i in self.fset:
            print i

        print "====== %s ======" % "wset"
        for i in self.wset:
            if i.startswith("p,-MD"):
                continue

            if i.startswith("p,-MT"):
                continue

            print i

        print "====== %s ======" % "mset"
        for i in self.mset:
            print i


def CMDLinker():
    grouper = ArgumentParser(add_help=False)

    # output
    grouper.add_argument("-o", type=str, default=None)

    # flags
    grouper.add_argument("-r", action="store_true")
    
    grouper.add_argument("-nostdlib", action="store_true")
    grouper.add_argument("--no-undefined", action="store_true")

    grouper.add_argument("--emit-relocs", action="store_true")
    grouper.add_argument("--build-id", action="store_true")

    # configs
    grouper.add_argument("-e", type=str, default=None)
    grouper.add_argument("-z", type=str, default=None)
    grouper.add_argument("-m", type=str, default=None)
    grouper.add_argument("-T", type=str, default=None)

    # src files
    grouper.add_argument("inputs", type=str, nargs="+")

    return grouper

def CMDArchiver():
    grouper = ArgumentParser(add_help=False)

    # output
    grouper.add_argument("o", type=str, default=None)

    # src files
    grouper.add_argument("inputs", type=str, nargs="*")

    return grouper

class CMDModule(object):
    def __init__(self, grouper, argv):
        # parse args
        args, rems = grouper.parse_known_args(argv)

        if len(rems) != 0:
            for r in rems:
                print r

            LOG_ERR("Argument group failed")
            sys.exit()

        # collect results
        self.srcs = args.inputs
        self.outs = args.o
        self.args = args 

class CMDLink(object):
    def __init__(self, goal):
        self.goal = goal 
        self.subs = set() 
        self.pars = set() 

    def link(self, sub):
        self.subs.add(sub.goal)
        sub.pars.add(self.goal)
