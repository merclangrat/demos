#       Makefile for kernel (overlay version)
#

# global definitions
DEFS =  -DKERNEL

# SPLFIX should be one of :splfix.movb, :splfix.mtps or :splfix.spl
SPLFIX=:splfix.mtps

# include file search path
INCLUDE= /usr/include

CPPFLAGS= ${DEFS} -I. -I${INCLUDE}
CFLAGS= -O ${CPPFLAGS} -V
CC =    cc
CPP =   /lib/cpp -P
AS =    as -V
LD =    ld

#  shorthand to avoid running make out of space
C =     ${CC} -S ${CFLAGS}
E =     ed - < ../conf/${SPLFIX}
A =     ${AS} - -o
I =     ${INCLUDE}

OFILE = demos

# move nami.o into the base segment if possible
# and (for 85) should be in the same segment
#
OV1 =
VARS= hk.o tm.o main.o sys5.o ipc5.o\
      sem.o fcntl.o msg.o\
      dw.o ls.o # rx.o my.o
VARS1=ureg.o sys1.o mem.o dkbad.o dkleave.o sys3.o pipe.o acct.o\
      prf.o partab.o sys.o vsys.o machdep.o nami.o fio.o alloc.o iget.o\
      kl.o tty.o ioctl.o ttynew.o text.o malloc.o bio.o subr.o\
      sys2.o syslocal.o sys4.o prim.o
BASE=sigjcl.o slp.o sysent.o clock.o trap.o rdwri.o dsort.o

#       boot.o must be loaded in the base well above 0 data
CONFOBJ = l.o mch.o c.o ioconf.o boot.o


#       l.o MUST be loaded first, param.o MUST be loaded last

# FOR OVERLAY KERNEL:
${OFILE}: ${BASE} ${CONFOBJ} ${VARS} ${VARS1} ${OV1} param.o Makefile\
	  ../conf/o2x checksys
	sh newvers.sh
	${CC} -c vers.c
	${LD} -n -X -v ${CONFOBJ} -Q ${VARS1} ${VARS}  ${OV1} -L ${BASE} vers.o param.o
#	-if [ `nm mch.o|sed -n 's/^\([^ ]*\) a IPK_XKOV/\1/p'` != "000000" ] ;\
#		then ../conf/o2x < a.out > $(OFILE) ; \
#		else mv a.out $(OFILE) ; \
#	fi
	mv a.out $(OFILE)
	-rm -f a.out
	size ${OFILE}
	./checksys ${OFILE}

d${OFILE}: vers.o ${BASE} ${CONFOBJ} ${VARS} ${VARS1} ${OV1} param.o Makefile ../conf/o2x checksys
	${LD} -X -n -v ${CONFOBJ} -Q ${VARS1} ${VARS}  ${OV1} -L ${BASE} vers.o param.o
	-if [ `nm mch.o|sed -n 's/^\([^ ]*\) a IPK_XKOV/\1/p'` != "000000" ] ;\
		then ../conf/o2x < a.out > $(OFILE) ; \
		else mv a.out $(OFILE) ; \
	fi
	-rm -f a.out
	size ${OFILE}
	./checksys ${OFILE}

../conf/o2x: ../conf/o2x.c ${I}/a.out.h ${I}/stdio.h
	${CC} -I. -I${INCLUDE} -O -o ../conf/o2x ../conf/o2x.c

## FOR NONOVERLAY KERNEL:
#${OFILE}: ${BASE} ${CONFOBJ} ${VARS} ${OV1} param.o Makefile checksys
#        sh newvers.sh
#        ${CC} -c vers.c
#        ${LD} -X -i -o ${OFILE} ${CONFOBJ} ${VARS} ${OV1} ${BASE} vers.o param.o
#        size ${OFILE}
#        ./checksys ${OFILE}
#
#tdemos:
#        ${LD} -X -i -o ${OFILE} ${CONFOBJ} ${VARS} ${OV1} ${BASE} vers.o param.o
#        size ${OFILE}
#        ./checksys ${OFILE}

install: demos
	cp h/localopts.h h/whoami.h /usr/include/sys
	cd ../ps ; make install clean
	mv /demos /demos.old
	cp demos /demos

h/whoami.h: h/localopts.h
	touch $@

