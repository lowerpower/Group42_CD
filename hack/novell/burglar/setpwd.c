/*
	 SETPWD 1.0 - Sets a Novell User Password
    Copyright (C) 1992  P.R.Lees

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
	
	P.R.Lees@ais.salford.ac.uk
	Network Systems Programmer
	University of Salford
	The Crescent
	Salford
	M5 4WT
	
	*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <process.h>
#include <nwcntask.h>
#include <nwbindry.h>

main(int argc,char *argv[])
{
int err;
printf("[SetPwd.nlm (c) 1992 P.R.Lees]\n");
if (argc!=3)
	{
	printf("Usage:\n\tLoad SetPwd <Username> <Password>\n");
	exit(2);
	}

err=ChangeBinderyObjectPassword(argv[1],OT_USER,"",strupr(argv[2]));
if (err) 
	{
	switch(err)
		{
		case 150:printf("Server out of Memory\n");break;
		case 215:printf("Password is not unique\n");break;
		case 240:printf("Wildcard not allowed\n");break;
		case 251:printf("No such Property\n");break;
		case 252:printf("No such Object\n");break;
		case 254:printf("Server bindery locked\n");break;
		case 255:printf("No such object or Bad Password\n");break;
		default:printf("Netware Error 0x%X (%d)\n",err,err);break;
		}
	exit(err);
	}
printf("Password of %s has been set to %s\n",strupr(argv[1]),strupr(argv[2]));
exit(0);
}