#
# Allow arbitrary magic numbers to be set for programs

TOP = ../..
EXTRACLN = setmagic
include $(TOP)/GNUmakefile.global

all:: setmagic

setmagic: setmagic.c
	$(NCC) -pipe -MD -o setmagic -g -O -Wall -Wno-comments -Wno-parentheses -Wno-char-subscripts   setmagic.c

