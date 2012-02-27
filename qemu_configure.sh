#!/bin/sh
# OS specific
#--target-list=i386-softmmu,arm-softmmu \
targetos=`uname -s`
case $targetos in
Linux*)
echo "checking for os... targetos $targetos"
exec ./configure \
 --target-list=i386-softmmu \
 --disable-werror \
 --audio-drv-list=pa \
 --enable-mixemu \
 --disable-vnc-tls \
 --enable-tcg-x86-opt \
 --enable-v4l2 \
 --enable-ffmpeg
# --enable-debug \
# --enable-profiler \
# --enable-gles2 --gles2dir=/usr
;;
CYGWIN*)
echo "checking for os... targetos $targetos"
exec ./configure \
 --target-list=i386-softmmu \
 --audio-drv-list=winwave \
 --audio-card-list=es1370 \
 --enable-mixemu \
 --disable-vnc-tls 
;;
MINGW*)
echo "checking for os... targetos $targetos"
exec ./configure \
 --target-list=i386-softmmu \
 --audio-drv-list=winwave \
 --audio-card-list=es1370 \
 --enable-mixemu \
 --disable-vnc-tls \
 --enable-ffmpeg
# --disable-vnc-jpeg \
# --disable-jpeg
;;
esac
