/*
**
** Эта штука добавляется в исходный файл перед
** запуском rcs               Давидов 04.28.88
**
** $Header: ranlib.c,v 1.2 89/01/23 22:45:40 abs Exp $
** $Log:	ranlib.c,v $
 * Revision 1.2  89/01/23  22:45:40  abs
 * расширена таблица символов
 * 
 * Revision 1.1  88/05/03  20:32:46  root
 * Initial revision
 *
*/

# ifndef        lint
/* NOXSTR  */
static char     Rcs_id[] = "$Header: ranlib.c,v 1.2 89/01/23 22:45:40 abs Exp $";
/* YESXSTR */
# endif         lint

/* AS: there are DEMOS headers in pdp11/, also with ifdefs */
#ifdef sparc
#include        <pdp11/ar.h>
#include        <pdp11/a.out.h>
#else
#include        <ar.h>
#include        <a.out.h>
#endif
#include        <stdio.h>
#define MAGIC   exp.a_magic
#ifdef sparc	/* AS: P3's SPARC magic */
#define GET16(v)   ((((v)&0377)<<8)+((v)>>8&0377))
#define PUT16(v)   ((((v)&0377)<<8)+((v)>>8&0377))
#define PUT32(v)   ((((long)(v)&0xFF0000)<<8)+((long)(v)>>8&0xFF0000)\
			+(((v)&0377)<<8)+((v)>>8&0377))
#define BADMAG  MAGIC!=GET16(A_MAGIC1) && MAGIC!=GET16(A_MAGIC2)  \
		&& MAGIC!=GET16(A_MAGIC3) && MAGIC!=GET16(A_MAGIC4)
#else
#define BADMAG  MAGIC!=A_MAGIC1 && MAGIC!=A_MAGIC2  \
		&& MAGIC!=A_MAGIC3 && MAGIC!=A_MAGIC4
#endif
struct  ar_hdr  arp;
struct  exec    exp;
FILE    *fi, *fo;
long    ftell();
#define TABSZ   1200
struct tab
{       char cname[8];
#ifdef sparc
	int cloc;
#else
	long cloc;
#endif
} tab[TABSZ];
int tnum;
int new;
char    tempnm[] = "__.SYMDEF";
char    firstname[17];

#ifdef sparc
char	arbin[] = "/home/mellorn/zaitcev/d22/bin/ar";
int	offdelta;
int	off, oldoff;
int	y;
#else
char	arbin[] = "ar";
long	offdelta;
long	off, oldoff;
#endif

