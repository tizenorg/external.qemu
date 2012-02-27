#!/bin/sh
#
# Copyright (C) 2010 Samsung Electronics, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

SDL_GFX=`dpkg -l | grep libsdl-gfx`
if test "$SDL_GFX" = ""
then
	echo "There is no libSDL_gfx library to run this emulator.\nPlease install libsdl-gfx package." >> $STDERR_LOGFILE
	exit 1
fi

CURDIR=`pwd`
if test -e "emulator-x86"
then
    EMULATOR_BIN_PATH=$CURDIR
elif test $0 != "$CURDIR/emulator"
then
    EMULATOR_BIN_PATH=`echo $0 | sed "s/\/emulator//g"`
    echo $EMULATOR_BIN_PATH
else
    if test -e ~/.tizensdk
    then
        tizenpath=`grep TIZEN_SDK_INSTALL_PATH ~/.tizensdk`
        SDK_PATH=`echo $tizenpath | cut -f2- -d"="`
        EMULATOR_BIN_PATH="$SDK_PATH/Emulator"
    fi  
fi

EMULATOR_DATA_PATH="$EMULATOR_BIN_PATH/data"
EMULATOR_KERNEL_PATH="${EMULATOR_DATA_PATH}/kernel-img"
EMULATOR_KERNEL_NAME_ARM="zImage_arm"
EMULATOR_KERNEL_NAME_X86="bzImage"
QEMU_BIOS_PATH="${EMULATOR_DATA_PATH}/pc-bios"

#export LD_LIBRARY_PATH=$EMULATOR_BIN_PATH

## enable emulator dump & logging
export EMUL_VERBOSE=9
EMUL_LOGFILE="$EMULATOR_BIN_PATH/emulator.log"
if test -e $EMUL_LOGFILE
then
	rm $EMUL_LOGFILE
fi
STDERR_LOGFILE="$EMULATOR_BIN_PATH/stderr.txt"
if test -e $STDERR_LOGFILE
then
	rm $STDERR_LOGFILE
fi

EMUL_DUMP=1
ulimit -c unlimited

GUEST_IP_ADDRESS="10.0.2.16"
HOST_QEMU_ADDRESS="10.0.2.2"

# port redirection
telnet_port=1201
ssh_port=1202
ide_port=3578
gps_port=3579
sensor_port=3580

display_help_mesgs () {
	echo "emulator - run emulator"
	echo "option"
	echo "\t --x86"
	echo "\t\t Start emulator with x86 mode"
	echo "\t --arm"
	echo "\t\t Start emulator with arm mode"
	echo "\t --no-kvm"
	echo "\t\t Disable kvm"
	echo "\t --no-dump"
	echo "\t\t Disable dump"
	echo "\t --debug-kernel"
	echo "\t\t Enable remote debugger for guest os"
	echo "\t --debug-qemu"
	echo "\t\t Run emulator with gdb"
	echo "\t --ddd-kernel"
	echo "\t\t Run emulator with DDD(Data Display Debugger)"
	echo "\t --t [new image] | --target [new image]"
	echo "\t\t Run emulator with new image"
	echo "\t --h | --help"
	echo "\t\t Display this help messages and exit"
	echo "\t --*"
	echo "\t\t Emulator option started with --"
	echo "\t -*"
	echo "\t\t Qemu option started with -"
	return 0 
}

