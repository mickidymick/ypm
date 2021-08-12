#!/bin/bash

if [ -d ~/.yed/ypm/yed_plugins ] && [ ! -z "$PLUGIN" ]
then
    my_loc=$(pwd)
    cd ~/.yed/ypm/yed_plugins
    echo "Git Deinit"
    git submodule deinit $PLUGIN -f 2>&1
    echo "Remove Plugin"
    rm ~/.yed/ypm/plugins/$PLUGIN.so 2>&1
    cd $my_loc
fi
