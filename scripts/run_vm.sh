#!/usr/bin/env bash
# run_vm.sh - run VHD image using the QEMU system emulator

SOURCE_DIR="$(dirname $(realpath "$0"))/.."
RAM_AMOUNT="$(cat /proc/meminfo | numfmt --field 2 --from-unit=Ki --to-unit=Gi | sed 's/ kB/G/g' | grep MemTotal | awk '{print $2/2}' | sed 's/G//g')"

: ${IMAGE_FILE:="ninex.hdd"}
: ${QEMU_ARCH:="$(uname -m)"}

info() {
    printf "\x1b[34m$(basename $0):\x1b[0m %s\n" "$*"
}

error() {
    printf "\x1b[31m$0:\x1b[0m %s\n" "$*" >&2
    exit 1
}

# Make sure QEMU exists...
command -v qemu-system-$QEMU_ARCH >/dev/null 2>&1 || error "'parted' is missing!"

usage() {
    echo -e "usage: $0 [-i input] [-k] [-w] [-d] [-o]\n"

    echo -e "Supported arguments:"
    echo -e "\t -i input                 specifies path to input image"
    echo -e "\t -k                       use kvm for hardware acceleration"
    echo -e "\t -w                       enables VNC (for WSL2)"
    echo -e "\t -d                       enable GDB stub (for debugging)"
    echo -e "\t -o                       path to OVMF.fd for $QEMU_ARCH"
    echo -e "\t -h                       shows this help message\n"
}

if [ $# -eq 0 ]; then
    usage
    exit 0
fi

# Then parse the command line, and add flags as needed
image=
ovmf_dir=
use_kvm=0
enable_vnc=0
debugging=0

while getopts i:kwdo:h arg; do
    case $arg in
    i) image="$OPTARG" ;;
    k) use_kvm=1 ;;
    w) enable_vnc=1 ;;
    d) debugging=1 ;;
    o) ovmf_dir="$OPTARG" ;;
    h)
        usage
        exit 0
        ;;
    ?)
        echo "See -h for help."
        exit 1
        ;;
    esac
done

if [ -z "$image" ]; then
    info "image file wasn't specified (assuming ${IMAGE_FILE})"
    image="${IMAGE_FILE}"
fi

if [ -z "$ovmf_dir" ]; then
    error "path to OVMF.fd wasn't specified"
fi

# Setup our default flags/variables...
QEMU_FLAGS=("-m" "${RAM_AMOUNT}G")

# Next, build up the command line
if [ "$use_kvm" = "1" ]; then
    QEMU_FLAGS+=("--enable-kvm")
fi
if [ "$debugging" = "1" ]; then
    QEMU_FLAGS+=("-s" "-S" "-no-reboot" "-no-shutdown")
fi
if [ "$enable_vnc" = "1" ]; then
    QEMU_FLAGS+=("-vnc" ":0")
fi

# Add in some arch-specific QEMU flags
if [ "$QEMU_ARCH" = "x86_64" ]; then
    QEMU_FLAGS+=("-M" "q35" "-cpu" "qemu64,+rdtscp" "-smp" "$(nproc)")
    QEMU_FLAGS+=("-bios" "${ovmf_dir}/OVMF.fd" "-debugcon" "stdio")
elif [ "$QEMU_ARCH" = "aarch64" ]; then
    QEMU_FLAGS+=("-smp" "4" "-M" "virt" "-serial" "stdio" "-device" "ramfb" "-cpu" "cortex-a72")
    QEMU_FLAGS+=("-device" "qemu-xhci" "-device" "usb-kbd" "-bios" "${ovmf_dir}/OVMF.fd")
else
    error "unknown arch $QEMU_ARCH"
fi

qemu-system-$QEMU_ARCH ${QEMU_FLAGS[@]} -hda $image
