#!/bin/sh

#Build-Depends: bison, flex, autoconf, gcc, libglu1-mesa-dev, libsdl1.2-dev, libgtk2.0-dev, libsdl-image1.2-dev, libsdl-gfx1.2-dev, debhelper, libxml2-dev, libasound2-dev

autoconf
./configure
make
