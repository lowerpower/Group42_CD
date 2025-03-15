/*=============================================================*\
 * ll.c - link looker                                          *
 * Copyright (C) 1994 by The Software System and lilo          *
 * Written by George Shearer (george@sphinx.biosci.wayne.edu)  *
 * Cleaned up to usability by lilo. :)                         *
 *                                                             *
 * September, 1994 - added help, version info and made the     *
 *                   program so you can actually use it. :)    *
 *                     --lilo                                  *
 *                                                             *
 * October 14, 1994 - cleaned up output flushing so you can    *
 *                    actually watch in something like real    *
 *                    time.  :)  --lilo                        *
 *                                                             *
 * October 28, 1994 - kill -1 will now produce a list of SPLIT *
 *                    servers.           -Doc                  *
 *                                                             *
 * November 4, 1994 - should compile on non-POSIX systems now. *
 *                    use -DHPSUCKS for HP-sUX systems 9.0x    *
 * November 15, 1994 - fixed a small bug in lilo's -h checker  *
\*=============================================================*/

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
   cc -O -s -o ll ll.c

   HP-UX cc:
   cc +O3 -Aa -s -DHPSUCKS -o ll ll.c

   GNU GCC:

   Linux:
     gcc -O2 -fomit-frame-pointer -funroll-loops -m486 -s -Wall -o ll ll.c

   BSD, SunOS 4.1.x, Slowaris 2.x, NeXT:
     gcc -O2 -funroll-loops -s -Wall -o ll ll.c */

#define VERSION "1.06"
#define BUFSIZE	400			/* IRC Server buffer */
#define SERVER	"irc.escape.com"	/* IRC Server        */
#define PORT	6667			/* IRC Port          */
#define LDELAY	30			/* Loop delay seconds*/
#define TIMEOUT	30			/* connection timeout*/

#define ESTABLISHED	1
#define INPROGRESS	2
#define SPLIT		1

#ifdef HPSUCKS
#define _INCLUDE_HPUX_SOURCE
#define _INCLUDE_XOPEN_SOURCE
#define _INCLUDE_POSIX_SOURCE
#endif

#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pwd.h>

unsigned short int session=0,link_count=0;
char in[BUFSIZE],out_buf[BUFSIZE],hostname[64];
char *ins=in;
char serverhost[81], nick[10], user[10], realname[81], lasttime[81];
time_t ltime;

struct irc_server {
  char *name;
  char *link;
  unsigned short int status;
  time_t time;
  struct irc_server *next;
} *sl1=(struct irc_server *)0,*sl2=(struct irc_server *)0;

void do_ping(char *,char *);
void do_001(char *,char *);
void do_error(char *,char *);
void do_364(char *,char *);
void do_365(char *,char *);

/* prototyping is lame when the function is in the same
   code, lilo outta just move the function before all
   other functions that call it  :-) */
char *stamp(time_t);

struct parsers {
   char *cmd;
   void (*func)(char *,char *);
} parsefuns[] = {
   { "PING", (void (*)())do_ping },
   { "001", (void (*)())do_001 },
   { "364",(void (*)())do_364 },
   { "365", (void (*)())do_365},
   { "ERROR",(void (*)())do_error},
   { (char *)0,(void (*)())0 }
};

struct sockaddr_in server;
int sock=0;

#ifndef sys_errlist
extern char *sys_errlist[];
#endif

#ifndef errno
extern int errno;
#endif

char *
mystrerror(int err) {
  return(sys_errlist[err]);
}

unsigned long int
resolver(char *host) {
  unsigned long int ip=0L;

  if(host && *host && (ip=inet_addr(host))==-1) {
    struct hostent *he;
    int x=0;

    while(!(he=gethostbyname((char *)host)) && x++<3) {
      fprintf(stderr,"."); fflush(stderr);
      sleep(1);
    }
    ip=(x<3) ? *(unsigned long *)he->h_addr_list[0] : 0L;
  }

  return(ip);
}

