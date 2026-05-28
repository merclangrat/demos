/*
 * $Log:	rk.c,v $
 * Revision 22.4  90/11/12  19:12:12  root
 * Новые вещи для СМ1425 и перемещение include.
 * 
 * Revision 22.3  89/04/27  13:50:30  korotaev
 * Изменения связанные с небольшим перемещением каталогов и файлов
 * 
 * Revision 22.2  89/04/24  22:02:25  avg
 * Правлена плюха с RKWCHECK - забыли return в rkintr после захода
 * на rkstart для контрольного чтения.
 * 
 * Revision 22.1  89/04/12  14:31:42  korotaev
 * "param.h" ==> <sys/param.h>
 *
 * Revision 22.0  89/03/25  12:31:09  korotaev
 * Отсюда начинается версия 2.2
 *
 * Revision 1.8  89/01/18  21:49:22  dvolodin
 * Правлены рудневские заблуждения.
 *
 * Revision 1.7.2.1  88/11/17  16:25:29  dvolodin
 * Правлены рудневские заблуждения.
 *
 * Revision 1.6  87/07/02  11:23:16  avg
 * Поправлен RKWCHECK.
 *
 * Revision 1.5  87/02/05  20:49:56  avg
 * Убрана диагностика про rkwc для экономии места в ядре.
 *
 * Revision 1.4  87/02/04  17:33:56  alex
 * Вставлен режим XX_SPL - работать на низком приоритете
 *
 * Revision 1.3  86/12/06  00:00:59  alex
 * Сделан многоконтроллерный вариант драйвера.
 * Добавлен сброс диска при DRE и после 6 попыток обмена.
 * Сделан rkopen.
 *
 * Revision 1.1  86/04/19  17:54:51  avg
 * Initial revision
 * Многоконтроллерный вариант
 */

#include "h/rk.h"
#if     NRK > 0
#ifndef NRKC
#define NRKC 1
#endif
#include <sys/param.h>
#include <sys/systm.h>
#include "../include/buf.h"
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include "../include/rkreg.h"

#ifndef RK_SPL
#define INTR_ARGS(ps) /* */
#define SPLL(ps) /* */
#define SPLM     /* */
#else
#ifdef  MENLO_KOV
#define INTR_ARGS(ps)  ,i_sp, i_r1, i_ov, i_nps, i_r0, i_pc, ps
#else
#define INTR_ARGS(ps)  ,i_sp, i_r1, i_nps, i_r0, i_pc, ps
#endif
#include "../include/psw.h"
#define SPLL(ps) splx(((ps)&PS_IPL)?(ps):PS_BR1)
#define SPLM _spl5()
#endif

#define NRKBLK  4872    /* Число блоков на устройстве */
#define RKC(unit) (unit>>3)
#define RKU(unit) (unit&07)
#define RKRESETT  12

struct  rkdevice *RKADDR[NRKC];
#ifdef RK_WCHECK
char rkwcheck[NRKC];
#endif

struct  buf     rktab[NRKC];
#ifdef  UCB_DBUFS
struct  buf     rrkbuf[NRK][NRKC];
#else
struct  buf     rrkbuf[NRKC];
#endif

rkattach(addr, unit)
struct rkdevice *addr;
{
	if (unit >= NRKC )
		return(0);
	RKADDR[unit] = addr;
	return(1);
}

rkopen(dev,rw)
dev_t dev;
{
	register struct device *addr;
	int dd,dn,status;

	dd = minor(dev);
	dn = RKU(dd);
	if(dn >= NRK || RKC(dd) >= NRKC || !(addr = RKADDR[RKC(dd)] )) {
		u.u_error = ENXIO;
		return;
	}
	spl5();
	while((addr->rkcs&RKCS_RDY) == 0) spl0(), spl5();
	addr->rkda = dn<<13;
	status = addr->rkds;
	spl0();

	if((status&RK_DRY) == 0) {
		u.u_error = ENXIO;
		return;
	}
	if(rw && (status&RK_WPS)) {
		u.u_error = EROFS;
		return;
	}
}

rkstrategy(bp)
register struct buf *bp;
{
	register s;
	int      c;
	register struct buf *tabrk;

	c = RKC(minor(bp->b_dev));
	tabrk = &rktab[c];
	if (RKADDR[c] == (struct rkdevice *) NULL) {
		bp->b_error = ENXIO;
		goto errexit;
	}
	if (bp->b_blkno >= NRKBLK) {
		bp->b_error = EINVAL;
errexit:
		bp->b_flags |= B_ERROR;
		iodone(bp);
		return;
	}
#ifdef UNIBUS_MAP
	mapalloc(bp);
#endif
	bp->av_forw = (struct buf *)NULL;
	s = spl5();
	if(tabrk->b_actf == NULL)
		tabrk->b_actf = bp;
	else
		tabrk->b_actl->av_forw = bp;
	tabrk->b_actl = bp;
	if(tabrk->b_active == NULL)
		rkstart(c);
	splx(s);
}

