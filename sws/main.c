#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "sws.h"

#define TRUE 1

int main(int, char**);
void mainloop(void);
void reap(int);
void usage(void);

//struct swsopts opts;

void
mainloop() {
	/* write me */

	/* bind to socket */
	/* select/accept, then for each connection fork a child that will
	 * call sws_request(socket) */

	struct sockaddr_in sws;
	struct sigaction sig;
	pid_t pid;
	int sock;
	int conn, max_connections;
	char buf[1024];

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("opening stream socket");
		exit(EXIT_FAILURE);
	}

	sws.sin_family = AF_INET;
	sws.sin_addr.s_addr = INADDR_ANY;
	sws.sin_port = 0;
	if (bind(sock, (struct sockaddr*)&sws, sizeof(sws))) {
		perror("binding stream socket");
		exit(EXIT_FAILURE);
	}

	sig.sa_handler = reap;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sig, NULL) < 0) {
		perror("sigaction error");
		exit(EXIT_FAILURE);
	}

	//wat do
	if (__sws_debug == 1)
	//if (opts.debug == 1)
		max_connections = 1;
	else
		max_connections = 10;

	/* Connection accept loop */
	//TODO: Make "pending_connections" var
	listen(sock, 5);
	do {
		conn = accept(sock, 0, 0);
		if (conn == -1)
			perror("connection accept error");
		else {
			bzero(buf, sizeof(buf));
			/* Fork here */
			if ((pid = fork()) < 0) {
				perror("error forking for connection");
				exit(EXIT_FAILURE);
			}
			else if (pid == 0) {
				//child
				close(sock);
				sws_request(conn);
			}
			else {
				//parent
				close(conn);
			}
		}
	} while (TRUE);
}

void
reap(int sig) {
	/* implement SIGCHLD handler here */
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

int
main(int argc, char **argv) {

	struct swsopts opts;
	char flag;

	while((flag = getopt(argc, argv, "6cdhiklps")) != -1) {
		switch(flag) {
		case '6':
			break;
		case 'c':
			opts.cgidir = optarg;
			break;
		case 'd':
			opts.debug = 1;
			break;
		case 'i':
			opts.ip = optarg;
			break;
		case 'l':
			opts.logfile = optarg;
		case 'h':
			/* FALLTHROUGH */
		case '?':
			/* FALLTHROUGH */
		default:
			usage();
		}
	}

	sws_init(opts);

	/* ... */

	/* daemonize if appropriate */

	mainloop();

	return EXIT_SUCCESS;
}

void
usage(void) {
	fprintf(stderr,
		"usage: sws [-6dh][-c dir][-i address][-l file][-p port][-s dir -k key] dir");
	exit(EXIT_FAILURE);
}
