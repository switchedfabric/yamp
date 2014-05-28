/* ------------------------------------------------------------------------ *
 * $Source: /usr/local/cvs/yamp/yamp.cpp,v $
 * $Revision: 1.9 $
 * $Date: 2003/08/29 11:03:53 $
 * $Log: yamp.cpp,v $
 * Revision 1.9  2003/08/29 11:03:53  wroos
 * Fixed 'mail from:' and 'Return-Path:' to reflect the reply-to address 
 * given on the command line. Mail exchangers outside our domain may lookup 
 * these addresses and the default (logged-on-user@your-host-name) often 
 * does not exist as an email account on Exchange (try resolving 
 * SYSTEM@remedy :-).
 * Also some cosmetic changes.
 *
 * Revision 1.8  2003/01/30 13:45:52  wroos
 * Changed version# to release to work with cvs tags
 *
 * Revision 1.7  2002/02/25 12:05:59  wroos
 * Added \r to every request, M$ ESMTP Mail 5+ wants it.
 *
 * Revision 1.6  2002/01/08 10:30:00  wroos
 * Fixed logic when reading message body from a text file (it used to skip a
 * a character when the buffer filled up).
 *
 * Revision 1.5  2001/11/22 11:13:43  wroos
 * Added support for multiple Reply-To address, lists now fold in the rfc822
 * part of the message.
 *
 * Revision 1.4  2001/11/02 07:16:43  wroos
 * Added command line option -t to specify Reply-To address and added support
 * for -b"Message body goes here" iso just -bfile-containing-message-body
 * as requested by Albert.
 *
 * Revision 1.3  2001/11/01 09:25:22  wroos
 * Fixed various +-1 things, version numbers, etc
 *
 * Revision 1.2  2001/11/01 09:00:08  wroos
 * Added rcsid
 *
 * Revision 1.1.1.1  2001/11/01 08:44:06  wroos
 * Initial revision
 *
 * ------------------------------------------------------------------------ */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <winsock2.h>

#include "yamp.h"

static const char BASE64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char PAD64 = '=';
short verbose = 0;
char *rcsid = "$Id: yamp.cpp,v 1.9 2003/08/29 11:03:53 wroos Exp $";
char *release="1.2";

/* ------------------------------------------------------------------------ *
 * main()
 * ------------------------------------------------------------------------ */
