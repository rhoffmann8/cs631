#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sws.h"

char *__sws_cgidir;
char *__sws_dir;
int __sws_debug = 0;
char *__sws_ip;
char *__sws_logfile;
int __sws_port = 8080;
char *__sws_secdir;
char *__sws_key;

void
sws_init(const struct swsopts opts) {
	__sws_cgidir = opts.cgidir;
	__sws_debug = opts.debug;
	__sws_dir = opts.dir;
	__sws_ip = opts.ip;
	__sws_logfile = opts.logfile;
	__sws_port = opts.port;
	__sws_secdir = opts.secdir;
	__sws_key = opts.key;

	/* write me */
}

void
sws_log(const char *msg) {
	/* write me */
}

void
sws_request(const int sock) {

	socklen_t length;
	struct sockaddr_storage client;
	char ipstr[INET6_ADDRSTRLEN];
	int port;

	bzero(&client, sizeof(struct sockaddr_storage));
	bzero(ipstr, INET6_ADDRSTRLEN);
	length = sizeof(client);
	/* this is just an example of what one might want to do */
	if (__sws_debug) {
		if (getpeername(sock, (struct sockaddr *)&client, &length) == -1) {
			fprintf(stderr, "Unable to get socket name: %s\n",
					strerror(errno));
			return;
		}

		if (client.ss_family == AF_INET) {
			struct sockaddr_in *s = (struct sockaddr_in *)&client;
			port = ntohs(s->sin_port);
			if (!inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr))) {
				fprintf(stderr,"inet_ntop error: %s\n", strerror(errno));
				return;
			}
		} else { // AF_INET6
			struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client;
			port = ntohs(s->sin6_port);
			if (!inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr))) {
				fprintf(stderr,"inet_ntop error: %s\n", strerror(errno));
				return;
			}
		}

		fprintf(stderr, "Connection from %s remote port %d\n",
				ipstr, port);

	}

	/* write me */
}
