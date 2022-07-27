# Ninex
**NOTE: I have given up on ninex for various reasons, such as the degrading code
quality, bugs on real hardware, and the fact that multiple kernel components weren't
planned out properly, resulting in a poor implmentation. With that said, I plan on
giving ninex a do-over once I find the energy, time and knowledge to properly write a
operating system kernel. Till then, Ninex will remain in a dormant state...**

9x, as the description says, is my attempt at writing a Unix clone in C, with the primary goal of running dash (the shell).
It has somewhat met that goal, currently being capable of somewhat running dash, although the `fork` syscall is heavily broken at the moment
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
