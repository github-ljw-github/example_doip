#!/bin/bash

build_qnx()
{
    mkdir -p _build_qnx
    cd _build_qnx
   
    cmake -D TARGET=QNX\
          -D CMAKE_C_COMPILER=qcc\
          -D CMAKE_CXX_COMPILER=q++\
          -D CMAKE_CXX_FLAGS="-Y _cxx -V gcc_ntoaarch64le"\
          -D CMAKE_C_FLAGS="-Y _cxx -V gcc_ntoaarch64le"\
    .. && make -j 1 --trace
}
build_ubuntu()
{
    mkdir -p _build_ubuntu
    cd _build_ubuntu
   
    cmake .. && make -j 1 --trace
}

if [ "$1" == "qnx" ];then
    build_qnx
else
    build_ubuntu
fi


