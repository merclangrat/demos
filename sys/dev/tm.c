/*
 * $Log:	tm.c,v $
 * Revision 22.5  90/11/12  19:14:25  root
 * Новые вещи для СМ1425 и перемещение include.
 * 
 * Revision 22.4  89/05/23  13:29:40  korotaev
 * Втавлена возможность писать на ленту с заданным числом 
 * расширенных межблочных промежутков.
 * 
 * Revision 22.3  89/04/27  13:54:15  korotaev
 * Изменения связанные с небольшим перемещением каталогов и файлов
 * 
 * Revision 22.2  89/04/20  20:52:07  alex
 * Выключена проверка бита TE_SELR, поскольку на многих
 * машинах (Мера, некоторые Электроники-79) этот бит на
 * старшие магнитофоны контроллером не выставляется.
 * 
 * Revision 22.1  89/04/12  14:35:56  korotaev
 * "param.h" ==> <sys/param.h>
 * 
 * Revision 22.0  89/03/25  12:33:33  korotaev
 * Отсюда начинается версия 2.2
 * 
 * Revision 1.3  88/11/01  21:05:39  dvolodin
 * *** empty log message ***
 * 
 * Revision 1.2  87/01/04  22:26:38  avg
 * Многоконтроллерная версия.
 *
 * Revision 1.1  86/04/19  17:55:06  avg
 * Initial revision
 *
 */

#include "h/tm.h"
#if     NTM > 0
#include <sys/param.h>
#include "../include/buf.h"
#include <sys/dir.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/systm.h>
#include "../include/tmreg.h"
#ifdef  TM_IOCTL
#include <sys/mtio.h>
#endif

#ifndef NTMC
#define NTMC  1
#endif

struct  tmdevice *TMADDR[NTMC];

struct  buf     tmtab[NTMC];
struct  buf     ctmbuf[NTMC];
/*
 * Raw tape operations use rtmbuf.  The driver
 * notices when rtmbuf is being used and allows the user
 * program to continue after errors and read records
 * not of the standard length (BSIZE).
 */
struct  buf     rtmbuf[NTMC];

/*
 * Software state per tape transport:
 *
 * 1. A tape drive is a unique-open device:  we refuse opens when it is already.
 * 2. We keep track of the current position on a block tape and seek
 *    before operations by forward/back spacing if necessary.
 * 3. We remember if the last operation was a write on a tape, so if a tape
 *    is open read write and the last thing done is a write we can
 *    write a standard end of tape mark (two eofs).
 * 4. We remember the status registers after the last command, using
 *    them internally and returning them to the SENSE ioctl.
 * 5. We remember the last density the tape was used at.  If it is
 *    not a BOT when we start using it and we are writing, we don't
 *    let the density be changed.
 */

struct te_softc {
	char    sc_openf;
	char    sc_lastio;
	daddr_t sc_blkno;
	daddr_t sc_nxrec;
	u_short sc_erreg;
#ifdef  TM_IOCTL
	u_short sc_dsreg;
	short   sc_resid;
#endif
	u_short sc_dens;
#ifdef  TM_TIMEOUT
	short   sc_timo;
	short   sc_tact;
#endif
	int     sc_gapcnt;
} te_softc[NTMC][NTM];

/* значения флага sc_lastio */
#define LASTIOW   01    /* last op - была операцией записи */
#define LASTIOWXG 02    /* last op - запись с xtnd interrecord gap */

/*
 * States for tmtab.b_active, the state flag.
 * This is used to sequence control in the driver.
 */
#define SSEEK   1
#define SIO     2
#define SCOM    3
#define SREW    4

#define TEUNIT(dev)     (minor(dev) & 007)
#define TEC(dev)        ((minor(dev) & 070)>>3)
#define TEDRIVE(dev)    (minor(dev) & 077)

/* bits in minor device */
#define T_1600BPI       0100
#define T_NOREWIND      0200

#define INF     32760

#ifdef  TM_TIMEOUT
int     tmtimer ();
#endif

tmattach(addr, unit)
struct tmdevice *addr;
{
	/*
	 * This driver supports only one controller.
	 */
	if (unit < NTMC) {
		TMADDR[unit] = addr;
		return(1);
	}
	return(0);
}

/*
 * Open the device.  Tapes are unique open
 * devices so we refuse if it is alredy open.
 * We also check that the tape is available and
 * don't block waiting here:  if you want to wait
 * for a tape you should timeout in user code.
 */
