Name:           qemu-arm-static
Url:            http://wiki.qemu.org/Index.html
License:        BSD 3-Clause; GPL v2 or later; LGPL v2.1 or later; X11/MIT
Group:          System/Emulators/PC
Summary:        Static Qemu for Arm
Version:        0.14.1
Release:        1
Source:         qemu-%version.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

BuildRequires:  eglibc-static  
BuildRequires:  zlib-devel
#BuildRequires:  texi2html which sane-backends-libs 
BuildRequires:  zlib-static 


%description
QEMU is an extremely well-performing CPU emulator that allows you to
choose between simulating an entire system and running userspace
binaries for different architectures under your native operating
system. It currently emulates x86, ARM, PowerPC and SPARC CPUs as well
as PC and PowerMac systems.



Authors:
--------
    Fabrice Bellard <fabrice.bellard@free.fr>

%prep
%setup -q -n qemu-%version

%build
export RPM_OPT_FLAGS=${RPM_OPT_FLAGS//-fno-omit-frame-pointer/-fomit-frame-pointer}
export CFLAGS="$RPM_OPT_FLAGS"
./configure --prefix=/usr \
        --enable-system --disable-linux-user \
        --target-list="arm-linux-user" \
        --static


%install
make DESTDIR=%{buildroot} install
mv %{buildroot}/usr/bin/qemu-arm %{buildroot}/usr/bin/qemu-arm-static
rm -rf %{buildroot}/usr/share
rm -rf %{buildroot}/usr/etc


%clean
rm -rf %{buildroot}

%files
%defattr(-, root, root)
%{_bindir}/qemu-arm-static