int main(int argc, char **argv)
{
	short attachments = 0;
	unsigned long l;
	int i, j, a, skip, numrcpts = 0;
	char *smtpsrv = NULL, *subject = NULL, *body = NULL, *afilename=NULL;
	char sender[32], hostname[65];
	SOCKET s;
 	WSADATA wsaptr;
	WORD wsaversion;
	char buf[BUFLEN];
	char ipaddress[16];
	FILE *fp;
	time_t ltime;
	struct tm *now;
	struct hostent *he = NULL;
	struct sockaddr_in addr;
	LIST *rcptroot = NULL, *rcpts = NULL;
	LIST *attroot = NULL, *atts = NULL;
	LIST *replytoroot = NULL, *replyto = NULL;
	struct _stat sb;

	for(i = 1; i < argc; i++)
	{
		skip = 0;
		for(j = 0; j < strlen(argv[i]) && ! skip; j++)
		{
			switch(argv[i][j])
			{
				case '-':
					break;
				case 'v':
					verbose = 1;
					break;
				case 'u':
					subject = &argv[i][j + 1];
					skip = 1;
					break;
				case 'b':
					body = &argv[i][j + 1];
					skip = 1;
					break;
				case 't':
					/*
					 * Build a list of replyto addresses
					 */
					{
						char *p = &argv[i][j + 1];
						char *rcpt = strtok(p, ",");
						while(1)
						{
							if(! replyto)
							{
								replytoroot = replyto = (LIST *)malloc(sizeof(LIST));
								if(! replyto)
								{
									fprintf(stderr, "[%s:%d] Error alloctaing memory\n",
									        __FILE__, __LINE__);
									exit(-1);
								}
								replyto->next = replyto->prev = NULL;
							}
							else
							{
								replyto->next = (LIST *)malloc(sizeof(LIST));
								if(! replyto->next)
								{
									fprintf(stderr, "[%s:%d] Error alloctaing memory\n",
									        __FILE__, __LINE__);
									exit(-1);
								}
								replyto->next->prev = replyto;
								replyto = replyto->next;
								replyto->next = NULL;
							}

							replyto->item = strdup(rcpt);
#ifdef DEBUG
							fprintf(stdout, "[%s:%d] Rcpt: %s\n", __FILE__, __LINE__, 
							        replyto->item);
							fflush(stdout);
#endif

							rcpt = NULL;
							rcpt = strtok(NULL, ",");
							if(! rcpt) break;
						}
					} 
					skip = 1;
					break;
				case 's':
					smtpsrv = &argv[i][j + 1];
					skip = 1;
					break;
				case 'r':
					/*
					 * Build a list of recipients
					 */
					{
						char *p = &argv[i][j + 1];
						char *rcpt = strtok(p, ",");
						while(1)
						{
							if(! rcpts)
							{
								rcptroot = rcpts = (LIST *)malloc(sizeof(LIST));
								if(! rcpts)
								{
									fprintf(stderr, "[%s:%d] Error alloctaing memory\n",
									        __FILE__, __LINE__);
									exit(-1);
								}
								rcpts->next = rcpts->prev = NULL;
							}
							else
							{
								rcpts->next = (LIST *)malloc(sizeof(LIST));
								if(! rcpts->next)
								{
									fprintf(stderr, "[%s:%d] Error alloctaing memory\n",
									        __FILE__, __LINE__);
									exit(-1);
								}
								rcpts->next->prev = rcpts;
								rcpts = rcpts->next;
								rcpts->next = NULL;
							}

							rcpts->item = strdup(rcpt);
#ifdef DEBUG
							fprintf(stdout, "[%s:%d] Rcpt: %s\n", __FILE__, __LINE__, 
							        rcpts->item);
							fflush(stdout);
#endif

							rcpt = NULL;
							rcpt = strtok(NULL, ",");
							if(! rcpt) break;
						}
					}
					skip = 1;
					break;
				case 'a':
					/*
					 * Build a list of attachments
					 */
					attachments = 1;
					{
						char *p = &argv[i][j + 1];
						char *att = strtok(p, ",");
						while(1)
						{
							if(! atts)
							{
								attroot = atts = (LIST *)malloc(sizeof(LIST));
								if(! atts)
								{
									fprintf(stderr, "[%s:%d] Error alloctaing memory\n",
									        __FILE__, __LINE__);
									exit(-1);
								}
								atts->next = atts->prev = NULL;
							}
							else
							{
								atts->next = (LIST *)malloc(sizeof(LIST));
								if(! atts->next)
								{
									fprintf(stderr, "[%s:%d] Error alloctaing memory\n",
									        __FILE__, __LINE__);
									exit(-1);
								}
								atts->next->prev = atts;
								atts = atts->next;
								atts->next = NULL;
							}

							atts->path = strdup(att);
							for(a = strlen(att) - 1; 
								a > 0 && att[a] != '\\' && att[a] != '/';
								a--);
							atts->item = strdup(&att[a ? a + 1 : 0]);
#ifdef DEBUG
							fprintf(stdout, "[%s:%d] Rcpt: %s (%s)\n", 
									__FILE__, __LINE__, atts->item, atts->path);
							fflush(stdout);
#endif

							att = NULL;
							att = strtok(NULL, ",");
							if(! att) break;
						}
					}
					skip = 1;
					break;
				default:
					fprintf(stderr, "[%s:%d] Unknown command line option '%c'\n", 
					        __FILE__, __LINE__, argv[i][j]);
					usage();
					exit(-1);
			}
		}
	}

	if(verbose) sig();

	if(! subject)
	{
		subject = strdup("No subject");
	}

	if(! smtpsrv || ! rcpts)
	{
		usage();
		exit(-1);
	}

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Initializing WSA\n", __FILE__, __LINE__);
	fflush(stdout);
#endif

	if(verbose) fprintf(stdout, "Initializing...\n");

	wsaversion = MAKEWORD(2, 0);
	WSAStartup(wsaversion, &wsaptr);

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Getting host and username\n", __FILE__, __LINE__);
	fflush(stdout);
#endif

	if(gethostname(hostname, 64) == -1)
	{
		fprintf(stderr, "[%s:%d] Error %s calling gethostname\n", __FILE__, 
		        __LINE__, WSAGetLastError());
		WSACleanup();
		return -1;
	}

	l = 32;
	if(GetUserName(sender, &l) == 0)
	{
		fprintf(stderr, "[%s:%d] Error %s calling GetUsername\n", __FILE__, 
		        __LINE__, GetLastError());
		WSACleanup();
		return -1;
	}

	if(verbose) fprintf(stdout, "Mail from %s@%s...\n", sender, hostname);

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Resolving smtpserver %s...\n", __FILE__, __LINE__, 
	        smtpsrv);
	fflush(stdout);
