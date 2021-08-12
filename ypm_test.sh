#!/bin/bash

if [ -d ~/.yed/yed_plugins ] && [ ! -z "$PLUGIN" ]
then
my_loc=$(pwd)
cd ~/.yed/yed_plugins
git submodule init $PLUGIN
cd $my_loc
fi
