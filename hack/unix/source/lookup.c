/*translate ip to alpha, faster than nslookup*/
/*thc*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>

main(argc, argv)
int argc;
char *argv[];

{  
  struct hostent *lookup;
  char           name[255];
  unsigned long  addr;

  if (argc != 2) {
      printf("Usage:  %s {ipnumber}\n", argv[0]);
      exit(1); } 
 if ((addr = inet_addr(argv[1])) == -1) {
      printf("Couldn't find %s\n", argv[1]);
      exit(1); }
  if ((lookup = gethostbyaddr((char *) &addr,sizeof(long),AF_INET)) != NULL) {
      printf("%s \t %s\n",argv[1], lookup->h_name);
	  *lookup->h_addr_list++; }  
  else
    printf("Error: couldn't find hostname for %s\n", argv[1]);
  }
