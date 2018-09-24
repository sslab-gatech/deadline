#!/usr/bin/env python

import re
import os
import sys

from os import chdir, chmod, getcwd, makedirs, walk
from os.path import abspath, dirname, exists, join
from shutil import rmtree
from contextlib import contextmanager
from subprocess import call, check_output, CalledProcessError
from termcolor import colored

# logs utils
def LOG_ERR(msg, detail = None):
    print >> sys.stdout, colored(msg, "red")
    if detail is not None:
        print >> sys.stderr, detail

def LOG_WRN(msg, detail = None):
    print >> sys.stdout, colored(msg, "yellow")
    if detail is not None:
        print >> sys.stderr, detail

def LOG_INF(msg, detail = None):
    print >> sys.stdout, colored(msg, "green")
    if detail is not None:
        print >> sys.stderr, detail

def LOG_DBG(msg, detail = None):
    print >> sys.stdout, msg
    if detail is not None:
        print >> sys.stderr, detail

# with stmts
@contextmanager
def cd(pn):
    cur = getcwd()
    chdir(pn)
    yield
    chdir(cur)

@contextmanager
def envpath(key, *pn):
    pns = ":".join(pn)

    if key in os.environ:
        cur = os.environ[key]
        os.environ[key] = pns + ":" + cur
        yield
        os.environ[key] = cur
    else:
        os.environ[key] = pns
        yield
        del os.environ[key]

# path utils
def resolve(*pn):
    return abspath(join(*pn))

def prepdn(pn):
    dn = dirname(pn)
    if exists(dn):
        return True
    else:
        try:
            makedirs(dn)
            return True
        except:
            return False

def mkdirs(pn, force = False):
    if exists(pn):
        if not force:
            ans = raw_input("%s exists, delete ? " % pn)
            if ans.lower() != "y":
                return False

        rmtree(pn)
        
    try:
        makedirs(pn)
        return True
    except:
        return False

def passthrough(sym, prog, args):
    if not prepdn(sym):
        return False

    content = '#!/bin/bash\nexec %s %s $@' % (prog, " ".join(args))

    with open(sym, "w") as f:
        f.write(content)

    try:
        chmod(sym, 0777)
        return True
    except:
        return False

def fcollect(root, ext):
    items = []

    for dirpath, dnames, fnames in walk(root):
        for f in fnames:
            if f.endswith(ext):
                items.append(join(dirpath, f))

    return items

# exec utils
def execute(*args):
    try:
        output = check_output(args).strip()
        result = 0
    except CalledProcessError as err:
        output = err.output.strip()
        result = err.returncode

    return (result, output)

def shell(cmd, out = None, err = None):
    return call(cmd, shell = True, stdout = out, stderr = err)

# common ops
def gitclone(ver, src, dst):
    if not mkdirs(dst):
        LOG_WRN("Checkout canceled")
        return False

    cmd = "git clone --recursive --branch %s %s %s" % \
            (ver, src, dst)

    if shell(cmd) == 0:
        LOG_INF("Checkout done")
        return True
    else:
        LOG_ERR("Checkout failed")
        return False 

# string utils
SPLIT_PATTERN = re.compile(r'''((?:[^ "']|"[^"]*"|'[^']*')+)''')

def shellsplit(cmd):
    return SPLIT_PATTERN.split(cmd)[1::2]
