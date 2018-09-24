#!/usr/bin/env python

import sys
import json
from os.path import exists, splitext

from enum import Enum
from collections import OrderedDict
from argparse import ArgumentParser

import networkx as nx
import matplotlib.pyplot as plt

# data parsing
class NodeLabel(Enum):
    ROOT_CFG 		= 1
    ROOT_SLICE		= 2
    HOST_FETCH 		= 3

class FlowNode(object):
    def __init__(self, nid, info):
        self.nid = nid
        self.info = info

        self.pred = []
        self.succ = []

        self.label = set() 

    def addPred(self, node):
        self.pred.append(node)

    def addSucc(self, node):
        self.succ.append(node)

    def addLabel(self, lab):
        self.label.add(lab)

    def hasPred(self, nid):
        for n in self.pred:
            if n.nid == nid:
                return True

        return False

    def hasSucc(self, nid):
        for n in self.succ:
            if n.nid == nid:
                return True

        return False

class FlowItem(object):
    def __init__(self, nid, perm):
        self.nid = nid

        self.perm = perm 
        self.temp = None

    def update(self, temp):
        self.temp = temp

    def restore(self):
        self.temp = None

class FlowGraph(object):
    def __init__(self, cfg):
        self.repo = OrderedDict()

        # init all nodes
        for rec in cfg["item"]:
            nid = rec["name"]
            self.repo[nid] = FlowItem(nid, FlowNode(nid, rec["size"]))

        # add links
        for rec in cfg["item"]:
            node = self.repo[rec["name"]].perm

            for elem in rec["pred"]:
                node.addPred(self.repo[elem].perm)

            for elem in rec["succ"]:
                node.addSucc(self.repo[elem].perm)

        # set root
        self.repo[cfg["root"]].perm.addLabel(NodeLabel.ROOT_CFG)

    def update(self, cfg, host):
        # update all nodes
        for rec in cfg["item"]:
            nid = rec["name"]
            assert nid in self.repo
            self.repo[nid].update(FlowNode(nid, rec["size"]))

        # add links
        for rec in cfg["item"]:
            node = self.repo[rec["name"]].temp

            for elem in rec["pred"]:
                node.addPred(self.repo[elem].temp)

            for elem in rec["succ"]:
                node.addSucc(self.repo[elem].temp)

        # set root
        self.repo[cfg["root"]].temp.addLabel(NodeLabel.ROOT_SLICE)

        # set host
        self.repo[host].temp.addLabel(NodeLabel.HOST_FETCH)

    def restore(self):
        for k in self.repo:
            self.repo[k].restore()

# visualizer
class NodeFormat(object):
    def __init__(self, node):
        self.alpha = 0.75 
        self.width = 1.0
        self.color = "red"
        self.shape = "o"

        if NodeLabel.ROOT_CFG in node.perm.label:
            self.shape = "s"

        if node.temp is not None:
            self.color = "blue"

            if NodeLabel.ROOT_SLICE in node.temp.label:
                self.shape = "s"

            if NodeLabel.HOST_FETCH in node.temp.label:
                self.color = "yellow"

class EdgeFormat(object):
    def __init__(self, mode):
        self.alpha = 1.0
        self.width = 2.0

        if mode == 0:
            self.color = "black"
            self.style = "solid"
        elif mode == 1:
            self.color = "green"
            self.style = "dashed"
        elif mode == 2:
            self.color = "blue"
            self.style = "solid"

class Grapher(object):
    def __init__(self):
        self.graph = nx.DiGraph()

    def addNode(self, nid, val, fmt):
        self.graph.add_node(nid, val = val, fmt = fmt)

    def addEdge(self, src, dst, val, fmt):
        self.graph.add_edge(src, dst, val = val, fmt = fmt)

    def draw(self):
        # layout
        try:
            pos = nx.nx_pydot.pydot_layout(self.graph, prog = "dot")
        except Exception as ex:
            print >> sys.stderr, "Unable to use DOT layout: %s" % ex
            pos = nx.spring_layout(self.graph)

        # draw nodes
        for n in self.graph.nodes(data = True):
            fmt = n[1]["fmt"]
            nx.draw_networkx_nodes(self.graph, pos, [n[0]],
                    cmap = plt.cm.Oranges, vmin = 0.0, vmax = 1.0,
                    node_color = fmt.color,
                    node_shape = fmt.shape,
                    alpha = fmt.alpha,
                    linewidths = fmt.width)

        # draw node text
        node_labels = nx.get_node_attributes(self.graph, "val")
        nx.draw_networkx_labels(self.graph, pos, labels = node_labels)

        # draw edges
        for e in self.graph.edges(data = True):
            fmt = e[2]["fmt"] 
            nx.draw_networkx_edges(self.graph, pos, [(e[0], e[1])], 
                    edge_color = fmt.color,
                    style = fmt.style,
                    alpha = fmt.alpha,
                    width = fmt.width)

        # show
        plt.show()

# analysis
class Fetch(object):
    def __init__(self, rec):
        self.inst = rec["inst"]
        self.host = rec["host"]
        self.slice = rec["slice"]

class Func(object):
    def __init__(self, fid, rec):
        self.fid = fid

        # flow graph
        self.graph = FlowGraph(rec["cfg"])

        # fetches
        self.fetches = []
        for v in rec["fetch"]:
            self.fetches.append(Fetch(v))

    def draw(self, fetch):
        self.graph.restore()
        self.graph.update(fetch.slice, fetch.host)

        print >> sys.stdout, "Fetch: %s" % fetch.inst

        g = Grapher()
        
        for k, v in self.graph.repo.items():
            if v.temp is None:
                info = "%d" % (v.perm.info)
            else:
                info = "%d/%d" % (v.temp.info, v.perm.info)

            g.addNode(k, info, NodeFormat(v))

        for k, v in self.graph.repo.items():
            for s in v.perm.succ:
                if v.temp is not None and v.temp.hasSucc(s.nid):
                    g.addEdge(v.nid, s.nid, "", EdgeFormat(2))
                else:
                    g.addEdge(v.nid, s.nid, "", EdgeFormat(0))

        for k, v in self.graph.repo.items():
            if v.temp is None:
                continue

            for s in v.temp.succ:
                if not v.perm.hasSucc(s.nid):
                    g.addEdge(v.nid, s.nid, "", EdgeFormat(1))

        g.draw()

    def show(self):
        if len(self.fetches) == 0:
            return

        print >> sys.stdout, "Function: %s" % self.fid

        for fetch in self.fetches:
            self.draw(fetch)

class Module(object):
    def __init__(self, fn):
        if not exists(fn):
            print >> sys.stderr, "%s does not exist" % fn
            sys.exit(-1)

        self.funcs = OrderedDict() 

        with open(fn, "r") as fp:
            mrec = json.load(fp)
            for k in mrec:
                self.funcs[k] = Func(k, mrec[k])

# entry point
if __name__ == "__main__":
    # init
    parser = ArgumentParser()
    parser.add_argument("files", action="append", default=[])
    parser.add_argument("-f", "--func", action="append", default=None)

    # parse
    args = parser.parse_args()

    # exec
    for fn in args.files:
        toks = splitext(fn) 
        if toks[1] == ".c" and "/srcs/" in toks[0]:
            fn = toks[0].replace("/srcs/", "/syms/") + ".sym"
        
        mod = Module(fn)
        for fid in mod.funcs:
            if args.func is not None and fid not in args.func:
                continue
        
            mod.funcs[fid].show()
