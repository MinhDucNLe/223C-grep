#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <grep.h>

#define	BLKSIZE	4096
#define	NBLK	2047

#define	FNSIZE	128
#define	LBSIZE	4096
#define	ESIZE	256
#define	GBSIZE	256
#define	NBRA	5
#define	KSIZE	9

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12
#define	CBACK	14
#define	CCIRC	15

#define	STAR	01

char	Q[]	= "";
char	T[]	= "TMP";
#define	READ	0
#define	WRITE	1

int	peekc;
int	lastc;
char	savedfile[FNSIZE];
char	file[FNSIZE];
char	linebuf[LBSIZE];
char	rhsbuf[LBSIZE/2];
char	expbuf[ESIZE+4];
int	given;
unsigned int	*addr1, *addr2;
unsigned int	*dot, *dol, *zero;
char	genbuf[LBSIZE];
long	count;
char	*nextip;
char	*linebp;
int	ninbuf;
int	io;
int	pflag;

int	vflag	= 1;
int	oflag;
int	listf;
int	listn;
int	col;
char	*globp;
int	tfile	= -1;
int	tline;
char	*tfname;
char	*loc1;
char	*loc2;
char	ibuff[BLKSIZE];
int	iblock	= -1;
char	obuff[BLKSIZE];
int	oblock	= -1;
int	ichanged;
int	nleft;
char	WRERR[]	= "WRITE ERROR";
int	names[26];
int	anymarks;
char	*braslist[NBRA];
char	*braelist[NBRA];
int	nbra;
int	subnewa;
int	subolda;
int	fchange;
int	wrapp;
int	bpagesize = 20;
unsigned nlall = 128;

jmp_buf	savej;

int main(int argc, char **argv) {
	FILE *p;
	char line[100];
	char nline[100];

	p = fopen(argv[2],"r");

	if (!p) {
		printf("Error: %s: No such file or directory\n", argv[2]);
		printf("Returned status value 1\n");
		return 1;
	} else if (p == NULL) {
		printf("Error: Possible syntax errors or inaccessible files\n");
		printf("Returned status value 2\n");
		return 2;
	} else {
		while(fscanf(p , "%[^\n]\n" , line)!=EOF) {
			strcpy(nline,line);
			if(strstr(nline , argv[1]) !=NULL) {
				printf("%s\n", line);
			} else { continue; }
		}
	}
	fclose(p);
	return 0;
}

void commands(void) {
	unsigned int *a1;
	int c;
	int temp;
	char lastsep;

	for (;;) {
	if (pflag) {
		pflag = 0;
		addr1 = addr2 = dot;
	}
	c = '\n';
	for (addr1 = 0;;) {
		lastsep = c;
		a1 = address();
		c = getchr();
		if (c!=',' && c!=';')
			break;
		if (lastsep==',')
			error(Q);
		if (a1==0) {
			a1 = zero+1;
			if (a1>dol)
				a1--;
		}
		addr1 = a1;
		if (c==';')
			dot = a1;
	}
	if (lastsep!='\n' && a1==0)
		a1 = dol;
	if ((addr2=a1)==0) {
		given = 0;
		addr2 = dot;	
	}
	else
		given = 1;
	if (addr1==0)
		addr1 = addr2;
	switch(c) {

	case 'l':
		listf++;

	case EOF:
		return;

	}
	error(Q);
	}
}

