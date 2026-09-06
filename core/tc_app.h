#ifndef TC_APP_H
#define TC_APP_H
/* The whole application: connect, draw, send. Platform-independent - a port's
 * main() brings up video/input/network and calls this. */
int tc_app_run(int W, int H, const char *host, int port, int use_tls,
               const char *nick, const char *hint);
#endif
