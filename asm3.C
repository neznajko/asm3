#include <stdio.h>  // printf
#include <unistd.h> // getopt
#include <libgen.h> // basename
#include <fcntl.h>  // open, read
#include <stdlib.h> // realloc
#include <string.h> // strcmp
using namespace std;
#include <forward_list>
#include <map>
#include <algorithm>
#include <iostream>
#include <list>
#define BIZ 4096 // Block size
int gas = 0; //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|GNU assembly flag
char **filenom; // command line file names
char *loadfile(char *filenom)
{
    int fd = open(filenom, 0);
    int n; // number of read bytes
    int size = 0; // realloc size
    char *buf = (char *) realloc(NULL, BIZ);
    char *ptr = buf; // read buffer pointer
    while((n = read(fd, ptr, BIZ)) == BIZ) {
	size += BIZ;
	buf = (char *) realloc(buf, size + BIZ);
	ptr = buf + size;
    } // Append end of token character and zero byte
    *(buf + size + n) = ' ';
    if (n == BIZ - 1) { 
	// after adding end of token character, at this point
	// there is no space left for the \0 byte, it's possible that
	// realloc takes care of this automaticaly but in any case
	// allocate space for one more byte
	size++;
	buf = (char *) realloc(buf, size + BIZ);
	*(buf + size + n) = '\0';
    }
    close(fd);
    return buf;
}
int spck(char c)
{
    return (c == ' ' || c == '\t');
}
#define clcknext() (c = *(++ibuf))
#define pushbuf(c) *(++obuf) = c
#define esck()					\
    for (; esc > 0; esc--)			\
	pushbuf('\n');	
#define skipspc()				\
    while (clcknext()) {			\
	if (spck(c) == 0) break;		\
    }						\
    if (c == 0) break;
// uskip_nasm (skip until)
#define uskip_nasm()				\
    while (clcknext()) {			\
	if (c == endc) {			\
	    if (*(ibuf-1) != '\\') break;	\
	}					\
	if (c == '\n') {			\
	    pushbuf('\n');			\
	}					\
    }						\
    if (endc == '\n') pushbuf('\n');		\
    if (c == 0) break;				\
    if (clcknext() == 0) break;			
int strck(char c)
{
    return (c == '\'' || c == '\"' || c == '`');
}
void nasm_post_loader(char *buf)
{
    char c; // buffer character
    char *ibuf = buf - 1;
    char *obuf = ibuf;
    int esc = 0; // escaped new line counter
    int endc; // ending characer (uskip_nasm)

    while (clcknext()) {
      spck:
    	if (spck(c)) { // space
	    esck();
	    pushbuf(' ');
	    skipspc();
    	}
	if (c == ';') { // line comment
	    esck();
	    endc = '\n';
	    uskip_nasm();
	    goto spck;
	} else if (strck(c)) { // string ck
	    esck();
	    endc = c;
	    uskip_nasm();
	    goto spck;
	} else { // \nl, nl and tkn character
	    if (c == '\\') { // probe for \nl
		if (*(ibuf+1) == '\n') { // peek next character
		    clcknext(); // update ibuf
		    esc++; // increment \nl counter
		    continue;
		}		
	    } else if (c == '\n') {
		esck();
	    }
	    pushbuf(c);
	}
    }
    pushbuf('\0'); // add zero byte
}
int gaslck(char c, char **ibuf)
{
    if (c == '#') return 1;
    if (c != '/') return 0;
    if (*(*ibuf+1) != '/') return 0; // peek next character
    ++(*ibuf); // update bufer pointer
    return 1;
}
// gcc will raise a warning unterminated string; newline inserted 
// but it will compile	    
#define skpGas()				\
    while (clcknext()) {			\
	if (c == '\n') {			\
	    pushbuf('\n');			\
	}					\
	if (c == endc) {			\
	    break;				\
	}					\
    }						\
    if (c == 0) break;				\
    if (clcknext() == 0) break;
#define skipblc()				\
    while (clcknext()) {			\
	if (c == '\n') {			\
	    pushbuf(c);				\
	    continue;				\
	}					\
	if (c != '*') continue;			\
	if (*(ibuf+1) == '/') {			\
	    ibuf++;				\
	    break;				\
	}					\
    }						\
    if (c == 0) break;				\
    if (clcknext() == 0) break;    