unsigned int *address(void) {
	int sign;
	unsigned int *a, *b;
	int opcnt, nextopand;
	int c;

	nextopand = -1;
	sign = 1;
	opcnt = 0;
	a = dot;
	do {
		do c = getchr(); while (c==' ' || c=='\t');
		if ('0'<=c && c<='9') {
			peekc = c;
			if (!opcnt)
				a = zero;
			a += sign*getnum();
		} else switch (c) {
		case '$':
			a = dol;
			// fall through
		case '.':
			if (opcnt)
				error(Q);
			break;
		case '\'':
			c = getchr();
			if (opcnt || c<'a' || 'z'<c)
				error(Q);
			a = zero;
			do a++; while (a<=dol && names[c-'a']!=(*a&~01));
			break;
		case '?':
			sign = -sign;
			// fall through
		case '/':
			compile(c);
			b = a;
			for (;;) {
				a += sign;
				if (a<=zero)
					a = dol;
				if (a>dol)
					a = zero;
				if (execute(a))
					break;
				if (a==b)
					error(Q);
			}
			break;
		default:
			if (nextopand == opcnt) {
				a += sign;
				if (a<zero || dol<a)
					continue;       // error(Q);
			}
			if (c!='+' && c!='-' && c!='^') {
				peekc = c;
				if (opcnt==0)
					a = 0;
				return (a);
			}
			sign = 1;
			if (c!='+')
				sign = -sign;
			nextopand = ++opcnt;
			continue;
		}
		sign = 1;
		opcnt++;
	} while (zero<=a && a<=dol);
	error(Q);
	//NOTREACHED
	return 0;
}

int getnum(void) {
	int r, c;

	r = 0;
	while ((c=getchr())>='0' && c<='9')
		r = r*10 + c - '0';
	peekc = c;
	return (r);
}

void setwide(void) {
	if (!given) {
		addr1 = zero + (dol>zero);
		addr2 = dol;
	}
}

void setnoaddr(void) {
	if (given)
		error(Q);
}

void newline(void) {
	int c;

	if ((c = getchr()) == '\n' || c == EOF)
		return;
	if (c=='p' || c=='l' || c=='n') {
		pflag++;
		if (c=='l')
			listf++;
		else if (c=='n')
			listn++;
		if ((c=getchr())=='\n')
			return;
	}
	error(Q);
}

void error(char *s) {
	int c;

	wrapp = 0;
	listf = 0;
	listn = 0;
	putchr('?');
	puts(s);
	count = 0;
	lseek(0, (long)0, 2);
	pflag = 0;
	if (globp)
		lastc = '\n';
	globp = 0;
	peekc = lastc;
	if(lastc)
		while ((c = getchr()) != '\n' && c != EOF)
			;
	if (io > 0) {
		close(io);
		io = -1;
	}
	longjmp(savej, 1);
}

int getchr(void) {
	char c;
	if (lastc=peekc) {
		peekc = 0;
		return(lastc);
	}
	if (globp) {
		if ((lastc = *globp++) != 0)
			return(lastc);
		globp = 0;
		return(EOF);
	}
	if (read(0, &c, 1) <= 0)
		return(lastc = EOF);
	lastc = c&0177;
	return(lastc);
}

char *getline_(unsigned int tl) {
	char *bp, *lp;
	int nl;

	lp = linebuf;
	bp = getblock(tl, READ);
	nl = nleft;
	tl &= ~((BLKSIZE/2)-1);
	while (*lp++ = *bp++)
		if (--nl == 0) {
			bp = getblock(tl+=(BLKSIZE/2), READ);
			nl = nleft;
		}
	return(linebuf);
}

char *getblock(unsigned int atl, int iof) {
	int bno, off;
	
	bno = (atl/(BLKSIZE/2));
	off = (atl<<1) & (BLKSIZE-1) & ~03;
	if (bno >= NBLK) {
		lastc = '\n';
		error(T);
	}
	nleft = BLKSIZE - off;
	if (bno==iblock) {
		ichanged |= iof;
		return(ibuff+off);
	}
	if (bno==oblock)
		return(obuff+off);
	if (iof==READ) {
		if (ichanged)
			blkio(iblock, ibuff, (long (*)(int, void*, unsigned long))write);
		ichanged = 0;
		iblock = bno;
		blkio(bno, ibuff, read);
		return(ibuff+off);
	}
	if (oblock>=0)
		blkio(iblock, ibuff, (long (*)(int, void*, unsigned long))write);;
	oblock = bno;
	return(obuff+off);
}

