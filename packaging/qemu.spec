Name:           qemu
VCS:            platform/upstream/qemu#15382d5fa1ea8d6e9e5441cead3494c82207c063
Url:            http://www.qemu.org/
Summary:        Universal CPU emulator
License:        BSD-3-Clause and GPL-2.0 and GPL-2.0+ and LGPL-2.1+ and MIT
Group:          System/Utilities
Version:        1.6.0
Release:        0
Source:         %name-%version.tar.bz2
# this is to make lint happy
Source300:      rpmlintrc
Source302:      bridge.conf
Source303:      baselibs.conf
Source400:      update_git.sh
# Patches auto-generated by git-buildpackage:
Patch0:         0001-XXX-dont-dump-core-on-sigabort.patch
Patch1:         0002-XXX-work-around-SA_RESTART-race-with-boehm-gc-ARM-on.patch
Patch2:         0003-qemu-0.9.0.cvs-binfmt.patch
Patch3:         0004-qemu-cvs-alsa_bitfield.patch
Patch4:         0005-qemu-cvs-alsa_ioctl.patch.gz
Patch5:         0006-qemu-cvs-alsa_mmap.patch
Patch6:         0007-qemu-cvs-gettimeofday.patch
Patch7:         0008-qemu-cvs-ioctl_debug.patch
Patch8:         0009-qemu-cvs-ioctl_nodirection.patch
Patch9:         0010-block-vmdk-Support-creation-of-SCSI-VMDK-images-in-q.patch
Patch10:        0011-linux-user-add-binfmt-wrapper-for-argv-0-handling.patch
Patch11:        0012-linux-user-Ignore-timer_create-syscall.patch
Patch12:        0013-linux-user-be-silent-about-capget-failures.patch
Patch13:        0014-PPC-KVM-Disable-mmu-notifier-check.patch
Patch14:        0015-linux-user-fix-segfault-deadlock.patch
Patch15:        0016-linux-user-binfmt-support-host-binaries.patch
Patch16:        0017-linux-user-arm-no-tb_flush-on-reset.patch
Patch17:        0018-linux-user-Ignore-broken-loop-ioctl.patch
Patch18:        0019-linux-user-lock-tcg.patch
Patch19:        0020-linux-user-Run-multi-threaded-code-on-a-single-core.patch
Patch20:        0021-linux-user-lock-tb-flushing-too.patch
Patch21:        0022-linux-user-Fake-proc-cpuinfo.patch
Patch22:        0023-linux-user-implement-FS_IOC_GETFLAGS-ioctl.patch
Patch23:        0024-linux-user-implement-FS_IOC_SETFLAGS-ioctl.patch
Patch24:        0025-linux-user-XXX-disable-fiemap.patch
Patch25:        0026-slirp-nooutgoing.patch
Patch26:        0027-vnc-password-file-and-incoming-connections.patch
Patch27:        0028-linux-user-add-more-blk-ioctls.patch
Patch28:        0029-linux-user-use-target_ulong.patch
Patch29:        0030-Add-support-for-DictZip-enabled-gzip-files.patch
Patch30:        0031-Add-tar-container-format.patch
Patch31:        0032-Legacy-Patch-kvm-qemu-preXX-dictzip3.patch.patch
Patch32:        0033-Legacy-Patch-kvm-qemu-preXX-report-default-mac-used..patch
Patch33:        0034-console-add-question-mark-escape-operator.patch
Patch34:        0035-Make-char-muxer-more-robust-wrt-small-FIFOs.patch
Patch35:        0036-linux-user-lseek-explicitly-cast-non-set-offsets-to-.patch
Patch36:        0037-virtfs-proxy-helper-Provide-__u64-for-broken-sys-cap.patch
Patch37:        0038-gdbstub-Fix-gdb_register_coprocessor-register-counti.patch
Patch38:        0039-roms-Build-vgabios.bin.patch
Patch39:        0040-configure-Enable-PIE-for-ppc-and-ppc64-hosts.patch
Patch40:        0041-Work-around-bug-in-gcc-4.8.patch
Patch41:        0042-Fix-size-used-for-memset-in-alloc_output_file.patch
Patch42:        0043-Avoid-strict-aliasing-warning-for-gcc-4.3.patch
Patch43:        0044-enable-32-bit-qemu-for-Tizen.patch
Patch44:        0045-virtfs-proxy-helper.c-fix-compile-error.patch
BuildRequires:  bison
BuildRequires:  curl-devel
BuildRequires:  e2fsprogs-devel
BuildRequires:  libattr-devel
BuildRequires:  libcap-devel
#BuildRequires:  libcap-ng-devel
#BuildRequires:  libgnutls-devel
BuildRequires:  gnutls-devel
#BuildRequires:  libjpeg8-devel
BuildRequires:  libpng-devel
BuildRequires:  ncurses-devel
# we must not install the qemu package when under qemu build
%if 0%{?qemu_user_space_build:1}
BuildRequires:  -post-build-checks
%endif
#BuildRequires:  zlib-devel-static
#BuildRequires:  glibc-devel-static
#BuildRequires:  libattr-devel-static
#BuildRequires:  glib2-devel-static
#BuildRequires:  pcre-devel-static
BuildRequires:  zlib-static
BuildRequires:  eglibc-static
BuildRequires:  libattr-devel
BuildRequires:  glib2-static
BuildRequires:  libpcre-devel
BuildRequires:  fdupes
#BuildRequires:  pwdutils

