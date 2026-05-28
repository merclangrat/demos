/*
 * $Header: rx.c,v 22.3 90/11/12 19:14:02 root Exp $
 *
 * RX11 floppy disk driver
 * $Log:	rx.c,v $
 * Revision 22.3  90/11/12  19:14:02  root
 * Новые вещи для СМ1425 и перемещение include.
 * 
 * Revision 22.2  89/04/27  13:53:21  korotaev
 * Изменения связанные с небольшим перемещением каталогов и файлов
 * 
 * Revision 22.1  89/04/12  14:34:49  korotaev
 * "param.h" ==> <sys/param.h>
 * 
 * Revision 22.0  89/03/25  12:32:57  korotaev
 * Отсюда начинается версия 2.2
 * 
 * Revision 1.4  87/02/24  16:00:47  dmitry
 * Приоритеты срублены окончательно.
 * 
 * Revision 1.3  87/01/17  19:15:32  dmitry
 *      Срублены приоритеты на циклах чтения и записи
 * буфера сектора.
 *      Вызовы функций установки приоритета заменены на inline.
 *      Добавлена установка b_resid.
 *      Работа с припиской сделана в соответствии с протоколом
 *               ДЕМОС 2.
 * 
 * Revision 1.2  86/12/30  14:50:16  alex
 * Переставлены интерливинги так, чтобы 0 был совместим с "RT".
 * 
 * Revision 1.1  86/12/26  20:07:00  dmitry
 * Первая рабочая версия для Демос 2.0
 * 
 */

#include "h/rx.h"
#if     NRX > 0
#include <sys/param.h>
#include "../include/buf.h"
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include "../include/rxreg.h"
#include <sys/seg.h>

extern  struct rxdevice *RXADDR;

/* #define DEBUG        1 */
#define MAXRETRY  10
#define TTIME   60      /* Timeout time in HZ */
#define RRATE   6       /* Recall rate for rxreset */
#define RESETMAX 10     /* Max. num. of reset recalls before timeout */
			/* RESETMAX*RRATE/60 = time in second */

#define RXWAIT  while((rxaddr->rxcs & (RX_TREQ | RX_DONE)) == 0) ;

struct  rxtype {
	int     secsize;                /* size (bytes) one sector */
	int     secpertrk;              /* sectors/track */
	int     secperblk;              /* sectors/unix block */
	int     numsec;                 /* total sectors on device */
	int     numblks;                /* number of blocks on device */
	int     secincr;                /* increment to get next sector of block */
	int     intrlv;                 /* interleaving factor */
	int     skew;                   /* sector skew across tracks */
	int     trkoffset;              /* offset num of 1st sec */
} rxtypes[] = {
		128, 26, 4, 76*26, 494, 2, 13, 6, 1,    /* Terak or RT11 format */
		128, 26, 4, 77*26, 500, 2, 13, 6, 0,    /* our "standard" format */
		128, 26, 4, 77*26, 500, 1, 26, 0, 0,    /* IBM format */
		128, 26, 4, 76*26, 494, 6, 13, 0, 2     /* CP/M format */
};

int NRXTYP = sizeof rxtypes / sizeof (struct rxtype) ;

struct  rxstat {
	int     fminor;                 /* present request device number */
	struct  rxtype *ftype;          /* present request floppy type */
	int     bytect;                 /* remaining bytes (neg) */
	int     sector;                 /* absolute sector (0..numsec-1) */
	int     toutact;                /* timeout active */
	int     reqnum;                 /* floppy request number for timeout */
	caddr_t coreaddr;               /* current core address for transfer */
	unsigned coreblk;               /* block no. to put in seg. register */
} rxstat;

struct  buf     rxtab;
struct  buf     rrxbuf;

rxattach(addr, unit)
struct rxdevice *addr;
{
	if (unit != 0 )
		return(0);
	RXADDR = addr;
	return(1);
}

rxstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	extern int rxtimeout();

#ifdef DEBUG
	if(minor(abp->b_dev) == 127) {
		rxdebug();
		iodone(abp);
		_spl0();
		return;
	}