#endif

	if(verbose) fprintf(stdout, "Resolving smtp server %s...\n", smtpsrv);

	he = gethostbyname(smtpsrv);
	if(! he)
	{
		fprintf(stderr, "[%s:%d] Error %d calling gethostbyname\n", __FILE__, 
		        __LINE__, WSAGetLastError());
		WSACleanup();
		return -1;
	}

	strcpy(ipaddress, inet_ntoa(*((struct in_addr *)he->h_addr)));

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Connecting to %s\n", __FILE__, __LINE__, 
	        ipaddress);
	fflush(stdout);
#endif

	s = socket(PF_INET, SOCK_STREAM, 0);

	if(s == INVALID_SOCKET)
	{
	 	fprintf(stderr,"[%s:%d] Error %d creating socket\n", __FILE__, __LINE__, 
		        WSAGetLastError());
	 	return -1;
	}
	
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(25);
	addr.sin_addr.s_addr = inet_addr(ipaddress);

	if(verbose) fprintf(stdout, "Connecting to %s:25\n", ipaddress);

	if(connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
	 	fprintf(stderr,"[%s:%d] Error %d connecting to %s:%d\n", __FILE__, 
		        __LINE__, WSAGetLastError(), smtpsrv, 25);
	 	return -1;
	}

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Establishing SMTP connection\n", __FILE__, 
	        __LINE__);
	fflush(stdout);
#endif

	if(verbose) fprintf(stdout, "Sending mail...\n", ipaddress);

	if(rx(s, buf) == -1 || strncmp(buf, "220", 3) != 0)
	{
		fprintf(stderr, "[%s:%d] %s returned %s\n", __FILE__, __LINE__, smtpsrv, 
		        buf);
		closesocket(s);
		WSACleanup();
		return -1;
	}

	sprintf(buf, "helo %s\r\n", hostname);
	tx(s, buf, strlen(buf));
	if(rx(s, buf) == -1 || strncmp(buf, "250", 3) != 0)
	{
		fprintf(stderr, "[%s:%d] %s returned %s\n", __FILE__, __LINE__, smtpsrv, 
		        buf);
		closesocket(s);
		WSACleanup();
		return -1;
	}

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Sending SMTP header\n", __FILE__, __LINE__);
	fflush(stdout);
#endif

	replyto = replytoroot;
	if(replyto) sprintf(buf, "mail from:%s\r\n", replyto->item);
	else sprintf(buf, "mail from:<%s@%s>\r\n", sender, hostname);
	tx(s, buf, strlen(buf));
	if(rx(s, buf) == -1 || strncmp(buf, "250", 3) != 0)
	{
		fprintf(stderr, "[%s:%d] %s returned %s\n", __FILE__, __LINE__, smtpsrv, 
				  buf);
		closesocket(s);
		WSACleanup();
		return -1;
	}

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Recipients\n", __FILE__, __LINE__);
	fflush(stdout);
#endif

	rcpts = rcptroot;
	while(1)
	{
		sprintf(buf, "rcpt to:<%s>\r\n", rcpts->item);

#ifdef DEBUG
		fprintf(stdout, "[%s:%d] %s", __FILE__, __LINE__, buf);
		fflush(stdout);
#endif

		tx(s, buf, strlen(buf));
		if(rx(s, buf) == -1 || strncmp(buf, "250", 3) != 0)
		{
			fprintf(stderr, "[%s:%d] %s returned %s\n", __FILE__, __LINE__, 
					  smtpsrv, buf);
			closesocket(s);
			WSACleanup();
			return -1;
		}

		if(rcpts->next) rcpts = rcpts->next;
		else break;
	}

	sprintf(buf, "data\r\n");
	tx(s, buf, strlen(buf));
	if(rx(s, buf) == -1 || strncmp(buf, "354", 3) != 0)
	{
		fprintf(stderr, "[%s:%d] %s returned %s\n", __FILE__, __LINE__, smtpsrv, 
				  buf);
		closesocket(s);
		WSACleanup();
		return -1;
	}

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] RFC 822 message body\n", __FILE__, __LINE__);
	fflush(stdout);
