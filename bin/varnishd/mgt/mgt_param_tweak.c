/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2011 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Functions for tweaking parameters
 *
 */

#include "config.h"

#include <grp.h>
#include <limits.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mgt/mgt.h"
#include "common/heritage.h"
#include "common/params.h"

#include "mgt/mgt_param.h"
#include "waiter/waiter.h"
#include "vav.h"
#include "vcli.h"
#include "vcli_common.h"
#include "vcli_priv.h"
#include "vnum.h"
#include "vss.h"

#include "mgt_cli.h"

/*--------------------------------------------------------------------*/

static int
tweak_generic_timeout(struct vsb *vsb, volatile unsigned *dst, const char *arg)
{
	unsigned u;

	if (arg != NULL) {
		u = strtoul(arg, NULL, 0);
		if (u == 0) {
			VSB_printf(vsb, "Timeout must be greater than zero\n");
			return (-1);
		}
		*dst = u;
	} else
		VSB_printf(vsb, "%u", *dst);
	return (0);
}

/*--------------------------------------------------------------------*/

int
tweak_timeout(struct vsb *vsb, const struct parspec *par, const char *arg)
{
	volatile unsigned *dest;

	dest = par->priv;
	return (tweak_generic_timeout(vsb, dest, arg));
}

/*--------------------------------------------------------------------*/

static int
tweak_generic_timeout_double(struct vsb *vsb, volatile double *dest,
    const char *arg, double min, double max)
{
	double u;
	char *p;

	if (arg != NULL) {
		p = NULL;
		u = strtod(arg, &p);
		if (*arg == '\0' || *p != '\0') {
			VSB_printf(vsb, "Not a number(%s)\n", arg);
			return (-1);
		}
		if (u < min) {
			VSB_printf(vsb,
			    "Timeout must be greater or equal to %.g\n", min);
			return (-1);
		}
		if (u > max) {
			VSB_printf(vsb,
			    "Timeout must be less than or equal to %.g\n", max);
			return (-1);
		}
		*dest = u;
	} else
		VSB_printf(vsb, "%.6f", *dest);
	return (0);
}

int
tweak_timeout_double(struct vsb *vsb, const struct parspec *par,
    const char *arg)
{
	volatile double *dest;

	dest = par->priv;
	return (tweak_generic_timeout_double(vsb, dest, arg,
	    par->min, par->max));
}

/*--------------------------------------------------------------------*/

int
tweak_generic_double(struct vsb *vsb, const struct parspec *par,
    const char *arg)
{
	volatile double *dest;
	char *p;
	double u;

	dest = par->priv;
	if (arg != NULL) {
		p = NULL;
		u = strtod(arg, &p);
		if (*p != '\0') {
			VSB_printf(vsb,
			    "Not a number (%s)\n", arg);
			return (-1);
		}
		if (u < par->min) {
			VSB_printf(vsb,
			    "Must be greater or equal to %.g\n",
				 par->min);
			return (-1);
		}
		if (u > par->max) {
			VSB_printf(vsb,
			    "Must be less than or equal to %.g\n",
				 par->max);
			return (-1);
		}
		*dest = u;
	} else
		VSB_printf(vsb, "%f", *dest);
	return (0);
}

/*--------------------------------------------------------------------*/

int
tweak_bool(struct vsb *vsb, const struct parspec *par, const char *arg)
{
	volatile unsigned *dest;
	int mode = 0;

	if (!strcmp(par->def, "off") || !strcmp(par->def, "on"))
		mode = 1;

	dest = par->priv;
	if (arg != NULL) {
		if (!strcasecmp(arg, "off"))
			*dest = 0;
		else if (!strcasecmp(arg, "disable"))
			*dest = 0;
		else if (!strcasecmp(arg, "no"))
			*dest = 0;
		else if (!strcasecmp(arg, "false"))
			*dest = 0;
		else if (!strcasecmp(arg, "on"))
			*dest = 1;
		else if (!strcasecmp(arg, "enable"))
			*dest = 1;
		else if (!strcasecmp(arg, "yes"))
			*dest = 1;
		else if (!strcasecmp(arg, "true"))
			*dest = 1;
		else {
			VSB_printf(vsb, "%s",
			    mode ?
				"use \"on\" or \"off\"\n" :
				"use \"true\" or \"false\"\n");
			return (-1);
		}
	} else if (mode) {
		VSB_printf(vsb, "%s", *dest ? "on" : "off");
	} else {
		VSB_printf(vsb, "%s", *dest ? "true" : "false");
	}
	return (0);
}