#endif
	bp = abp;
	/*
	 * test for valid request
	 */
	if(rxok(bp) == 0) {
		bp->b_flags |= B_ERROR;
		iodone(bp);
		return;
	}
	/*
	 * link buffer into device queue
	 */
	bp->av_forw = NULL;
	_spl5();
	if(rxtab.b_actf == NULL)
		rxtab.b_actf = bp;
	else
		rxtab.b_actl->av_forw = bp;
	rxtab.b_actl = bp;
	/*
	 * start rxtimeout if inactive
	 */
	if(rxstat.toutact == 0) {
		rxstat.toutact++;
		timeout(rxtimeout, (caddr_t)0, TTIME);
	}
	_spl0();
	/*
	 * start device if there is no current request
	 */
	if(rxtab.b_active == NULL)
		rxstart();
}

rxstart()
{
	register struct buf *bp;
	register struct rxdevice *rxaddr;
	register int dminor;

	rxaddr = RXADDR;
	/*
	 * if there is no request in queue...return
	 */
loop:   if((bp = rxtab.b_actf) == NULL)
		return;
	/*
	 * check if drive ready
	 */
	dminor = (minor(bp->b_dev) & 1) << 4;
	rxaddr->rxcs = dminor | RX_RDSTAT ;
	RXWAIT
	if((rxaddr->rxdb & RXES_READY) == 0) {
		printf("rx%d: Floppy not ready\n", minor(bp->b_dev));
		rxabtbuf();
		goto loop;
	}
	/*
	 * set active request flag
	 */
	rxtab.b_active++;
	rxsetup(bp);
	rxregs(bp);
}

rxintr()
{
	register struct buf *bp;
	register struct rxtype *rxt;
	register struct rxdevice *rxaddr;

	/*
	 * if there is no active request, false alarm.
	 */
	if( !( rxaddr = RXADDR ) || rxtab.b_active == NULL)
		return;
	rxtab.b_active = NULL;
	/*
	 * pointer to the buffer
	 */
	bp = rxtab.b_actf;
	/*
	 * pointer to a data structure describing
	 *  the type of device (i.e. interleaving)
	 */
	rxt = rxstat.ftype;
	/*
	 * check error bit
	 */
	if(rxaddr->rxcs & RX_ERR) {
		/*
		 * send read error register command
		 */
		short cssave = rxaddr->rxcs ;

		rxaddr->rxcs = RX_RDERR ;
		_spl1();
		RXWAIT
#ifndef UCB_DEVERR
		deverror(bp, rxaddr->rxcs, rxaddr->rxdb);
#else
		harderr(bp, "rx");
		printf("cs=%b, es=%b\n", cssave, RXCS_BITS,
			rxaddr->rxdb, RXES_BITS);
#endif
		/*
		 * make MAXRETRY retries on an error
		 */
		if(++rxtab.b_errcnt <= MAXRETRY) {
			rxreset(0);
			return;
		}
		/*
		 * return an i/o error
		 */
		bp->b_flags |= B_ERROR;
	} else {
		_spl1();
		/*
		 * if we just read a sector, we need to
		 *  empty the device buffer
		 */
		if(bp->b_flags & B_READ)
			rxempty();
		/*
		 * see if there is more data to read for
		 * this request.
		 */
		bp->b_resid = -( rxstat.bytect += rxt->secsize );
		rxstat.sector++;
		if(rxstat.bytect < 0 && rxstat.sector < rxt->numsec) {
			rxtab.b_active++;
			rxregs(bp);
			return;
		}
	}
	rxtab.b_errcnt = 0;
	/*
	 * unlink block from queue
	 */
	rxtab.b_actf = bp->av_forw;
	iodone(bp);
	/*
	 * start i/o on next buffer in queue
	 */
	rxstart();
}