parse_input_params () {
	while [ "$1" != "" ]
	do
		case $1 in
		nfs*)
			BOOT_OPTION="--target"
			shift ;;
		glx*)
			DGLES2_BACKEND=""
			shift ;;
		--arm)
			TARGET_ARCH='arm'
			if test "$TARGET_NAME" = ""
			then
				TARGET_NAME='emulimg.arm'
			fi
			shift;;
		--x86)
			TARGET_ARCH='i686'
			if test "$TARGET_NAME" = ""
			then
				TARGET_NAME='emulimg-default.x86'
			fi
			shift;;
		--no-kvm)
			kvm_opt=""
			shift ;;
		--no-dump)
			EMUL_DUMP=0
			EMUL_LOGFILE=""
			EMUL_VERBOSE=0
			ulimit -c 0
			emul_opts="--no-dump $emul_opts"
			shift ;;
		--ddd-kernel)
			DDD="ddd --gdb"
			qemu_common_opts="-S -s $qemu_common_opts"
			kvm_opt=""
			shift ;;
		--debug-kernel)
			qemu_common_opts="-S -s $qemu_common_opts"
			kvm_opt=""
			shift ;;
		--debug-qemu)
			GDB="gdb --args"
			EMUL_DUMP=0
			EMUL_LOGFILE=""
			EMUL_VERBOSE=0
			ulimit -c 0
			emul_opts="--no-dump $emul_opts"
			shift ;;
		--t|--target)
			while echo "$2" | grep "^[^-]"
			do
				TARGET_NAME="$2"
				shift
			done
			shift	;;
		--h|--help)
			display_help_mesgs
			exit 1 ;;
		--*)
			emul_opts="$emul_opts $1"
			while echo "$2" | grep "^[^-]" > /dev/null
			do
				emul_opts="$emul_opts $2"
				shift
			done
			shift	;;
		-*)
			qemu_common_opts="$qemu_common_opts $1"
			while echo "$2" | grep "^[^-]" > /dev/null
			do
				qemu_common_opts="$qemu_common_opts $2"
				shift
			done
			shift ;;
		*) shift ;;
		esac
	done
}

# find a suitable terminal program
set_terminal_type () {
	if test -x /usr/bin/gnome-terminal
	then
		EMULATOR_TERMINAL="/usr/bin/gnome-terminal --disable-factory -x"
	else
		EMULATOR_TERMINAL="/usr/bin/xterm -l -e"
	fi
	export EMULATOR_TERMINAL
}

set_user_env () {
	test -e "/dev/kvm" && kvm_opt="-enable-kvm"

	set_terminal_type
	parse_input_params $@

	if test "$TARGET_ARCH" = ""
	then
		TARGET_ARCH='i686'
		if test "$TARGET_NAME" = ""
		then
			TARGET_NAME='emulimg-default.x86'
		fi
	fi

	if test "$BOOT_OPTION" = "" -o "$BOOT_OPTION" = "--disk"
	then 
		BOOT_OPTION="--disk"

		if test "$TARGET_NAME" = 'emulimg-default.x86'
		then
			TARGET_PATH="$EMULATOR_BIN_PATH/$TARGET_NAME"
		else
			TARGET_PATH="$TARGET_NAME"
		fi

	else
		TARGET_PATH="/scratchbox/users/$USER/targets/$TARGET_NAME"
	fi
}