BuildRequires:  python
#BuildRequires:  pkgconfig(sdl)
Requires:       /usr/sbin/groupadd
Requires:       pwdutils
Requires:       timezone

%description
QEMU is an extremely well-performing CPU emulator that allows you to
choose between simulating an entire system and running userspace
binaries for different architectures under your native operating
system. It currently emulates x86, ARM, PowerPC and SPARC CPUs as well
as PC and PowerMac systems.

%package tools
Summary:        Universal CPU emulator -- Tools
Provides:       qemu:%_libexecdir/qemu-bridge-helper

%description tools
QEMU is an extremely well-performing CPU emulator that allows you to
choose between simulating an entire system and running userspace
binaries for different architectures under your native operating
system. It currently emulates x86, ARM, PowerPC and SPARC CPUs as well
as PC and PowerMac systems.

This sub-package contains various tools, including a bridge helper.

%package guest-agent
Summary:        Universal CPU emulator -- Guest agent
Provides:       qemu:%_bindir/qemu-ga

%description guest-agent
QEMU is an extremely well-performing CPU emulator that allows you to
choose between simulating an entire system and running userspace
binaries for different architectures under your native operating
system. It currently emulates x86, ARM, PowerPC and SPARC CPUs as well
as PC and PowerMac systems.

This sub-package contains the guest agent.

%package linux-user
Summary:        Universal CPU emulator -- Linux User binaries
Provides:       qemu:%_bindir/qemu-arm

%description linux-user
QEMU is an extremely well-performing CPU emulator that allows you to
choose between simulating an entire system and running userspace
binaries for different architectures under your native operating
system. It currently emulates x86, ARM, PowerPC and SPARC CPUs as well
as PC and PowerMac systems.

This sub-package contains statically linked binaries for running linux-user
emulations. This can be used together with the OBS build script to
run cross-architecture builds.

