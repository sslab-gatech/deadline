#!/usr/bin/env python

import os
import re
import json
import shlex
import signal

from os import walk
from os.path import splitext
from collections import OrderedDict
from subprocess import Popen
from multiprocessing import Pool
from abc import ABCMeta, abstractmethod, abstractproperty 

from cmd import *
from conf import *
from util import *

IRGEN_ERROR_FORMAT = re.compile("^(.+):(\d+):(\d+): error: (.*)$")
IRGEN_WARN_FORMAT = re.compile("^(.+):(\d+):(\d+): warning: (.*) \[(.*)\]$")

TRANS_FLAGS = ["-std-link-opts", "-O2", "-verify"]

# worker
class PoolWork(object):
    def __init__(self, redir, out, cmd):
        self.redir = redir
        self.out = out
        self.cmd = cmd

# helpers for multi-processing
def init_pool_worker():
    signal.signal(signal.SIGINT, signal.SIG_IGN)

def halt_pool_worker(signum, frame):
    raise RuntimeError()

# workers for subtasks
def irgen_worker(work):
    if not prepdn(work.out):
        LOG_ERR("Preparing %s failed" % work.out)
        return False
    
    with open(work.out, "w") as f:
        if shell(work.cmd, err = f) == 0:
            LOG_INF("IRGen done")
            return True
        else:
            LOG_ERR("IRGen fail")
            return False

def group_worker(work):
    if not prepdn(work.out):
        LOG_ERR("Preparing %s failed" % work.out)
        return False
    
    with open(work.out, "w") as f:
        if shell(work.cmd, err = f) == 0:
            LOG_INF("Group done")
            return True
        else:
            LOG_ERR("Group fail")
            return False

def trans_worker(work):
    if not prepdn(work.out):
        LOG_ERR("Preparing %s failed" % work.out)
        return False

    with open(work.out, "w") as f:
        if shell(work.cmd, err = f) == 0:
            LOG_INF("Trans done")
            return True
        else:
            LOG_ERR("Trans fail")
            return False

def check_worker(work):
    signal.signal(signal.SIGALRM, halt_pool_worker)
    signal.alarm(OPTS_TIME)

    if not prepdn(work.out):
        LOG_ERR("Preparing %s failed" % work.out)
        return False

    fout = open(os.devnull, "w")
    if work.redir:
        ferr = open(work.out, "w")
    else:
        ferr = None

    p = Popen(shlex.split(work.cmd), stdout = fout, stderr = ferr)

    try:
        if p.wait() == 0:
            LOG_INF("Check done")
            return 0 
        else:
            LOG_ERR("Check fail")
            return 1

    except RuntimeError:
        p.terminate()
        LOG_WRN("Check timeout: %s" % work.cmd)
        return -1

    finally:
        if fout is not None:
            fout.close()

        if ferr is not None:
            ferr.close()

        signal.alarm(0)
        signal.signal(signal.SIGALRM, signal.SIG_DFL)

def analyze_worker(pn):
    with open(pn, "r") as f:
        data = json.load(f, object_pairs_hook=OrderedDict)
        for fn in data:
            frec = data[fn]["result"]

            if frec["total"] != 0:
                print "%s: %d" % (fn, frec["total"])

            if frec["error"] != 0:
                LOG_ERR("Error: %d" % frec["error"])

            if frec["udf"] != 0:
                LOG_WRN("UDF: %d" % frec["udf"])

            if frec["sat"] != 0:
                LOG_INF("SAT: %d" % frec["sat"])

