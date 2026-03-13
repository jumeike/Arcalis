#!/bin/bash

rm -rf build
mkdir build && cd build
cmake -DTHRIFT_GEN_CPP_DIR=../../../../gen-cpp ..
make -j$(nproc)