%prep
%setup -q -n %name-%version
# 0001-XXX-dont-dump-core-on-sigabort.patch
%patch0 -p1
# 0002-XXX-work-around-SA_RESTART-race-with-boehm-gc-ARM-on.patch
%patch1 -p1
# 0003-qemu-0.9.0.cvs-binfmt.patch
%patch2 -p1
# 0004-qemu-cvs-alsa_bitfield.patch
%patch3 -p1
# 0005-qemu-cvs-alsa_ioctl.patch.gz
%patch4 -p1
# 0006-qemu-cvs-alsa_mmap.patch
%patch5 -p1
# 0007-qemu-cvs-gettimeofday.patch
%patch6 -p1
# 0008-qemu-cvs-ioctl_debug.patch
%patch7 -p1
# 0009-qemu-cvs-ioctl_nodirection.patch
%patch8 -p1
# 0010-block-vmdk-Support-creation-of-SCSI-VMDK-images-in-q.patch
%patch9 -p1
# 0011-linux-user-add-binfmt-wrapper-for-argv-0-handling.patch
%patch10 -p1
# 0012-linux-user-Ignore-timer_create-syscall.patch
%patch11 -p1
# 0013-linux-user-be-silent-about-capget-failures.patch
%patch12 -p1
# 0014-PPC-KVM-Disable-mmu-notifier-check.patch
%patch13 -p1
# 0015-linux-user-fix-segfault-deadlock.patch
%patch14 -p1
# 0016-linux-user-binfmt-support-host-binaries.patch
%patch15 -p1
# 0017-linux-user-arm-no-tb_flush-on-reset.patch
%patch16 -p1
# 0018-linux-user-Ignore-broken-loop-ioctl.patch
%patch17 -p1
# 0019-linux-user-lock-tcg.patch
%patch18 -p1
# 0020-linux-user-Run-multi-threaded-code-on-a-single-core.patch
%patch19 -p1
# 0021-linux-user-lock-tb-flushing-too.patch
%patch20 -p1
# 0022-linux-user-Fake-proc-cpuinfo.patch
%patch21 -p1
# 0023-linux-user-implement-FS_IOC_GETFLAGS-ioctl.patch
%patch22 -p1
# 0024-linux-user-implement-FS_IOC_SETFLAGS-ioctl.patch
%patch23 -p1
# 0025-linux-user-XXX-disable-fiemap.patch
%patch24 -p1
# 0026-slirp-nooutgoing.patch
%patch25 -p1
# 0027-vnc-password-file-and-incoming-connections.patch
%patch26 -p1
# 0028-linux-user-add-more-blk-ioctls.patch
%patch27 -p1
# 0029-linux-user-use-target_ulong.patch
%patch28 -p1
# 0030-Add-support-for-DictZip-enabled-gzip-files.patch
%patch29 -p1
# 0031-Add-tar-container-format.patch
%patch30 -p1
# 0032-Legacy-Patch-kvm-qemu-preXX-dictzip3.patch.patch
%patch31 -p1
# 0033-Legacy-Patch-kvm-qemu-preXX-report-default-mac-used..patch
%patch32 -p1
# 0034-console-add-question-mark-escape-operator.patch
%patch33 -p1
# 0035-Make-char-muxer-more-robust-wrt-small-FIFOs.patch
%patch34 -p1
# 0036-linux-user-lseek-explicitly-cast-non-set-offsets-to-.patch
%patch35 -p1
# 0037-virtfs-proxy-helper-Provide-__u64-for-broken-sys-cap.patch
%patch36 -p1
# 0038-gdbstub-Fix-gdb_register_coprocessor-register-counti.patch
%patch37 -p1
# 0039-roms-Build-vgabios.bin.patch
%patch38 -p1
# 0040-configure-Enable-PIE-for-ppc-and-ppc64-hosts.patch
%patch39 -p1
# 0041-Work-around-bug-in-gcc-4.8.patch
%patch40 -p1
# 0042-Fix-size-used-for-memset-in-alloc_output_file.patch
%patch41 -p1
# 0043-Avoid-strict-aliasing-warning-for-gcc-4.3.patch
%patch42 -p1
# 0044-enable-32-bit-qemu-for-Tizen.patch
%patch43 -p1
# 0045-virtfs-proxy-helper.c-fix-compile-error.patch
%patch44 -p1

%build
# build QEMU
mkdir -p dynamic
# build qemu-system
./configure --prefix=%_prefix \
	--sysconfdir=%_sysconfdir \
	--libexecdir=%_libexecdir \
	--enable-curl \
	--enable-virtfs \
	--disable-linux-aio \
	--extra-cflags="$QEMU_OPT_FLAGS" \
	--enable-system \
	--disable-linux-user
#	--enable-sdl

make %{?jobs:-j%jobs} V=1
mv *-softmmu/qemu-system-* dynamic
mv qemu-io qemu-img qemu-nbd qemu-bridge-helper dynamic
#mv qemu-img.1 qemu-nbd.8 dynamic
mv qemu-ga dynamic
mv fsdev/virtfs-proxy-helper dynamic
make clean
# build userland emus
./configure --prefix=%_prefix --sysconfdir=%_sysconfdir \
	--libexecdir=%_libexecdir \
	--enable-linux-user \
	--disable-system \
	--static --disable-linux-aio \
	--extra-cflags="$QEMU_OPT_FLAGS"
