#!/usr/bin/env python

from os.path import splitext

from conf import *
from util import *
from cmd import *
from app import *

# objects 
BUILDER = CMDBuilder(
        # defs and udef follow a black list approach
        defs = CMDChangeList(False, {}),
        udef = CMDChangeList(False, {}),
        # incs follow a black list approach
        incs = CMDChangeList(False, {}),
        ihdr = CMDChangeList(False, {}),
        isys = CMDChangeList(False, {}),
        # libs follow a white list approach
        libs = CMDChangeList(True, {}),
        elfs = CMDChangeList(True, {}),
        # flags follows a white list approach
        wset = CMDChangeList(True, {}),
        mset = CMDChangeList(True, {}),
        flag = CMDChangeList(True, {
            "nostdinc": CMDChange(None, None),
            "nostdlib": CMDChange(None, None),
            }),
        fset = CMDChangeList(True, {}),
        pars = CMDChangeList(True, {}),
        )
        
# functions
class APP_XNU_STABLE(App):
    def __init__(self, tag = "10.12"):
        super(APP_XNU_STABLE, self).__init__("xnu-stable", tag, BUILDER)

    def convert(self):
        return "%s" % self.tag

    def config_impl(self, psrc, pobj, pbin, pext):
        return True

    def build_impl(self, psrc, pobj, pbin, pext, plog):
        dirs = {
                "SRCROOT"           : psrc,
                "OBJROOT"           : pobj,
                "DSTROOT"           : pbin,
                "SYMROOT"           : pext,
                }

        cfgs = {
                "SDKROOT"           : "macosx",
                "ARCH_CONFIGS"      : "X86_64",
                "KERNEL_CONFIGS"    : "DEVELOPMENT",
                }

        dstr = " ".join("%s=%s" % (k, v) for (k, v) in dirs.items())
        cstr = " ".join("%s=%s" % (k, v) for (k, v) in cfgs.items())

        with cd(psrc):
            cmd = "make %s %s VERBOSE=YES MAKEJOBS=-j1" % (dstr, cstr)
            with open(plog, "w") as f:
                if shell(cmd, out = f) == 0:
                    LOG_INF("Build done")
                else:
                    LOG_ERR("Build failed")
                    return False

            cmd = "make %s %s installhdrs" % (dstr, cstr)
            if shell(cmd) == 0:
                LOG_INF("Install done")
            else:
                LOG_ERR("Install failed")
                return False

        return True

    def parse_line(self, line):
        line = line.strip()
        toks = line.split(" ", 1)
        if not toks[0].endswith("clang"):
            return (False, None)

        argv = shellsplit(toks[1])
        try:
            i = argv.index("&&")
            argv = argv[:i]
        except:
            pass

        return (True, argv)

    def parse_opts(self, opts):
        if opts.mode != "c":
            return (False, None)

        if not "KERNEL" in opts.defs:
            return (False, None)

        srcs = []
        for src in opts.srcs:
            base, ext = splitext(src)
            if ext not in [".c", ".o", ".s", ".S"]:
                return None
            
            if ext in [".c"]:
                srcs.append(src)

        return (True, srcs)

    def check_filter(self):
        return set([])