void blkio(int b, char *buf, long (*iofcn)(int, void*, unsigned long)) {
	lseek(tfile, (long)b*BLKSIZE, 0);
	if ((*iofcn)(tfile, buf, BLKSIZE) != BLKSIZE) {
		error(T);
	}
}

void compile(int eof) {
	int c;
	char *ep;
	char *lastep;
	char bracket[NBRA], *bracketp;
	int cclcnt;

	ep = expbuf;
	bracketp = bracket;
	if ((c = getchr()) == '\n') {
		peekc = c;
		c = eof;
	}
	if (c == eof) {
		if (*ep==0)
			error(Q);
		return;
	}
	nbra = 0;
	if (c=='^') {
		c = getchr();
		*ep++ = CCIRC;
	}
	peekc = c;
	lastep = 0;
	for (;;) {
		if (ep >= &expbuf[ESIZE]) {
				expbuf[0] = 0;
				nbra = 0;
				error(Q);
		}
		c = getchr();
		if (c == '\n') {
			peekc = c;
			c = eof;
		}
		if (c==eof) {
			if (bracketp != bracket) {
				expbuf[0] = 0;
				nbra = 0;
				error(Q);
		}
			*ep++ = CEOF;
			return;
		}
		if (c!='*')
			lastep = ep;
		switch (c) {

		case '\\':
			if ((c = getchr())=='(') {
				if (nbra >= NBRA) {
					expbuf[0] = 0;
					nbra = 0;
					error(Q);
			}
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;
			}
			if (c == ')') {
				if (bracketp <= bracket) {
					expbuf[0] = 0;
					nbra = 0;
					error(Q);
			}
					
				*ep++ = CKET;
				*ep++ = *--bracketp;
				continue;
			}
			if (c>='1' && c<'1'+NBRA) {
				*ep++ = CBACK;
				*ep++ = c-'1';
				continue;
			}
			*ep++ = CCHR;
			if (c=='\n') {
				expbuf[0] = 0;
				nbra = 0;
				error(Q);
			}
				
			*ep++ = c;
			continue;

		case '.':
			*ep++ = CDOT;
			continue;

		case '\n':
			expbuf[0] = 0;
			nbra = 0;
			error(Q);

		case '*':
			if (lastep==0 || *lastep==CBRA || *lastep==CKET) {
			*ep++ = CCHR;
			*ep++ = c;
			}
			*lastep |= STAR;
			continue;

		case '$':
			if ((peekc=getchr()) != eof && peekc!='\n') {
			*ep++ = CCHR;
			*ep++ = c;
			}
			*ep++ = CDOL;
			continue;

		case '[':
			*ep++ = CCL;
			*ep++ = 0;
			cclcnt = 1;
			if ((c=getchr()) == '^') {
				c = getchr();
				ep[-2] = NCCL;
			}
			do {
				if (c=='\n') {
					expbuf[0] = 0;
					nbra = 0;
					error(Q);
				}
				if (c=='-' && ep[-1]!=0) {
					if ((c=getchr())==']') {
						*ep++ = '-';
						cclcnt++;
						break;
					}
					while (ep[-1]<c) {
						*ep = ep[-1]+1;
						ep++;
						cclcnt++;
						if (ep>=&expbuf[ESIZE]) {
							expbuf[0] = 0;
							nbra = 0;
							error(Q);
						}
					}
				}
				*ep++ = c;
				cclcnt++;
				if (ep >= &expbuf[ESIZE]) {
					expbuf[0] = 0;
					nbra = 0;
					error(Q);
				}
			} while ((c = getchr()) != ']');
			lastep[1] = cclcnt;
			continue;
		}
	}
}

