#!/bin/bash

# Create build directory if it doesn't exist
mkdir -p build

# Enter build directory
cd build
rm -rf *

# Check if Thrift library exists
if [ ! -f "/usr/local/lib/libthrift.so" ]; then
    echo "Error: Thrift library not found in /usr/local/lib"
    echo "Please check your Thrift installation"
    exit 1
fi

# Check if Thrift headers exist
if [ ! -d "/usr/local/include/thrift" ]; then
    echo "Error: Thrift headers not found in /usr/local/include"
    echo "Please check your Thrift installation"
    exit 1
fi

# Run cmake with verbose output
cmake -DCMAKE_VERBOSE_MAKEFILE=OFF ..

# Build the project
cmake --build .

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build completed successfully!"
    echo "Executables are in the build directory"
    
    # Check library dependencies
    echo "Checking library dependencies..."
    ldd composepost_server
    ldd composepost_client
else
    echo "Build failed!"
    exit 1
fi