make %{?jobs:-j%jobs} V=1

%install
make install DESTDIR=$RPM_BUILD_ROOT
rm -fr $RPM_BUILD_ROOT/%_datadir/doc
install -m 755 dynamic/qemu-system-* $RPM_BUILD_ROOT/%_bindir
install -m 755 dynamic/qemu-io $RPM_BUILD_ROOT/%_bindir
install -m 755 dynamic/qemu-img $RPM_BUILD_ROOT/%_bindir
install -m 755 dynamic/qemu-nbd $RPM_BUILD_ROOT/%_bindir
install -m 755 dynamic/qemu-ga $RPM_BUILD_ROOT/%_bindir
install -m 755 dynamic/virtfs-proxy-helper $RPM_BUILD_ROOT/%_bindir
install -d -m 755 $RPM_BUILD_ROOT/%_sbindir
install -m 755 scripts/qemu-binfmt-conf.sh $RPM_BUILD_ROOT/%_sbindir
install -d -m 755 $RPM_BUILD_ROOT/%_libexecdir
install -m 755 dynamic/qemu-bridge-helper $RPM_BUILD_ROOT/%_libexecdir
install -d -m 755 $RPM_BUILD_ROOT/%_mandir/man1
install -D -m 644 %{SOURCE302} $RPM_BUILD_ROOT/%{_sysconfdir}/qemu/bridge.conf
%ifnarch %ix86 x86_64
ln -sf ../../../emul/ia32-linux $RPM_BUILD_ROOT/usr/share/qemu/qemu-i386
%endif
%ifnarch ia64
mkdir -p $RPM_BUILD_ROOT/emul/ia32-linux
%endif
%fdupes -s $RPM_BUILD_ROOT

%clean
rm -rf ${RPM_BUILD_ROOT}

%pre
%{_bindir}/getent group kvm >/dev/null || %{_sbindir}/groupadd -r kvm 2>/dev/null
%{_bindir}/getent group qemu >/dev/null || %{_sbindir}/groupadd -r qemu 2>/dev/null
%{_bindir}/getent passwd qemu >/dev/null || \
  %{_sbindir}/useradd -r -g qemu -G kvm -d / -s /sbin/nologin \
  -c "qemu user" qemu

%files
#%license COPYING
%defattr(-, root, root)
#%doc COPYING COPYING.LIB Changelog README VERSION
%_bindir/qemu-system-*
%_datadir/%name
%ifnarch %ix86 x86_64 ia64
%dir /emul/ia32-linux
%endif
%dir %_sysconfdir/%name
%config %_sysconfdir/%name/target-x86_64.conf

%files tools
%defattr(-, root, root)
%_bindir/qemu-io
%_bindir/qemu-img
%_bindir/qemu-nbd
%_bindir/virtfs-proxy-helper
%verify(not mode) %_libexecdir/qemu-bridge-helper
%dir %_sysconfdir/%name
%config %_sysconfdir/%name/bridge.conf

%files guest-agent
%defattr(-, root, root)
%attr(755,root,kvm) %_bindir/qemu-ga

%files linux-user
%defattr(-, root, root)
%_bindir/qemu-alpha
%_bindir/qemu-arm
%_bindir/qemu-armeb
%_bindir/qemu-cris
%_bindir/qemu-i386
%_bindir/qemu-m68k
%_bindir/qemu-microblaze
%_bindir/qemu-microblazeel
%_bindir/qemu-mips
%_bindir/qemu-mips64
%_bindir/qemu-mips64el
%_bindir/qemu-mipsel
%_bindir/qemu-mipsn32
%_bindir/qemu-mipsn32el
%_bindir/qemu-or32
%_bindir/qemu-ppc64abi32
%_bindir/qemu-ppc64
%_bindir/qemu-ppc
%_bindir/qemu-s390x
%_bindir/qemu-sh4
%_bindir/qemu-sh4eb
%_bindir/qemu-sparc32plus
%_bindir/qemu-sparc64
%_bindir/qemu-sparc
%_bindir/qemu-unicore32
%_bindir/qemu-x86_64
%_bindir/qemu-*-binfmt
%_sbindir/qemu-binfmt-conf.sh

%changelog