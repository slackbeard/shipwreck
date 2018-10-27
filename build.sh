#!/bin/bash

#rm -rf build/*
mkdir -p build
cd build
cmake ../src && make && mv disk-flat.vmdk ../vm/
