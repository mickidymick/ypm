#!/bin/bash

if [ ! -d ~/.yed/ypm/yed_plugins ]
then
    my_loc=$(pwd)
    cd ~/.yed/ypm
    git clone https://github.com/mickidymick/yed_plugins.git
    cd $my_loc
fi

if [ ! -d ~/.yed/ypm/plugins ]
then
    mkdir ~/.yed/ypm/plugins
fi

if [ ! -f ~/.yed/ypm/yedrc_plugins ]
then
    touch ~/.yed/ypm/yedrc_plugins
fi
