/*
 * Rob Hoffmann, CS631
 * main.c - Main functions for simple web server
 *
 * Option parsing and creation of connections is handled here.
 * IPV6 support is enabled using the -6 flag.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "sws.h"

/* Connection properties */
#define MAX_CONN 20
#define PENDING_CONN 10

#define _PATH_DEVNULL "/dev/null"

int main(int, char**);
void mainloop(void);
void reap(int);
void usage(void);

struct swsopts opts;
int cur_connections, ipv6;

void
mainloop() {

	struct sockaddr_in sws;
	struct sockaddr_in6 sws6;
	struct sigaction sig;
	pid_t pid;
	socklen_t sin_size;
	int sock, domain, opt;
	int conn, max_connections, pending_connections;
	char buf[1024];

	/* Set port */
	if (opts.port) {
		if (!ipv6) {
			if (!(sws.sin_port = htons(opts.port))) {
				fprintf(stderr, "Invalid port\n");
				exit(EXIT_FAILURE);
				/* NOTREACHED */
			}
		} else {
			if (!(sws6.sin6_port = htons(opts.port))) {
				fprintf(stderr, "Invalid port\n");
				exit(EXIT_FAILURE);
				/* NOTREACHED */
			}
		}
	} else {
		if (!ipv6)
			sws.sin_port = htons(8080);
		else
			sws6.sin6_port = htons(8080);
	}

	/* Set domain */
	domain = (ipv6 == 0) ? AF_INET : AF_INET6;

	/* Set IP */
	if (opts.ip) {
		if ((inet_pton(domain, opts.ip, (ipv6 == 1) ?
			(void*)&sws6.sin6_addr : (void*)&sws.sin_addr)) <= 0) {
			fprintf(stderr, "Invalid IP\n");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	} else {
		if (!ipv6)
			sws.sin_addr.s_addr = INADDR_ANY;
		else
			sws6.sin6_addr = in6addr_any;
	}

	/* Set maximum connections */
	max_connections = (opts.debug == 1) ? 1 : MAX_CONN;

	/* Create socket */
	if ((sock = socket(domain, SOCK_STREAM, 0)) < 0) {
		perror("opening stream socket");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	opt = 1;
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	/* Bind to specified address(es) */
	sws.sin_family = sws6.sin6_family = domain;
	if (bind(sock, (ipv6 == 1) ?
		(struct sockaddr*)&sws6 : (struct sockaddr*)&sws,
		(ipv6 == 1) ? sizeof(sws6) : sizeof(sws))) {
		perror("binding stream socket");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	/* Set up signal handler */
	sig.sa_handler = reap;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sig, NULL) < 0) {
		perror("sigaction");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	/* Connection accept loop */
	cur_connections = 0;
	pending_connections = PENDING_CONN;

	/* Listen on socket */
	if (listen(sock, pending_connections) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	/* Accept loop */
	do {
		conn = accept(sock, 0, &sin_size);
		if (conn == -1)
			perror("accept");
		else {
			cur_connections++;
			if (cur_connections > max_connections)
				close(conn);
			else {
				bzero(buf, sizeof(buf));
				/* Fork to handle connection */
				if ((pid = fork()) < 0) {
					perror("error forking for connection");
					exit(EXIT_FAILURE);
					/* NOTREACHED */
				}
				else if (pid == 0) {
					/* Child */
					close(sock);
					bzero(buf, sizeof(buf));

					/* Pass socket to handler */
					sws_request(conn);

					close(conn);
					(errno > 0) ? exit(EXIT_FAILURE) : exit(EXIT_SUCCESS);
					/* NOTREACHED */
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
	/* Wait for dead processes in non-blocking mode */
	cur_connections--;
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

int
main(int argc, char **argv) {

	pid_t pid;
	int fd;
	char flag;
	extern char *optarg;

	while((flag = getopt(argc, argv, "6c:dhi:k:l:p:s:")) != -1) {
		switch(flag) {
		case '6':
			ipv6 = 1;
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
				/* NOTREAHCED */
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
	}

	/* Option error checking done in sws_init */
	sws_init(opts);

	if (!opts.debug) {
		/* Daemonize if -d not set */
		if ((pid = fork()) < 0) {
			perror("error forking for daemon");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}

		/* Exit the parent */
		if (pid > 0) {
			exit(EXIT_SUCCESS);
			/* NOTREACHED */
		}

		if (setsid() < 0) {
			perror("setsid");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}

		if (chdir("/") < 0) {
			perror("chdir");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}

		/* Make sure we don't get output on console */
		fd = open(_PATH_DEVNULL, O_RDWR, 0);
		if (fd < 0) {
			perror("opening descriptor for daemon");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		} else {
			if (dup2(fd, STDIN_FILENO) < 0 ||
			    dup2(fd, STDOUT_FILENO) < 0 ||
			    dup2(fd, STDERR_FILENO) < 0) {
				perror("dup2");
				exit(EXIT_FAILURE);
				/* NOTREACHED */
			}
			if (fd > STDERR_FILENO)
				(void)close(fd);
		}
	}

	mainloop();

	return EXIT_SUCCESS;
}

void
usage(void) {
	fprintf(stderr,
		"usage: sws [-6dh][-c dir][-i address][-l file][-p port][-s dir -k key] dir\n");
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}