tmopen(dev, flag)
register dev_t dev;
{
	int     s;
	u_short olddens, dens;
	register teunit = TEUNIT(dev);
	int      tec    = TEC(dev);
	register struct te_softc *sc = &te_softc[tec][teunit];

	if (teunit >= NTM || TMADDR[tec] == (struct tmdevice *) NULL) {
		u.u_error = ENXIO;
		return;
	}
	else
		if (sc->sc_openf) {
			u.u_error = EBUSY;
			return;
		}
	olddens = sc->sc_dens;
#ifdef  DDMT
	dens = TM_IE | TM_GO | (teunit << 8);
	if ((minor(dev) & T_1600BPI) == 0)
		dens |= TM_D800;
#else
	dens = TM_IE | TM_GO | TM_D800 | (teunit << 8);
#endif
	sc->sc_dens = dens;

	tmtab[0].b_flags |= B_TAPE;     /* KLUGE, bio LOOKS ONLY d_tab[0] */
get:
	tmcommand(dev, TM_SENSE, 1);
	if (sc->sc_erreg & TMER_SDWN) {
		sleep ((caddr_t) &lbolt, PZERO+1);
		goto get;
	}
	sc->sc_dens = olddens;

	if ((sc->sc_erreg & (TMER_TUR)) != (TMER_TUR)) {
		uprintf("tm%d: offline\n", tec*8 + teunit);
		u.u_error = EIO;
		return;
	}
	if ((flag & FWRITE) && (sc->sc_erreg & TMER_WRL)) {
		uprintf("tm%d: no ring\n", tec*8 + teunit);
		u.u_error = EIO;
		return;
	}
#ifdef  DDMT
	if ((sc->sc_erreg & TMER_BOT) == 0 && (flag & FWRITE) && dens != sc->sc_dens) {
		uprintf("tm%d: can't change density in mid-tape\n", tec*8 + teunit);
		u.u_error = EIO;
		return;
	}
#endif
	sc->sc_openf = 1;
	sc->sc_blkno = (daddr_t) 0;
	sc->sc_nxrec = (daddr_t) 65535;
	sc->sc_lastio = 0;
	sc->sc_dens = dens;
	sc->sc_gapcnt = 0;
#ifdef  TM_TIMEOUT
	s = spl6();
	if (sc->sc_tact == 0) {
		sc->sc_timo = INF;
		sc->sc_tact = 1;
		timeout(tmtimer, (caddr_t) dev, 5 * hz);
	}
	splx(s);
#endif
}

/*
 * Close tape device.
 *
 * If tape was open for writing or last operation was
 * a write, then write two EOF's and backspace over the last one.
 * Unless this is a non-rewinding special file, rewind the tape.
 * Make the tape available to others.
 */
tmclose(dev, flag)
register dev_t dev;
register flag;
{
	register struct te_softc *sc = &te_softc[TEC(dev)][TEUNIT(dev)];

	if( flag == FWRITE || (flag & FWRITE) && (sc->sc_lastio & LASTIOW) ){
		tmcommand(dev, TM_WEOF, 1);
		tmcommand(dev, TM_WEOF, 1);
		tmcommand(dev, TM_SREV, 1);
	}
	if ((minor(dev) & T_NOREWIND) == 0)
		/*
		 * 0 count means don't hang waiting for rewind complete.
		 * Rather ctmbuf stays busy until the operation completes
		 * preventing further opens from completing by
		 * preventing a TM_SENSE from completing.
		 */
		tmcommand(dev, TM_REW, 0);
	sc->sc_openf = 0;
}

/*
 * Execute a command on the tape drive
 * a specified number of times.
 */
tmcommand(dev, com, count)
register dev_t dev;
register u_short count;
{
	int     s;
	register struct buf *bp;

	bp = &ctmbuf[TEC(dev)];
	s = spl5();
	while (bp->b_flags & B_BUSY) {
		/*
		 * This special check is because B_BUSY never
		 * gets cleared in the non-waiting rewind case.
		 */
		if (bp->b_repcnt == 0 && (bp->b_flags & B_DONE))
			break;
		bp->b_flags |= B_WANTED;
		sleep((caddr_t)bp, PRIBIO);
	}
	bp->b_flags = B_BUSY | B_READ;
	splx(s);
	bp->b_dev = dev;
	bp->b_repcnt = -count;
	bp->b_command = com;
	bp->b_blkno = (daddr_t) 0;
	tmstrategy(bp);
	/*
	 * In case of rewind from close, don't wait.
	 * This is the only case where count can be 0.
	 */
	if (count == 0)
		return;
	iowait(bp);
	if (bp->b_flags & B_WANTED)
		wakeup((caddr_t)bp);
	bp->b_flags &= B_ERROR;
}

