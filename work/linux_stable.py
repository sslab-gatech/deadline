#!/usr/bin/env python

from os.path import basename, dirname, splitext
from fnmatch import fnmatch

from conf import *
from util import *
from cmd import *
from app import *

# objects 
BUILDER = CMDBuilder(
        # defs and udef follow a black list approach
        defs = CMDChangeList(False, {
            "CC_HAVE_ASM_GOTO": CMDChange(None, False),
            }),
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

FILTER = [
        "crypto/*",
        "init/*",
        "lib/*",
        "drivers/gpu/drm/vmwgfx/vmwgfx.o",
        ]
        
# functions
class APP_LINUX_STABLE(App):
    def __init__(self, tag = "4.13.2"):
        super(APP_LINUX_STABLE, self).__init__("linux-stable", tag, 
                BUILDER, None)

        toks = self.tag.split(".")
        if int(toks[0]) < 4:
            self.grouper = CMDLinker()

        elif int(toks[1]) <= 12:
            self.grouper = CMDLinker()

        else:
            self.grouper = CMDArchiver()

    def convert(self):
        return "v%s" % self.tag

    def config_impl(self, psrc, pobj, pbin, pext):
        with cd(psrc):
            cmd = "make O=%s allyesconfig" % pobj
            if shell(cmd) == 0:
                LOG_INF("Config done")
            else:
                LOG_ERR("Config failed")
                return False

        return True

    def build_impl(self, psrc, pobj, pbin, pext, plog):
        with cd(pobj):
            cmd = "make V=1 vmlinux modules"
            with open(plog, "w") as f:
                if shell(cmd, out = f) == 0:
                    LOG_INF("Build done")
                else:
                    LOG_ERR("Build failed")
                    return False

            cmd = "make INSTALL_HDR_PATH=%s headers_install" % pbin
            if shell(cmd) == 0:
                LOG_INF("Install done")
            else:
                LOG_ERR("Install failed")
                return False

        return True

    def parse_line(self, line):
        line = line.strip()
        toks = line.split(" ", 1)
        if toks[0] != "gcc":
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

        if not "__KERNEL__" in opts.defs:
            return (False, None)

        srcs = []
        for src in opts.srcs:
            base, ext = splitext(src)
            if ext not in [".c", ".o", ".S"]:
                return None
            
            if ext in [".c"]:
                srcs.append(src)

        return (True, srcs)

    def group_line_until_4_12(self, line):
        line = line.strip()
        toks = line.split(" ", 1)
        if toks[0] != "ld":
            return (False, None)

        argv = shellsplit(toks[1])
        try:
            i = argv.index(";")
            argv = argv[:i]
        except:
            pass

        return (True, argv)

    def group_line_after_4_13(self, line):
        line = line.strip()
        if not "ar rcSTPD " in line:
            return (False, None)

        if not line.startswith("rm -f " ):
            return (False, None)

        argv = shellsplit(line.split(";")[1].strip())

        return (True, argv[2:])

    def group_line(self, line):
        toks = self.tag.split(".")
        if int(toks[0]) < 4:
            return self.group_line_until_4_12(line)

        elif int(toks[1]) <= 12:
            return self.group_line_until_4_12(line)

        else:
            return self.group_line_after_4_13(line)

    def group_opts_until_4_12(self, opts):
        # filtering
        if opts.args.m != "elf_x86_64":
            return (False, None)

        if opts.args.e is not None:
            return (False, None)

        if opts.args.z is not None:
            return (False, None)

        if opts.args.emit_relocs is not None:
            return (False, None)

        # ignore kernel modules
        if splitext(opts.outs)[1] == ".ko":
            return (False, None)

        # do not link drivers and staging drivers
        if opts.outs == "drivers/built-in.o":
            return (False, None)

        if opts.outs == "drivers/staging/built-in.o":
            return (False, None)

        # collect srcs
        srcs = []
        for src in opts.srcs:
            base, ext = splitext(src)
            if ext not in [".o"]:
                return None

            srcs.append(src)

        return (True, srcs)

    def group_opts_after_4_13(self, opts):
        # filtering
        if len(opts.srcs) == 0:
            return (False, None)

        # ignore kernel modules
        if splitext(opts.outs)[1] == ".ko":
            return (False, None)

        # do not link drivers and staging drivers
        if opts.outs == "drivers/built-in.o":
            return (False, None)

        if opts.outs == "drivers/staging/built-in.o":
            return (False, None)

        # collect srcs
        srcs = []
        for src in opts.srcs:
            base, ext = splitext(src)
            if ext not in [".o"]:
                return None

            srcs.append(src)

        return (True, srcs)

    def group_opts(self, opts):
        toks = self.tag.split(".")
        if int(toks[0]) < 4:
            return self.group_opts_until_4_12(opts)

        elif int(toks[1]) <= 12:
            return self.group_opts_until_4_12(opts)

        else:
            return self.group_opts_after_4_13(opts)

    def group_mark(self, tops):
        marks = []
        for goal in tops:
            if basename(goal) != "built-in.o":
                continue

            toks = dirname(goal).split("/")
            if len(toks) > 3:
                continue

            if len(toks) == 3 and \
                    (toks[0] != "drivers" or toks[1] != "staging"):
                continue

            if len(toks) == 2 and toks[0] != "drivers":
                continue

            marks.append(goal)

        return marks

    def check_filter(self, fn):
        for filt in FILTER:
            if fnmatch(fn, filt):
                return True

        return False
