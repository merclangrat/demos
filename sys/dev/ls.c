/*
* ls.c - Debug version of LS/XL-style serial line driver for DEMOS/2.9BSD
*/

#include "h/ls.h"
#if	NLS > 0
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/file.h>

/* -------- Device register layout -------- */
struct lsregs {
    u_short rcsr; /* Receiver Control/Status - 0176570 */
    u_short rbuf; /* Receiver Data           - 0176572 */
    u_short tcsr; /* Transmitter Control/Status - 0176574 */
    u_short tbuf; /* Transmitter Data            - 0176576 */
};

#define LSADDR ((struct lsregs *) 0176570)

/* -------- Receiver status bits -------- */
#define XL_RDPE   0100000  /* bit 15: parity error */
#define XL_OVR    0010000  /* bit 12: overflow */
#define XL_DSC    0000200  /* bit 7: data available */
#define XL_DIE    0000100  /* bit 6: RX interrupt enable */
#define XL_STE    0000001  /* bit 0: stop bit error */

/* -------- Transmitter status bits -------- */
#define XLXCSR_TRDY 0000200 /* bit 7: TX buffer empty */
#define XLXCSR_TIE  0000100 /* bit 6: TX interrupt enable */
#define XLXCSR_MM   0000004 /* bit 2: maintenance loopback */
#define XLXCSR_BRK  0000001 /* bit 0: force break */

/* -------- Local state -------- */
struct tty ls_tty;
struct clist ls_inq;     /* staging queue from ISR */
int ls_ichars = 0;       /* number of chars pending in ls_inq (and/or a head char) */

int lsstart();

/* -------- Open routine -------- */
lsopen(dev, flag)
dev_t dev;
int flag;
{
    register struct tty *tp;
    int s;

    if (minor(dev) != 0) {
        u.u_error = ENXIO;
        return;
    }

    tp = &ls_tty;
    tp->t_addr = (caddr_t)LSADDR;
    tp->t_state |= CARR_ON; /* always consider carrier present */
    tp->t_oproc = lsstart;
    tp->t_line = DFLT_LDISC; /* VERY important */
    tp->t_ispeed = tp->t_ospeed = B9600;
    tp->t_flags = ANYP | RAW | TANDEM;
    /* tp->t_local |= LINTRUP; */

#ifdef LSDEBUG
    printf("LS: open start (dev=%d, minor=%d) tp=%o t_line=%d t_state=%o\n",
           dev, minor(dev), tp, tp->t_line, tp->t_state);
#endif

    /* Enable RX interrupts */
    LSADDR->rcsr |= XL_DIE;
    ttychars(tp);
    ttyopen(dev, tp);

#ifdef LSDEBUG
    printf("LS: after ttyopen: u.u_error=%d, tp->t_state=%o, t_ispeed=%d t_ospeed=%d\n",
           u.u_error, tp->t_state, tp->t_ispeed, tp->t_ospeed);

    if (u.u_error) 
        printf("LS: ttyopen failed, u.u_error=%d\n", u.u_error);

#endif

}

/* -------- Close routine -------- */
lsclose(dev, flag)
dev_t dev;
int flag;
{
    register struct tty *tp = &ls_tty;
#ifdef	LSDEBUG
    printf("LS: close (minor %d)\n", minor(dev));
#endif
    ttyclose(tp);
    /* Disable interrupts */
    LSADDR->rcsr &= ~XL_DIE;
    LSADDR->tcsr &= ~XLXCSR_TIE;
}

/* -------- Read/write/ioctl wrappers -------- */
lsread(dev)
dev_t dev;
{
    register struct tty *tp;
    tp = &ls_tty;
#ifdef	LSDEBUG
    printf("LS: read()\n");
#endif
    (*linesw[tp->t_line].l_read)(tp);
#ifdef	LSDEBUG
    printf("LS: l_read called, inq=%d, canq=%d\n",
            tp->T_rawq.c_cc,tp->T_canq.c_cc);
#endif
}

lswrite(dev)
dev_t dev;
{
    register struct tty *tp;
    tp = &ls_tty;
#ifdef	LSDEBUG
    printf("LS: write()\n");
#endif
    (*linesw[tp->t_line].l_write)(tp);
}

lsioctl(dev, cmd, addr, flag)
dev_t dev;
int cmd;
caddr_t addr;
int flag;
{
    register struct tty *tp = &ls_tty;
    int s;

#ifdef	LSDEBUG
    printf("LS: ioctl(cmd=%o)\n", cmd);
#endif

    switch(ttioctl(tp, cmd, addr, flag)) {
        case TIOCSETN:
        case TIOCSETP:
        case TIOCSETA:
        case TIOCSETB:
            break; 
        case TIOCSBRK: /* set break */
#ifdef	LSDEBUG
            printf("LS: set break\n");
#endif
            LSADDR->tcsr |= XLXCSR_BRK;
            break;
        case TIOCCBRK: /* clear break */
#ifdef	LSDEBUG
            printf("LS: clear break\n");
#endif
            LSADDR->tcsr &= ~XLXCSR_BRK;
            break;
        case 0:
            break;
        default:
            u.u_error = ENOTTY;
    }
}

