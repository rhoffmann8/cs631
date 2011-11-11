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

/* Connection properties */
#define MAX_CONN 20
#define PENDING_CONN 10

int main(int, char**);
void mainloop(void);
void reap(int);
void usage(void);

struct swsopts opts;

void
mainloop() {

	struct sockaddr_in sws;
	struct sigaction sig;
	pid_t pid;
	int sock;
	int conn, cur_connections, max_connections, pending_connections;
	char buf[1024];
	char str[INET_ADDRSTRLEN];
	socklen_t sin_size;

	/* Set port */
	if (opts.port) {
		if(!(sws.sin_port = htons(opts.port))) {
			fprintf(stderr, "Invalid port\n");
			exit(EXIT_FAILURE);
		}
	}
	else
		sws.sin_port = htons(8080);

	/* Set IP */
	//TODO: ipv6 check
	if (opts.ip) {
		if((sws.sin_addr.s_addr = inet_pton(AF_INET, opts.ip, str)) <= 0) {
			fprintf(stderr, "Invalid IP\n");
			exit(EXIT_FAILURE);
		}
	}
	else
		sws.sin_addr.s_addr = INADDR_ANY;

	/* Set maximum connections */
	if (opts.debug == 1)
		max_connections = 1;
	else
		max_connections = MAX_CONN;

	/* Create socket */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("opening stream socket");
		exit(EXIT_FAILURE);
	}

	/* Bind to specified address(es) */
	sws.sin_family = AF_INET;
	if (bind(sock, (struct sockaddr*)&sws, sizeof(sws))) {
		perror("binding stream socket");
		exit(EXIT_FAILURE);
	}

	/* Set up signal handler */
	sig.sa_handler = reap;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sig, NULL) < 0) {
		perror("sigaction error");
		exit(EXIT_FAILURE);
	}

	/* Connection accept loop */
	cur_connections = 0;
	pending_connections = PENDING_CONN;

	/* Listen on socket */
	if (listen(sock, pending_connections) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	/* Accept loop */
	do {
		conn = accept(sock, 0, &sin_size);
		cur_connections++;
		if (conn == -1)
			perror("connection accept error");
		else {
			if (cur_connections > max_connections)
				close(conn);
			else {
				fprintf(stderr, "connection accepted\n");
				bzero(buf, sizeof(buf));
				/* Fork to handle connection */
				if ((pid = fork()) < 0) {
					perror("error forking for connection");
					exit(EXIT_FAILURE);
				}
				else if (pid == 0) {
					/* Child */
					close(sock);
					read(conn, buf, 5);
					sws_request(conn);
					cur_connections--;
					exit(0);
				}
				else {
					/* Parent */
					close(conn);
				}
			}
		}
	} while (1);
}

void
reap(int sig) {
	/* implement SIGCHLD handler here */
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

int
main(int argc, char **argv) {

	char flag;
	extern char *optarg;
	extern int optopt;

	while((flag = getopt(argc, argv, "6c:dhi:k:l:p:s:")) != -1) {
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
		case 'k':
			opts.key = optarg;
			break;
		case 'l':
			opts.logfile = optarg;
			break;
		case 'p':
			if(!(opts.port = atoi(optarg))) {
				fprintf(stderr, "Invalid port\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 's':
			opts.secdir = optarg;
			break;
		case 'h':
			/* FALLTHROUGH */
		case '?':
			/* FALLTHROUGH */
		default:
			usage();
		}
	}

	if (optind < argc) {
		opts.dir = argv[optind];
		optind++;
	} else {
		fprintf(stderr, "No content directory specified\n");
		exit(EXIT_FAILURE);
	}

	if (opts.key && !opts.secdir) {
		fprintf(stderr, "Key specified without secure directory\n");
		exit(EXIT_FAILURE);
	}

	if (!opts.key && opts.secdir) {
		fprintf(stderr, "Secure mode specified without key\n");
		exit(EXIT_FAILURE);
	}

	sws_init(opts);

	/* ... */
	//error check other arguments

	/* daemonize if appropriate */

	mainloop();

	return EXIT_SUCCESS;
}

void
usage(void) {
	fprintf(stderr,
		"usage: sws [-6dh][-c dir][-i address][-l file][-p port][-s dir -k key] dir\n");
	exit(EXIT_FAILURE);
}
