# Ninex
9x, as the description says, is my attempt at writing a Unix clone in C, with the primary goal of running dash (the shell).
As for building the kernel, you got 2 choices...
  - [CrecentOS](https://github.com/cleanbaja/CrecentOS), a reference operating system that uses 9x
  - Manually building the kernel using autoconf/automake (which I recommend)

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

There really isn't much documentation at the moment, but feel free to email me with any questions at <cleanbaja@protonmail.com>.
<br>I also hang around the [OSDEV](https://discord.gg/osdev) server (*sigterm*) from time to time.
