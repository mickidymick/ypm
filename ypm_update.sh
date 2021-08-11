#!/bin/bash

if [ -d ~/.yed/yed_plugins ]
then
    if [ ! -z "$PLUGIN" ]
    then
        if [ $PLUGIN != ALL ]
        then
            my_loc=$(pwd)
            cd ~/.yed/ypm/yed_plugins
            git submodules update $PLUGIN
            cd $my_loc
        else
            my_loc=$(pwd)
            cd ~/.yed/ypm/yed_plugins
            git pull
            cd $my_loc
        fi
    fi
fi