/*--------------------------------------------------------------------*/

int
tweak_generic_uint(struct vsb *vsb, volatile unsigned *dest, const char *arg,
    unsigned min, unsigned max)
{
	unsigned u;
	char *p;

	if (arg != NULL) {
		p = NULL;
		if (!strcasecmp(arg, "unlimited"))
			u = UINT_MAX;
		else {
			u = strtoul(arg, &p, 0);
			if (*arg == '\0' || *p != '\0') {
				VSB_printf(vsb, "Not a number (%s)\n", arg);
				return (-1);
			}
		}
		if (u < min) {
			VSB_printf(vsb, "Must be at least %u\n", min);
			return (-1);
		}
		if (u > max) {
			VSB_printf(vsb, "Must be no more than %u\n", max);
			return (-1);
		}
		*dest = u;
	} else if (*dest == UINT_MAX) {
		VSB_printf(vsb, "unlimited");
	} else {
		VSB_printf(vsb, "%u", *dest);
	}
	return (0);
}

/*--------------------------------------------------------------------*/

int
tweak_uint(struct vsb *vsb, const struct parspec *par, const char *arg)
{
	volatile unsigned *dest;

	dest = par->priv;
	(void)tweak_generic_uint(vsb, dest, arg,
	    (uint)par->min, (uint)par->max);
	return (0);
}

/*--------------------------------------------------------------------*/

static void
fmt_bytes(struct vsb *vsb, uintmax_t t)
{
	const char *p;

	if (t & 0xff) {
		VSB_printf(vsb, "%jub", t);
		return;
	}
	for (p = "kMGTPEZY"; *p; p++) {
		if (t & 0x300) {
			VSB_printf(vsb, "%.2f%c", t / 1024.0, *p);
			return;
		}
		t /= 1024;
		if (t & 0x0ff) {
			VSB_printf(vsb, "%ju%c", t, *p);
			return;
		}
	}
	VSB_printf(vsb, "(bogus number)");
}

static int
tweak_generic_bytes(struct vsb *vsb, volatile ssize_t *dest, const char *arg,
    double min, double max)
{
	uintmax_t r;
	const char *p;

	if (arg != NULL) {
		p = VNUM_2bytes(arg, &r, 0);
		if (p != NULL) {
			VSB_printf(vsb, "Could not convert to bytes.\n");
			VSB_printf(vsb, "%s\n", p);
			VSB_printf(vsb,
			    "  Try something like '80k' or '120M'\n");
			return (-1);
		}
		if ((uintmax_t)((ssize_t)r) != r) {
			fmt_bytes(vsb, r);
			VSB_printf(vsb, " is too large for this architecture.\n");
			return (-1);
		}
		if (max != 0. && r > max) {
			VSB_printf(vsb, "Must be no more than ");
			fmt_bytes(vsb, (uintmax_t)max);
			VSB_printf(vsb, "\n");
			return (-1);
		}
		if (r < min) {
			VSB_printf(vsb, "Must be at least ");
			fmt_bytes(vsb, (uintmax_t)min);
			VSB_printf(vsb, "\n");
			return (-1);
		}
		*dest = r;
	} else {
		fmt_bytes(vsb, *dest);
	}
	return (0);
}

/*--------------------------------------------------------------------*/