h/space.h: h/video.h
	touch $@

${I}/sys/sem.h: ${I}/sys/ipc.h
	touch $@

${I}/sys/tty.h: ${I}/sys/ioctl.h ${I}/sgtty.h
	touch $@

${I}/sys/user.h: ${I}/errno.h ${I}/sys/fperr.h ${I}/sys/dir.h ${I}/a.out.h
	touch $@

h/param.h: h/whoami.h ${I}/signal.h ../include/psw.h ${I}/sys/types.h
h/param.h: ../include/iopage.h ${I}/sys/param.h
	touch $@

vers.o: version newvers.sh
	sh newvers.sh
	${CC} -c vers.c

version:
	cat /dev/null > version

mch.o: assym.s ../sys/mch1.s ../sys/mch2.s
	cat ../sys/mch1.s ../sys/mch2.s | ${CPP} ${CPPFLAGS} >mch.i
	${AS} -o mch.o assym.s mch.i
	-rm -f mch.i

l.o:
	${CPP} ${CPPFLAGS} ../conf/l.s >l.i
	${AS} -o l.o l.i
	-rm -f l.i

boot.o: ../sys/boot.s
	${CPP} ${CPPFLAGS} ../sys/boot.s >boot.i
	${AS} -o boot.o boot.i
	-rm -f boot.i

assym.s: ../conf/genassym.c h/whoami.h h/param.h ${I}/stdio.h
assym.s: ${I}/sys/dir.h ${I}/sys/inode.h ${I}/sys/user.h ${I}/sys/seg.h
assym.s: ${I}/sys/vm.h h/dz.h ${I}/sys/proc.h
	cc ${CFLAGS} ../conf/genassym.c
	./a.out > assym.s
	-rm -f a.out

param.o:
	${CC} -S ${CFLAGS} ../sys/param.c
	ed - param.s <../conf/:comm-to-bss
	${AS} -o param.o param.s
	-rm -f param.s

checksys: ../conf/checksys.c h/param.h ${I}/a.out.h ${I}/stdio.h
checksys: ${I}/sys/tty.h ${I}/sys/fcntl.h ${I}/ediag.h
	${CC} -I. -I${INCLUDE} -O -o checksys ../conf/checksys.c

clean:
	-rm -f *.[oibs]

#
# uSTROJSTWA RAZBITY PO [INAM. eSLI ODNO USTROJSTWO M.B. NA RAZNYH [INAH
# ILI \TO OBY^NYJ MA[INNO-NEZAWISIMYJ FAJL, TO EGO FAJL SIDIT W DCOMMON.
# eSLI WY HOTITE DOBAWITX NOWYJ FAJL, TO WPISYWAETE EGO W OPTIONAL.
#

OPTIONAL=
CONF= ../conf/c.c ioconf.c ../conf/l.s
SYS= ../sys/acct.c ../sys/alloc.c ../sys/clock.c ../sys/fio.c ../sys/iget.c\
 ../sys/ioctl.c ../sys/machdep.c ../sys/main.c ../sys/malloc.c ../sys/nami.c\
 ../sys/pipe.c ../sys/prf.c ../sys/prim.c ../sys/rdwri.c ../sys/sigjcl.c\
 ../sys/slp.c ../sys/subr.c ../sys/sys1.c ../sys/sys2.c ../sys/sys3.c\
 ../sys/sys4.c ../sys/sysent.c ../sys/syslocal.c ../sys/text.c ../sys/trap.c\
 ../sys/ureg.c ../sys/vsys.c ../sys/sys5.c ../sys/ipc5.c ../sys/fcntl.c\
 ../sys/msg.c ../sys/mch1.s ../sys/mch2.s ../sys/boot.s ../sys/sem.c\
 ../sys/param.c
DEV= ../dev/bio.c     ../dev/hk.c\
 ../dev/dkbad.c    \
 ../dev/dkleave.c ../dev/tm.c\
 ../dev/dsort.c  ../dev/kl.c   \
 ../dev/sys.c     \
 ../dev/tty.c  ../dev/dw.c\
 ../dev/partab.c    \
 ../dev/mem.c    \
 ../dev/ttynew.c ../dev/ls.c # ../dev/rx.c ../dev/my.c

