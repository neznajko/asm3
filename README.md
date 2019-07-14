![hi](pix/above.png)  
**Assembly** *tree*
## asm3
Print caller-callee tree for both *Nasm* and *Gas* assembly provided
caller name and mode (call or macro) and source files. Cycle calls
are detected and reversed callee-caller tree can be printed as well.

This relationship is more native to *C* language for example, but
in assembly it can also be done. There is no problem with macros,
bcoz they have fixed beginning and ending, whereas given only single
label without additional information we can't tell in advance is it
a beginning of a procedure, but we can make a call list with all
labels that have been called, so if a label is in that list, it's
a beginning of a function, plus the boss function which follows
the global keyword. With the ending, the function ends if there is
a *ret* instruction, end of file, or another function begins. The last
one is bcoz we might have function, let's say *quit*, which make system
call *exit* etc.

## Blade ***Runner***
Let's for example try the program on these super cool source files,
*debug.asm* and **spam.asm:**
```assembly
%include "spam.asm"
global _start
%macro boom
	call baz
	poom
	%macro \
		boom2
		call foo
	%endmacro
	poom
%endmacro
qvit:   call exit
_start: call baz
	call foo
	poom
	boom
	call \
	qvit
```
```assembly
baz:    call foo
	ret
foo 	call baz
	ret	
%macro poom
	nop
%endmacro
```
The command:
```
asm3 -n _start debug.asm spam.asm
```
will give the following output:
```
_start <debug.asm, 13>
`-----> baz <spam.asm, 1>
`       `-----* foo <spam.asm, 3>
`               `-----* baz <spam.asm, 1> (c)
`-----> foo <spam.asm, 3>
`       `-----* baz <spam.asm, 1>
`               `-----* foo <spam.asm, 3> (c)
`-----> poom <spam.asm, 5> (m)
`-----> boom <debug.asm, 3> (m)
`       `-----> baz <spam.asm, 1>
`       `       `-----* foo <spam.asm, 3>
`       `               `-----* baz <spam.asm, 1> (c)
`       `-----* poom <spam.asm, 5> (m)
`-----* qvit <debug.asm, 12>
        `-----* exit <>
```
The information in the brackets is <filenom, linum>, *(m)* means it's
a macro call, *(c)* means cycle detected. On command line rootnom
following -n option and source files are mandatory.
There are few more options that we will demonstrate immediately.
The command:
```
asm3 -mrn poom debug.asm spam.asm
```
will produce the following output:
```
poom <spam.asm, 5> (m)
`-----> _start <debug.asm, 13>
`-----* boom <debug.asm, 3> (m)
        `-----* _start <debug.asm, 13>
```
*-m* indicates that *poom* is a macro (bcoz we might have function with the
same name), *-r* will force the program to print the reverse tree. For
*Gas* it's the same but *-g* option has to be given.

## pugs and stuff
There is one issue with *Nasm* keywords caught by testing the program
on a source file from the author of the only assembly book I've read,
Jeff Duntemann. The problem was that *Nasm* allows UPPERCASE keywords
and main was declared *GLOBAL*. That exception was figured but if you
are using *CALL* or *RET* or *%MACRO* the program won't work. And if
you ask me why, I would answer bcoz a program should have at least one
bug and 3 issues, you can quote me.

![bye](pix/vandal.png)
<<<<<<< HEAD
https://youtu.be/pfcgoSoPY-8
=======
https://youtu.be/3vOdFiFc4tA
>>>>>>> 9a41a2c7074833704391942b6becac69f776c407
## The End