void
clean_sl2(void) {
  while(sl2) {
    struct irc_server *temp=sl2->next;
    if(sl2->name)
      free(sl2->name);
    if(sl2->link)
      free(sl2->link);
    free(sl2);
    sl2=temp;
  }
  sl2=(struct irc_server *)0;
}

void
exit_program(char *why) {
  fprintf(stderr,"\nExiting program. (%s)\n",why);

  if(sock)
    close(sock);

  while(sl1) {
    struct irc_server *temp=sl1->next;
    if(sl1->name)
      free(sl1->name);
    if(sl1->link)
      free(sl1->link);
    free(sl1);
    sl1=temp;
  }

  clean_sl2();

  if(in)
    free(in);

  exit(0);
}

int mystrccmp(register char *s1,register char *s2) {
   while((((*s1)>='a'&&(*s1)<='z')?(*s1)-32:*s1)==
        (((*s2)>='a'&&(*s2)<='z')?(*s2++)-32:*s2++))
     if(*s1++==0) return 0;
   return (*(unsigned char *)s1-*(unsigned char *)--s2);
}

char *mstrcpy(char **to,char *from) {
  if(from) {
    if((*to=(char *)malloc(strlen(from)+1)))
      strcpy(*to,from);
  }
  else
    *to=(char *)0;
  return(*to);
}

char *digtoken(char **string,char *match) {
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

void signal_handler(void) {
  exit_program("caught signal");
}

void signal_alarm(void) {
  exit_program("timed out waiting for server interaction.");
}

void
out(void) {
  int length=strlen(out_buf);
  errno=0;
  if(write(sock,out_buf,length)!=length)
    exit_program(mystrerror(errno));
}

void
init_server(void) {
  int length;

  sprintf(out_buf,"USER %s %s %s :%s\nNICK %s\nMODE %s +is", \
    user, user, user, realname, nick, nick);
  length=strlen(out_buf);

  errno=0;

  if(write(sock,out_buf,length)==length) {
    fputs("established\n",stderr);
    session=ESTABLISHED;
    alarm(TIMEOUT);
    sprintf(out_buf,"LINKS\n");
    out();
  }
  else
    exit_program(mystrerror(errno));
}

void
heartbeat(void) {
  strcpy(out_buf,"LINKS\n");
  out();
  signal(SIGALRM,(void (*)())heartbeat);
  alarm(LDELAY);
}

void
do_364(char *from,char *left) {
  struct irc_server *serv;
  char *sv1,*sv2;
  char *nick;

  serv=(struct irc_server *)malloc(sizeof(struct irc_server));
  serv->next=sl2;

  serv->status=0;
  nick=digtoken(&left," ");
  sv1=digtoken(&left," ");
  sv2=digtoken(&left," ");

  mstrcpy(&serv->name,sv1);
  mstrcpy(&serv->link,sv2);
  sl2=serv;
}

int
findserv(struct irc_server *serv,char *name) {
  for(;serv;serv=serv->next)
    if(!mystrccmp(name,serv->name))
      return(1);
  return(0);
}

void
show_split(void) {
  struct irc_server *serv=sl1;

  signal(SIGHUP,(void (*)())show_split);
  for(;serv;serv=serv->next) {
    if(serv->status & SPLIT) {
      printf("%s SPLIT: %s [%s]\n",stamp(serv->time),serv->name,serv->link);
      fflush(stdout);
    }
  }
}

void
do_365(char *from,char *left) {
  struct irc_server *serv=sl1;

  for(;serv;serv=serv->next) {
    if(!findserv(sl2,serv->name)) {
      if(!(serv->status & SPLIT)) {
        serv->time=time(NULL);
        printf("%s SPLIT: %s [%s]\n",stamp(serv->time),serv->name,serv->link);
        fflush(stdout);
        serv->status|=SPLIT;
      }
    }
    else
      if(serv->status & SPLIT) {
        serv->time=time(NULL);
        printf("%s MERGE: %s [%s]\n",stamp(serv->time),serv->name,serv->link);
        fflush(stdout);
        serv->status&=~SPLIT;
      }
  }

  serv=sl2;

  for(;serv;serv=serv->next) {
    if(!findserv(sl1,serv->name)) {
      struct irc_server *serv2;

      serv2=(struct irc_server *)malloc(sizeof(struct irc_server));
      serv2->next=sl1;
      serv2->status=0;
      mstrcpy(&serv2->name,serv->name);
      mstrcpy(&serv2->link,serv->link);
      sl1=serv2;
      serv2->time=time(NULL);
      if(link_count) {
        printf("%s ADDED: %s [%s]\n",stamp(serv2->time),serv->name,serv->link);
        fflush(stdout);
      }
    }
  }

  link_count=1;
  clean_sl2();
}

void
do_ping(char *from,char *left) {
  sprintf(out_buf,"PING :%s\n",hostname);
  out();
}

void
do_001(char *from,char *left) {
  fprintf(stderr,"Logged into server %s as nickname %s\n\n",from,nick);
  fflush(stderr);
  alarm(0);
  signal(SIGALRM,(void (*)())heartbeat);
  alarm(LDELAY);
}

void
do_error(char *from,char *left) {
  fprintf(stderr,"Server error: %s\n",left);
  fflush(stderr);
}

void
parse2(void) {
  char *from,*cmd,*left;

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
    int command;
    *left++=(char)0;
    left=(*left==':') ? left+1 : left;
    for(command=0;parsefuns[command].cmd;command++) {
      if(!mystrccmp(parsefuns[command].cmd,cmd)) {
        parsefuns[command].func(from,left);
        break;
      }
    }
  }
}