depend:
	grep '^#include' ${CONF} ${SYS} ${DEV} ${OPTIONAL} |\
	awk -f ../conf/Awk.depend > \#makedep
	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' > \#eddep
	echo '$$r #makedep' >> \#eddep
	echo 'w' >> \#eddep
	cp Makefile Makefile.b
	ed - Makefile < \#eddep
	-rm -f \#eddep \#makedep
	echo '# DEPENDENCIES MUST END HERE' >> Makefile
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile
	echo '# see make depend above' >> Makefile

c.o: ../conf/c.c
		cp ../conf/c.c c.c
		${C} c.c
		${E} c.s
		${A} c.o c.s
		-rm -f c.[cs]

ioconf.o:
	${C} ioconf.c
	${E} ioconf.s
	${A} ioconf.o ioconf.s
	-rm -f ioconf.s

acct.o:
	${C} ../sys/acct.c
	${E} acct.s
	${A} acct.o acct.s
	-rm -f acct.s

alloc.o:
	${C} ../sys/alloc.c
	${E} alloc.s
	${A} alloc.o alloc.s
	-rm -f alloc.s

clock.o:
	${C} ../sys/clock.c
	${E} clock.s
	${A} clock.o clock.s
	-rm -f clock.s

fio.o:
	${C} ../sys/fio.c
	${E} fio.s
	${A} fio.o fio.s
	-rm -f fio.s

iget.o:
	${C} ../sys/iget.c
	${E} iget.s
	${A} iget.o iget.s
	-rm -f iget.s

ioctl.o:
	${C} ../sys/ioctl.c
	${E} ioctl.s
	${A} ioctl.o ioctl.s
	-rm -f ioctl.s

machdep.o:
	${C} ../sys/machdep.c
	${E} machdep.s
	${A} machdep.o machdep.s
	-rm -f machdep.s

main.o:
	${C} ../sys/main.c
	${E} main.s
	${A} main.o main.s
	-rm -f main.s

malloc.o:
	${C} ../sys/malloc.c
	${E} malloc.s
	${A} malloc.o malloc.s
	-rm -f malloc.s

nami.o:
	${C} ../sys/nami.c
	${E} nami.s
	${A} nami.o nami.s
	-rm -f nami.s

pipe.o:
	${C} ../sys/pipe.c
	${E} pipe.s
	${A} pipe.o pipe.s
	-rm -f pipe.s

prf.o:
	${C} ../sys/prf.c
	${E} prf.s
	${A} prf.o prf.s
	-rm -f prf.s

prim.o:
	${C} ../sys/prim.c
	${E} prim.s
	${A} prim.o prim.s
	-rm -f prim.s

rdwri.o:
	${C} ../sys/rdwri.c
	${E} rdwri.s
	${A} rdwri.o rdwri.s
	-rm -f rdwri.s

sem.o:
	${C} ../sys/sem.c
	${E} sem.s
	${A} sem.o sem.s
	-rm -f sem.s

sigjcl.o:
	${C} ../sys/sigjcl.c
	${E} sigjcl.s
	${A} sigjcl.o sigjcl.s
	-rm -f sigjcl.s

slp.o:
	${C} ../sys/slp.c
	${E} slp.s
	${A} slp.o slp.s
	-rm -f slp.s

subr.o:
	${C} ../sys/subr.c
	${E} subr.s
	${A} subr.o subr.s
	-rm -f subr.s

sys1.o:
	${C} ../sys/sys1.c
	${E} sys1.s
	${A} sys1.o sys1.s
	-rm -f sys1.s

sys2.o:
	${C} ../sys/sys2.c
	${E} sys2.s
	${A} sys2.o sys2.s
	-rm -f sys2.s

sys3.o:
	${C} ../sys/sys3.c
	${E} sys3.s
	${A} sys3.o sys3.s
	-rm -f sys3.s

sys4.o:
	${C} ../sys/sys4.c
	${E} sys4.s
	${A} sys4.o sys4.s
	-rm -f sys4.s

sysent.o:
	${C} ../sys/sysent.c
	${E} sysent.s
	${A} sysent.o sysent.s
	-rm -f sysent.s

syslocal.o:
	${C} ../sys/syslocal.c
	${E} syslocal.s
	${A} syslocal.o syslocal.s
	-rm -f syslocal.s

