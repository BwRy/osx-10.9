/*
 * Copyright (c) 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <fcntl.h>

#include "roken.h"
#include "resolve.h"

static unsigned char basic[] = {
    0x12, 0x67, 0x84, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x03, 'f', 'o', 'o', 0x00,
    0x00, 0x10, 0x00, 0x01,
    0x03, 'f', 'o', 'o', 0x00,
    0x00, 0x10, 0x00, 0x01,
    0x00, 0x00, 0x12, 0x67, 0xff, 0xff
};
static size_t basic_len = 36;

static unsigned char dns_srv__kerberos__udp_SU_SE_[] = {
  0x24, 0x5a, 0x01, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
  0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62, 0x65, 0x72, 0x6f, 0x73, 0x04, 0x5f,
  0x75, 0x64, 0x70, 0x02, 0x53, 0x55, 0x02, 0x53, 0x45, 0x00, 0x00, 0x21,
  0x00, 0x01, 0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62, 0x65, 0x72, 0x6f, 0x73,
  0x04, 0x5f, 0x75, 0x64, 0x70, 0x02, 0x53, 0x55, 0x02, 0x53, 0x45, 0x00,
  0x00, 0x21, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4d, 0x00, 0x20, 0x00, 0x0a,
  0x00, 0x01, 0x00, 0x58, 0x0f, 0x6b, 0x64, 0x63, 0x2d, 0x70, 0x72, 0x6f,
  0x64, 0x2d, 0x73, 0x6c, 0x61, 0x76, 0x65, 0x32, 0x02, 0x69, 0x74, 0x02,
  0x73, 0x75, 0x02, 0x73, 0x65, 0x00, 0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62,
  0x65, 0x72, 0x6f, 0x73, 0x04, 0x5f, 0x75, 0x64, 0x70, 0x02, 0x53, 0x55,
  0x02, 0x53, 0x45, 0x00, 0x00, 0x21, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4d,
  0x00, 0x20, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x58, 0x0f, 0x6b, 0x64, 0x63,
  0x2d, 0x70, 0x72, 0x6f, 0x64, 0x2d, 0x73, 0x6c, 0x61, 0x76, 0x65, 0x33,
  0x02, 0x69, 0x74, 0x02, 0x73, 0x75, 0x02, 0x73, 0x65, 0x00
};
static size_t dns_srv__kerberos__udp_SU_SE__len = 166;

static unsigned char dns_srv__kerberos__udp_KTH_SE_[] = {
    0xf0, 0xc5, 0x01, 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62, 0x65, 0x72, 0x6f, 0x73, 0x04, 0x5f,
    0x75, 0x64, 0x70, 0x03, 0x4b, 0x54, 0x48, 0x02, 0x53, 0x45, 0x00, 0x00,
    0x21, 0x00, 0x01, 0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62, 0x65, 0x72, 0x6f,
    0x73, 0x04, 0x5f, 0x75, 0x64, 0x70, 0x03, 0x4b, 0x54, 0x48, 0x02, 0x53,
    0x45, 0x00, 0x00, 0x21, 0x00, 0x01, 0x00, 0x00, 0x23, 0x2a, 0x00, 0x1e,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x58, 0x0b, 0x6b, 0x72, 0x62, 0x2d, 0x73,
    0x6c, 0x61, 0x76, 0x65, 0x2d, 0x31, 0x03, 0x73, 0x79, 0x73, 0x03, 0x6b,
    0x74, 0x68, 0x02, 0x73, 0x65, 0x00, 0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62,
    0x65, 0x72, 0x6f, 0x73, 0x04, 0x5f, 0x75, 0x64, 0x70, 0x03, 0x4b, 0x54,
    0x48, 0x02, 0x53, 0x45, 0x00, 0x00, 0x21, 0x00, 0x01, 0x00, 0x00, 0x23,
    0x2a, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x05, 0x6b, 0x72,
    0x62, 0x2d, 0x31, 0x03, 0x73, 0x79, 0x73, 0x03, 0x6b, 0x74, 0x68, 0x02,
    0x73, 0x65, 0x00, 0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62, 0x65, 0x72, 0x6f,
    0x73, 0x04, 0x5f, 0x75, 0x64, 0x70, 0x03, 0x4b, 0x54, 0x48, 0x02, 0x53,
    0x45, 0x00, 0x00, 0x21, 0x00, 0x01, 0x00, 0x00, 0x23, 0x2a, 0x00, 0x1e,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x58, 0x0b, 0x6b, 0x72, 0x62, 0x2d, 0x73,
    0x6c, 0x61, 0x76, 0x65, 0x2d, 0x32, 0x03, 0x73, 0x79, 0x73, 0x03, 0x6b,
    0x74, 0x68, 0x02, 0x73, 0x65, 0x00
};
static size_t dns_srv__kerberos__udp_KTH_SE__len = 222;

static unsigned char dns_srv__kerberos__udp_ATHENA_MIT_EDU_[] = {
    0x6d, 0x4d, 0x01, 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62, 0x65, 0x72, 0x6f, 0x73, 0x04, 0x5f,
    0x75, 0x64, 0x70, 0x06, 0x41, 0x54, 0x48, 0x45, 0x4e, 0x41, 0x03, 0x4d,
    0x49, 0x54, 0x03, 0x45, 0x44, 0x55, 0x00, 0x00, 0x21, 0x00, 0x01, 0x09,
    0x5f, 0x6b, 0x65, 0x72, 0x62, 0x65, 0x72, 0x6f, 0x73, 0x04, 0x5f, 0x75,
    0x64, 0x70, 0x06, 0x41, 0x54, 0x48, 0x45, 0x4e, 0x41, 0x03, 0x4d, 0x49,
    0x54, 0x03, 0x45, 0x44, 0x55, 0x00, 0x00, 0x21, 0x00, 0x01, 0x00, 0x00,
    0x08, 0xcc, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x0a, 0x4b,
    0x45, 0x52, 0x42, 0x45, 0x52, 0x4f, 0x53, 0x2d, 0x31, 0x03, 0x4d, 0x49,
    0x54, 0x03, 0x45, 0x44, 0x55, 0x00, 0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62,
    0x65, 0x72, 0x6f, 0x73, 0x04, 0x5f, 0x75, 0x64, 0x70, 0x06, 0x41, 0x54,
    0x48, 0x45, 0x4e, 0x41, 0x03, 0x4d, 0x49, 0x54, 0x03, 0x45, 0x44, 0x55,
    0x00, 0x00, 0x21, 0x00, 0x01, 0x00, 0x00, 0x08, 0xcc, 0x00, 0x1a, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x58, 0x0a, 0x4b, 0x45, 0x52, 0x42, 0x45, 0x52,
    0x4f, 0x53, 0x2d, 0x32, 0x03, 0x4d, 0x49, 0x54, 0x03, 0x45, 0x44, 0x55,
    0x00, 0x09, 0x5f, 0x6b, 0x65, 0x72, 0x62, 0x65, 0x72, 0x6f, 0x73, 0x04,
    0x5f, 0x75, 0x64, 0x70, 0x06, 0x41, 0x54, 0x48, 0x45, 0x4e, 0x41, 0x03,
    0x4d, 0x49, 0x54, 0x03, 0x45, 0x44, 0x55, 0x00, 0x00, 0x21, 0x00, 0x01,
    0x00, 0x00, 0x08, 0xcc, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58,
    0x08, 0x4b, 0x45, 0x52, 0x42, 0x45, 0x52, 0x4f, 0x53, 0x03, 0x4d, 0x49,
    0x54, 0x03, 0x45, 0x44, 0x55, 0x00
};
static size_t dns_srv__kerberos__udp_ATHENA_MIT_EDU__len = 246;

struct testcase {
    unsigned char *buf;
    size_t buf_len;
};

#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

static sig_atomic_t val = 0;

static RETSIGTYPE
segv_handler(int sig)
{
    val = 1;
}

int
main(int argc, char **argv)
{
#ifndef HAVE_MMAP
    return 77;			/* signal to automake that this test
                                   cannot be run */
