#!/bin/bash

valgrind --leak-check=full --show-reachable=yes --track-origins=yes \
    ./dst/bin/rb   \
    --input plugin:freeimage,rsc=./examples/foo.txt, \
    --decode plugin:freeimage,  \
    --process plugin=artistic, \
    --encode dst_fmt:BMP,plugin:freeimage,  \
    --output rsc:list.txt,dir:out,ext:bmp,plugin:freeimage,
