/*	$OpenBSD: libyywrap.c,v 1.4 1996/12/10 22:22:03 millert Exp $	*/

/* libyywrap - flex run-time support library "yywrap" function */

/* $Header: /cvs/exopc/lib/libl/libyywrap.c,v 1.2 1998/08/31 18:38:14 alaric Exp $ */

#include <sys/cdefs.h>

int yywrap __P((void));

int
yywrap()
	{
	return 1;
	}