rkstart(c)
{
	register struct rkdevice *rkaddr = RKADDR[c];
	register struct buf *bp;
	int com;
	register struct buf *tabrk = &rktab[c];
	daddr_t bn;
	int dn, cn, sn;

	if ((bp = tabrk->b_actf) == NULL)
		return;
	tabrk->b_active++;
	bn = bp->b_blkno;
	dn = minor(bp->b_dev);
	cn = bn / 12;
	sn = bn % 12;
	rkaddr->rkcs = RKCS_RESET | RKCS_GO;
	while((rkaddr->rkcs & RKCS_RDY) == 0)
	;
	rkaddr->rkda = (dn << 13) | (cn << 4) | sn;
	rkaddr->rkba = bp->b_un.b_addr;
	rkaddr->rkwc = -(bp->b_bcount >> 1);
	com = ((bp->b_xmem & 3) << 4) | RKCS_IDE | RKCS_GO;
	if(bp->b_flags & B_READ)
		com |= RKCS_RCOM;
	else {
		com |= RKCS_WCOM;
#ifdef RK_WCHECK
		if( rkwcheck[c] ) {
			com ^= (RKCS_WCHK^RKCS_WCOM);
			rkwcheck[c] = 0;
		} else
			rkwcheck[c] = 1;
#endif
	}
	rkaddr->rkcs = com;
#ifdef  RK_DKN
	dk_busy |= 1 << RK_DKN;
	dk_numb[RK_DKN]++;
	dk_wds[RK_DKN] += bp->b_bcount >> 6;
#endif  RK_DKN
}

rkintr(c INTR_ARGS(ps))
{
	register struct rkdevice *rkaddr = RKADDR[c];
	register struct buf *bp;
	register struct buf *tabrk = &rktab[c];
	int err;

	if (tabrk->b_active == NULL)
		return;
#ifdef  RK_DKN
	dk_busy &= ~(1 << RK_DKN);
#endif  RK_DKN
	bp = tabrk->b_actf;
	tabrk->b_active = NULL;
	SPLL(ps);

	if ((rkaddr->rkcs & RKCS_ERR) ||
	    ((bp->b_flags&B_PHYS) == 0 && rkaddr->rkwc) ) {
#ifdef RK_WCHECK
		rkwcheck[c] = 0;
#endif
		while ((rkaddr->rkcs & RKCS_RDY) == 0)
			;
		err = rkaddr->rker;
#ifndef APG_DIAG
#ifdef  UCB_DEVERR
		bp->b_dev <<= 3;
		harderr(bp, "rk");
		printf("er=%b ds=%b\n", rkaddr->rker, RKER_BITS,
		       rkaddr->rkds, RK_BITS);
		bp->b_dev >>= 3;
#else
		deverror(bp, rkaddr->rker, rkaddr->rkds);
#endif
#endif
		rkaddr->rkcs = RKCS_RESET | RKCS_GO;
		while((rkaddr->rkcs & RKCS_RDY) == 0)
			;
		if( ++tabrk->b_errcnt > 15 )
			goto error;
		SPLM;
		if ( tabrk->b_errcnt%7 == 6 || (err & RKER_DRE) ) {
			rkaddr->rkda = (RKU(minor(bp->b_dev))) << 13;
			rkaddr->rkcs = RKCS_GO|RKCS_DRESET;
			timeout(rkstart, (caddr_t)c, RKRESETT);
			return;
		}
		rkstart(c);
		return;
error:
#ifdef APG_DIAG
		deverror(bp, rkaddr->rker,rkaddr->rkds);
#endif
		bp->b_flags |= B_ERROR;
	}
	SPLM;
#ifdef RK_WCHECK
	if( rkwcheck[c] ) {
		rkstart(c);
		return;
	}
#endif
	tabrk->b_errcnt = 0;
	tabrk->b_actf = bp->av_forw;
	bp->b_resid = -(rkaddr->rkwc << 1);
	SPLL(ps);
	iodone(bp);
	SPLM;
	rkstart(c);
}

rkread(dev)
dev_t   dev;
{
	register struct buf *bufrrk = &rrkbuf[RKC(minor(dev))];
#ifdef  UCB_DBUFS
	register int unit = RKU(minor(dev));

	if (unit >= NRK)
		u.u_error = ENXIO;
	else
		physio(rkstrategy, &bufrrk[unit], dev, B_READ);
#else
	physio(rkstrategy, bufrrk, dev, B_READ);
#endif
}

rkwrite(dev)
dev_t   dev;
{
	register struct buf *bufrrk = &rrkbuf[RKC(minor(dev))];
#ifdef  UCB_DBUFS
	register int unit = RKU(minor(dev));

	if (unit >= NRK)
		u.u_error = ENXIO;
	else
		physio(rkstrategy, &bufrrk[unit], dev, B_WRITE);
#else
	physio(rkstrategy, bufrrk, dev, B_WRITE);
#endif
}
#endif  NRK