rxreset(flag)
{
	register struct rxdevice *rxaddr;

	rxaddr = RXADDR;
	/*
	 * Check to see if this is a call from rxintr or
	 * a recall from timeout.
	 */
	if(flag) {
		if(rxaddr->rxcs & RX_DONE) {
			rxtab.b_active = 0;
			_spl1();
			rxstart();
		} else
			if(flag > RESETMAX) {
				printf("rx%d: Reset timeout\n", minor(rxtab.b_actf->b_dev));
				rxabtbuf();
				_spl1();
				rxstart();
			} else {
				timeout(rxreset, (caddr_t)flag+1, RRATE);
				/*
				 * Keep rxtimeout from timing out.
				 */
				rxstat.reqnum++;
			}
	} else {
		rxaddr->rxcs = RX_INIT;
		rxtab.b_active++;
		rxstat.reqnum++;
		timeout(rxreset, (caddr_t)1, 1);
	}
}

rxregs(abp)
struct buf *abp;
{
	register struct buf *bp;
	register struct rxtype *rxt;
	register struct rxdevice *rxaddr;
	int     dminor, cursec, curtrk;

	rxaddr = RXADDR;
	/*
	 * set device bit into proper position for command
	 */
	dminor = rxstat.fminor << 4;
	bp = abp;
	rxt = rxstat.ftype;
	/*
	 * increment interrupt request number
	 */
	rxstat.reqnum++;
	/*
	 * if command is read, initiate the command
	 */
	if(bp->b_flags & B_READ){
		RXWAIT
		rxaddr->rxcs = dminor | RX_INTR | RX_READ;
	} else {
		/*
		 * if command is write, fill the device buffer,
		 *   then initiate the write
		 */
		rxfill();
		RXWAIT
		rxaddr->rxcs = dminor | RX_INTR | RX_WRITE;
	}
	/*
	 * set track number
	 */
	curtrk = rxstat.sector / rxt->secpertrk;
	/*
	 * set sector number
	 */
	dminor = rxstat.sector % rxt->secpertrk;
	cursec = (dminor % rxt->intrlv) * rxt->secincr +
		(dminor / rxt->intrlv);
	/*
	 * add skew to sector
	 */
	cursec = (cursec + curtrk * rxt->skew)
		% rxt->secpertrk;
	/*
	 * massage registers
	 */
	RXWAIT
	rxaddr->rxdb = cursec + 1;
	RXWAIT
	rxaddr->rxdb = curtrk + rxt->trkoffset;
}

rxok(abp)
struct buf *abp;
{
	register struct buf *bp;
	register int type;
	register int dminor;

	/*
	 * get sub-device number and type from dminor device number
	 */
	dminor = minor((bp = abp)->b_dev);
	type = dminor >> 3;
	/*
	 * check for valid type
	 *
	 * check for block number within range of device
	 */
	if(!RXADDR || type >= NRXTYP ||
		bp->b_blkno >= (daddr_t)rxtypes[type].numblks)
		return(0);
	return(1);
}

rxsetup(abp)
struct buf *abp;
{
	register struct buf *bp;
	register int dminor;
	register struct rxtype *rxt;

	/*
	 * get dminor device number from buffer
	 */
	dminor = minor((bp = abp)->b_dev);
	/*
	 * get sub-device number from dminor device number
	 */
	rxt = rxstat.ftype = &rxtypes[dminor >> 3];
	/*
	 * make sure device number is only 0 or 1
	 */
	rxstat.fminor = dminor & 1;
	/*
	 * get byte count to read from buffer (negative number)
	 */
	rxstat.bytect = -bp->b_bcount;
	/*
	 * transform block number into the first
	 * sector to read on the floppy
	 */
	rxstat.sector = (int)bp->b_blkno * rxt->secperblk;
	/*
	 * set the core address to get or put bytes.
	 */
	rxstat.coreaddr = (caddr_t)(((unsigned int)bp->b_un.b_addr & 077) + 0120000);
	rxstat.coreblk = (unsigned int)((((unsigned int)bp->b_un.b_addr >> 6) & 01777) |
		(bp->b_xmem << 10));
}

