#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <glob.h>
#include "grep.h"

#define BLKSIZE  4096
#define NBLK  2047 
#define FNSIZE  128  
#define LBSIZE  4096
#define GBSIZE  256    
#define WRITE  1  /* const int EOF = -1; */

static int  peekc, lastc, given, ninbuf, io, pflag;
static int  vflag  = 1, listf, listn, col, tfile  = -1, tline, iblock  = -1, oblock  = -1, ichanged, nleft;
static int  names[26], anymarks, subnewa, fchange, wrapp;
static unsigned nlall = 128;  
static unsigned int  *addr1, *addr2, *dot, *dol, *zero;

static long  count;
static char  Q[] = "", T[] = "TMP", savedfile[FNSIZE], file[FNSIZE], linebuf[LBSIZE], inputbuf[GBSIZE];
static char  genbuf[LBSIZE], *nextip, *linebp, *globp, tmpXXXXX[50] = "/tmp/eXXXXX";
char *mkdtemp(char *);
static char  *tfname, ibuff[BLKSIZE], obuff[BLKSIZE];
static char  line[70],  *linp  = line;

int main(int argc, char *argv[]) { 
  	if (argc < 3) { fprintf(stderr, "Usage: ./grep PATTERN file(s)\n");  
		exit(1);
  	} else { zero = (unsigned *)malloc(nlall * sizeof(unsigned));  
		tfname = mkdtemp(tmpXXXXX);  init();
   		readfile(argv[2]); 
		search(argv[1]);
  	}
  	printf("\n");
  	printf("\nquitting...\n");
  	exit(1);
}
void search(const char* re) {  FILE *fp; 
	int fline = 0, out = 0; 
	char fbuf[GBSIZE];
   	fp = fopen(file, "r");
   	while(fgets(fbuf, GBSIZE, fp) != 0) {
        	if((strstr(fbuf, re)) != 0) { printf("%s", fbuf); 
			out++; 
		} 
		fline++; 
	}
}
void readfile(const char* fn) {  filename(fn);  
	init();
  	addr2 = zero;  
	if ((io = open((const char*)file, 0)) < 0) { lastc = '\n';  
		error(file); 
	}  
	setwide();  
	squeeze(0);
  	ninbuf = 0;
	append(getfile, addr2);  
	exfile();
}
int append(int (*f)(void), unsigned int *a) { unsigned int *a1, *a2, *rdot;  
	int nline, tl;  nline = 0;  dot = a;
  	while ((*f)() == 0) {
    		if ((dol-zero)+1 >= nlall) {  unsigned *ozero = zero;  
			nlall += 1024;
      			if ((zero = (unsigned *)realloc((char *)zero, nlall*sizeof(unsigned)))==NULL) {  error("MEM?"); 
			}
      			dot += zero - ozero;  dol += zero - ozero;
    		}
    		tl = putline();  
		nline++;  
		a1 = ++dol;  
		a2 = a1+1;  
		rdot = ++dot;
    		while (a1 > rdot) { *--a2 = *--a1; }  
  	}
  	return(nline);
}
int cclass(char *set, int c, int af) {  int n;  if (c == 0) { return(0); }  n = *set++;
  	while (--n) { if (*set++ == c) { return(af); } }  
	return(!af);
}
void error(char *s) {  int c;  
	wrapp = 0;  
	listf = 0;  
	listn = 0;  
	putchr_('?');  
	puts_(s);
  	count = 0;  
	lseek(0, (long)0, 2);  
	pflag = 0;  
	if (globp) { lastc = '\n'; }  
	globp = 0;  
	peekc = lastc;
  	if(lastc) { while ((c = getchr()) != '\n' && c != EOF) { } }
  	if (io > 0) { close(io);  
		io = -1; 
	};
}
void exfile(void) {  close(io);  
	io = -1;  
	if (vflag) {  putd();  
		puts_(" characters read"); 
	}  
}
void filename(const char* fn) { 
	strcpy(file, fn);  
	strcpy(savedfile, fn);  
}
char *getblock(unsigned int atl, int iof) {  int off = 0, bno = (atl/(BLKSIZE/2));  
  	if (bno >= NBLK) {  lastc = '\n';  
		error(T);  
	}  
	nleft = BLKSIZE - off;
  	if (bno==iblock) {  ichanged |= iof;  
		return(ibuff+off);  
	}  
	if (bno==oblock)  { return(obuff+off);  }
  	oblock = bno;  
	return(obuff+off);
}
int getchr(void) {  char c = 0;
  	if ((lastc=peekc)) {  peekc = 0;  
		return(lastc); 
	}
  	if (globp) {  
		if ((lastc = *globp++) != 0) { return(lastc); }  
		globp = 0;  
		return(EOF);  
	}
  	lastc = c&0177;  
	return(lastc);
}
int getfile(void) {  int c;  
	char *lp = linebuf, *fp = nextip;
  	do {
    		if (--ninbuf < 0) {
      		if ((ninbuf = (int)read(io, genbuf, LBSIZE)-1) < 0) {
        			if (lp>linebuf) { puts_("'\\n' appended");  
					*genbuf = '\n';  
				} else { return(EOF); }
      		}
      		fp = genbuf;  
			while(fp < &genbuf[ninbuf]) {  if (*fp++ & 0200) { break; }  }  
			fp = genbuf;
    		}
    		c = *fp++;  
		if (c=='\0') { continue; }
    		if (c&0200 || lp >= &linebuf[LBSIZE]) {  lastc = '\n';  
		error(Q);  
		}  
		count++;
  	} while (c != '\n');
  	*--lp = 0; 
	nextip = fp;  
	return(0);
}
void init(void) {  int *markp;  
	close(tfile);  
	tline = 2;
  	for (markp = names; markp < &names[26]; )  {  *markp++ = 0;  }
  	subnewa = 0;  
	anymarks = 0;  
	iblock = -1;  
	oblock = -1;  
	ichanged = 0;
  	close(creat(tfname, 0600));  
	tfile = open(tfname, 2);  
	dot = dol = zero;  
	memset(inputbuf, 0, sizeof(inputbuf));
}
void newline(void) {  int c;  
	if ((c = getchr()) == '\n' || c == EOF) { return; }
  	if (c == 'p' || c == 'l' || c == 'n') {  pflag++;
    		if (c == 'l') { listf++;  } else if (c == 'n') { listn++; }
    		if ((c = getchr()) == '\n') { return; }
  	}  
	error(Q);
}
void nonzero(void) { squeeze(1); }
void print(void) {  unsigned int *a1 = addr1; 
	nonzero();
  	do {  
		if (listn) {  count = a1 - zero;  
			putd();  
			putchr_('\t');  
		} 
	} 
	while (a1 <= addr2);
	dot = addr2;  
	listf = 0;  
	listn = 0;  
	pflag = 0;
}
void putchr_(int ac) {  char *lp = linp;  
	int c = ac;
  	if (listf) {
    		if (c == '\n') {
      			if (linp != line && linp[-1]==' ') {  *lp++ = '\\';  
				*lp++ = 'n';  }
    		} else {
      			if (col > (72 - 4 - 2)) {  col = 8;  
				*lp++ = '\\';  
				*lp++ = '\n';  
				*lp++ = '\t';  
			}  
			col++;
      			if (c=='\b' || c=='\t' || c=='\\') {
        			*lp++ = '\\';
       				if (c=='\b') { c = 'b'; 
				} else if (c=='\t') { c = 't'; }  
				col++;
      			} else if (c<' ' || c=='\177') {
       				*lp++ = '\\';   
				*lp++ = ((c >> 3) & 07) + '0';  
				c = (c & 07) + '0';  
				col += 3;
      			}
    		}
  	}
	linp = lp;
}
void putd(void) {  int r = count % 10;  
	count /= 10;  
	if (count) { putd(); }  
	putchr_(r + '0');  
}
void putfile(void) {  unsigned int *a1;  
	char *fp, *lp = 0;  
	int n, nib = BLKSIZE;  
	fp = genbuf;  
	a1 = addr1;
  	do {
    		for (;;) {
      			if (--nib < 0) {
        			n = (int)(fp-genbuf);
				nib = BLKSIZE-1;  
				fp = genbuf;
      			}
     			count++;  
			if ((*fp++ = *lp++) == 0) {  fp[-1] = '\n';  
				break; 
			}
    		}
  	} while (a1 <= addr2);
  	n = (int)(fp-genbuf);
}
int putline(void) {  char *bp, *lp;  
	int nl = 0;  
	unsigned int tl = 0;  
	fchange = 1;  
	lp = linebuf;
	bp = getblock(tl, WRITE);  
	nl = nleft;  
  	while ((*bp = *lp++)) {
    		if (*bp++ == '\n') {  *--bp = 0;  
			linebp = lp;  
			break;  
		}
    		if (--nl == 0) {  bp = getblock(tl += (BLKSIZE/2), WRITE);  
			nl = nleft;  
		}
  	}
  	nl = tline;  
	tline += (((lp - linebuf) + 03) >> 1) & 077776;  
	return(nl);
}
void puts_(char *sp) {  col = 0;  
	while (*sp) { putchr_(*sp++); } 
	 putchr_('\n');  
}
void setwide(void) { 
	if (!given) { addr1 = zero + (dol>zero);  
		addr2 = dol; 
	} 
}
void squeeze(int i) { if (addr1 < zero+i || addr2 > dol || addr1 > addr2) { error(Q); } }