text.o:
	${C} ../sys/text.c
	${E} text.s
	${A} text.o text.s
	-rm -f text.s

trap.o:
	${C} ../sys/trap.c
	${E} trap.s
	${A} trap.o trap.s
	-rm -f trap.s

ureg.o:
	${C} ../sys/ureg.c
	${E} ureg.s
	${A} ureg.o ureg.s
	-rm -f ureg.s

vsys.o:
	${C} ../sys/vsys.c
	${E} vsys.s
	${A} vsys.o vsys.s
	-rm -f vsys.s

sys5.o:
	${C} ../sys/sys5.c
	${E} sys5.s
	${A} sys5.o sys5.s
	-rm -f sys5.s

ipc5.o:
	${C} ../sys/ipc5.c
	${E} ipc5.s
	${A} ipc5.o ipc5.s
	-rm -f ipc5.s

fcntl.o:
	${C} ../sys/fcntl.c
	${E} fcntl.s
	${A} fcntl.o fcntl.s
	-rm -f fcntl.s

msg.o:
	${C} ../sys/msg.c
	${E} msg.s
	${A} msg.o msg.s
	-rm -f msg.s

bio.o:
	${C} ../dev/bio.c
	${E} bio.s
	${A} bio.o bio.s
	-rm -f bio.s

dkbad.o:
	${C} ../dev/dkbad.c
	${E} dkbad.s
	${A} dkbad.o dkbad.s
	-rm -f dkbad.s

dkleave.o:
	${C} ../dev/dkleave.c
	${E} dkleave.s
	${A} dkleave.o dkleave.s
	-rm -f dkleave.s

dsort.o:
	${C} ../dev/dsort.c
	${E} dsort.s
	${A} dsort.o dsort.s
	-rm -f dsort.s
ls.o:
	$(C) ../dev/ls.c
	$(E) ls.s
	$(A) ls.o ls.s
	-rm -rf ls.s

kl.o:
	${C} ../dev/kl.c
	${E} kl.s
	${A} kl.o kl.s
	-rm -f kl.s

mem.o:
	${C} ../dev/mem.c
	${E} mem.s
	${A} mem.o mem.s
	-rm -f mem.s

partab.o:
	${C} ../dev/partab.c
	${E} partab.s
	${A} partab.o partab.s
	-rm -f partab.s

tty.o:
	${C} ../dev/tty.c
	${E} tty.s
	${A} tty.o tty.s
	-rm -f tty.s


ttynew.o:
	${C} ../dev/ttynew.c
	${E} ttynew.s
	${A} ttynew.o ttynew.s
	-rm -f ttynew.s

hk.o:
	${C} ../dev/hk.c
	${E} hk.s
	${A} hk.o hk.s
	-rm -f hk.s

hp.o:
	${C} ../dev/hp.c
	${E} hp.s
	${A} hp.o hp.s
	-rm -f hp.s

sys.o:
	${C} ../dev/sys.c
	${E} sys.s
	${A} sys.o sys.s
	-rm -f sys.s

#rx.o:
#	${C} ../dev/rx.c
#	${E} rx.s
#	${A} rx.o rx.s
#	-rm -f rx.s

#my.o:
#	${C} ../dev/my.c
#	${E} my.s
#	${A} my.o my.s
#	-rm -f my.s

tm.o:
	${C} ../dev/tm.c
	${E} tm.s
	${A} tm.o tm.s
	-rm -f tm.s

ildl.o:
	${C} ../dev/ildl.c
	${E} ildl.s
	${A} ildl.o ildl.s
	-rm -f ildl.s

dw.o:
	${C} ../dev/dw.c
	${E} dw.s
	${A} dw.o dw.s
	-rm -f dw.s


