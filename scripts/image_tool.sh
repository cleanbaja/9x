#!/usr/bin/env bash
# image_tool.sh - generate VHD image with limine/9x kernel installed (for testing)

SOURCE_DIR="$(dirname $(realpath "$0"))/.."
KERNEL_FILE="nxkern"
CONFIG_FILE="root/limine.cfg"

: ${IMAGE_FILE:="ninex.hdd"}
: ${LIMINE_DIR:="${SOURCE_DIR}/limine"}

info() {
    printf "\x1b[34m$(basename $0):\x1b[0m %s\n" "$*"
}

error() {
    printf "\x1b[31m$0:\x1b[0m %s\n" "$*" >&2
    exit 1
}

# check commands that may not be normally installed
command -v parted >/dev/null 2>&1 || error "'parted' is missing!"
command -v mkfs.fat >/dev/null 2>&1 || error "'mkfs.fat' is missing!"

# unmount previous loopback device, if it exists
[ -f lpdev ] &&
    info "running losetup (requires sudo)..." &&
    sudo losetup -d $(cat lpdev) &&
    rm -f lpdev

# make sure we have limine ready to use
if [ ! -d "${LIMINE_DIR}" ]; then
    info "downloading limine..."
    git clone --depth 1 --branch v4.x-branch-binary \
        https://github.com/limine-bootloader/limine "${LIMINE_DIR}" >/dev/null
    make -C "${LIMINE_DIR}"
fi

# create the image file (if it doesn't exist)
if [ ! -f "${IMAGE_FILE}" ]; then
    info "creating new image '${IMAGE_FILE}'..."
    dd if=/dev/zero bs=1M count=0 seek=64 of="${IMAGE_FILE}"
    parted -s "${IMAGE_FILE}" mklabel gpt
    parted -s "${IMAGE_FILE}" mkpart ESP fat32 2048s 100%
    parted -s "${IMAGE_FILE}" set 1 esp on
    "${LIMINE_DIR}/limine-deploy" "${IMAGE_FILE}"

    info "running losetup (requires sudo)..."
    sudo losetup -f >lpdev
    sudo losetup -Pf "${IMAGE_FILE}"
    sudo mkfs.fat -F 32 $(cat lpdev)p1 >/dev/null
    sudo losetup -d $(cat lpdev) && rm -f lpdev
fi

# make sure that the kernel exists, before updating the image
if [ ! -f "${KERNEL_FILE}" ]; then
    error "'${KERNEL_FILE}' doesn't exist! (try running './configure' and/or 'make')"
fi

info "updating image (requires sudo)..."
sudo losetup -f >lpdev
sudo losetup -Pf "${IMAGE_FILE}"
trap 'remove_tmpdir; trap - EXIT; exit' EXIT INT TERM QUIT HUP

TEMP_DIR="$(mktemp -d)"
remove_tmpdir() {
    if [ -d "${TEMP_DIR}" ]; then
        info "removing TEMP_DIR from shell command abort (requires sudo)..."
        sudo umount "${TEMP_DIR}"
        sudo losetup -d $(cat lpdev)
        rm -rf lpdev "${TEMP_DIR}"
    fi
}

sudo mount $(cat lpdev)p1 "${TEMP_DIR}"
sudo mkdir -p "${TEMP_DIR}"/EFI/BOOT
sudo mkdir -p "${TEMP_DIR}"/boot

sudo cp "${KERNEL_FILE}" "${SOURCE_DIR}/root/limine.cfg" "${TEMP_DIR}"/boot/
sudo cp "${LIMINE_DIR}"/limine.sys "${TEMP_DIR}"/
sudo cp "${LIMINE_DIR}"/BOOTX64.EFI "${TEMP_DIR}"/EFI/BOOT/
sudo cp "${LIMINE_DIR}"/BOOTAA64.EFI "${TEMP_DIR}"/EFI/BOOT/
sync

sudo umount "${TEMP_DIR}"
sudo losetup -d $(cat lpdev)
sudo rm -rf lpdev "${TEMP_DIR}"