void
parse(int length) {
  char *s=in;

  *(ins+length)=(char)0;

  for(;;) {
    ins=s;
    while(*s && *s!=(char)13 && *s!=(char)10)
      s++;
    if(*s) {
      while(*s && (*s==(char)13 || *s==(char)10))
        *s++=(char)0;
      parse2();
    }
    else
      break;
  }
  strcpy(in,ins);
  ins=in+(s-ins);
}

void
process_server(void) {
  int x=0;

  for(;;) {
    fd_set rd,wr;
    struct timeval timeout;

    timeout.tv_usec=0; timeout.tv_sec=1;
    FD_ZERO(&rd); FD_ZERO(&wr);

    FD_SET(sock,&rd);
    if(session==INPROGRESS)
      FD_SET(sock,&wr);

    errno=0;

#ifdef HPSUCKS
    select((size_t)FD_SETSIZE,(int *)&rd,(int *)&wr,(int *)0,(session==INPROGRESS)?(const struct timeval *)&timeout:(const struct timeval *)0);
#else
    select(getdtablesize(),(fd_set *)&rd,(fd_set *)&wr,(fd_set *)0,(session==INPROGRESS)?(struct timeval *)&timeout:(struct timeval *)0);
#endif

    if(errno==EINTR)
      continue;

    errno=0;
    if(session==INPROGRESS) {
      if(FD_ISSET(sock,&wr)) {
        init_server();
         continue;
      }
      else {
        if(x++>=TIMEOUT)
          exit_program("connection timed out");
        fprintf(stderr,"."); fflush(stderr);
      }
    }

    if(FD_ISSET(sock,&rd)) {
      int length=read(sock,ins,BUFSIZE-(ins-in));

      if(length<1) {
        if(session!=INPROGRESS) {
          if(!errno) {
            fputs("Connection closed by foreign host.",stderr);
            errno=ENOTCONN;
          }
          else
            fprintf(stderr,"Connection to %s closed.\n",
                   inet_ntoa(server.sin_addr));
          fflush(stderr);
        }
        exit_program(mystrerror(errno));
      }
      if(strpbrk(in,"\x0a\x0d"))
        parse(length);
      else
        ins=(BUFSIZE-((ins+length)-in)<1)?in:ins+length;
    }
  }
}