#endif

	rcpts = rcptroot;
	if(rcpts)
	{
		sprintf(buf, "To:%s", rcpts->item);
		tx(s, buf, strlen(buf));

		while(1)
		{
			if(verbose) fprintf(stdout, "To %s\n", rcpts->item);
			if(rcpts->next) rcpts = rcpts->next;
			else break;
			sprintf(buf, ",\n\t%s", rcpts->item);
			tx(s, buf, strlen(buf));
		}

		sprintf(buf, "\r\n");
		tx(s, buf, strlen(buf));
	}

	replyto = replytoroot;
	if(replyto) sprintf(buf, "From:%s\r\n", replyto->item);
	else sprintf(buf, "From:%s@%s\r\n", sender, hostname);
	tx(s, buf, strlen(buf));

	replyto = replytoroot;
	if(replyto) sprintf(buf, "Return-Path:%s\r\n", replyto->item);
	else sprintf(buf, "Return-Path:%s@%s\r\n", sender, hostname);
	tx(s, buf, strlen(buf));

	replyto = replytoroot;
	if(replyto)
	{
		sprintf(buf, "Reply-To:%s", replyto->item);
		tx(s, buf, strlen(buf));

		while(1)
		{
			if(verbose) fprintf(stdout, "Reply to %s\n", replyto->item);
			if(replyto->next) replyto = replyto->next;
			else break;
			sprintf(buf, ",\r\n\t%s", replyto->item);
			tx(s, buf, strlen(buf));
		}

		sprintf(buf, "\r\n");
		tx(s, buf, strlen(buf));
	}

	sprintf(buf, "Subject:%s\r\n", subject);
	tx(s, buf, strlen(buf));

	time(&ltime);
	now = localtime(&ltime);
	strftime(buf, BUFLEN - 1, "Date:%a, %d %b %y %H:%M:%S +0200\r\n", now);
	tx(s, buf, strlen(buf));

	sprintf(buf, "MIME-Version: 1.0 (sent by yamp (Yet Another Mail Program) " \
	        "because M$ sucks so terribly)\r\n");
	tx(s, buf, strlen(buf));

	if(attachments)
	{
		sprintf(buf, "Content-type: multipart/mixed; boundary=abc\r\n");
		tx(s, buf, strlen(buf));
	}
	else
	{
		sprintf(buf, "Content-type: text/plain\r\n");
		tx(s, buf, strlen(buf));
	}

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Message body\n", __FILE__, __LINE__);
	fflush(stdout);