void gas_post_loader(char *buf)
{
    char c; // buffer character
    char *ibuf = buf - 1;
    char *obuf = ibuf;
    int endc; // ending character

    while (clcknext()) {
      spck:
    	if (spck(c)) { // space
	    pushbuf(' ');
	    skipspc();
    	}
	if (gaslck(c, &ibuf)) { // line comment
	    endc = '\n';
	    skpGas();
	    goto spck;
	} else if (c == '\"') { // string ck
	    endc = '\"';
	    skpGas();
	    goto spck;
	} else if (c == '/') { // block comment
	    if (*(ibuf+1) == '*') {
		ibuf++; // update buffer pointer
		// skip block comment
		skipblc();
	    }
	}
	pushbuf(c);
    }
    pushbuf('\0'); // add zero byte
}
struct token {
    char *str; // string
    struct token *next;
};
struct line {
    int ifile; // file index
    int linum; // line number
    struct token tknhead; // token list head
    struct line *next; // next line
};
struct line src;
struct line *ssp = &src; // src stack pointer
struct token *tsp; // tkn stack pointer
void consline(int ifile, int linum)
{
    struct line *p = (struct line *) calloc(1, sizeof *p);
    p->ifile = ifile; // set file index
    p->linum = linum; // set line number
    tsp = &(p->tknhead); // what a name
    ssp->next = p; // link
    ssp = p; // update src stack pointer
}
#define revind()				\
    do {					\
	if (c == '\n') {			\
	    nlc++;				\
	} else {				\
	    if (c != ' ') break;		\
	}					\
    } while (clcknext());			\
    if (c == 0) goto fiNiSh;
void constoken(char *buf)
{
    struct token *p = (struct token *) calloc(1, sizeof *p);
    p->str = buf;
    tsp->next = p; // link
    tsp = p; // update tkn stack pointer
}
void consrc(int ifile, char *buf)
{
    char c; // character
    int nlc = 1; // new line counter
    char *ibuf = buf - 1; 

    clcknext();
    revind();    
    consline(ifile, nlc);
    while (1) {
	do {
	    constoken(ibuf);
	    // zip
	    nlc = 0;
	    while (clcknext()) {
		if (c == ' ') {
		    break;
		} else if (c == '\n') {
		    nlc++;
		    break;
		}
	    }
	    *ibuf = 0;
	    if (clcknext() == 0) goto fiNiSh;
	    //
	    revind();
	} while (nlc == 0);
	nlc += ssp->linum; // revert
	consline(ifile, nlc);
    }
  fiNiSh:
    return;
}
void dumpsrc(void) 
{
    struct line *p = &src;
    struct token *q;

    while (p = p->next) {
	printf("%i, %i\n", p->ifile, p->linum);
	q = &(p->tknhead);
	while (q = q->next) 
	    printf(" %s ", q->str);
	printf("\n");
    }
}
typedef enum {CALL, MAC} mode;
// around here I'm switching from C to C++ and from char * to strings
// coz of the STL algorithms
forward_list<string> mactab;
forward_list<string> calltab;
typedef pair<string, mode> node;
map<node, struct line *> loctab;
#define skan(FILE, KEY)				\
    find((FILE).begin(), (FILE).end(), KEY)
forward_list<string> Kales[] = {
    {"call"},
    {"call", "callb", "calls", "callw", "calll", "callq", "callt"}
};
forward_list<string> Red[] = {
    {"ret"},
    {"ret", "retb", "rets", "retw", "retl", "retq", "rett"}
};
forward_list<string> global[] = {
    {"global"},
    {".global", ".globl"}
};
forward_list<string> macdef[] = {{"%macro"}, {".macro"}};
forward_list<string> endmac[] = {{"%endmacro"}, {".endm"}};
struct token *keyck(forward_list<string> key, struct line *p)
// ck if there is a token on the line which matches one of the
// keywords in the list. On success returns token pointer,
// otherwise NULL
{
    struct token *q = &(p->tknhead);
    
    while (q = q->next)
	if (skan(key, q->str) != key.end()) break;

    return q;	
}
void consmac(void)
// macro definition keyword should be the first on the line but because
// of the escaped new lines for Nasm macro name is either next token or
// the first token on the next line. Consturcts both mactab and loctab
{
    struct line *p = &src;
    struct token *q;

    while (p = p->next) {
	if ((q = keyck(macdef[gas], p)) == NULL) continue;
	q = q->next;
	if (q == NULL) { // Nasm \new line
	    p = p->next; // advance to next line
	    q = p->tknhead.next;
	}
	mactab.push_front(q->str);
	loctab[{q->str, MAC}] = p;
    }
}
void dump_mactab(void)
{
    cout << "mactab:\n";
    for (auto m: mactab)
	cout << "  " << m << endl;
}
void dump_loctab(void)
{
    struct line *p;
    cout << "loctab:\n";
    for (auto l: loctab) {
	p = l.second;
	cout << "  " << l.first.first << ' '
	     << l.first.second << ": "
	     << p << ' ';
	if (p) {
	    cout << "<"
		 << p->ifile << ','
		 << p->linum << ">" << endl;
	} else {
	    cout << "<,>\n";
	}
    }
}
// Both functions skan for call keyword and return pointer
// to the following token or NULL if not found
struct token *isgascall(struct line **p)
{
    struct token *q = keyck(Kales[gas], *p);

