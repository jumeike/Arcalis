./configure --without-java --without-go --without-python --without-kotlin --without-php --without-d --without-netstd --without-lua --without-py3 --without-ruby --without-rs --without-swift --without-perl --without-nodejs --without-haxe --without-erlang --without-dart --without-dpdk CXXFLAGS="-DENABLE_TRACING -DENABLE_GEM5 -g -O0"
make clean && make -j$(nproc) && sudo make install