# check if running from the build directory
set_devel_env () {
	build_dir="`dirname $0`"
	if test \! -x "$build_dir/emulator-x86" -a \! -x "$build_dir/emulator-arm"
	then
		return 1
	fi

	echo "Running from build directory"
	EMULATOR_BIN_PATH="$build_dir"

	case ${TARGET_ARCH} in
		*arm)
		        test -d "$build_dir/../etc" || mkdir "$build_dir/../etc"
		        test -f "$build_dir/../etc/DEBUGCH" || cp "$build_dir/DEBUGCH" "$build_dir/../etc/" 
		        test -d "$build_dir/../arm" || mkdir "$build_dir/../arm"
		        test -h "$build_dir/../arm/conf" || ln -s "$build_dir/../conf" "$build_dir/../arm/conf"
		        test -h "$build_dir/../arm/data" || ln -s "$build_dir/../data" "$build_dir/../arm/data"
		        test -d "$build_dir/../arm/VMs" || mkdir "$build_dir/../arm/VMs" 
		        test -d "$build_dir/../arm/VMs/default" || mkdir "$build_dir/../arm/VMs/default" 
		        test -d "$build_dir/../arm/VMs/default/logs" || mkdir "$build_dir/../arm/VMs/default/logs" 
		        test -f "$build_dir/../arm/VMs/default/config.ini" || cp "$build_dir/config_dbg_arm.ini" "$build_dir/../arm/VMs/default/config.ini" 

			# find the target path
			test "$BOOT_OPTION" = "--disk" && TARGET_PATH="$EMULATOR_BIN_PATH/../../emulator-image/$TARGET_NAME"

			# fine the kernel image
			if test -d "$EMULATOR_BIN_PATH/../../../kernel/linux-2.6.32/arch/arm/boot"
			then
				EMULATOR_KERNEL_PATH="$EMULATOR_BIN_PATH/../../../kernel/linux-2.6.32/arch/arm/boot"
				EMULATOR_KERNEL_NAME_ARM="zImage"
				if test "$DDD" != "" 
				then
					DDD="$DDD --debugger /usr/bin/arm-linux/bin/arm-linux-gdb $EMULATOR_KERNEL_PATH/../../../vmlinux"
				fi
			else
				DDD=""
			fi
			;;
		*86)	
		        test -d "$build_dir/../etc" || mkdir "$build_dir/../etc"
		        test -f "$build_dir/../etc/DEBUGCH" || cp "$build_dir/DEBUGCH" "$build_dir/../etc/" 
		        test -d "$build_dir/../x86" || mkdir "$build_dir/../x86"
		        test -h "$build_dir/../x86/conf" || ln -s "$build_dir/../conf" "$build_dir/../x86/conf"
		        test -h "$build_dir/../x86/data" || ln -s "$build_dir/../data" "$build_dir/../x86/data"
		        test -d "$build_dir/../x86/VMs" || mkdir "$build_dir/../x86/VMs" 
		        test -d "$build_dir/../x86/VMs/default" || mkdir "$build_dir/../x86/VMs/default" 
		        test -d "$build_dir/../x86/VMs/default/logs" || mkdir "$build_dir/../x86/VMs/default/logs" 
		        test -f "$build_dir/../x86/VMs/default/config.ini" || cp "$build_dir/config_dbg_x86.ini" "$build_dir/../x86/VMs/default/config.ini" 

			# find the target path
			test "$BOOT_OPTION" = "--disk" && TARGET_PATH="$EMULATOR_BIN_PATH/../../../../emulator-image/$TARGET_NAME"

			# fine the kernel image
			if test -d "$EMULATOR_BIN_PATH/../../../emulator-kernel/arch/x86/boot"
			then
				EMULATOR_KERNEL_PATH="$EMULATOR_BIN_PATH/../../../emulator-kernel/arch/x86/boot"
				if test "$DDD" != "" 
				then
					DDD="$DDD $EMULATOR_KERNEL_PATH/../../../vmlinux"
				fi
			else
				DDD=""
			fi
			;;
	esac

	# find the bios path
	if test -d "$EMULATOR_BIN_PATH/../data/pc-bios"
	then
		QEMU_BIOS_PATH="$EMULATOR_BIN_PATH/../data/pc-bios"
		echo "Found BIOS at $QEMU_BIOS_PATH"
	fi
}

set_emulator_options () {
	#disable dump
	if test EMUL_DUMP = 0
	then
		EMUL_LOGFILE=""
		EMUL_VERBOSE=0
		ulimit -c 0
		rm emulator.klog
		emul_opts="--no-dump $emul_opts"
	fi
}

set_qemu_hw_options () {
	#qemu cpu selection
	qemu_arm_opts="$qemu_arm_opts -M s5pc110"
	qemu_x86_opts="$qemu_x86_opts -M tizen-x86-machine"

	#qemu memory size
#	qemu_common_opts="$qemu_common_opts -m 256"

	#qemu network hw selection
	qemu_arm_opts="$qemu_arm_opts -net nic,model=s5pc1xx-usb-otg"
	#qemu_x86_opts="$qemu_x86_opts -net nic,model=rtl8139"
	qemu_x86_opts="$qemu_x86_opts -net nic,model=virtio"

	#emulator_gps
	#qemu_common_opts="$qemu_common_opts -serial pipe:/tmp/gpsdevice"
	qemu_common_opts="$qemu_common_opts -redir udp:$gps_port:${GUEST_IP_ADDRESS}:$gps_port"
	
	#emulator_sensor
#	qemu_common_opts="$qemu_common_opts -redir udp:$sensor_port:${GUEST_IP_ADDRESS}:$sensor_port"

	#acclerator : for opengl module (not necessary now)
#	qemu_x86_opts="$qemu_x86_opts -device Accelerator"

	#sound
	qemu_x86_opts="$qemu_x86_opts -soundhw all"

	#touchpad
	qemu_x86_opts="$qemu_x86_opts -usb -usbdevice maru-touchscreen"

	#graphic
#	modified by caramis...
#	qemu_x86_opts="$qemu_x86_opts -vga std -bios bios.bin -L ${QEMU_BIOS_PATH}"
	qemu_x86_opts="$qemu_x86_opts -vga tizen -bios bios.bin -L ${QEMU_BIOS_PATH}"

	#keyboard
	qemu_common_opts="-usbdevice keyboard $qemu_common_opts"
}

