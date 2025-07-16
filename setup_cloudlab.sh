# A script to prepare `d6515` machines in CloudLab for DPDK experiments
# with Mellanox ConnectX-6Dx NICs.

cd; sudo apt update

# Install dependencies.
sudo apt install -y cmake
sudo apt install -y meson
sudo apt install -y ninja-build
sudo apt install -y rdma-core
sudo apt install -y libibverbs-dev
sudo apt install -y libevent-dev
sudo apt install -y libgflags-dev
sudo apt install -y libnuma-dev
sudo apt install -y autoconf automake libtool pkg-config
sudo apt install -y \
  g++ \
  automake \
  libtool \
  pkg-config \
  flex \
  bison \
  libboost-all-dev \
  libevent-dev \
  libssl-dev \
  libzstd-dev \
  liblz4-dev \
  zlib1g-dev
sudo apt install -y python3-pip
pip3 install pyelftools

# Get DPDK.
wget https://fast.dpdk.org/rel/dpdk-25.03.tar.gz
tar -xvf dpdk-25.03.tar.gz

# Build DPDK.
cd dpdk-25.03
meson setup build
cd build
ninja
sudo ninja install
