#!/bin/bash

if [ -d ~/.yed/ypm/yed_plugins ] && [ ! -z "$PLUGIN" ]
then
    my_loc=$(pwd)
    cd ~/.yed/ypm/yed_plugins
    echo "Git Init"
    git submodule init $PLUGIN 2>&1
    echo "Git Update"
    git submodule update $PLUGIN 2>&1
    cd $PLUGIN
    chmod +x build.sh 2>&1
    echo "Plugin Build"
    ./build.sh 2>&1
    echo "Add Plugin"
    cp $PLUGIN.so ~/.yed/ypm/plugins/ 2>&1
    cd $my_loc
else
    echo "Run ypm-init"
fi