#endif

	sprintf(buf, "\r\n");
	tx(s, buf, strlen(buf));

	if(body)
	{
		if(attachments)
		{
			sprintf(buf, "This is a multi-part message in MIME format.\r\n");
			tx(s, buf, strlen(buf));

			sprintf(buf, "\r\n");
			tx(s, buf, strlen(buf));

			sprintf(buf, "--abc\r\n");
			tx(s, buf, strlen(buf));

			sprintf(buf, "\r\n");
			tx(s, buf, strlen(buf));
		}

		if(_stat(body, &sb) == 0) /* File exists */
		{
			fp = fopen(body, "r");
			if(! fp)
			{
				fprintf(stderr, "[%s:%d] Error opening file %s for reading\n", 
					        __FILE__, __LINE__, body);
				return -1;
			}
			else
			{
				while(1)
				{
					j = 0;
					while(j < BUFLEN - 1)
					{
						i = fgetc(fp);
						if(feof(fp)) break;
						buf[j] = i;
						buf[j + 1] = 0;
						j++;
					}

					tx(s, buf, strlen(buf));

					if(feof(fp)) break;
				}
			}
			fclose(fp);
		}
		else
		{
			/*
			 * -b flag is text to be included directly
			 */
			tx(s, body, strlen(body));
		}
	}
	else if (fseek(stdin, 1, SEEK_SET) == 0) 
		/* 
		 * Check if there is something to read on stdin, other wise you wait 
		 */
	{
		fseek(stdin, 0, SEEK_SET); /* Rewind again */

		if(attachments)
		{
			sprintf(buf, "This is a multi-part message in MIME format.\r\n");
			tx(s, buf, strlen(buf));

			sprintf(buf, "\r\n");
			tx(s, buf, strlen(buf));

			sprintf(buf, "--abc\r\n");
			tx(s, buf, strlen(buf));

			sprintf(buf, "\r\n");
			tx(s, buf, strlen(buf));
		}

		while(1)
		{
			j = 0;
			while(j < BUFLEN - 1)
			{
				i = fgetc(stdin);
				if(feof(stdin)) break;
				buf[j] = i;
				buf[j + 1] = 0;
				j++;
			}

			tx(s, buf, strlen(buf));

			if(feof(stdin)) break;
		}
	}
	else
	{
		sprintf(buf, "%s", "No message body\r\n");
		tx(s, buf, strlen(buf));
	}

	if(attachments)
	{
		atts = attroot;
		while(atts)
		{
			if(verbose) fprintf(stdout, "Attaching file %s...\n", atts->item);

			sprintf(buf, "\r\n");
			tx(s, buf, strlen(buf));

			sprintf(buf, "--abc\r\n");
			tx(s, buf, strlen(buf));

			sprintf(buf, "Content-type: application/octet-stream; name=%s\r\n", 
			        atts->item);
			tx(s, buf, strlen(buf));

			sprintf(buf, "Content-transfer-encoding: base64\r\n");
			tx(s, buf, strlen(buf));

			sprintf(buf, "\r\n");
			tx(s, buf, strlen(buf));

			fp = fopen(atts->path, "rb");
			if(! fp)
			{
				fprintf(stderr, "[%s:%d] Error opening file %s for reading\n", 
				        __FILE__, __LINE__, atts->path);
				closesocket(s);
				WSACleanup();
				return -1;
			}
			else
			{
				while(1)
				{
					unsigned char binbuf[BUFLEN];
					char b64buf[BUFLEN * 2], b64_76buf[78];
					int k = 0;

					j = 0;
					i = fgetc(fp);
					while(! feof(fp))
					{
						binbuf[j] = i;
						binbuf[j + 1] = 0;
						j++;
						if(j >= BUFLEN -1) break;
						i = fgetc(fp);
					}

					j = tob64(binbuf, b64buf, j);

					for(k = 0; k < j; k += 76)
					{
						int l = (j - k < 76) ? j - k : 76;
						strncpy(b64_76buf, &b64buf[k], l);
						b64_76buf[l++] = '\n';
						b64_76buf[l] = 0;
						tx(s, b64_76buf, l);
					}

					if(feof(fp)) break;
				}
			}

			fclose(fp);

			atts = atts->next;
		}

		sprintf(buf, "\r\n");
		tx(s, buf, strlen(buf));

		sprintf(buf, "--abc--\r\n");
		tx(s, buf, strlen(buf));
	}

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] EOM\n", __FILE__, __LINE__);
	fflush(stdout);
#endif

	sprintf(buf, "\r\n.\r\n");
	tx(s, buf, strlen(buf));
	if(rx(s, buf) == -1)
	{
		fprintf(stderr, "[%s:%d] %s returned %s\n", __FILE__, __LINE__, smtpsrv, 
		        buf);
		closesocket(s);
		WSACleanup();
		return -1;
	}

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Closing\n", __FILE__, __LINE__);
	fflush(stdout);
#endif

	sprintf(buf, "quit\r\n");
	tx(s, buf, strlen(buf));
	if(rx(s, buf) == -1 || strncmp(buf, "221", 3) != 0)
	{
		fprintf(stderr, "[%s:%d] %s returned %s\n", __FILE__, __LINE__, smtpsrv, 
		        buf);
		closesocket(s);
		WSACleanup();
		return -1;
	}

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] Tearing down connection\n", __FILE__, __LINE__);
	fflush(stdout);
#endif

	closesocket(s);
	WSACleanup();

	if(verbose) fprintf(stdout, "Done\n");

	return 1;
}

/* ------------------------------------------------------------------------- *
 * tx()
 * ------------------------------------------------------------------------- */
int tx(SOCKET s, char *str, int packetlen)
{
 	size_t wbytes;
	int n;

	for(wbytes = 0; wbytes < packetlen; wbytes += n)
	{
	 	n=send(s, str + wbytes, packetlen - wbytes, 0);
		if(n == SOCKET_ERROR)
		{
			fprintf(stderr,"[%s:%d] Error %d sending\n", __FILE__, __LINE__, 
					  WSAGetLastError());
			return -1;
		}
	}
 	return 1;
}

