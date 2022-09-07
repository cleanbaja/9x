# Ninex

9x is a lightweight (and hooby) unix clone with the primary goal of running dash (the shell).
It has somewhat met that goal, currently being capable of somewhat running dash, although the `fork` syscall is heavily broken at the moment.
The kernel supports many advanced x86_64 features, such as 5-level paging, AVX-512, TCE (Translation Cache Extensions) and SMEP/SMAP, while
supporting a SunOS style VM, which implements a HAT layer, and virual memory segments/address spaces.

## Building

Run the following to build the kernel
```
./autogen.sh
make -j `nproc`
```

To build a disk image which you can flash to a USB drive, run this...
```
make ninex.hdd
```

Or to run in QEMU, run the following command
```
make run

# uses KVM (much faster than TCG, which is used by default)
make run-kvm
```

There really isn't much documentation at the moment, but feel free to email me with any questions at <cleanbaja@protonmail.com>.
<br>I also hang around the [OSDEV](https://discord.gg/osdev) server (*sigterm*) from time to time.