main(argc, argv)
char **argv;
{
	char buf[256];

	--argc;
	while(argc--) {
		fi = fopen(*++argv,"r");
		if (fi == NULL) {
			fprintf(stderr, "nm: cannot open %s\n", *argv);
			continue;
		}
		off = sizeof(exp.a_magic);
		fread((char *)&exp, 1, sizeof(MAGIC), fi);
		  /* get magic no. */
		if (MAGIC != ARMAG)
		{       fprintf(stderr, "not archive: %s\n", *argv);
			continue;
		}
		fseek(fi, 0L, 0);
		new = tnum = 0;
		if(nextel(fi) == 0)
		{       fclose(fi);
			continue;
		}
		do {
			int o;
			register n;
			struct nlist sym;
			int nst;

			fread((char *)&exp, 1, sizeof(struct exec), fi);
			if (BADMAG)            /* archive element not in  */
				continue;      /* proper format - skip it */
#ifdef sparc
			o = (int)(PUT16(exp.a_text) + PUT16(exp.a_data));
#else
			o = (long)exp.a_text + exp.a_data;
#endif
			if ((exp.a_flag & 01) == 0)
				o *= 2;
			fseek(fi, o, 1);
#ifdef sparc
			n = PUT16(exp.a_syms) / sizeof(struct nlist);
#else
			n = exp.a_syms / sizeof(struct nlist);
#endif
			if (n == 0) {
				fprintf(stderr, "nm: %s-- no name list\n", arp.ar_name);
				continue;
			}
			while (--n >= 0) {
				fread((char *)&sym, 1, sizeof(sym), fi);
#ifdef sparc
				nst = GET16(sym.n_type);
#else
				nst = sym.n_type;
#endif
				if ((nst&N_EXT)==0) 
					continue;
				switch (nst&N_TYPE) {

				case N_UNDF:
					continue;

				default:
					stash(&sym);
					continue;
				}
			}
		} while(nextel(fi));
		new = fixsize();
		fclose(fi);
		fo = fopen(tempnm, "w");
		if(fo == NULL)
		{       fprintf(stderr, "can't create temporary\n");
			exit(1);
		}
#ifdef sparc
		/* AS: after all fixes we need to adjust to sparc format */
		for(y=0; y<tnum; y++)
			tab[y].cloc = PUT32(tab[y].cloc);
#endif
		fwrite((char *)tab, tnum, sizeof(struct tab), fo);
		fclose(fo);
		if(new)
			sprintf(buf, "%s rlb %s %s %s\n", arbin, firstname, *argv, tempnm);
		else    sprintf(buf, "%s rl %s %s\n", arbin, *argv, tempnm);
		if(system(buf))
			fprintf(stderr, "can't execute %s\n", buf);
		else fixdate(*argv);
		unlink(tempnm);
	}
	exit(0);
}

nextel(af)
FILE *af;
{
	unsigned size;
	register r;

	oldoff = off;
	fseek(af, off, 0);
	r = fread((char *)&arp, 1, sizeof(struct ar_hdr), af);  /* read archive header */
	if (r <= 0)
		return(0);
#ifdef sparc
	/* AS: not sure if it's correct, but seems to be */
	size = (PUT16(arp.ar_size[0]) << 16)+PUT16(arp.ar_size[1]);
	if (size & 1) size++;
	off = (int)ftell(af) + size;  /* offset to next element */
#else
	if (arp.ar_size & 1)
		++arp.ar_size;
	off = ftell(af) + arp.ar_size;  /* offset to next element */
#endif
	return(1);
}

stash(s) struct nlist *s;
{       int i;
	if(tnum >= TABSZ)
	{       fprintf(stderr, "symbol table overflow\n");
		exit(1);
	}
	for(i=0; i<8; i++) {
		tab[tnum].cname[i] = s->n_name[i];
	}
	tab[tnum].cloc = oldoff;
	tnum++;
}

fixsize()
{	int i;
	int size;
	offdelta = tnum * sizeof(struct tab) + sizeof(arp);
	off = sizeof(MAGIC);
	nextel(fi);
	if(strncmp(arp.ar_name, tempnm, 14) == 0)
	{	new = 0;
#ifdef sparc
	/* AS: not sure if it's correct, but seems to be */
	size = (PUT16(arp.ar_size[0]) << 16)+PUT16(arp.ar_size[1]);
	offdelta -= sizeof(arp) + size;
#else
	offdelta -= sizeof(arp) + arp.ar_size;
#endif
	}
	else
	{	new = 1;
		strncpy(firstname, arp.ar_name, 14);
	}
	for(i=0; i<tnum; i++)
		tab[i].cloc += offdelta;
	return(new);
}

/* patch time */
fixdate(s) char *s;
{
#ifdef sparc
	int timex;
	long time();
#else
	long timex, time();
#endif
	int fd;
	fd = open(s, 1);
	if(fd < 0)
	{	fprintf(stderr, "can't reopen %s\n", s);
		return;
	}
	timex = time(NULL) + 5; /* should be enough time */
	lseek(fd, sizeof(exp.a_magic) + ((char *)&arp.ar_date-(char *)&arp), 0);
#ifdef sparc
	timex = PUT32(timex);
#endif
	write(fd, (char *)&timex, sizeof(timex));
	close(fd);
}
