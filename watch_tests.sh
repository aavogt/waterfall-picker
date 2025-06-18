#!/bin/bash
ls CMakeLists.txt {test,src}/* | entr bash -c "set -e
  cmake -Bbuild . -DENABLE_TESTING=ON
  make
  ./test_advance"
