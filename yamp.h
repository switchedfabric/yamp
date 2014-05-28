/* ------------------------------------------------------------------------ *
 * $Source: /usr/local/cvs/yamp/yamp.h,v $
 * $Revision: 1.3 $
 * $Date: 2001/11/02 07:16:43 $
 * $Log: yamp.h,v $
 * Revision 1.3  2001/11/02 07:16:43  wroos
 * Added command line option -t to specify Reply-To address and added support
 * for -b"Message body goes here" iso just -bfile-containing-message-body
 * as requested by Albert.
 *
 * Revision 1.2  2001/11/01 09:25:22  wroos
 * Fixed various +-1 things, version numbers, etc
 *
 * Revision 1.1.1.1  2001/11/01 08:44:06  wroos
 * Initial revision
 *
 * ------------------------------------------------------------------------ */

#ifndef __YAMP_H__
#define __YAMP_H__

#define majorversion "1"
#define minorversion "20"
#define BUFLEN 256

typedef struct _list
{
	char *item;
	char *path;
	struct _list *next, *prev;
} LIST;

void sig();
void usage();
int tx(SOCKET, char *, int);
int rx(SOCKET, char *);
int tob64(unsigned char *, char *, int);
void version(int *, int*);

#endif __YAMP_H__