set_qemu_options () {
	#kenel_image
	if test -d "$EMULATOR_KERNEL_PATH"
	then 
		qemu_arm_opts="$qemu_arm_opts -kernel $EMULATOR_KERNEL_PATH/$EMULATOR_KERNEL_NAME_ARM"
		qemu_x86_opts="$qemu_x86_opts -kernel $EMULATOR_KERNEL_PATH/$EMULATOR_KERNEL_NAME_X86"
	else
		echo "Cannot find kernel image !!!!" >> $STDERR_LOGFILE
		exit 1
	fi

	#qemu_time_setting
	#qemu_common_opts="$qemu_common_opts -localtime"
	qemu_common_opts="$qemu_common_opts -rtc base=utc"

	#qemu_network mode
	qemu_common_opts="$qemu_common_opts -net user"

	#qemu_telnet
	#qemu_common_opts="$qemu_common_opts -redir tcp:$telnet_port:${GUEST_IP_ADDRESS}:23"
	#emul_opts="$emul_opts  --telnet-port $telnet_port"

	#qemu_ssh
	qemu_common_opts="$qemu_common_opts -redir tcp:$ssh_port:${GUEST_IP_ADDRESS}:22"
	#emul_opts="$emul_opts  --ssh-port $ssh_port"

	#emulator_ide_port
	qemu_common_opts="$qemu_common_opts -redir tcp:$ide_port:${GUEST_IP_ADDRESS}:$ide_port"

	#qemu monitor
	qemu_common_opts="$qemu_common_opts -monitor tcp:127.0.0.1:9000,server,nowait"
}

set_qemu_debug_options () {
	#setup ports for gdbstub
	for port in 9990 9991 9992 9993 9994 9995 9996 9997 9998 9999
	do
		debug_ports="$debug_ports -redir tcp:$port:${GUEST_IP_ADDRESS}:$port"
	done
}

set_user_env $@
test "`basename $0`" = "emulator.sh" && set_devel_env || DDD=""
set_emulator_options
set_qemu_hw_options
set_qemu_options
#set_qemu_debug_options

if test "$DDD" != "" 
then
	ps -ef | grep "$DDD" | grep -v "grep" > /dev/null || $DDD > /dev/null 2>&1 &
fi

case ${TARGET_ARCH} in
	*arm)
		DGLES2_BACKEND='env DGLES2_BACKEND=osmesa'
		# export DGLES2_BACKEND="osmesa"
		exec $GDB $DGLES2_BACKEND "${EMULATOR_BIN_PATH}/emulator-arm" \
			$emul_opts -- $qemu_arm_opts $qemu_common_opts $debug_ports
		;;
	*86)
		if test $EMUL_DUMP = 1
		then
		exec $GDB "${EMULATOR_BIN_PATH}/emulator-x86" \
			$BOOT_OPTION "$TARGET_PATH" \
			$emul_opts -- $qemu_x86_opts $qemu_common_opts $debug_ports $kvm_opt 1>> $EMUL_LOGFILE 2> stderr.log
		else
		exec $GDB "${EMULATOR_BIN_PATH}/emulator-x86" \
			$emul_opts -- $qemu_x86_opts $qemu_common_opts $debug_ports $kvm_opt
		fi
		;;
	*)
		echo "Unknown target architecture: ${TARGET_ARCH}" >> $STDERR_LOGFILE
		;;
esac

