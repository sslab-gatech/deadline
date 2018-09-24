#!/bin/bash

# paths
LLVM=$ROOT/../../llvm/bins/bin

ROOT=$(pwd)
SRCS=$ROOT/srcs
BINS=$ROOT/bins

# init paths
rm -rf $BINS
mkdir $BINS

# clone z3
git clone https://github.com/Z3Prover/z3.git $SRCS

# configure
cd $SRCS
CXX=$LLVM/clang++ CC=$LLVM/clang \
  python scripts/mk_make.py --noomp --prefix=$BINS

# build
cd build
make -j$(nproc)

# install
make install