/*
 * Queue a tape operation.
 */
tmstrategy(bp)
register struct buf *bp;
{
	register s;
	register tec = TEC(bp->b_dev);

#ifdef UNIBUS_MAP
	if (bp != &ctmbuf[tec])
		mapalloc(bp);
#endif
	bp->av_forw = NULL;
	s = spl5();
	if (tmtab[tec].b_actf == NULL)
		tmtab[tec].b_actf = bp;
	else
		tmtab[tec].b_actl->av_forw = bp;
	tmtab[tec].b_actl = bp;
	/*
	 * If the controller is not busy, get
	 * it going.
	 */
	if (tmtab[tec].b_active == 0)
		tmstart(tec);
	splx(s);
}

tmstart(tec)
{
	daddr_t blkno;
	int cmd, teunit;
	register struct tmdevice *tmaddr = TMADDR[tec];
	register struct buf *bp;
	register struct te_softc *sc;

loop:
	if ((bp = tmtab[tec].b_actf) == NULL)
		return;
	teunit = TEUNIT(bp->b_dev);
	/*
	 * Record pre-transfer status (e.g. for TM_SENSE).
	 */
	sc = &te_softc[tec][teunit];
	tmaddr->tmcs = TM_DCLR;
	tmaddr->tmcs = teunit << 8;
	sc->sc_erreg = tmaddr->tmer;
#ifdef  TM_IOCTL
	sc->sc_dsreg = tmaddr->tmcs;
	sc->sc_resid = tmaddr->tmbc;
#endif
	/*
	 * Default is that the last command was NOT a write command;
	 * if we do a write command we will notice this in tmintr().
	 */
	sc->sc_lastio = 0;
	if (sc->sc_openf < 0 || (tmaddr->tmcs & TM_CUR) == 0) {
		/*
		 * Have had a hard error on a non-raw tape
		 * or the tape unit is now unavailable
		 * (e.g. taken off line).
		 */
		bp->b_flags |= B_ERROR;
		goto next;
	}
	if (bp == &ctmbuf[tec]) {
		/*
		 * Execute control operation with the specified count.
		 */
		if (bp->b_command == TM_SENSE) {
			goto next;
		}
#ifdef  TM_TIMEOUT
		/*
		 * Set next state; give 10 minutes to complete
		 * rewind or 10 seconds per iteration (minimum 60
		 * seconds and max 10 minutes) to complete other ops.
		 */
#else
		/*
		 * Set next state.
		 */
#endif
		if (bp->b_command == TM_REW) {
			if (tmaddr->tmer & TMER_BOT)
				goto next;
			tmtab[tec].b_active = SREW;
#ifdef  TM_TIMEOUT
			sc->sc_timo = 10 * hz;
#endif
		} else {
			tmtab[tec].b_active = SCOM;
#ifdef  TM_TIMEOUT
			sc->sc_timo = MIN(MAX(10 * bp->b_repcnt, hz), 10 * hz);
#endif
		}
		if (bp->b_command == TM_SFORW || bp->b_command == TM_SREV)
			tmaddr->tmbc = bp->b_repcnt;
		goto dobpcmd;
	}
	/*
	 * The following checks handle boundary cases for operation
	 * on non-raw tapes.  On raw tapes the initialization of
	 * sc->sc_nxrec by tmphys causes them to be skipped normally
	 * (except in the case of retries).
	 */
	if (dbtofsb(bp->b_blkno) > sc->sc_nxrec) {
		/*
		 * Can't read past known end-of-file.
		 */
		bp->b_flags |= B_ERROR;
		bp->b_error = ENXIO;
		goto next;
	}
	if (dbtofsb(bp->b_blkno) == sc->sc_nxrec && bp->b_flags & B_READ) {
		/*
		 * Reading at end of file returns 0 bytes.
		 * Buffer will be cleared (if written) in writei.
		 */
		bp->b_resid = bp->b_bcount;
		goto next;
	}
	if ((bp->b_flags & B_READ) == 0)
		/*
		 * Writing sets EOF.
		 */
		sc->sc_nxrec = dbtofsb(bp->b_blkno) + (daddr_t) 1;
	/*
	 * If the data transfer command is in the correct place,
	 * set up all registers and do the transfer.
	 */
	if ((blkno = sc->sc_blkno) == dbtofsb(bp->b_blkno)) {
		tmaddr->tmbc = -bp->b_bcount;
		if ((bp->b_flags & B_READ) == 0) {
			if( tmtab[tec].b_errcnt || sc->sc_gapcnt > 0 ){
				sc->sc_lastio |= LASTIOWXG;
				cmd = TM_WIRG;
			} else
				cmd = TM_WCOM;
		} else
			cmd = TM_RCOM;
		tmtab[tec].b_active = SIO;
		tmaddr->tmba = bp->b_un.b_addr;
		cmd = sc->sc_dens | ((bp->b_xmem & 03) << 4) | cmd;
#ifdef  TM_TIMEOUT
		sc->sc_timo = hz;       /* premature, but should serve */
#endif
		tmaddr->tmcs = cmd;
		return;
	}
	/*
	 * Tape positioned incorrectly;
	 * set to seek forward or backward to the correct spot.
	 * This happens for raw tapes only on error retries.
	 */
	tmtab[tec].b_active = SSEEK;
	if (blkno < dbtofsb(bp->b_blkno)) {
		bp->b_command = TM_SFORW;
		tmaddr->tmbc = (short) (blkno - dbtofsb(bp->b_blkno));
	} else {
		bp->b_command = TM_SREV;
		tmaddr->tmbc = (short) (dbtofsb(bp->b_blkno) - blkno);
	}
#ifdef  TM_TIMEOUT
	sc->sc_timo = MIN(MAX(10 * -tmaddr->tmbc, hz), 10 * hz);
#endif

dobpcmd:
	/*
	 * Do the command in bp.
	 */
	tmaddr->tmcs = (sc->sc_dens | bp->b_command);
	return;

next:
	/*
	 * Done with this operation due to error or
	 * the fact that it doesn't do anything.
	 * Dequeue the transfer and continue processing.
	 */
	tmtab[tec].b_errcnt = 0;
	tmtab[tec].b_actf = bp->av_forw;
	iodone(bp);
	goto loop;
}