/* -------- Start routine -------- */
lsstart(tp)
register struct tty *tp;
{
    register int c;

    /* If line is paused/busy/timing, don't touch hardware */
    if (tp->t_state & (TIMEOUT|BUSY|TTSTOP))
        return;

    /* UART must report ready before we stuff a byte */
    if ((LSADDR->tcsr & XLXCSR_TRDY) == 0)
        return;

    /* If queue is low, wake any sleepers (writers) */
    if (tp->t_outq.c_cc <= TTLOWAT(tp)) {
        if (tp->t_state & ASLEEP)
            wakeup((caddr_t)&tp->t_outq);
    }

    /* Nothing to send? we're done (lstint will disable TIE if needed) */
    if ((c = getc(&tp->t_outq)) < 0)
        return;

#ifdef LSDEBUG
    printf("LS: start TX char 0%o\n", c & 0377);
#endif

    /* Write the byte, mark busy, and ensure TX interrupts are enabled */
    LSADDR->tbuf = c & 0377;
    tp->t_state |= BUSY;
    LSADDR->tcsr |= XLXCSR_TIE;
}

/* -------- Interrupt: receiver -------- */
lsrint()
{
        register struct tty *tp = &ls_tty;
        register int c, status;

        status = LSADDR->rcsr;

#ifdef LSDEBUG
        printf("LS: RX interrupt (status=%o)\n", status);
#endif

        if ((status & XL_DSC) == 0)
                return;

        /* Fetch the byte as soon as possible */
        c = LSADDR->rbuf & 0377;

        if (status & XL_RDPE) printf("LS: parity error\n");
        if (status & XL_STE)  printf("LS: stop bit error\n");

        /* If hardware overflow flagged, drop the byte (like KL) */
        if (status & XL_OVR) {
                printf("LS: hardware overflow (rcsr=%o)\n", status);
                return;
        }
        /*
         * Stage into a clist first. If another char is already staged,
         * just enqueue and leave quickly. This keeps hard IRQ short.
         */
        if (ls_ichars++) {
                if (putc(c, &ls_inq)) {
                        /* putc returned non-zero => queue full, undo count */
                        ls_ichars--;
#ifdef LSDEBUG
                        printf("LS: RX queue overflow (ls_ichars=%d)\n", ls_ichars);
#endif
                }
#ifdef LSDEBUG
                else {
                        printf("LS: staged (queued) c=%o, ls_ichars=%d, q_cc=%d\n",
                               c, ls_ichars, ls_inq.c_cc);
                }
#endif
                return;
        }

        /*
         * No backlog: deliver this char immediately, then
         * drain any queued chars under spl1() the KL way.
         */
        {
                register int s, delivered, cc;
                s = spl1();

#ifdef LSDEBUG
                printf("LS: deliver head c=%o (pre q_cc=%d)\n", c, ls_inq.c_cc);
#endif
                (*linesw[tp->t_line].l_input)(c, tp);
                delivered = 1;
                splx(s);
                ls_ichars--;

                /* Now drain whatever accumulated in ls_inq while we were busy */
drain_again:
                delivered = 0;
                while (ls_ichars > 0 && ls_inq.c_cc > 0) {
                        s = spl1();
                        c = getc(&ls_inq);      /* returns 0..255 or -1 */
                        splx(s);
                        if (c < 0) break;

#ifdef LSDEBUG
                        cc = ls_inq.c_cc;
                        printf("LS: drain c=%o (post q_cc=%d)\n", c, cc);
#endif
                        (*linesw[tp->t_line].l_input)(c, tp);
                        ls_ichars--;
                        delivered = 1;
                }

                /* If more arrived during draining, loop once more (like KL) */
                if (ls_ichars > 0 && delivered)
                        goto drain_again;

                /* Safety: reset counter if things got weird */
                if (ls_inq.c_cc == 0 && ls_ichars != 0)
                        ls_ichars = 0;
        }
}

/* -------- Interrupt: transmitter -------- */
lstint()
{
    register struct tty *tp = &ls_tty;

#ifdef LSDEBUG
    printf("LS: TX interrupt\n");
#endif

    /* We just finished sending one char */
    tp->t_state &= ~BUSY;

    /* Feed the next byte (ttstart will call lsstart) */
    ttstart(tp);

    /* If nothing got started (queue empty), turn off TX interrupts */
    if ((tp->t_state & BUSY) == 0)
        LSADDR->tcsr &= ~XLXCSR_TIE;

    /* Writers may be sleeping on low-water */
    if ((tp->t_state & ASLEEP) && tp->t_outq.c_cc <= TTLOWAT(tp))
        wakeup((caddr_t)&tp->t_outq);
}

#endif
