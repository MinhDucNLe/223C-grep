long	lseek(int, long, int);

char *getblock(unsigned int atl, int iof);
char *getline_(unsigned int tl);
int advance(char *lp, char *ep);
int backref(int i, char *lp);
void blkio(int b, char *buf, long (*iofcn)(int, void*, unsigned long));
int cclass(char *set, int c, int af);
void commands(void);
void compile(int eof);
void error(char *s);
int execute(unsigned int *addr);
int getchr(void);
int getnum(void);
unsigned int *address(void);
void newline(void);
void putchr(int ac);
void setwide(void);
void setnoaddr(void);