#else /* HAVE_MMAP */
    size_t n, count = 10000;
    int ret, i;
    struct sigaction sa;
    
    struct testcase tests[4];
    
    memset(tests, 0, sizeof(tests));
    tests[0].buf     = basic;
    tests[0].buf_len = basic_len;
    tests[1].buf     = dns_srv__kerberos__udp_SU_SE_;
    tests[1].buf_len = dns_srv__kerberos__udp_SU_SE__len;
    tests[2].buf     = dns_srv__kerberos__udp_KTH_SE_;
    tests[2].buf_len = dns_srv__kerberos__udp_KTH_SE__len;
    tests[3].buf     = dns_srv__kerberos__udp_ATHENA_MIT_EDU_;
    tests[3].buf_len = dns_srv__kerberos__udp_ATHENA_MIT_EDU__len;

    
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = segv_handler;
    sigaction (SIGSEGV, &sa, NULL);

    size_t pagesize = getpagesize();

    for (i = 0; val == 0 && i < sizeof(tests)/sizeof(tests[0]); ++i) {
	const struct testcase *t = &tests[i];
	struct rk_dns_reply *reply;
	unsigned char *p1, *p2;
	unsigned char *buf;

	printf("test %d: ", i);

	p1 = (unsigned char *)mmap(0, 2 * pagesize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (p1 == (unsigned char *)MAP_FAILED)
	    err (1, "mmap");
	p2 = p1 + pagesize;
	ret = mprotect ((void *)p2, pagesize, 0);
	if (ret < 0)
	    err (1, "mprotect");
	buf = p2 - t->buf_len;

	for (n = 0; n < count; n++) {

	    memcpy (buf, t->buf, t->buf_len);
	    if (n != 0)
		buf[n % t->buf_len] = arc4random();

	    reply = rk_dns_parse_reply (buf, t->buf_len);
	    if (reply) {
		if(reply->q.type == rk_ns_t_srv)
		    rk_dns_srv_order(reply);
		rk_dns_free_data(reply);
	    }
	    if ((n % 1000) == 0)
		printf("%d...", (int)n);
	}
	printf("\n");
	ret = munmap ((void *)p1, 2 * pagesize);
	if (ret < 0)
	    err (1, "munmap");
    }
    return val;
#endif /* HAVE_MMAP */
}