rxempty()
{
	register int i;
	register char *cp;
	register int wc;
	segm    save5;
	struct rxdevice *rxaddr;

	rxaddr = RXADDR;
	/*
	 * start empty buffer command
	 */
	rxaddr->rxcs = RX_EMPTY ;
	/*
	 * get core address and byte count
	 */
	cp = rxstat.coreaddr;
	wc = ((rxstat.bytect <= -128)? 128 : -rxstat.bytect);
	/*
	 * save and set segmentation register.
	 */
	saveseg5( save5 );
	mapseg5( rxstat.coreblk, 01006 );
	/*
	 * move wc bytes from the device buffer
	 *   into the in core buffer
	 */
	for(i=wc; i>0; --i) {
		RXWAIT
		*cp++ = rxaddr->rxdb;
	}
	/*
	 * sluff excess bytes
	 */
	for(i=128-wc; i>0; --i) {
		RXWAIT
		cp = (char *)rxaddr->rxdb;
	}
	restorseg5( save5 );
	rxstat.coreblk += 2;
}

rxfill()
{
	register int i;
	register char *cp;
	register int wc;
	segm    save5;
	struct rxdevice *rxaddr;

	rxaddr = RXADDR;
	/*
	 * initiate the fill buffer command
	 */
	rxaddr->rxcs = RX_FILL ;
	/*
	 * get core address and byte count
	 */
	cp = rxstat.coreaddr;
	wc = ((rxstat.bytect <= -128)? 128 : -rxstat.bytect);
	/*
	 * save and set segmentation register.
	 */
	saveseg5( save5 );
	mapseg5( rxstat.coreblk, 01006 );
	/*
	 * move wc bytes from the in-core buffer to
	 *   the device buffer
	 */
	for(i=wc;  i>0; --i) {
		RXWAIT
		rxaddr->rxdb = *cp++;
	}
	/*
	 * sluff excess bytes
	 */
	for(i=128-wc; i>0; --i) {
		RXWAIT
		rxaddr->rxdb = 0;
	}
	restorseg5(save5);
	rxstat.coreblk += 2;
}

/*ARGSUSED*/
rxtimeout(dummy)
{
	static int prevreq;
	register struct buf *bp;

	bp = rxtab.b_actf;
	/*
	 * if the queue isn't empty and the current request number is the
	 * same as last time, abort the buffer and restart i/o.
	 */
	if(bp) {
		if(prevreq == rxstat.reqnum) {
			printf("rx%d: Floppy timeout\n", minor(bp->b_dev));
			rxabtbuf();
			_spl1();
			rxstart();
		}
		prevreq = rxstat.reqnum;
		timeout(rxtimeout, (caddr_t)0, TTIME);
	} else {
		/*
		 * if queue is empty, just quit and rxstrategy will
		 * restart us.
		 */
		rxstat.toutact = 0;
	}
}

rxabtbuf()
{
	register struct buf *bp;

	/*
	 * abort the current buffer with an error and unlink it.
	 */
	bp = rxtab.b_actf;
	bp->b_flags |= B_ERROR;
	rxtab.b_actf = bp->av_forw;
	rxtab.b_errcnt = 0;
	rxtab.b_active = NULL;
	iodone(bp);
}

rxread(dev)
dev_t	dev;
{
	bphysio(rxstrategy, &rrxbuf, dev, B_READ);
}

rxwrite(dev)
dev_t	dev;
{
	bphysio(rxstrategy, &rrxbuf, dev, B_WRITE);
}

#ifdef DEBUG
rxdebug() {
	register struct buf *bp;

	_spl5();
	printf("Debug:  &rxtab=%o, &rxstat=%o\n", &rxtab, &rxstat);
	printf(" rxstat:  fminor=%l, bytect=%l, sec=%l\n",
		rxstat.fminor, -rxstat.bytect, rxstat.sector);
	printf("   reqnum=%l\n", rxstat.reqnum);
	printf(" rxtab:  d_active=%l, buffers:\n", rxtab.b_active);
	for(bp=rxtab.b_actf; bp; bp=bp->av_forw)
		printf(" dev=%l/%l, blkno=%l, bcnt=%l, flags=%o.\n", major(bp->b_dev),
			minor(bp->b_dev), bp->b_blkno, -bp->b_bcount, bp->b_flags);
	putchar('\n');
}
#endif  DEBUG

#endif  NRX
