#!/bin/bash -eu

scons

./main `cat input_vector` < test.in | sort -n > test.out

diff -q test.out test.save
