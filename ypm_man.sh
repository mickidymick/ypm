#!/bin/bash

if [ -d ~/.yed/ypm/yed_plugins ] && [ ! -z "$PLUGIN" ] && [ ! -z "$WIDTH" ]
then
    my_loc=$(pwd)
    cd ~/.yed/ypm/yed_plugins/"$PLUGIN"/
    MANWIDTH=$WIDTH man -l man.7 2>&1
    cd $my_loc
fi