/* ------------------------------------------------------------------------- *
 * rx()
 * ------------------------------------------------------------------------- */
int rx(SOCKET s, char *str)
{
 	int r;

 	memset(str, 0, BUFLEN);
 	r = recv(s, str, BUFLEN - 1, 0);
 	if(r == SOCKET_ERROR)
	{
		fprintf(stderr,"[%s:%d] Error %d receiving\n", __FILE__, __LINE__, 
				  WSAGetLastError());
		return -1;
	}

 	return r;
}

/* ------------------------------------------------------------------------ *
 * sig()
 * ------------------------------------------------------------------------ */
void sig()
{
	fprintf(stdout, "yamp release %s - Yet Another Mail Program because M$ " \
	        "sucks so terribly\nBugs, sugggestions to wroos@shoprite.co.za\n", 
			  release);
}

/* ------------------------------------------------------------------------ *
 * usage()
 * ------------------------------------------------------------------------ */
void usage()
{
	sig();

	fprintf(stdout, "Usage: [echo \"Message body\" |] yamp flags " \
	        "[< message.body]\n");
	fprintf(stdout, "Required flags\n");
	fprintf(stdout, "\t-ssmtpserver                    SMTP server address\n");
	fprintf(stdout, "\t-rrecipient[,recipient...]      Recipients\n");
	fprintf(stdout, "Optional flags\n");
	fprintf(stdout, "\t-v                              Verbose\n");
	fprintf(stdout, "\t-usubject                       Subject line\n");
	fprintf(stdout, "\t-bmessage-body                  Message body text or " \
	        "file containing it\n");
	fprintf(stdout, "\t-aattachment[,attachment...]    Optional attachments\n");
	fprintf(stdout, "\t-treply-to-address              Address to reply to\n");
}

/* ------------------------------------------------------------------------ *
 * tob64()
 * ------------------------------------------------------------------------ */
int tob64(unsigned char *source, char *target, int sourcelen)
{
	int targetlen = 0;
	unsigned char in[3], out[4], *ptr;

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] tob64 source: [%s], length: %d\n", __FILE__, 
	        __LINE__, source, sourcelen);
	fflush(stdout);
#endif

	ptr = source;
	while(sourcelen > 2)
	{
		in[0] = *ptr++;
		in[1] = *ptr++;
		in[2] = *ptr++;
		sourcelen -= 3;

		out[0] = in[0] >> 2;
		out[1] = ((in[0] & 0x03) << 4) + (in[1] >> 4);
		out[2] = ((in[1] & 0x0f) << 2) + (in[2] >> 6);
		out[3] = in[2] & 0x3f;

		if(out[0] >= 64) fprintf(stderr, "[%s:%d]\n", __FILE__, __LINE__);
		if(out[1] >= 64) fprintf(stderr, "[%s:%d]\n", __FILE__, __LINE__);
		if(out[2] >= 64) fprintf(stderr, "[%s:%d]\n", __FILE__, __LINE__);
		if(out[3] >= 64) fprintf(stderr, "[%s:%d]\n", __FILE__, __LINE__);
		target[targetlen++] = BASE64[out[0]];
		target[targetlen++] = BASE64[out[1]];
		target[targetlen++] = BASE64[out[2]];
		target[targetlen++] = BASE64[out[3]];
	}

	/*
	 * Now worry about padding
	 */
	if(sourcelen != 0)
	{
		int i;

		in[0] = in[1] = in[2] = 0;

		for(i = 0; i < sourcelen; i++)
		{
			in[i] = *ptr++;
		}

		out[0] = in[0] >> 2;
		out[1] = ((in[0] & 0x03) << 4) + (in[1] >> 4);
		out[2] = ((in[1] & 0x0f) << 2) + (in[2] >> 6);

		target[targetlen++] = BASE64[out[0]];
		target[targetlen++] = BASE64[out[1]];

		if(sourcelen == 1)
		{
			target[targetlen++] = PAD64;
		}
		else
		{
			target[targetlen++] = BASE64[out[2]];
		}
		target[targetlen++] = PAD64;
	}
	target[targetlen] = 0;

#ifdef DEBUG
	fprintf(stdout, "[%s:%d] tob64 target: [%s], length: %d\n", __FILE__, 
	        __LINE__, target, targetlen);
	fflush(stdout);
#endif

	return targetlen;
}
