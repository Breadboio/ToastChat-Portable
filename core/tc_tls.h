#ifndef TC_TLS_H
#define TC_TLS_H
#include <stddef.h>
#include "../platform/platform.h"

/* TLS over an already-connected tc_sock, using mbedtls with OUR OWN trust
 * anchors (core/tc_cacert.h). That is the important part: because we bundle the
 * roots and the cipher implementations, the console's own ancient CA store and
 * limited TLS support never enter into it.
 *
 * Built only when TC_HAVE_TLS is defined. Wii has no mbedtls portlib in
 * devkitPro, so it builds without and tc_tls_connect() reports failure. */

typedef struct tc_tls tc_tls;

tc_tls *tc_tls_connect(tc_sock *sock, const char *hostname);
int     tc_tls_send(tc_tls *t, const void *buf, size_t n);   /* -1 err, else n */
int     tc_tls_recv(tc_tls *t, void *buf, size_t n);         /* 0 = would-block */
void    tc_tls_free(tc_tls *t);
const char *tc_tls_error(void);
int     tc_tls_available(void);
#endif
