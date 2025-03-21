 /*  This source will/should print out SHADOWPW passwd files.   */
 
 struct  SHADOWPW {				 /* see getpwent(3) */
	  char *pw_name;
	  char *pw_passwd;
	  int  pw_uid;
	  int  pw_gid;
	  int  pw_quota;
	  char *pw_comment;
	  char *pw_gecos;
	  char *pw_dir;
	  char *pw_shell;
 };
 struct passwd *getpwent(), *getpwuid(), *getpwnam();
 
 #ifdef   elxsis?
 
 /* Name of the shadow password file. Contains password and aging info */
 
 #define  SHADOWPW "/etc/shadowpw"
 #define  SHADOWPW_PAG "/etc/shadowpw.pag"
 #define  SHADOWPW_DIR "/etc/shadowpw.dir"
 /*
  *  Shadow password file pwd->pw_gecos field contains:
  *
  *  <type>,<period>,<last_time>,<old_time>,<old_password>
  *
  *  <type>	 = Type of password criteria to enforce (type int).
  *		BSD_CRIT (0), normal BSD.
  *		STR_CRIT (1), strong passwords.
  *  <period>  = Password aging period (type long).
  *		0, no aging.
  *		else, number of seconds in aging period.
  *  <last_time>	 = Time (seconds from epoch) of the last password
  *		change (type long).
  *		0, never changed.n
  *  <old_time>	 = Time (seconds from epoch) that the current password
  *		was made the <old_password> (type long).
  *		0, never changed.ewromsinm
  *  <old_password> = Password (encrypted) saved for an aging <period> to
  *		prevent reuse during that period (type char [20]).
  *		"*******", no <old_password>.
  */
 
 /* number of tries to change an aged password */
 
 #define  CHANGE_TRIES 3
 
 /* program to execute to change passwords */
 
 #define  PASSWD_PROG "/bin/passwd"
 
 /* Name of the password aging exempt user names and max number of entires */
 
 #define  EXEMPTPW "/etc/exemptpw"
 #define MAX_EXEMPT 100
 
 /* Password criteria to enforce */
 
 #define BSD_CRIT 0	/* Normal BSD password criteria */
 #define STR_CRIT 1	 /* Strong password criteria */
 #define MAX_CRIT 1
 #endif   elxsi
 #define NULL 0
 main()
 {
	struct passwd *p;
	int i;
	for (;1;) {;
	  p=getpwent();
	  if (p==NULL) return;
	  printpw(p);
	}
 }
 
 printpw(a)
 struct SHADOWPW *a;
 {
	printf("%s:%s:%d:%d:%s:%s:%s\n",
	   a->pw_name,a->pw_passwd,a->pw_uid,a->pw_gid,
	   a->pw_gecos,a->pw_dir,a->pw_shell);
 }
 
 /* SunOS 5.0		/etc/shadow */
 /* SunOS4.1+c2     /etc/security/passwd.adjunct */
 