# app base class
class App(object):
    __metaclass__ = ABCMeta

    def __init__(self, app, tag, builder, grouper):
        self.app = app 
        self.tag = tag
        self.builder = builder 
        self.grouper = grouper 
    
    @property
    def path_repo(self):
        return resolve(PATH_APPS, self.app)

    @property
    def full_name(self):
        return "%s-%s" % (self.app, self.tag)

    @property
    def path_logs(self):
        return resolve(PATH_LOGS, self.full_name)

    @property
    def path_outs(self):
        return resolve(PATH_OUTS, self.full_name)

    @property
    def path_srcs(self):
        return resolve(PATH_SRCS, self.full_name)

    @property
    def path_objs(self):
        return resolve(PATH_OBJS, self.full_name)

    @property
    def path_bins(self):
        return resolve(PATH_BINS, self.full_name)

    @property
    def path_exts(self):
        return resolve(PATH_EXTS, self.full_name)

    @property
    def path_bcfs(self):
        return resolve(PATH_BCFS, self.full_name)

    @property
    def path_mods(self):
        return resolve(PATH_MODS, self.full_name)

    @property
    def path_tras(self):
        return resolve(PATH_TRAS, self.full_name)

    @property
    def path_syms(self):
        return resolve(PATH_SYMS, self.full_name)

    @property
    def path_log_build(self):
        return resolve(self.path_logs, "build.log")

    @property
    def path_log_parse(self):
        return resolve(self.path_logs, "parse.log")

    @property
    def path_log_irgen(self):
        return resolve(self.path_logs, "irgen.log")

    @property
    def path_log_group(self):
        return resolve(self.path_logs, "group.log")

    @property
    def path_log_links(self):
        return resolve(self.path_logs, "links.log")

    @property
    def path_log_trans(self):
        return resolve(self.path_logs, "trans.log")

    @property
    def path_log_check(self):
        return resolve(self.path_logs, "check.log")

    @abstractmethod
    def convert(self):
        return

    def checkout(self):
        ver = self.convert()
        if ver is None:
            LOG_ERR("Tag parsing failed")
            return False

        psrc = self.path_srcs
        return gitclone(ver, self.path_repo, psrc)

    @abstractmethod
    def config_impl(self, psrc, pobj, pbin, pext):
        LOG_ERR("Should never reach here")
        return False

    def config(self):
        psrc = self.path_srcs
        if not exists(psrc):
            LOG_ERR("Src path %s does not exist" % psrc)
            return False

        pobj = self.path_objs
        if not mkdirs(pobj):
            LOG_WRN("Config canceled")
            return False

        pbin = self.path_bins
        pext = self.path_exts

        return self.config_impl(psrc, pobj, pbin, pext)

    @abstractmethod
    def build_impl(self, psrc, pobj, pbin, pext, plog):
        LOG_ERR("Should never reach here")
        return False

    def build(self):
        psrc = self.path_srcs
        if not exists(psrc):
            LOG_ERR("Src path %s does not exist" % psrc)
            return False

        pobj = self.path_objs
        if not exists(pobj):
            LOG_ERR("Obj path %s does not exist" % pobj)
            return False
        
        pbin = self.path_bins
        if not mkdirs(pbin):
            LOG_WRN("Build canceled")
            return False

        pext = self.path_exts
        if not mkdirs(pext):
            LOG_WRN("Build canceled")
            return False
        
        plog = self.path_log_build
        if not prepdn(plog):
            LOG_WRN("Log path %s cannot be prepared" % plog)
            return False

        return self.build_impl(psrc, pobj, pbin, pext, plog)

    @abstractmethod
    def parse_line(self, line):
        LOG_ERR("Should never reach here")
        return None

    @abstractmethod
    def parse_opts(self, opts):
        LOG_ERR("Should never reach here")
        return None 
    
    def parse(self):
        log = self.path_log_build
        if not exists(log):
            LOG_ERR("Log path %s does not exist" % log)
            return False
        
        parser = CMDParser()
        runs = OrderedDict()
        dups = []
        
        # collect appeared flags
        collect = CMDCollect()

        with open(log) as f:
            for line in f:
                # analyze the line to extract argv section
                result = self.parse_line(line)
                if result is None or len(result) != 2:
                    LOG_ERR("Line parsing failed: %s" % line)
                    return False

                if result[0] == False:
                    continue

                opts = CMDResult(parser, result[1])

                # collect appeared flags
                collect.update(opts)

                # analyze the opts to get source files
                result = self.parse_opts(opts)
                if result is None or len(result) != 2:
                    LOG_ERR("Opt parsing failed: %s" % line)
                    return False

                if result[0] is False:
                    continue

                self.builder.build(opts)
                run = opts.organize()

                for src in result[1]:
                    if src in runs:
                        dups.append(src)
                    
                    runs[src] = run

        out = self.path_log_parse
        with open(out, "w") as f:
            json.dump(runs, f, indent = 2)
            
        for i in dups:
            LOG_WRN("[duplicate] %s" % i)

        # output collected flags
        collect.show()

        return True

    @abstractmethod
    def group_line(self, line):
        LOG_ERR("Should never reach here")
        return None

    @abstractmethod
    def group_opts(self, opts):
        LOG_ERR("Should never reach here")
        return None 

    @abstractmethod
    def group_mark(self, tops):
        LOG_ERR("Should never reach here")
        return None

    def group(self):
        log = self.path_log_build
        if not exists(log):
            LOG_ERR("Log path %s does not exist" % log)
            return False
        
        psrc = self.path_srcs
        pobj = self.path_objs
        
        pbcf = self.path_bcfs
        if not exists(pbcf):
            LOG_ERR("Bcf path %s does not exist" % pbcf)
            return False

        plog = self.path_log_irgen
        if not exists(plog):
            LOG_ERR("Log path %s does not exist" % plog)
            return False

        pout = self.path_outs
        if not exists(pout):
            LOG_ERR("Group cancelled")
            return False

        runs = OrderedDict()
        dups = []

        # collect included objs
        with open(log) as f:
            for line in f:
                # analyze the line to extract argv section
                result = self.group_line(line)
                if result is None or len(result) != 2:
                    LOG_ERR("Line grouping failed: %s" % line)
                    return False

                if result[0] == False:
                    continue

                opts = CMDModule(self.grouper, result[1])

                # analyze the opts to get source files
                result = self.group_opts(opts)
                if result is None or len(result) != 2:
                    LOG_ERR("Opt grouping failed: %s" % line)
                    return False

                if result[0] is False:
                    continue

                if opts.outs in runs:
                    dups.append(opts.outs)

                runs[opts.outs] = result[1] 

        # ignore dups
        for goal in dups:
            del runs[goal]

        # build ld hierarchy
        arch = OrderedDict()

        for goal in runs:
            arch[goal] = CMDLink(goal)

        for goal in runs:
            for src in runs[goal]:
                if src in arch:
                    arch[goal].link(arch[src])

        # collect top level targets
        tops = []
        for goal in arch:
            if len(arch[goal].pars) == 0:
                tops.append(goal)

        marks = self.group_mark(tops)

        # find all available objs
        bcfs = set()
        with open(plog, "r") as f:
            for l in f:
                toks = l.strip().split(" ")
                if toks[0] != "done":
                    continue

                r = toks[1]
                if r[0] == "/":
                    r = resolve(pbcf, r[len(psrc)+1:])
                else:
                    r = resolve(pbcf, r)

                r = splitext(r)[0] + ".bc"
                bcfs.add(r)

        # bottom up
        f = open(self.path_log_group, "w")

        count = 0
        coll = []
        failed = OrderedDict()

        while len(arch) != 0:
            f.write("==== iteration %d ====\n" % count)

            cmds = []
            outs = []
            reds = []
            srcs = []

            for goal in arch:
                if len(arch[goal].subs) != 0:
                    continue

                # define output
                if goal[0] == "/":
                    out = resolve(pbcf, goal[len(psrc)+1:])
                    red = resolve(pout, goal[len(psrc)+1:])
                else:
                    out = resolve(pbcf, goal)
                    red = resolve(pout, goal)

                out = splitext(out)[0] + ".bc"
                red = splitext(red)[0] + ".group"

                # define input
                infs = []
                for r in runs[goal]:
                    if r[0] == "/":
                        inf = resolve(pbcf, r[len(psrc)+1:])
                    else:
                        inf = resolve(pbcf, r)

                    inf = splitext(inf)[0] + ".bc"
                    if inf in bcfs:
                        infs.append(inf)
                    else:
                        if goal not in failed:
                            failed[goal] = set()

                        failed[goal].add(inf)

                # create cmd
                if not prepdn(out):
                    LOG_ERR("Cannot prepare file path %s" % out)
                    return False

                cmd = PoolWork(True, red, "%s -o %s %s" % \
                        (LLVM_BIN_BLD, out, " ".join(infs)))

                cmds.append(cmd)
                outs.append(out)
                reds.append(red)
                srcs.append(goal)

            # run commands
            with cd(pbcf):
                pool = Pool(OPTS_NCPU, init_pool_worker)

                try:
                    work = pool.map(group_worker, cmds)
                except KeyboardInterrupt:
                    pool.terminate()
                    pool.join()

            # consolidate output
            for i, r in enumerate(outs):
                if work[i]:
                    f.write("done %s\n" % srcs[i])
                    bcfs.add(r)
                else:
                    f.write("fail %s\n" % srcs[i])
                    coll.append(reds[i])

            # clean up
            for goal in srcs:
                del arch[goal]

            for goal in arch:
                for k in srcs:
                    if k in arch[goal].subs:
                        arch[goal].subs.remove(k)

            count += 1

        f.close()

        # output
        for i in dups:
            LOG_WRN("[duplicate] %s" % i)

        LOG_WRN("%d linking failures" % len(coll))
        for fn in coll:
            LOG_WRN(fn)
            with open(fn, "r") as f:
                print f.read()

        LOG_WRN("%d goals have only partial linking" % len(failed))
        for obj in sorted(failed):
            print obj

        # move to mods folder
        with open(self.path_log_links, "w") as f:
            for m in marks:
                if m[0] == "/":
                    out = resolve(pbcf, m[len(psrc)+1:])
                else:
                    out = resolve(pbcf, m)

                out = splitext(out)[0] + ".bc"

                if exists(out):
                    f.write("done %s\n" % m)
                else:
                    f.write("fail %s\n" % m)

        return True

    def irgen(self, inputs, force):
        psrc = self.path_srcs
        if not exists(psrc):
            LOG_ERR("Src path %s does not exist" % psrc)
            return False
        
        pobj = self.path_objs
        if not exists(pobj):
            LOG_ERR("Obj path %s does not exist" % pobj)
            return False

        plog = self.path_log_parse
        if not exists(plog):
            LOG_ERR("Log path %s does not exist" % plog)
            return False

        pout = self.path_outs
        if not mkdirs(pout):
            LOG_WRN("IRGen cancelled")
            return False
        
        pbcf = self.path_bcfs
        
        f = open(plog, "r")
        data = json.load(f, object_pairs_hook=OrderedDict)
        f.close()
        
        if inputs is None:
            runs = data 
        else:
            runs = OrderedDict()
            for i in inputs:
                runs[i] = data[i]

        cmds = [] 
        outs = []
        srcs = []

        for r in runs:
            if r[0] == "/":
                out = resolve(pbcf, r[len(psrc)+1:])
                red = resolve(pout, r[len(psrc)+1:])
            else:
                out = resolve(pbcf, r)
                red = resolve(pout, r)

            out = splitext(out)[0] + ".bc"
            red = splitext(red)[0] + ".irgen"

            if not prepdn(out):
                LOG_ERR("Cannot prepare file path %s" % out)
                return False

            if exists(out) and not force:
                cmd = PoolWork(True, red, "echo 'skip'")
            else:
                cmd = PoolWork(True, red, "%s -emit-llvm %s -o %s %s" % \
                        (LLVM_BIN_CLA, runs[r], out, r))

            cmds.append(cmd)
            outs.append(out)
            srcs.append(r)
    
        with cd(pobj):
            pool = Pool(OPTS_NCPU, init_pool_worker) 

            try:
                work = pool.map(irgen_worker, cmds)
            except KeyboardInterrupt:
                pool.terminate()
                pool.join()

        with open(self.path_log_irgen, "w") as f:
            for i, r in enumerate(runs.keys()):
                if work[i]:
                    f.write("done %s\n" % srcs[i])
                else:
                    f.write("fail %s\n" % srcs[i])

        return True

    def trans(self, inputs, force):
        psrc = self.path_srcs
        
        pbcf = self.path_bcfs
        if not exists(pbcf):
            LOG_ERR("Bcf path %s does not exist" % pbcf)
            return False

        plog = self.path_log_links
        if not exists(plog):
            LOG_ERR("Log path %s does not exist" % pmod)
            return False

        pout = self.path_outs
        if not exists(pout):
            LOG_WRN("Trans cancelled")
            return False
        
        ptra = self.path_tras

        data = OrderedDict()
        with open(plog, "r") as f:
            for l in f:
                toks = l.strip().split(" ")
                data[toks[1]] = toks[0]

        runs = []
        if inputs is None:
            for i in data:
                if data[i] == "done":
                    runs.append(i)
        else:
            for i in inputs:
                if data[i] == "done":
                    runs.append(i)

        cmds = []
        outs = []
        reds = []
        srcs = [] 

        for r in runs:
            if r[0] == "/":
                inf = resolve(pbcf, r[len(psrc)+1:])
                out = resolve(ptra, r[len(psrc)+1:])
                red = resolve(pout, r[len(psrc)+1:])
            else:
                inf = resolve(pbcf, r)
                out = resolve(ptra, r)
                red = resolve(pout, r)

            inf = splitext(inf)[0] + ".bc"
            out = splitext(out)[0] + ".ll"
            red = splitext(red)[0] + ".trans"

            if not prepdn(out):
                LOG_ERR("Cannot prepare file path %s" % out)
                return False
            
            if exists(out) and not force:
                cmd = PoolWork(True, red, "echo 'skip'")
            else:
                cmd = PoolWork(True, red, 
                        "%s %s %s > %s" % \
                                (LLVM_BIN_OPT, " ".join(TRANS_FLAGS), inf, out))

            cmds.append(cmd)
            outs.append(out)
            reds.append(red)
            srcs.append(r)

        with cd(pbcf):
            pool = Pool(OPTS_NCPU, init_pool_worker)

            try:
                work = pool.map(trans_worker, cmds)
            except KeyboardInterrupt:
                pool.terminate()
                pool.join()

                LOG_WRN("Interrupted")
                return False

        with open(self.path_log_trans, "w") as f:
            for i, r in enumerate(runs):
                if work[i]:
                    f.write("done %s\n" % srcs[i])
                else:
                    f.write("fail %s\n" % srcs[i])

        for i, fn in enumerate(reds):
            with open(fn, "r") as f:
                content = f.read()
                if len(content) != 0:
                    LOG_WRN(srcs[i])
                    print content

    @abstractmethod
    def check_filter(self):
        LOG_ERR("Should never reach here")
        return None

    def check(self, inputs, redir):
        psrc = self.path_srcs

        ptra = self.path_tras
        if not exists(ptra):
            LOG_ERR("Tra path %s does not exist" % ptra)
            return False

        plog = self.path_log_trans
        if not exists(plog):
            LOG_ERR("Log path %s does not exist" % plog)
            return False

        pout = self.path_outs
        if not exists(pout):
            LOG_WRN("Check cancelled")
            return False
 
        psym = self.path_syms
        
        data = OrderedDict()
        with open(plog, "r") as f:
            for l in f:
                toks = l.strip().split(" ")
                data[toks[1]] = toks[0]

        runs = []
        if inputs is None:
            for i in data:
                if data[i] == "done" and not self.check_filter(i):
                    runs.append(i)
        else:
            for i in inputs:
                if data[i] == "done" and not self.check_filter(i):
                    runs.append(i)

        cmds = [] 
        outs = []
        reds = []
        srcs = []

        for r in runs:
            if r[0] == "/":
                inf = resolve(ptra, r[len(psrc)+1:])
                out = resolve(psym, r[len(psrc)+1:])
                red = resolve(pout, r[len(psrc)+1:])
            else:
                inf = resolve(ptra, r)
                out = resolve(psym, r)
                red = resolve(pout, r)

            inf = splitext(inf)[0] + ".ll"
            out = splitext(out)[0] + ".sym"
            red = splitext(red)[0] + ".check"

            if not prepdn(out):
                LOG_ERR("Cannot prepare file path %s" % out)
                return False

            cmd = PoolWork(redir, red, 
                    "%s -load %s -KSym -symf %s -disable-verify %s" % \
                            (LLVM_BIN_OPT, PASS_KSYM, out, inf))

            cmds.append(cmd)
            outs.append(out)
            reds.append(red)
            srcs.append(r)
            
        with cd(ptra):
            pool = Pool(OPTS_NCPU, init_pool_worker) 
                
            try:
                work = pool.map(check_worker, cmds)
            except KeyboardInterrupt:
                pool.terminate()
                pool.join()

                LOG_WRN("Interrupted")
                return False

        with open(self.path_log_check, "w") as f:
            for i, r in enumerate(runs):
                if work[i] == 0:
                    f.write("done %s\n" % srcs[i])
                elif work[i] == -1:
                    f.write("time %s\n" % srcs[i])
                else:
                    f.write("fail %s\n" % srcs[i])

        for i, fn in enumerate(reds):
            with open(fn, "r") as f:
                content = f.read()
                if len(content) != 0:
                    LOG_WRN(srcs[i])
                    print content

        return True

    def __dump_irgen(self, fp):
        erec = dict() 
        wrec = set()

        for l in fp:
            m = IRGEN_ERROR_FORMAT.match(l)
            if m is not None:
                src = m.group(1)
                if src not in erec:
                    erec[src] = []
                
                erec[src].append(m.group(4))
                continue
            
            m = IRGEN_WARN_FORMAT.match(l)
            if m is not None:
                wrec.add(m.group(5))
                continue
        
        for k in sorted(erec):
            LOG_INF(k)
            for i in erec[k]:
                LOG_ERR(i)

        return wrec

    def dump(self):
        pout = self.path_outs
        if not exists(pout):
            LOG_ERR("Out path %s does not exist" % pout)
            return False

        dset = set()
        for dname, dlist, flist in os.walk(pout):
            for fn in flist:
                if splitext(fn)[1] == ".irgen":
                    with open(resolve(dname, fn), "r") as f:
                        dset.update(self.__dump_irgen(f))

        for k in sorted(dset):
            print k

        return True

    def stat(self):
        plog = self.path_log_check
        if not exists(plog):
            LOG_ERR("Log path %s does not exist" % plog)
            return False
        
        count = 0
        timed = [] 
        fails = []

        with open(plog, "r") as f:
            for l in f:
                toks = l.strip().split(" ")

                if toks[0] == "fail":
                    fails.append(toks[1])
                elif toks[0] == "time":
                    timed.append(toks[1])
                else:
                    assert toks[0] == "done"

                count += 1

        LOG_DBG("%s / %s failures" % (len(fails), count))
        for i in fails:
            LOG_ERR(i)

        LOG_DBG("%s / %s timeouts" % (len(timed), count))
        for i in timed:
            LOG_WRN(i)

        return True

    def result(self):
        pout = self.path_outs
        if not exists(pout):
            LOG_ERR("Out path %s does not exist" % pout)
            return False

        dset = dict()
        for dname, dlist, flist in os.walk(pout):
            for fn in flist:
                if splitext(fn)[1] == ".check":
                    with open(resolve(dname, fn), "r") as f:
                        content = f.read()
                        if len(content) != 0:
                            LOG_WRN(resolve(dname, fn))
                            for line in content.splitlines():
                                if line.startswith("[!]"):
                                    continue

                                toks = line.split("::")
                                k = toks[1]
                                v = toks[0]
                                if k not in dset:
                                    dset[k] = set()

                                dset[k].add(v)
        
        for x in sorted(dset.items(), key = lambda x : len(x[1]), reverse = True):
            LOG_INF("%s %d" % (x[0], len(x[1])))
            for i in x[1]:
                print i

        return True

    def analyze(self):
        plog = self.path_log_check
        if not exists(plog):
            LOG_ERR("Log path %s does not exist" % plog)
            return False
        
        psym = self.path_syms
        
        with open(plog, "r") as f:
            for l in f:
                toks = l.strip().split(" ")
                if toks[0] != "done":
                    continue

                pn = resolve(psym, splitext(toks[1])[0] + ".sym")
                analyze_worker(pn)

        return True

