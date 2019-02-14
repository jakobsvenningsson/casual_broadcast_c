#!/bin/bash

make

./casual_broadcast 0 processes 100 > ./out/A.out &

./casual_broadcast 1 processes 100 > ./out/B.out &

