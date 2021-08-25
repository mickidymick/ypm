#!/bin/bash
gcc $(yed --print-cflags) $(yed --print-ldflags) -o ypm.so -g ypm.c
