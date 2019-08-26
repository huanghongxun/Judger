#!/bin/sh
#
# Script to generate a basic chroot environment with a Java JRE
# to allow for Java programs to run in a chroot.
#
# This script downloads and installs a Ubuntu base system.
# Minimum requirements: a Linux system with glibc >= 2.3, wget, ar and
# a POSIX shell in /bin/sh. About 335/610 MB disk space is needed. It
# must be run as root and will install the debootstrap package.
#
# Part of the DOMjudge Programming Contest Jury System and licensed
# under the GNU GPL. See README and COPYING for details.

# Abort when a single command fails:
set -e

cleanup() {
    # Unmount things on cleanup
    umount -f "$CHROOTDIR/proc" >/dev/null 2>&1  || /bin/true
    umount -f "$CHROOTDIR/sys" >/dev/null 2>&1  || /bin/true
    umount -f "$CHROOTDIR/dev/pts" >/dev/null 2>&1  || /bin/true
}
trap cleanup EXIT

# Default directory where to build the chroot tree:
CHROOTDIR=""

# Fallback Linux distribution and release (codename) to bootstrap (note: overriden right below):
DISTRO="Ubuntu"
RELEASE="disco" # Ubuntu 19.04

# List of possible architectures to install chroot for:
UBUNTU_ARCHLIST="amd64,arm64,armhf,i386,powerpc,ppc64el,s390x"

# If host system is Ubuntu, use its release and architecture by default
if [ "x$(lsb_release -i -s || true)" = 'xUbuntu' ]; then
	DISTRO="$(lsb_release -i -s)"
	if [ "x$(lsb_release -c -s)" != "xsid" ]; then
		RELEASE="$(lsb_release -c -s)"
	fi
	if [ -z "$ARCH" ]; then
		ARCH="$(dpkg --print-architecture)"
	fi
fi

usage()
{
	cat <<EOF
Usage: $0 [options]...
Creates a chroot environment using Ubuntu.
Options:
  -a <arch>   Machine archicture to use.
  -d <dir>    Directory where to build the chroot environment.
  -R <releas> Ubuntu release to use: 'xenial', 'bionic', 'disco'.
  -i <debs>   List of extra package names to install (comma separated).
  -r <debs>   List of extra package names to remove (comma separated).
  -l <debs>   List of local package files to install (comma separated).
  -M <mirror> Ubuntu mirror.
  -y          Force overwriting the chroot dir and downloading debootstrap.
  -h          Display this help.
This script must be run as root, <chrootdir> is the non-existing
target location of the chroot (unless '-y' is specified). If the host
runs Ubuntu, the host architecture and release are used.
Available architectures:
Ubuntu: $UBUNTU_ARCHLIST

Variables:
  \$DEBPROXY             caching proxy for apt-get.
EOF
}

error()
{
    echo "Error: $*"
    echo
    usage
    exit 1
}

# Read command-line parameters:
while getopts 'a:d:i:r:l:M:R:yh' OPT ; do
	case $OPT in
		a) ARCH=$OPTARG ;;
		d) CHROOTDIR=$OPTARG ;;
        R) RELEASE=$OPTARG ;;
		i) INSTALLDEBS_EXTRA=$OPTARG ;;
		r) REMOVEDEBS_EXTRA=$OPTARG ;;
		l) LOCALDEBS=$OPTARG ;;
		M) DEBMIRROR=$OPTARG ;;
		y) FORCEYES=1 ;;
		h) SHOWHELP=1 ;;
		\?) error "Could not parse options." ;;
	esac