/*
 * The interrupt routine.
 */
tmintr(tec)
{
	register struct tmdevice *tmaddr;
	register struct buf *bp;
	int teunit;
	int state;
	register struct te_softc *sc;

	tec &= 07;
	tmaddr = TMADDR[tec];
	if ((bp = tmtab[tec].b_actf) == NULL)
		return;
	teunit = TEUNIT(bp->b_dev);
	sc = &te_softc[tec][teunit];
	/*
	 * If last command was a rewind, and tape is still
	 * rewinding, wait for the rewind complete interrupt.
	 */
	if (tmtab[tec].b_active == SREW) {
		tmtab[tec].b_active = SCOM;
		if (tmaddr->tmer & TMER_RWS) {
#ifdef  TM_TIMEOUT
			sc->sc_timo = 10 * hz;           /* 10 minutes */
#endif
			return;
		}
	}
	/*
	 * An operation completed... record status.
	 */
#ifdef  TM_TIMEOUT
	sc->sc_timo = INF;
#endif
	sc->sc_erreg = tmaddr->tmer;
#ifdef  TM_IOCTL
	sc->sc_dsreg = tmaddr->tmcs;
	sc->sc_resid = tmaddr->tmbc;
#endif
	if ((bp->b_flags & B_READ) == 0){
		sc->sc_lastio |= LASTIOW;
		if( sc->sc_lastio & LASTIOWXG )
			sc->sc_gapcnt--;
	}
	state = tmtab[tec].b_active;
	tmtab[tec].b_active = 0;
	/*
	 * Check for errors.
	 */
	if (tmaddr->tmcs & TM_ERR) {
		while(tmaddr->tmer & TMER_SDWN)
			;                       /* await settle down */
		/*
		 * If we hit the end of the tape file, update our position.
		 */
		if (tmaddr->tmer & TMER_EOF) {
			tmseteof(bp);           /* set blkno and nxrec */
			state = SCOM;           /* force completion */
			/*
			 * Stuff bc so it will be unstuffed correctly
			 * later to get resid.
			 */
			tmaddr->tmbc = -bp->b_bcount;
			goto opdone;
		}
		/*
		 * If we were reading raw tape and the only error was that the
		 * record was too long, then we don't consider this an error.
		 */
		if (bp == &rtmbuf[tec] && (bp->b_flags & B_READ) &&
		    (tmaddr->tmer & (TMER_HARD | TMER_SOFT)) == TMER_RLE)
			goto ignoreerr;
		/*
		 * If error is not hard, and this was an i/o operation
		 * retry up to 8 times.
		 */
		if ((tmaddr->tmer & TMER_HARD) == 0 && state == SIO) {
			if (++tmtab[tec].b_errcnt < 13) {
				sc->sc_blkno++;
				goto opcont;
			}
		} else {
			/*
			 * Some of the foolish MT's (made in USSR)
			 * are unable correctly perform OFFLINE:
			 * it selects LOCAL, but indicates ILC !
			 */
			if( state == SCOM && bp->b_command == TM_OFFL )
				goto ignoreerr;
			/*
			 * Hard or non-i/o errors on non-raw tape
			 * cause it to close.
			 */
			if (sc->sc_openf > 0 && bp != &rtmbuf[tec])
				sc->sc_openf = -1;
		}
		/*
		 * Couldn't recover error
		 */
#ifdef  UCB_DEVERR
		printf("tm%d: err bn%D er=%b\n",
		   tec*8 + teunit, bp->b_blkno, sc->sc_erreg, TMER_BITS);
#else
		deverror(bp, sc->sc_erreg, 0);
#endif
		bp->b_flags |= B_ERROR;
		goto opdone;
	}
	/*
	 * Advance tape control finite state machine.
	 */
ignoreerr:
	switch (state) {
	case SIO:
		/*
		 * Read/write increments tape block number.
		 */
		sc->sc_blkno++;
		/*
		 * Если запрос на запись с xtnd interrecord gap,
		 * то повторить его.
		 */
		if( sc->sc_gapcnt > 0 && (sc->sc_lastio & LASTIOWXG) )
			goto opcont;
		goto opdone;
	case SCOM:
		/*
		 * For forward/backward space record update current position.
		 */
		if (bp == &ctmbuf[tec])
			switch (bp->b_command) {
			case TM_SFORW:
				sc->sc_blkno -= (daddr_t) (bp->b_repcnt);
				break;
			case TM_SREV:
				sc->sc_blkno += (daddr_t) (bp->b_repcnt);
				break;
			}
		goto opdone;
	case SSEEK:
		sc->sc_blkno = dbtofsb(bp->b_blkno);
		goto opcont;
	default:
		panic("tmintr");
	}
opdone:
	/*
	 * Reset error count and remove
	 * from device queue.
	 */
	tmtab[tec].b_errcnt = 0;
	tmtab[tec].b_actf = bp->av_forw;
	bp->b_resid = -tmaddr->tmbc;

	/*
	 * HACKED IN ORDER TO AVOID OUR LOCAL MT'S BUG:
	 * If rec length <= 2 bytes then assume it was tape marker
	 */
	if((bp->b_bcount - bp->b_resid) <= 2 &&
	    bp->b_bcount != bp->b_resid         ) {
		tmseteof(bp);           /* set blkno and nxrec */
		state = SCOM;           /* force completion */
		bp->b_resid = bp->b_bcount;
		printf("tm%d: lost mark\n", tec*8 + teunit);
	}

	iodone(bp);

opcont:
	tmstart(tec);
}

