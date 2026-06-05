## SPARC cross-compile toolchain

It is included to the DEMOS archive.  
In this subdirectory, there are files I was working on.

Originally, the toolchain was created for SunOS 4 but I could run it on Solaris 10.

There are, in `bin` subdirectory of the archive:
- binutils (ar, as, cc, ld, nm, size, strip)
- tools for RCS (ci, co, rcs, rlog)

In `compl` subdirectory, there are sources modified for SPARC (using **ifdef sparc** for endianness because SPARC is big-endian).  
`cpp` binary is `compl/cpp` subdirectory.

What I had to build myself:
- C compiler stages (c0, c1, c2)
- patch ld to support overlays (-Z and -L flags), ported from 2.9BSD
- fix ranlib to create libraries' tables of contents.

There are hardcoded paths to search libraries and stages of the compiler:
- binaries in `/home/mellorn/zaitcev/d22/bin`
- include files are in `/home/mellorn/zaitcev/d22/include`
- libraries (.a) and compiler stages (cpp, c0, c1, c2) are in `/home/mellorn/zaitcev/d22/lib`