done
shift $((OPTIND-1))
if [ $# -gt 0 ]; then
	error "Additional arguments specified."
fi

if [ -n "$SHOWHELP" ]; then
	usage
	exit 0
fi

if [ "$(id -u)" != 0 ]; then
    echo "Warning: you probably need to run this program as root."
fi

[ -z "$CHROOTDIR" ] && error "No chroot directory given or default known."
[ -z "$ARCH" ]      && error "No architecture given or detected."

# Packages to include during bootstrap process (comma separated):
INCLUDEDEBS="software-properties-common"

# Packages to install after upgrade (space separated):
INSTALLDEBS="snapd snapd-xdg-open curl make librabbitmq-dev libcurl4-openssl-dev golang nodejs lua5.3 rustc xz-utils default-jdk-headless pypy python-pip python3 python3-pip clang ruby scala libboost-all-dev cmake libgtest-dev gcc gcc-9 g++ g++-9 gfortran gnat gcc-multilib g++-multilib libsqlite3-dev libmysqlclient-dev libpq-dev fp-compiler valgrind locales"
# For C# support add: mono-mcs mono-devel
# However running mono within chroot still gives errors...

# Packages to remove after upgrade (space separated):
REMOVEDEBS=""

# Which debootstrap package to install on non-Ubuntu systems:
# The version below supports Ubuntu version up to Ubuntu 19.04.
[ -z "$DEBOOTDEB" ] && DEBOOTDEB="debootstrap_1.0.114ubuntu1_all.deb"

# The Ubuntu mirror/proxy below can be passed as environment
# variables; if none are given the following defaults are used.

# Ubuntu mirror, modify to match closest mirror
[ -z "$DEBMIRROR" ] && DEBMIRROR="https://mirrors.ustc.edu.cn/ubuntu/"

# A local caching proxy to use for apt-get,
# (typically an install of aptcacher-ng), for example:
#DEBPROXY="http://aptcacher-ng.example.com:3142/"
[ -z "$DEBPROXY" ] && DEBPROXY=""

INSTALLDEBS="$INSTALLDEBS $(echo $INSTALLDEBS_EXTRA | tr , ' ')"
REMOVEDEBS=" $REMOVEDEBS  $(echo $REMOVEDEBS_EXTRA  | tr , ' ')"

# To prevent (libc6) upgrade questions:
export DEBIAN_FRONTEND=noninteractive

if [ -e "$CHROOTDIR" ]; then
	if [ ! "$FORCEYES" ]; then
		printf "'%s' already exists. Remove? (y/N) " "$CHROOTDIR"
		read -r yn
		if [ "$yn" != "y" ] && [ "$yn" != "Y" ]; then
			error "Chrootdir already exists, exiting."
		fi
	fi
	cleanup
	rm -rf "$CHROOTDIR"
fi

mkdir -p "$CHROOTDIR"
cd "$CHROOTDIR"
CHROOTDIR="$PWD"

if [ ! -x /usr/sbin/debootstrap ]; then

	echo "This script will install debootstrap on your system."
	if [ ! "$FORCEYES" ]; then
		printf "Continue? (y/N) "
		read -r yn
		if [ "$yn" != "y" ] && [ "$yn" != "Y" ]; then
			exit 1;
		fi
	fi

	if [ -f /etc/debian_version ]; then

		cd /
		apt-get install debootstrap

	else
		mkdir "$CHROOTDIR/debootstrap"
		cd "$CHROOTDIR/debootstrap"

		wget "$DEBMIRROR/pool/main/d/debootstrap/${DEBOOTDEB}"

		ar -x "$DEBOOTDEB"
		cd /
		zcat "$CHROOTDIR/debootstrap/data.tar.gz" | tar xv

		rm -rf "$CHROOTDIR/debootstrap"
	fi
fi

INCLUDEOPT=""
if [ -n "$INCLUDEDEBS" ]; then
	INCLUDEOPT="--include=$INCLUDEDEBS"
fi
EXCLUDEOPT=""
if [ -n "$EXCLUDEDEBS" ]; then
	EXCLUDEOPT="--exclude=$EXCLUDEDEBS"
fi

BOOTSTRAP_COMMAND="/usr/sbin/debootstrap"
if [ -n "$DEBPROXY" ]; then
    BOOTSTRAP_COMMAND="http_proxy=\"$DEBPROXY\" $BOOTSTRAP_COMMAND"
fi

echo "Running debootstrap to install base system, this may take a while..."

if [ -z $DEBMIRROR ]; then
	eval $BOOTSTRAP_COMMAND $INCLUDEOPT $EXCLUDEOPT \
		--variant=minbase --arch "$ARCH" "$RELEASE" "$CHROOTDIR" "http://mirrors.aliyun.com/ubuntu"
else
	eval $BOOTSTRAP_COMMAND $INCLUDEOPT $EXCLUDEOPT \
		--variant=minbase --arch "$ARCH" "$RELEASE" "$CHROOTDIR" "$DEBMIRROR"
fi

rm -f "$CHROOTDIR/etc/resolv.conf"
cp /etc/resolv.conf /etc/hosts /etc/hostname "$CHROOTDIR/etc" || true
cp /etc/ssl/certs/ca-certificates.crt "$CHROOTDIR/etc/ssl/certs/" || true
cp -r /usr/share/ca-certificates/* "$CHROOTDIR/usr/share/ca-certificates" || true
cp -r /usr/local/share/ca-certificates/* "$CHROOTDIR/usr/local/share/ca-certificates" || true



# Add apt proxy settings if desired
if [ -n "$DEBPROXY" ]; then
    echo "Acquire::http::Proxy \"$DEBPROXY\";" >> "$CHROOTDIR/etc/apt/apt.conf"
fi

mount -t proc proc "$CHROOTDIR/proc"
mount -t sysfs sysfs "$CHROOTDIR/sys"

# Required for some warning messages about writing to log files
mount --bind /dev/pts "$CHROOTDIR/dev/pts"

# Prevent perl locale warnings in the chroot:
export LC_ALL=C

chroot "$CHROOTDIR" /bin/sh -c debconf-set-selections <<EOF
passwd	passwd/root-password-crypted	password
passwd	passwd/user-password-crypted	password
passwd	passwd/root-password			password
passwd	passwd/root-password-again		password
passwd	passwd/user-password-again		password
passwd	passwd/user-password			password
passwd	passwd/shadow					boolean	true
passwd	passwd/username-bad				note
passwd	passwd/password-mismatch		note
passwd	passwd/username					string
passwd	passwd/make-user				boolean	true
passwd	passwd/md5						boolean	false
passwd	passwd/user-fullname			string
passwd	passwd/user-uid					string
passwd	passwd/password-empty			note
debconf	debconf/priority				select	high
debconf	debconf/frontend				select	Noninteractive
locales	locales/locales_to_be_generated		multiselect
locales	locales/default_environment_locale	select	None
EOF

# Disable upstart init scripts(so upgrades work), we don't need to actually run
# any services in the chroot, so this is fine.
# Refer to: http://ubuntuforums.org/showthread.php?t=1326721
chroot "$CHROOTDIR" /bin/sh -c "dpkg-divert --local --rename --add /sbin/initctl"
chroot "$CHROOTDIR" /bin/sh -c "ln -s /bin/true /sbin/initctl"

# Support mirrors over SSL.
chroot "$CHROOTDIR" /bin/sh -c "apt-get update && apt-get install apt-transport-https"

cat > "$CHROOTDIR/etc/apt/sources.list" <<EOF
deb $DEBMIRROR $RELEASE main restricted universe multiverse
deb $DEBMIRROR $RELEASE-security main restricted universe multiverse
deb $DEBMIRROR $RELEASE-updates main restricted universe multiverse
deb $DEBMIRROR $RELEASE-backports main restricted universe multiverse
EOF

cat > "$CHROOTDIR/etc/apt/apt.conf" <<EOF
APT::Get::Assume-Yes "true";
APT::Get::Force-Yes "false";
APT::Get::Purge "true";
APT::Install-Recommends "false";
Acquire::Retries "3";
Acquire::PDiffs "false";
EOF

# Upgrade the system, and install/remove packages as desired
chroot "$CHROOTDIR" /bin/sh -c "apt-get update && apt-get dist-upgrade"
chroot "$CHROOTDIR" /bin/sh -c "apt-get clean"
chroot "$CHROOTDIR" /bin/sh -c "apt-get install $INSTALLDEBS"
if [ -n "$LOCALDEBS" ]; then
	DIR=$(mktemp -d -p "$CHROOTDIR/tmp" 'dj_chroot_debs_XXXXXX')
	# shellcheck disable=SC2046
	cp $(echo "$LOCALDEBS" | tr , ' ') "$DIR"
	chroot "$CHROOTDIR" /bin/sh -c "dpkg -i /${DIR#$CHROOTDIR}/*.deb"
fi

#chroot "$CHROOTDIR" /bin/sh -c "snap install kotlin"

# Do some cleanup of the chroot
chroot "$CHROOTDIR" /bin/sh -c "apt-get remove --purge $REMOVEDEBS"
chroot "$CHROOTDIR" /bin/sh -c "apt-get autoremove --purge"
chroot "$CHROOTDIR" /bin/sh -c "apt-get clean"

# Install required python packages
chroot "$CHROOTDIR" /bin/sh -c "pip install xmltodict"
chroot "$CHROOTDIR" /bin/sh -c "pip3 install xmltodict"

# Install GTest
chroot "$CHROOTDIR" /bin/sh -c "cd /usr/src/gtest && cmake . && make && cp *.a /usr/lib"

# Install D-lang compiler suite
#chroot "$CHROOTDIR" /bin/sh -c "curl -fsS https://dlang.org/install.sh | bash -s dmd"

# Install testlib
#wget "https://raw.githubusercontent.com/MikeMirzayanov/testlib/master/testlib.h" -P "$CHROOTDIR/usr/include"

# Install oclint
#wget "https://github.com/oclint/oclint/releases/download/v0.13.1/oclint-0.13.1-x86_64-linux-4.4.0-112-generic.tar.gz" -P "$CHROOTDIR/tmp"
#tar -C "$CHROOTDIR/tmp/" -xzf "$CHROOTDIR/tmp/oclint-0.13.1-x86_64-linux-4.4.0-112-generic.tar.gz" 
#cp -p $(find "$CHROOTDIR/tmp/oclint-0.13.1/bin" -name "oclint*") "$CHROOTDIR/usr/local/bin/"
#cp -rp "$CHROOTDIR/tmp/oclint-0.13.1/lib/*" "$CHROOTDIR/usr/local/lib/"
# cp -rp "$CHROOTDIR/tmp/oclint-0.13.1/include/*" "$CHROOTDIR/usr/local/include/"

# Clean temp directory
chroot "$CHROOTDIR" /bin/sh -c "rm -r /tmp/*"

# Remove unnecessary setuid bits
chroot "$CHROOTDIR" /bin/sh -c "chmod a-s /usr/bin/wall /usr/bin/newgrp \
	/usr/bin/chage /usr/bin/chfn /usr/bin/chsh /usr/bin/expiry \
	/usr/bin/gpasswd /usr/bin/passwd \
	/bin/su /bin/mount /bin/umount /sbin/unix_chkpwd" || true

# Disable root account
sed -i "s/^root::/root:*:/" "$CHROOTDIR/etc/shadow"

# Create a dummy file to test that in the judging environment no
# access to group root readable files is available:
echo "This file should not be readable inside the judging environment!" \
	> "$CHROOTDIR/etc/root-permission-test.txt"
chmod 0640 "$CHROOTDIR/etc/root-permission-test.txt"

umount "$CHROOTDIR/dev/pts"
umount "$CHROOTDIR/sys"
umount "$CHROOTDIR/proc"

echo "Done building chroot in $CHROOTDIR"
exit 0