#ifdef  TM_TIMEOUT
tmtimer(dev)
register dev_t  dev;
{
	register s;
	register struct te_softc *sc = &te_softc[TEC(dev)][TEUNIT(dev)];

	if (sc->sc_timo != INF && (sc->sc_timo -= 5) < 0) {
		printf("tm%d: lost intr\n", TEDRIVE(dev));
		sc->sc_timo = INF;
		s = spl5();
		tmintr(TEC(dev));
		splx(s);
	}
	timeout(tmtimer, (caddr_t) dev, 5 * hz);
}
#endif

tmseteof(bp)
register struct buf *bp;
{
	int     tec = TEC(bp->b_dev);
	register struct tmdevice *tmaddr = TMADDR[tec];
	daddr_t bn = dbtofsb(bp->b_blkno);
	register struct te_softc *sc = &te_softc[tec][TEUNIT(bp->b_dev)];

	if (bp == &ctmbuf[tec]) {
		if (sc->sc_blkno > bn) {
			/* reversing */
			sc->sc_nxrec = bn - (daddr_t) (tmaddr->tmbc);
			sc->sc_blkno = sc->sc_nxrec;
		} else {
			/* spacing forward */
			sc->sc_blkno = bn + (daddr_t) (tmaddr->tmbc);
			sc->sc_nxrec = sc->sc_blkno - (daddr_t) 1;
		}
		return;
	}
	/* eof on read */
	sc->sc_nxrec = bn;
}

