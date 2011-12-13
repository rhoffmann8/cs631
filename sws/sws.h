#ifndef _SWS_H_
#define _SWS_H_

/* This struct can be used to pass command-line options to the SWS
 * library via the sws_init function. */
struct swsopts {
	char *cgidir;
	int debug;
	char *dir;
	char *ip;
	char *logfile;
	int port;
	char *secdir;
	char *key;
} opts;

struct request {
	int method;
	int simple;
	char *path;
	char *if_mod_since;
} request;

/* Passed a struct swsopts, this function initializes the internal
 * variables used by the SWS library.  Furthermore, this function performs
 * whatever other initialization tasks are necessary.  This includes (but
 * is not limited to) verifying the validity of the given options or
 * opening any logfiles. */
void sws_init(const struct swsopts);

/* This function will log the given message to the appropriate location,
 * ie the logfile (if any) or stderr if in debug mode. */
void sws_log(const char *);

/* This function takes a connected socket as its only argument.  It will
 * read data from the socket, handle the request and write a response to
 * the socket as appropriate.  The socket passed is assumed to be
 * connected and the function will not close it. */
void sws_request(const int);

int sws_get_line(int, char*, int);

int sws_parse_method(struct request*, char*);

#endif /* _SWS_H_ */