# DO NOT DELETE THIS LINE
# DEPENDENCIES MUST END HERE
c.o: ../conf/c.c
c.o: h/param.h ${I}/sys/systm.h ../include/buf.h ${I}/sys/tty.h ${I}/sys/conf.h
c.o: ${I}/sys/proc.h ${I}/sys/text.h ${I}/sys/dir.h ${I}/sys/user.h
c.o: ${I}/sys/file.h ${I}/sys/inode.h ${I}/sys/acct.h ../include/map.h
c.o: ${I}/sys/filsys.h ../include/mount.h h/bk.h h/dh.h h/dhu.h h/dj.h h/dn.h
c.o: h/dz.h h/hk.h h/hp.h h/hs.h h/ht.h h/lp.h h/pc.h h/pty.h h/mpy.h h/rk.h
c.o: h/rw.h h/dw.h h/my.h h/fd.h h/rl.h h/rm.h h/rp.h h/tm.h h/ts.h h/xp.h
c.o: h/rx.h h/ra.h h/vd.h h/ks.h h/kl.h h/wl.h h/kx.h h/ky.h h/video.h h/mk.h
ioconf.o: ioconf.c
ioconf.o: h/param.h ${I}/sys/systm.h h/dw.h h/fd.h h/hk.h h/hp.h h/hs.h h/kx.h
ioconf.o: h/ky.h h/my.h h/ra.h h/rk.h h/rl.h h/rm.h h/rp.h h/rw.h h/rx.h h/xp.h
ioconf.o: ../include/hpreg.h ../include/ra_info.h
l.o: ../conf/l.s
l.o: h/whoami.h ../include/trap.h ../include/iopage.m ../include/koverlay.h
l.o: h/dh.h h/dhu.h h/dj.h h/dn.h h/dw.h h/dz.h h/fd.h h/hk.h h/hp.h h/hs.h
l.o: h/ht.h h/kl.h h/ks.h h/kx.h h/ky.h h/lp.h h/mk.h h/my.h h/pc.h h/ra.h
l.o: h/rk.h h/rl.h h/rm.h h/rp.h h/rw.h h/rx.h h/tm.h h/ts.h h/video.h h/wl.h
l.o: h/xp.h
acct.o: ../sys/acct.c
acct.o: h/param.h ${I}/sys/systm.h ${I}/sys/acct.h ${I}/sys/dir.h
acct.o: ${I}/sys/user.h ${I}/sys/inode.h ${I}/sys/proc.h ${I}/sys/seg.h
alloc.o: ../sys/alloc.c
alloc.o: h/param.h ${I}/sys/systm.h ${I}/sys/filsys.h ../include/mount.h
alloc.o: ${I}/sys/fblk.h ${I}/sys/conf.h ../include/buf.h ${I}/sys/inode.h
alloc.o: ${I}/sys/ino.h ${I}/sys/dir.h ${I}/sys/user.h ../include/quota.h
clock.o: ../sys/clock.c
clock.o: h/param.h ${I}/sys/systm.h ../include/callout.h ${I}/sys/seg.h
clock.o: ${I}/sys/dir.h ${I}/sys/user.h ${I}/sys/proc.h ../include/reg.h
clock.o: ../include/psw.h ${I}/sys/text.h ${I}/sys/vm.h
fio.o: ../sys/fio.c
fio.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
fio.o: ${I}/sys/fcntl.h ${I}/sys/seg.h ${I}/sys/filsys.h ${I}/sys/file.h
fio.o: ${I}/sys/conf.h ${I}/sys/inode.h ../include/reg.h ${I}/sys/acct.h
fio.o: ${I}/sys/proc.h
iget.o: ../sys/iget.c
iget.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
iget.o: ${I}/sys/inode.h ${I}/sys/ino.h ${I}/sys/filsys.h ../include/mount.h
iget.o: ${I}/sys/conf.h ../include/buf.h ../include/quota.h ../include/inline.h
ioctl.o: ../sys/ioctl.c
ioctl.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
ioctl.o: ${I}/sys/tty.h ${I}/sys/proc.h ${I}/sys/inode.h ${I}/sys/file.h
ioctl.o: ../include/reg.h ${I}/sys/conf.h ${I}/sys/ioctl.h ${I}/sys/fcntl.h
machdep.o: ../sys/machdep.c
machdep.o: h/param.h ${I}/sys/systm.h ${I}/sys/acct.h ${I}/sys/dir.h
machdep.o: ${I}/sys/user.h ${I}/sys/inode.h ${I}/sys/proc.h ${I}/sys/seg.h
machdep.o: ../include/map.h ../include/reg.h ../include/buf.h ../include/psw.h
machdep.o: ${I}/sys/tty.h ${I}/sys/reboot.h ../include/uba.h
machdep.o: ../include/iopage.h ${I}/sys/fcntl.h h/video.h
main.o: ../sys/main.c
main.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
main.o: ${I}/sys/filsys.h ../include/mount.h ../include/map.h ${I}/sys/proc.h
main.o: ${I}/sys/inode.h ${I}/sys/seg.h ${I}/sys/conf.h ../include/buf.h
main.o: ${I}/sys/file.h h/space.h ${I}/sys/sem.h ${I}/sys/msg.h
malloc.o: ../sys/malloc.c
malloc.o: h/param.h ${I}/sys/systm.h ../include/map.h ${I}/sys/vm.h
nami.o: ../sys/nami.c
nami.o: h/param.h ${I}/sys/systm.h ${I}/sys/inode.h ${I}/sys/filsys.h
nami.o: ../include/mount.h ${I}/sys/dir.h ${I}/sys/user.h ../include/buf.h
nami.o: ../include/quota.h ${I}/sys/seg.h
pipe.o: ../sys/pipe.c
pipe.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
pipe.o: ${I}/sys/inode.h ${I}/sys/file.h ../include/reg.h ../include/inline.h
prf.o: ../sys/prf.c
prf.o: h/param.h ${I}/sys/systm.h ${I}/sys/filsys.h ../include/mount.h
prf.o: ${I}/sys/seg.h ../include/buf.h ${I}/sys/conf.h ../include/inline.h
prf.o: ${I}/sys/user.h ${I}/sys/proc.h ${I}/sys/reboot.h
prim.o: ../sys/prim.c
prim.o: h/param.h h/dh.h h/dz.h h/dhu.h ${I}/sys/tty.h ${I}/sys/systm.h
prim.o: ${I}/sys/conf.h ../include/buf.h ${I}/sys/seg.h ../include/map.h
prim.o: ../include/uba.h
rdwri.o: ../sys/rdwri.c
rdwri.o: h/param.h ${I}/sys/systm.h ${I}/sys/inode.h ${I}/sys/dir.h
rdwri.o: ${I}/sys/user.h ../include/buf.h ${I}/sys/conf.h ${I}/sys/file.h
sigjcl.o: ../sys/sigjcl.c
sigjcl.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
sigjcl.o: ${I}/sys/proc.h ${I}/sys/inode.h ../include/reg.h ../include/psw.h
sigjcl.o: ${I}/sys/text.h ${I}/sys/seg.h ${I}/sys/vm.h
slp.o: ../sys/slp.c
slp.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
slp.o: ${I}/sys/proc.h ${I}/sys/text.h ../include/map.h ${I}/sys/file.h
slp.o: ${I}/sys/inode.h ../include/buf.h ${I}/sys/seg.h ${I}/sys/vm.h
slp.o: ../include/inline.h
subr.o: ../sys/subr.c
subr.o: h/param.h ${I}/sys/systm.h ${I}/sys/conf.h ${I}/sys/inode.h
subr.o: ${I}/sys/dir.h ${I}/sys/user.h ../include/buf.h ../include/quota.h
sys1.o: ../sys/sys1.c
sys1.o: h/param.h ${I}/sys/systm.h ../include/map.h ${I}/sys/dir.h
sys1.o: ${I}/sys/user.h ${I}/sys/proc.h ../include/buf.h ../include/reg.h
sys1.o: ../include/psw.h ${I}/sys/inode.h ${I}/sys/seg.h ${I}/sys/acct.h
sys1.o: ${I}/sys/file.h ${I}/sys/wait.h h/space.h
sys2.o: ../sys/sys2.c
sys2.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
sys2.o: ../include/reg.h ${I}/sys/file.h ${I}/sys/inode.h ../include/quota.h
sys2.o: ${I}/sys/proc.h ../include/inline.h ${I}/sys/fcntl.h
sys3.o: ../sys/sys3.c
sys3.o: h/param.h ${I}/sys/systm.h ${I}/sys/ino.h ../include/reg.h
sys3.o: ../include/buf.h ${I}/sys/filsys.h ../include/mount.h ${I}/sys/dir.h
sys3.o: ${I}/sys/user.h ${I}/sys/inode.h ${I}/sys/file.h ${I}/sys/conf.h
sys3.o: ${I}/sys/stat.h ../include/inline.h ${I}/sys/ustat.h
sys4.o: ../sys/sys4.c
sys4.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
sys4.o: ../include/reg.h ${I}/sys/inode.h ${I}/sys/proc.h ${I}/sys/timeb.h
sys4.o: ../include/quota.h ${I}/sys/reboot.h ${I}/sys/filsys.h
sysent.o: ../sys/sysent.c
sysent.o: h/param.h ${I}/sys/systm.h ../include/quota.h h/space.h
syslocal.o: ../sys/syslocal.c
syslocal.o: h/param.h ${I}/sys/dir.h ${I}/sys/proc.h ${I}/sys/seg.h
syslocal.o: ${I}/sys/user.h ${I}/sys/systm.h ${I}/sys/inode.h ${I}/sys/ino.h
syslocal.o: ${I}/sys/file.h ${I}/sys/conf.h ${I}/sys/tty.h
syslocal.o: ${I}/sys/autoconfig.h
text.o: ../sys/text.c
text.o: h/param.h ${I}/sys/systm.h ../include/map.h ${I}/sys/dir.h
text.o: ${I}/sys/user.h ${I}/sys/proc.h ${I}/sys/text.h ${I}/sys/inode.h
text.o: ../include/buf.h ${I}/sys/seg.h
trap.o: ../sys/trap.c
trap.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
trap.o: ${I}/sys/proc.h ../include/reg.h ../include/psw.h ${I}/sys/seg.h
trap.o: ${I}/sys/vm.h ../include/trap.h ../include/inline.h ../include/iopage.h
trap.o: ../include/koverlay.h
ureg.o: ../sys/ureg.c
ureg.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
ureg.o: ${I}/sys/proc.h ${I}/sys/text.h ${I}/sys/seg.h
vsys.o: ../sys/vsys.c
vsys.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
vsys.o: ${I}/sys/proc.h
sys5.o: ../sys/sys5.c
sys5.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/inode.h
sys5.o: ${I}/sys/user.h ../include/quota.h
ipc5.o: ../sys/ipc5.c
ipc5.o: h/param.h ${I}/errno.h ${I}/sys/types.h ${I}/signal.h ${I}/sys/proc.h
ipc5.o: ${I}/sys/dir.h ${I}/sys/user.h ${I}/sys/ipc.h ${I}/sys/sem.h
ipc5.o: ${I}/sys/msg.h h/space.h
fcntl.o: ../sys/fcntl.c
fcntl.o: h/param.h ${I}/errno.h ${I}/sys/inode.h ${I}/sys/fcntl.h
fcntl.o: ${I}/sys/seg.h ${I}/sys/systm.h ${I}/sys/user.h ${I}/sys/file.h
fcntl.o: ${I}/sys/proc.h
msg.o: ../sys/msg.c
msg.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/types.h ${I}/signal.h
msg.o: ${I}/sys/user.h ${I}/sys/proc.h ../include/buf.h ${I}/errno.h
msg.o: ../include/map.h h/space.h ${I}/sys/ipc.h ${I}/sys/msg.h ${I}/sys/seg.h
msg.o: ../include/buf.h
mch1.o: ../sys/mch1.s
mch1.o: h/whoami.h h/ht.h h/tm.h h/ts.h ${I}/a.out.h ../include/cpu.m
mch1.o: ../include/trap.h ${I}/sys/reboot.h ../include/iopage.m
mch1.o: ../include/koverlay.h h/dz.h
boot.o: ../sys/boot.s
boot.o: h/whoami.h ../include/iopage.m
sem.o: ../sys/sem.c
sem.o: h/param.h ${I}/sys/types.h ${I}/sys/dir.h ../include/map.h ${I}/errno.h
sem.o: ${I}/signal.h h/space.h ${I}/sys/sem.h ${I}/sys/user.h ${I}/sys/proc.h
sem.o: ../include/buf.h ${I}/sys/ipc.h
param.o: ../sys/param.c
param.o: h/param.h ${I}/sys/systm.h ../include/buf.h ${I}/sys/tty.h
param.o: ${I}/sys/conf.h ${I}/sys/proc.h ${I}/sys/text.h ${I}/sys/dir.h
param.o: ${I}/sys/user.h ${I}/sys/file.h ${I}/sys/inode.h ${I}/sys/filsys.h
param.o: ../include/mount.h ../include/callout.h ${I}/sys/acct.h
param.o: ../include/map.h ${I}/sys/seg.h h/space.h
bio.o: ../dev/bio.c
bio.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
bio.o: ../include/buf.h ${I}/sys/conf.h ${I}/sys/proc.h ${I}/sys/seg.h
bio.o: ${I}/sys/vm.h ../include/uba.h
hk.o: ../dev/hk.c
hk.o: h/hk.h h/param.h ${I}/sys/systm.h ../include/buf.h ${I}/sys/conf.h
hk.o: ${I}/sys/dir.h ${I}/sys/user.h ../include/uba.h ../include/inline.h
hk.o: ../include/hkreg.h ../include/dkbad.h ../include/psw.h
dkbad.o: ../dev/dkbad.c
dkbad.o: h/param.h ../include/buf.h ../include/dkbad.h
dkleave.o: ../dev/dkleave.c
dkleave.o: h/param.h ../include/buf.h
#rx.o: ../dev/rx.c
#rx.o: h/rx.h h/param.h ../include/buf.h ${I}/sys/conf.h ${I}/sys/dir.h
#rx.o: ${I}/sys/user.h ../include/rxreg.h ${I}/sys/seg.h
#my.o: ../dev/my.c
#my.o: h/my.h h/param.h ../include/buf.h ${I}/sys/conf.h ${I}/sys/dir.h
#my.o: ../include/iopage.m ../include/myreg.h ${I}/sys/systm.h ${I}/sys/user.h
tm.o: ../dev/tm.c
tm.o: h/tm.h h/param.h ../include/buf.h ${I}/sys/dir.h ${I}/sys/conf.h
tm.o: ${I}/sys/file.h ${I}/sys/user.h ${I}/sys/systm.h ../include/tmreg.h
tm.o: ${I}/sys/mtio.h
dsort.o: ../dev/dsort.c
dsort.o: h/param.h ${I}/sys/systm.h ../include/buf.h
kl.o: ../dev/kl.c
kl.o: h/kl.h h/param.h ${I}/sys/conf.h ${I}/sys/dir.h ${I}/sys/user.h
kl.o: ${I}/sys/tty.h ${I}/sys/systm.h ${I}/sys/file.h ../include/klreg.h
kl.o: h/video.h
ls.o: ../dev/ls.c
ls.o: h/ls.h h/param.h $(I)/sys/conf.h $(I)/sys/dir.h $(I)/sys/user.h
ls.o: $(I)/sys/tty.h $(I)/sys/systm.h $(I)/sys/file.h
sys.o: ../dev/sys.c
sys.o: h/param.h ${I}/sys/conf.h ${I}/sys/dir.h ${I}/sys/user.h ${I}/sys/tty.h
sys.o: ${I}/sys/proc.h
tty.o: ../dev/tty.c
tty.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h ${I}/sys/tty.h
tty.o: ${I}/sys/proc.h ${I}/sys/inode.h ${I}/sys/file.h ../include/reg.h
tty.o: ${I}/sys/conf.h ../include/buf.h h/bk.h
dw.o: ../dev/dw.c
dw.o: h/dw.h h/param.h ${I}/sys/systm.h ../include/buf.h ${I}/sys/conf.h
dw.o: ${I}/sys/dir.h ${I}/sys/user.h ${I}/sys/seg.h ../include/iopage.h
dw.o: ../include/dwreg.h
partab.o: ../dev/partab.c
partab.o: h/param.h ${I}/sys/tty.h
mem.o: ../dev/mem.c
mem.o: h/param.h ${I}/sys/dir.h ${I}/sys/user.h ${I}/sys/conf.h ${I}/sys/seg.h
ttynew.o: ../dev/ttynew.c
ttynew.o: h/param.h ${I}/sys/systm.h ${I}/sys/dir.h ${I}/sys/user.h
ttynew.o: ${I}/sys/tty.h ${I}/sys/proc.h ${I}/sys/inode.h ${I}/sys/file.h
ttynew.o: ../include/reg.h ${I}/sys/conf.h ../include/buf.h

# DEPENDENCIES MUST END HERE
# IF YOU PUT STUFF HERE IT WILL GO AWAY
# see make depend above