    if (q == NULL) return NULL;
    
    return q->next;
}
struct token *isnasmcall(struct line **p)
{
    struct token *q = keyck(Kales[gas], *p);

    if (q == NULL) return NULL;
    q = q->next;
    if (q == NULL) { // \new line
	*p = (*p)->next; // advance to next line
	q = (*p)->tknhead.next;
    }
    return q;
}
struct token * (*iscall[])(struct line **p) = {
    isnasmcall, isgascall };
void conscalltab(void)
// skans for call and global instr
{
    struct line *p = &src;
    struct token *q;

    while (p = p->next) {
	if ((q = keyck(global[gas], p)) == NULL) {
	    if ((q = iscall[gas](&p)) == NULL) continue;
	} else {
	    q = q->next;
	}
	if (skan(calltab, q->str) == calltab.end())
	    calltab.push_front(q->str);
    }
}
void dump_calltab(void)
{
    cout << "calltab:\n";
    for (auto c: calltab)
	cout << "  " << c << endl;
}
bool labelck(char *haystack, const char *needle)
// Gas allows labels as label_A:needle:label_B: etc. this unit is    
// considered as one token located at haystack, the pointer of needle
// is a string to char * entry from calltab
{
    char *s = strstr(haystack, needle);

    if (s == NULL) return false; // not found
    // ck if needle is followed by :
    size_t len = strlen(needle);
    if (*(s + len) != ':') return false; // negative
    // ck if needle is at the begining of haystack
    if (s == haystack) return true;
    // otherwise ck if it is  preceeded by :
    if (*(s - 1) == ':') return true;

    return false;
}
struct line *snoop_gas(string needle)
{
    struct line *p = &src;
    struct token *q;