char *stamp(time_t ltime) {
  strftime(lasttime, 81, "%x %X", localtime(&ltime));
  return (char *) &lasttime;
}

void
main(int argc,char *argv[]) {
  unsigned short int sport=PORT;
  unsigned int loop;
  struct passwd *passent;
  
  fprintf(stderr, "Link Looker v%s, written and designed by Dr. Delete.\n" \
                  "                   Enhanced by lilo.\n" \
                  "Type '%s -h' or '%s --help' for more information.\n\n",
    VERSION, argv[0], argv[0]);
    
  for(loop=1; loop<argc; loop++) {
    if(!strcmp("-h", argv[loop]) || !strcmp("--help", argv[loop])) {
        fprintf(stderr,"Format:\n\n" \
        "  %s [-h] [<nick> [<server>[:<port>] ['<real name field>' " \
        "[<username>]]]]\n\n  where:\n\n" \
        "  <nick>            is the nickname to be used (default is userid),\n" \
        "  <server>          is the hostname of the server to be used,\n" \
        "  <port>            is a four digit port number (default is 6667),\n" \
        "  <real name field> is a description of the user (default is gecos info), and\n" \
        "  <username>        is the user account field, used only if identd\n" \
        "                    has not been enabled on the user's system.\n", \
        argv[0]);
      exit(1);
    }
  }
  
  passent=getpwuid(getuid());

  if(argc>1)
    strncpy(nick,argv[1],9);
  else
    strncpy(nick,passent->pw_name,9);

  if(argc>2) {
    char *port=strchr(argv[2],':');
    sport=(port)?atoi(port+1):sport;
    strncpy(serverhost,argv[2],80);
    if(port)
      serverhost[port-argv[2]]=(char)0;
  }
  else
    strncpy(serverhost,SERVER,80);
    
  if(argc>3)
    strncpy(realname,argv[3],80);
  else {
    char *comma=strchr(passent->pw_gecos,',');
    strncpy(realname,passent->pw_gecos,80);
    if(comma)
      realname[comma-(passent->pw_gecos)]=(char)0;
  }

  if(argc>4)
    strncpy(user,argv[4],9);
  else
    strncpy(user,passent->pw_name,9);

  signal(SIGPIPE,(void (*)())signal_handler);
  signal(SIGHUP,(void (*)())show_split);
  signal(SIGINT,(void (*)())signal_handler);
  signal(SIGTERM,(void (*)())signal_handler);
  signal(SIGBUS,(void (*)())signal_handler);
  signal(SIGABRT,(void (*)())signal_handler);
  signal(SIGSEGV,(void (*)())signal_handler);
  signal(SIGALRM,(void (*)())signal_alarm);

  errno=0;
  if((sock=socket(AF_INET,SOCK_STREAM,0))>0) {
    server.sin_family=AF_INET;
    server.sin_port=htons(sport);
    fprintf(stderr,"Resolving %s...",serverhost); fflush(stderr);
    if((server.sin_addr.s_addr=resolver(serverhost))) {
      fputs("done\n",stderr);
      fflush(stderr);

      setsockopt(sock,SOL_SOCKET,SO_LINGER,0,0);
      setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,0,0);
      setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,0,0);

      fcntl(sock,F_SETFL,(fcntl(sock,F_GETFL)|O_NDELAY));

      fprintf(stderr,"Connecting to %s...",inet_ntoa(server.sin_addr));
      fflush(stderr);

      errno=0;
      if(connect(sock,(struct sockaddr *)&server,sizeof(server))) {
        if(errno!=EINPROGRESS && errno!=EWOULDBLOCK)
          exit_program(mystrerror(errno));
        else
          session=INPROGRESS;
      }
      else
        init_server();

      gethostname(hostname,64);
      process_server();
    }
    else
      exit_program("resolve failed");
  }
  else {
    fprintf(stderr,"Failed to allocate an AF_INET socket. (%s)\n", \
      mystrerror(errno));
    fflush(stderr);
  }
}