int
tweak_bytes(struct vsb *vsb, const struct parspec *par, const char *arg)
{
	volatile ssize_t *dest;

	assert(par->min >= 0);
	dest = par->priv;
	return (tweak_generic_bytes(vsb, dest, arg, par->min, par->max));
}


/*--------------------------------------------------------------------*/

int
tweak_bytes_u(struct vsb *vsb, const struct parspec *par, const char *arg)
{
	volatile unsigned *d1;
	volatile ssize_t dest;

	assert(par->max <= UINT_MAX);
	assert(par->min >= 0);
	d1 = par->priv;
	dest = *d1;
	if (tweak_generic_bytes(vsb, &dest, arg, par->min, par->max))
		return (-1);
	*d1 = dest;
	return (0);
}

/*--------------------------------------------------------------------
 * XXX: slightly magic.  We want to initialize to "nobody" (XXX: shouldn't
 * XXX: that be something autocrap found for us ?) but we don't want to
 * XXX: fail initialization if that user doesn't exists, even though we
 * XXX: do want to fail it, in subsequent sets.
 * XXX: The magic init string is a hack for this.
 */

int
tweak_user(struct vsb *vsb, const struct parspec *par, const char *arg)
{
	struct passwd *pw;

	(void)par;
	if (arg != NULL) {
		if (*arg != '\0') {
			pw = getpwnam(arg);
			if (pw == NULL) {
				VSB_printf(vsb, "Unknown user");
				return(-1);
			}
			REPLACE(mgt_param.user, pw->pw_name);
			mgt_param.uid = pw->pw_uid;
		} else {
			mgt_param.uid = getuid();
		}
	} else if (mgt_param.user) {
		VSB_printf(vsb, "%s (%d)", mgt_param.user, (int)mgt_param.uid);
	} else {
		VSB_printf(vsb, "UID %d", (int)mgt_param.uid);
	}
	return (0);
}

/*--------------------------------------------------------------------
 * XXX: see comment for tweak_user, same thing here.
 */

int
tweak_group(struct vsb *vsb, const struct parspec *par, const char *arg)
{
	struct group *gr;

	(void)par;
	if (arg != NULL) {
		if (*arg != '\0') {
			gr = getgrnam(arg);
			if (gr == NULL) {
				VSB_printf(vsb, "Unknown group");
				return(-1);
			}
			REPLACE(mgt_param.group, gr->gr_name);
			mgt_param.gid = gr->gr_gid;
		} else {
			mgt_param.gid = getgid();
		}
	} else if (mgt_param.group) {
		VSB_printf(vsb, "%s (%d)", mgt_param.group, (int)mgt_param.gid);
	} else {
		VSB_printf(vsb, "GID %d", (int)mgt_param.gid);
	}
	return (0);
}

/*--------------------------------------------------------------------*/

static void
clean_listen_sock_head(struct listen_sock_head *lsh)
{
	struct listen_sock *ls, *ls2;

	VTAILQ_FOREACH_SAFE(ls, lsh, list, ls2) {
		CHECK_OBJ_NOTNULL(ls, LISTEN_SOCK_MAGIC);
		VTAILQ_REMOVE(lsh, ls, list);
		free(ls->name);
		free(ls->addr);
		FREE_OBJ(ls);
	}
}

