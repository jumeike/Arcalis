#!/bin/bash

# Generate Thrift files
thrift -r --gen cpp minimal_social.thrift

# Create build directory and build
rm -rf build
mkdir build
cd build
cmake ..
make

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build completed successfully!"
else
    echo "Build failed!"
    exit 1
fi