int execute(unsigned int *addr) {
	char *p1, *p2;
	int c;

	for (c=0; c<NBRA; c++) {
		braslist[c] = 0;
		braelist[c] = 0;
	}
	p2 = expbuf;
	if (addr == (unsigned *)0) {
		if (*p2==CCIRC)
			return(0);
		p1 = loc2;
	} else if (addr==zero)
		return(0);
	else
		p1 = getline_(*addr);
	if (*p2==CCIRC) {
		loc1 = p1;
		return(advance(p1, p2+1));
	}
	// fast check for first character
	if (*p2==CCHR) {
		c = p2[1];
		do {
			if (*p1!=c)
				continue;
			if (advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while (*p1++);
		return(0);
	}
	// regular algorithm
	do {
		if (advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while (*p1++);
	return(0);
}

int advance(char *lp, char *ep) {
	char *curlp;
	int i;

	for (;;) switch (*ep++) {

	case CCHR:
		if (*ep++ == *lp++)
			continue;
		return(0);

	case CDOT:
		if (*lp++)
			continue;
		return(0);

	case CDOL:
		if (*lp==0)
			continue;
		return(0);

	case CEOF:
		loc2 = lp;
		return(1);

	case CCL:
		if (cclass(ep, *lp++, 1)) {
			ep += *ep;
			continue;
		}
		return(0);

	case NCCL:
		if (cclass(ep, *lp++, 0)) {
			ep += *ep;
			continue;
		}
		return(0);

	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CBACK:
		if (braelist[i = *ep++]==0)
			error(Q);
		if (backref(i, lp)) {
			lp += braelist[i] - braslist[i];
			continue;
		}
		return(0);

	case CBACK|STAR:
		if (braelist[i = *ep++] == 0)
			error(Q);
		curlp = lp;
		while (backref(i, lp))
			lp += braelist[i] - braslist[i];
		while (lp >= curlp) {
			if (advance(lp, ep))
				return(1);
			lp -= braelist[i] - braslist[i];
		}
		continue;

	case CDOT|STAR:
		curlp = lp;
		while (*lp++)
			;
				do {
			lp--;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);

	default:
		error(Q);

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep)
			;
		ep++;
				do {
			lp--;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);

	case CCL|STAR:
	case NCCL|STAR:
		curlp = lp;
		while (cclass(ep, *lp++, ep[-1]==(CCL|STAR)))
			;
		ep += *ep;
				do {
			lp--;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);
	}
}

int backref(int i, char *lp) {
	char *bp;

	bp = braslist[i];
	while (*bp++ == *lp++)
		if (bp >= braelist[i])
			return(1);
	return(0);
}

int cclass(char *set, int c, int af) {
	int n;

	if (c==0)
		return(0);
	n = *set++;
	while (--n)
		if (*set++ == c)
			return(af);
	return(!af);
}

char	line[70];
char	*linp	= line;

void putchr(int ac) {
	char *lp;
	int c;

	lp = linp;
	c = ac;
	if (listf) {
		if (c=='\n') {
			if (linp!=line && linp[-1]==' ') {
				*lp++ = '\\';
				*lp++ = 'n';
			}
		} else {
			if (col > (72-4-2)) {
				col = 8;
				*lp++ = '\\';
				*lp++ = '\n';
				*lp++ = '\t';
			}
			col++;
			if (c=='\b' || c=='\t' || c=='\\') {
				*lp++ = '\\';
				if (c=='\b')
					c = 'b';
				else if (c=='\t')
					c = 't';
				col++;
			} else if (c<' ' || c=='\177') {
				*lp++ = '\\';
				*lp++ =  (c>>6)    +'0';
				*lp++ = ((c>>3)&07)+'0';
				c     = ( c    &07)+'0';
				col += 3;
			}
		}
	}
	*lp++ = c;
	if(c == '\n' || lp >= &line[64]) {
		linp = line;
		write(oflag?2:1, line, lp-line);
		return;
	}
	linp = lp;
}