    while (p = p->next) {
	q = p->tknhead.next;
	if (labelck(q->str, needle.c_str()))
	    return p;
    }
    return NULL;
}
struct line *snoop_nasm(string needle)
// Nasm label colons : are optional so we first ck for normal labels:
// and if not found than search for an exact match
{
    struct line *p = snoop_gas(needle);
    
    if (p) return p;

    p = &src;
    struct token *q;

    while (p = p->next) {
	q = p->tknhead.next;
	if (needle == q->str) return p;
    }
    return NULL;
}
struct token *ismacall(struct line *p)
// one can think of a macro az a user defined instruction hence
// we check if there is a macro call on the line
{
    return keyck(mactab, p);
}
bool calldef(struct line *p)
// call definition ck, this is like a reverse snoop.
{
    // we need to ck only the first token
    char *haystack = p->tknhead.next->str;
    
    for (auto c: calltab) {
	if (labelck(haystack, c.c_str())) return true;
    }
    // if Nasm ck for an exact match
    if (gas == 0) {
	for (auto c: calltab) {
	    if (haystack == c) return true;
	}
    }
    return false;
}
void usage(char *prognom)
{
    printf("Usage: %s -n name [-ghmr] file...\n", prognom);
    printf("  -n name       root name\n");
    printf("  -g            gas flag\n");
    printf("  -h            help\n");
    printf("  -m            set root mode to MAC\n");
    printf("  -r            reverse tree\n");
}
void (*post_loader[])(char *) = { nasm_post_loader, gas_post_loader };
struct line * (*snoop[])(string) = { snoop_nasm, snoop_gas };
void consloc(void)
// macro locations were figured by the consmac function here we
// construct only callee and boss function locations
{
    for (auto c: calltab)
	loctab[{c, CALL}] = snoop[gas](c);
}
void test_Zone(void)
{
    struct line *p = &src;

    while (p = p->next) {
	if (calldef(p)) {
	    cout << p->ifile << ' ' << p->linum << endl;
	}
    }
    exit(0);
}
typedef map<node, list<node>> tree;
tree str8; // straight caller callee tree
tree revs; // reverse callee caller tree
void bldmac3(node n, struct line *p)
{
    // there might be nested macros so if macdef s++ if endmac s--
    // end exit if s == 0
    int s = 1; // es counter
    struct token *q;
    node m;
    list<node> l;

    str8[n] = {}; // initialise n with 0 subtrees

    while (p = p->next) {
	if (keyck(macdef[gas], p)) { // ck if macdef
	    s++;
	    continue;
	}
	if (keyck(endmac[gas], p)) { // ck if endmac
	    if (--s == 0) return; // ve ar don!
	    continue;
	}
	if (s > 1) continue; // ignore nested macro calls
	if (q = iscall[gas](&p)) {
	    m = {q->str, CALL};
	} else if (q = ismacall(p)) {
	    m = {q->str, MAC};
	} else {
	    continue;
	}
	l = str8[n];
	if (skan(l, m) == l.end()) str8[n].push_back(m);
    }
}
void bldcall3(node n, struct line *p)
{
    struct token *q;
    node m;
    list<node> l;

    str8[n] = {}; // initialise n with 0 subtrees

    if (p == NULL) return;
    if (keyck(Red[gas], p)) return;
    if (q = iscall[gas](&p)) {
	str8[n].push_back({q->str, CALL});
    } else if (q = ismacall(p)) {
	str8[n].push_back({q->str, MAC});
    }
    while (p = p->next) {
	if (calldef(p)) return; // new func def
	if (keyck(Red[gas], p)) { // ck if ret instr
	    return; // ve ar don!
	}
	if (q = iscall[gas](&p)) {
	    m = {q->str, CALL};
	} else if (q = ismacall(p)) {
	    m = {q->str, MAC};
	} else {
	    continue;
	}
	l = str8[n];
	if (skan(l, m) == l.end()) str8[n].push_back(m);
    }
}
void bldstr8(void)
{
    for (auto l: loctab) {
	if (l.first.second == CALL) {
	    bldcall3(l.first, l.second);
	} else {
	    bldmac3(l.first, l.second);
	}
    }
}
void bldrevs(void)
{
    for (auto r : str8) {
	for (auto n : r.second) {
	    revs[n].push_back(r.first);
	}
    }
}
void bld3(void)
{
    bldstr8();
    bldrevs();
}
void ck3(tree t)
{
    for (auto r: t) {
	cout << r.first.first << ' ' << r.first.second << ":\n";
	for (auto s: r.second) {
	    cout << "  " << s.first << ' ' << s.second << endl;
	}
    }
}
void dumpnode(node n)
{
    string nom = n.first;
    mode mod = n.second;
    struct line *p = loctab[n];
        
    cout << nom << " <";
    if (p) {
	cout << filenom[p->ifile] << ", "
	     << p->linum;
    }
    cout << ">";
    if (mod == MAC)
	cout << " (m)";
}
void dump3(node root, string indent, tree t)
{    
    static string hook = "`-----> ";
    static string hooK = "`-----* ";
    static list<node> hstr; // history stack (detecting cycles)

    dumpnode(root);
    if (skan(hstr, root) != hstr.end()) {
    	cout << " (c)\n";
    	return;
    }
    cout << endl;
    hstr.push_back(root);

    list<node> s = t[root];
    if (s.empty()) {
    	return;
    }
    node back;    
    back = s.back();
    s.pop_back();
    for (auto n : s) {
    	cout << indent << hook;
    	dump3(n, indent + "`       ", t);
    	if (!hstr.empty())
    	    hstr.pop_back();
    }
    cout << indent << hooK;
    dump3(back, indent + "        ", t);
    if (!hstr.empty())
    	hstr.pop_back();
}
int main(int argc, char *argv[])
{
    int revsflag = 0; //--------------reverse-flag----------------- */
    int opt; //==================================option=character=====
    char *rootnom = NULL; //                         _start, main etc.
    char *prognom = basename(argv[0]); //^^^^^^^^^^^^^^^program^name^^
    mode rootmod = CALL; // default root mode

    while ((opt = getopt(argc, argv, "ghmn:r")) != -1) {
    	switch (opt) {
    	    case 'g': gas = 1;
    		break;
    	    case 'h': usage(prognom);
    		return 0;
	    case 'm': rootmod = MAC;
		break;
    	    case 'n': rootnom = optarg;
    		break;
    	    case 'r': revsflag = 1;
    		break;
    	    default : usage(prognom);
    		return 2;
    	}
    }
    if (rootnom == NULL || optind == argc) {
    	usage(prognom);
    	return 1;
    }
    char *buf;
    int nfiles = argc - optind;
    int ifile; // file index

    filenom = (char **) malloc(nfiles * sizeof (char *));
    
    for (int j = optind; j < argc; j++) {
	ifile = j - optind;
	filenom[ifile] = argv[j];
	buf = loadfile(argv[j]);
	post_loader[gas](buf);
	consrc(ifile, buf);
    }
    consmac();
    conscalltab();
    consloc();
    bld3();
#ifdef DEBUG
    dump_mactab();
    dump_calltab();
    dump_loctab();
    cout << "str8:\n";
    ck3(str8);
    cout << "revs:\n";
    ck3(revs);
#endif
    dump3({rootnom, rootmod}, "", revsflag ? revs : str8);
    return 0;
}
//``````````````````````````````````````````````````````````````````````
//                                                                  log:
