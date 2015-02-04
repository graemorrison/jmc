/* --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- */
/*                                 jmc.c - a Join Multi Cast utility                                       */
/*                                                                                                         */
/* This utility is designed to simply issue an IGMP join on the given network interface for the given      */
/* multicast groups and maintain that join until the process dies.                                         */
/*                                                                                                         */
/* The process runs in foregound (-n option) with a "Press <return> to continue" prompt,                   */
/* or forks to a daemon, which sleeps until killed.                                                        */
/*                                                                                                         */
/*                                            Version 2.0                                                  */
/*                                 Copyright (c) 2006, gtmorrison.net                                      */
/*                                                                                                         */
/*                This program is free software: you can redistribute it and/or modify                     */
/*                it under the terms of the GNU General Public License as published by                     */
/*                the Free Software Foundation, either version 3 of the License, or                        */
/*                                (at your option) any later version.                                      */
/*                                                                                                         */
/*                 This program is distributed in the hope that it will be useful,                         */
/*                 but WITHOUT ANY WARRANTY; without even the implied warranty of                          */
/*                 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                           */
/*                                                                                                         */
/*                           GNU General Public License for more details.                                  */
/*                                                                                                         */
/*                 You should have received a copy of the GNU General Public License                       */
/*                 along with this program.  If not, see <http://www.gnu.org/licenses/>.                   */
/* --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <string.h>

#define	TRUE		1
#define	FALSE		0
#define ADDRLEN		16+1
#define MCADDRMAX	64

int	DEBUG=FALSE;
int	VERBOSE=FALSE;
int	DAEMON=TRUE;
int	FINISHED=FALSE;

void usage() {

	fprintf(stdout,"Usage: jmc {-n} {-v} {-d} {-i <interface>} <multicast group> <multicast group> ... <multicast group>\n");
	fprintf(stdout,"           {-n} {-v} {-d} {-i <interface>} -c <file listing groups 1 per line\n");

	fprintf(stdout,"\n       -n\tDo not daemonise\n");
	fprintf(stdout,"       -v\tShow verbose info\n");
	fprintf(stdout,"       -d\tShow debug info\n");

	return;
}

void handoff(int insig) {

	switch(insig) {

		default:
			FINISHED=TRUE;
	}

	return;
}

////////////////////////////////////////////
main(int argc, char **argv) {

int	i, sock, errflg=0, mcstart, mcgroupcount;
char	c, dummy[ADDRLEN], iface[16+1], cfile[256+1];
char	*p, *mclist[MCADDRMAX];

struct	ip_mreqn mcgroup;

extern	char	*optarg;
extern	int	optind;

FILE	*cfd;

////
// manage the signals

	signal(1,handoff);
	signal(2,handoff);
	signal(15,handoff);
	signal(16,handoff);
	signal(17,handoff);

////
// Do some command line processing
//

	bzero(iface,(sizeof(iface))); bzero(cfile,(sizeof(cfile))); i = 1;

	while ((c = getopt(argc, argv, "dni:c:hv")) != EOF) {
		i++;
		switch (c) {
			case 'd':
				DEBUG=TRUE;
				break;

			case 'n':
				DAEMON=FALSE;
				break;

			case 'i':
				strcpy(iface,optarg); i++;
				break;

			case 'c':
				strcpy(cfile,optarg); i++;
				break;

			case 'h':
				usage();
				exit(0);

			case 'v':
				VERBOSE=TRUE;
				break;

			case '?':
				errflg++;
				break;

			default:
				break;
		}
	}
//

	if ( (argc < 2) || errflg ) {
		usage();
		exit(EOF);
	}

////
//

	if ( strlen(iface)<1 ) { strcpy(iface,"lo"); }

	if (DEBUG || VERBOSE) {
		fprintf(stdout,"Using Interface [%s]\n",iface);
	}


////
// get the starting point of the mcgroups in the arg list

	mcstart = i; mcgroupcount=0;

////
// get the multicast groups to subscribe to

	if (DEBUG && (strlen(cfile)>0)) fprintf(stdout,"cfile: %s\n",cfile);

	if (strlen(cfile) > 0) {

		if ((cfd=fopen(cfile,"r"))==NULL) {
			fprintf(stderr,"Error opening config file [%s], %s\n",cfile,strerror(errno));
			exit(EOF);
		}

		while ((fgets(dummy,ADDRLEN,cfd))!=NULL) {
			if (DEBUG) fprintf(stdout,"\t[%s]\n",dummy);

			dummy[(strlen(dummy)-1)] = '\0';
			p = (char *)malloc(ADDRLEN);
			mclist[mcgroupcount++] = (strcpy(p,dummy));
			
			if (DEBUG) fprintf(stdout,"\t[%s]\n",mclist[(mcgroupcount-1)]);
		}

		fclose(cfd);
	} else {
		for (i=mcstart;i<argc;i++) {
			if (DEBUG) fprintf(stdout,"\tIn [%s]\n",argv[i]);

			p = (char *)malloc(ADDRLEN);
			mclist[mcgroupcount++] = (strcpy(p,argv[i]));

			if (DEBUG) fprintf(stdout,"\tOut[%s]\n",mclist[(mcgroupcount-1)]);
		}
	}
////
// open a socket

	if ((sock=socket(AF_INET,SOCK_DGRAM,0))==EOF) {
		fprintf(stderr,"Error getting socket:[%d] %s.\n",errno,strerror(errno));
		exit(EOF);
	}

////
// now we can apply for membership of the groups


	for (i=0;i<mcgroupcount;i++) {
		if (DEBUG || VERBOSE) fprintf(stdout,"Joining group (%02d)[%s]\n",i,mclist[i]);

		mcgroup.imr_multiaddr.s_addr	= inet_addr(mclist[i]);
		mcgroup.imr_address.s_addr	= INADDR_ANY;
		mcgroup.imr_ifindex		= if_nametoindex(iface);

		if ((setsockopt(sock,0,IP_ADD_MEMBERSHIP, &mcgroup, sizeof(mcgroup)))==EOF) {
			fprintf(stderr,"Error joining group:[%d] %s.\n",errno,strerror(errno));
		}
	}

	if (DAEMON) {
		if ((i=fork()) != 0) {
			fprintf(stdout,"Daemon [%d]\n",i);
			 exit(0);
		}

////
// now we wait until death


		while (!FINISHED) { sleep(60); }
		
	} else {
		fprintf(stdout,"[Press <RETURN> to drop membership]");
		fgets(dummy,ADDRLEN,stdin);
	}

////
// now we can resign membership of the groups

	for (i=0;i<mcgroupcount;i++) {
		if (DEBUG) fprintf(stdout,"Leaving group (%02d)[%s]\n",i,mclist[i]);

		mcgroup.imr_multiaddr.s_addr	= inet_addr(mclist[i]);
		mcgroup.imr_address.s_addr	= INADDR_ANY;
		mcgroup.imr_ifindex		= if_nametoindex(iface);

		if ((setsockopt(sock,0,IP_DROP_MEMBERSHIP, &mcgroup, sizeof(mcgroup)))==EOF) {
			fprintf(stderr,"Error joining group:[%d] %s.\n",errno,strerror(errno));
		}
	}
}
