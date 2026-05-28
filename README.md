## DEMOS Restoration project

DEMOS is a version of UNIX, based on 2.9BSD, created in the USSR. 

***I am working on a website with much more information. If you need something now, or an image for quick start, just contact me.***

The original image was taken from [Serge Vakulenko's repository](https://github.com/sergev/vak-opensource). 
There's Version 3.0, built for DVK-4 (it can be run on SimH or DVK Emulator).

I am running DEMOS on FPGA, using [DVK simulator](https://github.com/forth32/dvk-fpga) 
To make it working, I had to recompile the kernel, and because sources of v. 3.0 are not available (I couldn't find them), I used sources of v. 2.2, thanks to Serge Vakulenko and other DEMOS engineers who preserved it.

The archive with sources (and a lot of DEMOS stuff) is [here](http://lizaurus.com/d22.tar.gz).

### Kernel sources in 'sys'

This subdirectory contains kernel sources (taken from the archive)  with my changes to make it working on the simulation and start from СМ 5408 (RK07, hk in DEMOS) drive simulation.

There is also original RCS repository tree. Original files are kept as '.orig'.

Also, I had to change the FPGA project itself (with no knowledge of Verilog), because it could not work with two RK devices. It seems to be designed for mostly RT-11 which reads and writes sequentially, UNIX is more complex. With my changes, DEMOS can read from one RK and write to another, also use RK and DW (Winchester drive). 
Floppy drives simulations are not working in DEMOS... yet, and this is FPGA simulation limitation, not system's.

But, I guess, because they are not real floppies and everything is on MicroSD card... it's not worth to spend more time on that.

**All Cyrillic comments are in KOI8-R, as they originally were!**

### LS - IRPS simulation driver

In case it's enabled, FPGA simulates IRPS (IFSS), a "current loop interface", and the board is visible as a USB serial port if connected to a PC. 
Kermit for RT-11 can be used out of the box for transferring files.

And this driver allows to use it as a serial port in DEMOS. It's not very reliable but working! 
It's possible to use Kermit and even UUCP, my DEMOS is sending and receiving emails and files.

**Disclaimer:** I used an AI tool to help me creating it ;)

### SPARC cross toolchain (WIP)

The archive also contains a cross-compiler toolchain to build DEMOS software on SPARC (originally on SunOS 4).

There were some parts missing: 

- stages of the compiler (c0, c1, c2) - they can be recompiled using a SunOS compiler (I used original compiler from SunOS 4, not newer SPARC compilers)
- 'ld' built for SPARC didn't support overlays, I made ld from 2.9BSD working on SPARC (using something from that toolchain's ld).

Solaris has a very good binary compatibility, then, I could run this toolchain on Solaris 10. 
It fails sometimes, but better than nothing :)

**There will be more here, stay tuned!**