tmread(dev)
register dev_t dev;
{
	tmphys(dev);
	bphysio(tmstrategy, &rtmbuf[TEC(dev)], dev, B_READ);
}

tmwrite(dev)
register dev_t dev;
{
	tmphys(dev);
	bphysio(tmstrategy, &rtmbuf[TEC(dev)], dev, B_WRITE);
}

/*
 * Set up sc_blkno and sc_nxrec
 * so that the tape will appear positioned correctly.
 */
tmphys(dev)
dev_t dev;
{
	daddr_t a;
	register struct te_softc *sc = &te_softc[TEC(dev)][TEUNIT(dev)];

	a = dbtofsb(u.u_offset >> 9);
	sc->sc_blkno = a;
	sc->sc_nxrec = a + 1;
	if( sc->sc_gapcnt < 0 )
		sc->sc_gapcnt = 0;
}

#ifdef  TM_IOCTL
/*ARGSUSED*/
tmioctl(dev, cmd, addr, flag)
dev_t dev;
caddr_t addr;
{
	int     tec = TEC(dev);
	register struct buf *bp = &ctmbuf[tec];
	register struct te_softc *sc = &te_softc[tec][TEUNIT(dev)];
	register callcount;
	u_short fcount;
	struct mtop mtop;
	struct mtget mtget;
	/* we depend on the values and order of the MT codes here */
	static tmops[] = {TM_WEOF,TM_SFORW,TM_SREV,TM_SFORW,TM_SREV,TM_REW,TM_OFFL,TM_SENSE};

	switch (cmd) {

	case MTIOCTOP:  /* tape operation */
		if (copyin((caddr_t)addr, (caddr_t)&mtop, sizeof(mtop))) {
			u.u_error = EFAULT;
			return;
		}
		switch(mtop.mt_op) {
		case MTWEOF:
			callcount = mtop.mt_count;
			fcount = 1;
			break;
		case MTFSF:
		case MTBSF:
			callcount = mtop.mt_count;
			fcount = INF;
			break;
		case MTFSR:
		case MTBSR:
			callcount = 1;
			fcount = mtop.mt_count;
			break;
		case MTREW:
		case MTOFFL:
		case MTNOP:
			callcount = 1;
			fcount = 1;
			break;
		default:
			u.u_error = ENXIO;
			return;
		}
		if (callcount <= 0 || fcount <= 0) {
			u.u_error = ENXIO;
			return;
		}
		while (--callcount >= 0) {
			tmcommand(dev, tmops[mtop.mt_op], fcount);
			if ((mtop.mt_op == MTFSR || mtop.mt_op == MTBSR) &&
			    bp->b_resid) {
				u.u_error = EIO;
				break;
			}
			if ((bp->b_flags & B_ERROR) || sc->sc_erreg & TMER_BOT)
				break;
		}
		geterror(bp);
		return;
	case MTIOCGET:
		mtget.mt_erreg = sc->sc_erreg;
		mtget.mt_dsreg = sc->sc_dsreg;
		mtget.mt_resid = sc->sc_resid;
		mtget.mt_type = MT_ISTM;
		mtget.mt_gapcnt = -sc->sc_gapcnt;
		if (copyout((caddr_t)&mtget, addr, sizeof(mtget)))
			u.u_error = EFAULT;
		return;
	case MTIOCSET:  /* set same regim for nex op */
		if (copyin((caddr_t)addr, (caddr_t)&mtop, sizeof(mtop))) {
			u.u_error = EFAULT;
			return;
		}
		switch(mtop.mt_op) {
		case MTWXGAP:   /* write with xtnd interrecord gap */
			if( mtop.mt_count < 0 ){
				u.u_error = EINVAL;
				return;
			}
			sc->sc_gapcnt = mtop.mt_count;
			break;
		default:
			u.u_error = ENXIO;
			return;
		}
		return;
	default:
		u.u_error = ENXIO;
	}
}
#endif
#endif  NTM
