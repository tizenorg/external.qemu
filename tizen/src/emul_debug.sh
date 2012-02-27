#!/bin/sh
CURDIR=`pwd`
gdb --args $CURDIR/emulator-x86 --vtm default \
 -- -net nic,model=virtio -usb -usbdevice maru-touchscreen -vga tizen -bios bios.bin -L $CURDIR/../x86/data/pc-bios -kernel $CURDIR/../x86/data/kernel-img/bzImage -usbdevice keyboard -rtc base=utc -net user --enable-kvm -redir tcp:1202:10.0.2.16:22
