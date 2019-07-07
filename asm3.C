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
#define BIZ 4096 // Block size
int gas = 0; //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|GNU assembly flag
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
map<pair<string, mode>, struct line *> loctab;
void consmac_nasm(void)
// macro definition keyword should be the first on the line but because
// of the escaped new lines macro name is either next token or the
// first token on the next line. Consturcts both mactab and loctab
{
    static char macdef[] = "%macro";

    struct line *p = &src;
    struct token *q;

    while (p = p->next) {
	q = p->tknhead.next; // first token on the line
	if (strcmp(macdef, q->str)) continue; // no match
	q = q->next;
	if (q == NULL) { // \new line
	    p = p->next; // advance to next line
	    q = p->tknhead.next;
	}
	mactab.push_front(q->str);
	loctab[{q->str, MAC}] = p;
    }
}
void consmac_gas(void)
// the same without the \new lines stuff
{
    static char macdef[] = ".macro";

    struct line *p = &src;
    struct token *q;

    while (p = p->next) {
	q = p->tknhead.next; // first token on the line
	if (strcmp(macdef, q->str)) continue; // no match
	mactab.push_front(q->next->str);
	loctab[{q->next->str, MAC}] = p;
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
forward_list<string> calltab;
#define skan(FILE, KEY)				\
    find((FILE).begin(), (FILE).end(), KEY)
forward_list<string> Kales = {
    "call",
    "callb",
    "calls",
    "callw",
    "calll",
    "callq",
    "callt"
};
// Gas global keywords
forward_list<string> global = {
    ".global",
    ".globl"
};
bool isGasKey(forward_list<string> key, char *str)
{
    return (skan(key, str) != key.end());
}
void conscalltab_gas(void)
{
    struct line *p = &src;
    struct token *q;

    while (p = p->next) {
	q = &(p->tknhead);
	while (q = q->next) {
	    // ck if call instr or boss function
	    if ((isGasKey(global, q->str) ||
		 isGasKey(Kales, q->str)) == 0) continue;
	    // avoid duplicates
	    if (skan(calltab, q->next->str) == calltab.end())
		calltab.push_front(q->next->str);
	}
    }    
}
bool isNasmKey(forward_list<string> key, char *str)
{
    return (key.front() == str);
}
void conscalltab_nasm(void)
{
    static string globdef = "global";

    struct line *p = &src;
    struct token *q;

    while (p = p->next) {
	q = &(p->tknhead); 
	while (q = q->next) {
	    if (((globdef == q->str) ||
		 isNasmKey(Kales, q->str)) == 0) continue; // no match
	    q = q->next;
	    if (q == NULL) { // \new line
		p = p->next; // advance to next line
		q = p->tknhead.next;
	    }
	    if (skan(calltab, q->str) == calltab.end())
		calltab.push_front(q->str);
	}
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
void usage(char *prognom)
{
    printf("Usage: %s -n name [-ghr] file...\n", prognom);
    printf("  -n name       root name\n");
    printf("  -g            gas flag\n");
    printf("  -h            help\n");
    printf("  -r            reverse tree\n");
}
void (*post_loader[])(char *) = { nasm_post_loader, gas_post_loader };
void (*consmac[])(void) = { consmac_nasm, consmac_gas };
void (*conscalltab[])(void) = { conscalltab_nasm, conscalltab_gas };
struct line * (*snoop[])(string) = { snoop_nasm, snoop_gas };
void consloc(void)
// macro locations were figured by the consmac function here we
// construct only callee and boss function locations
{
    for (auto c: calltab)
	loctab[{c, CALL}] = snoop[gas](c);
}
int main(int argc, char *argv[])
{
    int revsflag = 0; //--------------reverse-flag----------------- */
    int opt; //==================================option=character=====
    char *rootnom = NULL; //                         _start, main etc.
    char *prognom = basename(argv[0]); //^^^^^^^^^^^^^^^program^name^^

    while ((opt = getopt(argc, argv, "gn:hr")) != -1) {
    	switch (opt) {
    	    case 'g': gas = 1;
    		break;
    	    case 'n': rootnom = optarg;
    		break;
    	    case 'h': usage(prognom);
    		return 0;
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
    for (int j = optind; j < argc; j++) {
	buf = loadfile(argv[j]);
	post_loader[gas](buf);
	consrc(j - optind, buf);
    }
    consmac[gas]();
    conscalltab[gas]();
    consloc();
    dump_loctab();
    return 0;
}
//``````````````````````````````````````````````````````````````````````
//                                                                  log:
// - write q <- isgascall(p) and q <- isnasmcall(&p)
// isgasglobal, isnasmglobal, isgasmacdef, isnasmacdef and rewrite the
// code
// - write is gasmacend, nasmacend, isgasret, isnasmret,
// isgasmacall, isnasmacall etc.
// - bld3