int
tweak_listen_address(struct vsb *vsb, const struct parspec *par,
    const char *arg)
{
	char **av;
	int i, retval = 0;
	struct listen_sock		*ls;
	struct listen_sock_head		lsh;

	(void)par;
	if (arg == NULL) {
		VSB_quote(vsb, mgt_param.listen_address, -1, 0);
		return (0);
	}

	av = VAV_Parse(arg, NULL, ARGV_COMMA);
	if (av == NULL) {
		VSB_printf(vsb, "Parse error: out of memory");
		return(-1);
	}
	if (av[0] != NULL) {
		VSB_printf(vsb, "Parse error: %s", av[0]);
		VAV_Free(av);
		return(-1);
	}
	if (av[1] == NULL) {
		VSB_printf(vsb, "Empty listen address");
		VAV_Free(av);
		return(-1);
	}
	VTAILQ_INIT(&lsh);
	for (i = 1; av[i] != NULL; i++) {
		struct vss_addr **ta;
		int j, n;

		n = VSS_resolve(av[i], "http", &ta);
		if (n == 0) {
			VSB_printf(vsb, "Invalid listen address ");
			VSB_quote(vsb, av[i], -1, 0);
			retval = -1;
			break;
		}
		for (j = 0; j < n; ++j) {
			ALLOC_OBJ(ls, LISTEN_SOCK_MAGIC);
			AN(ls);
			ls->sock = -1;
			ls->addr = ta[j];
			ls->name = strdup(av[i]);
			AN(ls->name);
			VTAILQ_INSERT_TAIL(&lsh, ls, list);
		}
		free(ta);
	}
	VAV_Free(av);
	if (retval) {
		clean_listen_sock_head(&lsh);
		return (-1);
	}

	REPLACE(mgt_param.listen_address, arg);

	clean_listen_sock_head(&heritage.socks);
	heritage.nsocks = 0;

	while (!VTAILQ_EMPTY(&lsh)) {
		ls = VTAILQ_FIRST(&lsh);
		VTAILQ_REMOVE(&lsh, ls, list);
		CHECK_OBJ_NOTNULL(ls, LISTEN_SOCK_MAGIC);
		VTAILQ_INSERT_TAIL(&heritage.socks, ls, list);
		heritage.nsocks++;
	}
	return (0);
}

/*--------------------------------------------------------------------*/

int
tweak_string(struct vsb *vsb, const struct parspec *par, const char *arg)
{
	char **p = TRUST_ME(par->priv);

	AN(p);
	/* XXX should have tweak_generic_string */
	if (arg == NULL) {
		VSB_quote(vsb, *p, -1, 0);
	} else {
		REPLACE(*p, arg);
	}
	return (0);
}

/*--------------------------------------------------------------------*/

int
tweak_waiter(struct vsb *vsb, const struct parspec *par, const char *arg)
{

	/* XXX should have tweak_generic_string */
	(void)par;
	return (WAIT_tweak_waiter(vsb, arg));
}

/*--------------------------------------------------------------------*/

int
tweak_poolparam(struct vsb *vsb, const struct parspec *par, const char *arg)
{
	volatile struct poolparam *pp, px;
	char **av;
	int retval = 0;

	pp = par->priv;
	if (arg == NULL) {
		VSB_printf(vsb, "%u,%u,%g",
		    pp->min_pool, pp->max_pool, pp->max_age);
	} else {
		av = VAV_Parse(arg, NULL, ARGV_COMMA);
		do {
			if (av[0] != NULL) {
				VSB_printf(vsb, "Parse error: %s", av[0]);
				retval = -1;
				break;
			}
			if (av[1] == NULL || av[2] == NULL || av[3] == NULL) {
				VSB_printf(vsb,
				    "Three fields required:"
				    " min_pool, max_pool and max_age\n");
				retval = -1;
				break;
			}
			px = *pp;
			retval = tweak_generic_uint(vsb, &px.min_pool, av[1],
			    (uint)par->min, (uint)par->max);
			if (retval)
				break;
			retval = tweak_generic_uint(vsb, &px.max_pool, av[2],
			    (uint)par->min, (uint)par->max);
			if (retval)
				break;
			retval = tweak_generic_timeout_double(vsb,
			    &px.max_age, av[3], 0, 1e6);
			if (retval)
				break;
			if (px.min_pool > px.max_pool) {
				VSB_printf(vsb,
				    "min_pool cannot be larger"
				    " than max_pool\n");
				retval = -1;
				break;
			}
			*pp = px;
		} while(0);
		VAV_Free(av);
	}
	return (retval);
}
