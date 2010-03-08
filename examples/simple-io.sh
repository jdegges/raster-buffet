#!/bin/bash

# This script takes in two parameters:
# 1 - Input file name
# 2 - Desired output file name

out=`echo "$1" | ./dst/bin/rb   \
    --input plugin:freeimage,rsc=-, \
    --decode plugin:freeimage,  \
    --process plugin=artistic, \
    --encode dst_fmt:BMP,plugin:freeimage,  \
    --output rsc:-,dir:out,ext:bmp,plugin:freeimage, 2> /dev/null`
mv $out $2
