#!/bin/bash

# Default values
ENABLE_GEM5=false
ENABLE_TRACING=false
DEBUG=false

# Parse command-line options
while [[ $# -gt 0 ]]; do
  case "$1" in
    --gem5)
      ENABLE_GEM5=true
      shift
      ;;
    --tracing)
      ENABLE_TRACING=true
      shift
      ;;
    --debug)
      DEBUG=true
      shift
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [--gem5] [--tracing] [--debug]"
      exit 1
      ;;
  esac
done

# Construct CXXFLAGS
CXXFLAGS=""
if $ENABLE_GEM5; then
  CXXFLAGS+=" -DENABLE_GEM5"
fi
if $ENABLE_TRACING; then
  CXXFLAGS+=" -DENABLE_TRACING"
fi
if $DEBUG; then
  CXXFLAGS+=" -g -O0 -mssse3"
fi

# Run configure and build
./configure --without-java --without-go --without-python --without-kotlin --without-php \
  --without-d --without-netstd --without-lua --without-py3 --without-ruby --without-rs \
  --without-swift --without-perl --without-nodejs --without-haxe --without-erlang \
  --without-dart --without-dpdk CXXFLAGS="$CXXFLAGS"

make clean
make -j$(nproc)
sudo make install

