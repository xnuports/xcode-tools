/*
 * agvtool -- Apple-generic versioning for Xcode projects.
 *
 * Two numbers, kept in two places.  The build number lives in the project
 * file as CURRENT_PROJECT_VERSION and DYLIB_CURRENT_VERSION, and, when -all
 * is given, as CFBundleVersion in each target's Info.plist.  The marketing
 * version lives only in the Info.plists, as CFBundleShortVersionString.
 *
 * Both files are edited in place, a value at a time, rather than parsed and
 * written back: a project file carries comments, ordering and an Xcode
 * layout that nothing here has any business rewriting, and the same goes
 * for a hand-edited Info.plist.
 *
 * The tag and submit operations wanted CVS or Subversion, neither of which
 * macOS has shipped for years, so they say what Apple's says when neither
 * is enabled.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Apple's exit statuses, which scripts around this tool test. */
#define EX_NOOPERATION	1
#define EX_BADOPERATION	2
#define EX_NOPROJECT	3
#define EX_NOSCM	4
#define EX_NOMARKETING	6

struct strv {
	char	**v;
	size_t	  n;
};

static void
strv_add(struct strv *s, char *item)
{
	char **nv = realloc(s->v, (s->n + 1) * sizeof(*nv));

	if (nv == NULL)
		err(1, NULL);
	s->v = nv;
	s->v[s->n++] = item;
}

static bool
strv_has(const struct strv *s, const char *item)
{
	size_t i;

	for (i = 0; i < s->n; i++) {
		if (strcmp(s->v[i], item) == 0)
			return (true);
	}
	return (false);
}

static void
usage(void)
{
	printf("\n");
	printf("agvtool - Apple-generic versioning tool for Xcode projects\n");
	printf("  usage:\n");
	printf("    agvtool help\n");
	printf("    agvtool what-version | vers [-terse]\n");
	printf("    agvtool [-noscm | -usecvs | -usesvn] next-version | bump "
	    "[-all]\n");
	printf("    agvtool [-noscm | -usecvs | -usesvn] new-version [-all] "
	    "<versNum>\n");
	printf("    agvtool [-noscm | -usecvs | -usesvn] tag [-force | -F] "
	    "[-noupdatecheck | -Q] [-baseurlfortag]\n");
	printf("    agvtool what-marketing-version | mvers [-terse | "
	    "-terse1]\n");
	printf("    agvtool [-noscm | -usecvs | -usesvn] new-marketing-version "
	    "<versString>\n");
	printf("\n");
}

static char *
xstrdup(const char *s)
{
	char *p = strdup(s);

	if (p == NULL)
		err(1, NULL);
	return (p);
}

static char *
read_file(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	char *buf;
	long n;

	if (f == NULL)
		return (NULL);
	if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0) {
		fclose(f);
		return (NULL);
	}
	rewind(f);
	if ((buf = malloc((size_t)n + 1)) == NULL)
		err(1, NULL);
	if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
		free(buf);
		fclose(f);
		return (NULL);
	}
	fclose(f);
	buf[n] = '\0';
	if (len != NULL)
		*len = (size_t)n;
	return (buf);
}

static bool
write_file(const char *path, const char *buf, size_t len)
{
	FILE *f = fopen(path, "wb");

	if (f == NULL)
		return (false);
	if (fwrite(buf, 1, len, f) != len) {
		fclose(f);
		return (false);
	}
	return (fclose(f) == 0);
}

/* The one .xcodeproj in the working directory. */
static char *
find_project(void)
{
	DIR *d = opendir(".");
	struct dirent *e;
	char *found = NULL;

	if (d == NULL)
		return (NULL);
	while ((e = readdir(d)) != NULL) {
		size_t n = strlen(e->d_name);

		if (n < 11 || strcmp(e->d_name + n - 10, ".xcodeproj") != 0)
			continue;
		if (found == NULL)
			found = xstrdup(e->d_name);
	}
	closedir(d);
	return (found);
}

/*
 * Collect the values of `<key> = <value>;' in a project file.  The syntax is
 * an old-style property list, but every setting sits on its own line, which
 * is all this needs to know.
 */
static void
pbx_values(const char *text, const char *key, struct strv *out)
{
	size_t keylen = strlen(key);
	const char *p = text;

	while ((p = strstr(p, key)) != NULL) {
		const char *q = p + keylen, *end;
		char *val;

		/* The name has to stand alone, not end another setting. */
		if (p != text && (isalnum((unsigned char)p[-1]) ||
		    p[-1] == '_')) {
			p = q;
			continue;
		}
		while (*q == ' ' || *q == '\t')
			q++;
		if (*q != '=') {
			p = q;
			continue;
		}
		q++;
		while (*q == ' ' || *q == '\t')
			q++;
		for (end = q; *end != '\0' && *end != ';' && *end != '\n';
		    end++)
			;
		val = malloc((size_t)(end - q) + 1);
		if (val == NULL)
			err(1, NULL);
		memcpy(val, q, (size_t)(end - q));
		val[end - q] = '\0';
		/* Values Xcode chose to quote are still just values. */
		if (val[0] == '"' && strlen(val) > 1 &&
		    val[strlen(val) - 1] == '"') {
			memmove(val, val + 1, strlen(val) - 1);
			val[strlen(val) - 2] = '\0';
		}
		if (!strv_has(out, val))
			strv_add(out, val);
		else
			free(val);
		p = end;
	}
}

/* Replace every `<key> = <value>;' value, returning how many changed. */
static char *
pbx_set(const char *text, const char *key, const char *value, int *changed)
{
	size_t keylen = strlen(key), cap = strlen(text) + 256, len = 0;
	char *out = malloc(cap);
	const char *p = text;

	if (out == NULL)
		err(1, NULL);
	*changed = 0;
	while (*p != '\0') {
		const char *hit = strstr(p, key), *q, *end;
		size_t n;

		if (hit == NULL)
			break;
		q = hit + keylen;
		if ((hit != text && (isalnum((unsigned char)hit[-1]) ||
		    hit[-1] == '_'))) {
			n = (size_t)(q - p);
			goto copy;
		}
		while (*q == ' ' || *q == '\t')
			q++;
		if (*q != '=') {
			n = (size_t)(q - p);
			goto copy;
		}
		q++;
		while (*q == ' ' || *q == '\t')
			q++;
		for (end = q; *end != '\0' && *end != ';' && *end != '\n';
		    end++)
			;
		n = (size_t)(q - p);
		while (len + n + strlen(value) + 1 > cap) {
			cap *= 2;
			if ((out = realloc(out, cap)) == NULL)
				err(1, NULL);
		}
		memcpy(out + len, p, n);
		len += n;
		memcpy(out + len, value, strlen(value));
		len += strlen(value);
		(*changed)++;
		p = end;
		continue;
copy:
		while (len + n + 1 > cap) {
			cap *= 2;
			if ((out = realloc(out, cap)) == NULL)
				err(1, NULL);
		}
		memcpy(out + len, p, n);
		len += n;
		p += n;
	}
	{
		size_t n = strlen(p);

		while (len + n + 1 > cap) {
			cap *= 2;
			if ((out = realloc(out, cap)) == NULL)
				err(1, NULL);
		}
		memcpy(out + len, p, n);
		len += n;
		out[len] = '\0';
	}
	return (out);
}

/*
 * Read one string value out of an XML property list.  A whole plist parser
 * is more machinery than a single <key>/<string> pair needs, and staying
 * textual is what lets the file be written back untouched apart from that
 * one value.
 */
static const char *
plist_find(const char *text, const char *key, size_t *vlen)
{
	char pattern[256];
	const char *p, *q;

	snprintf(pattern, sizeof(pattern), "<key>%s</key>", key);
	if ((p = strstr(text, pattern)) == NULL)
		return (NULL);
	if ((p = strstr(p, "<string>")) == NULL)
		return (NULL);
	p += strlen("<string>");
	if ((q = strstr(p, "</string>")) == NULL)
		return (NULL);
	*vlen = (size_t)(q - p);
	return (p);
}

static bool
plist_set(const char *path, const char *key, const char *value)
{
	size_t len, vlen;
	char *text = read_file(path, &len);
	const char *at;
	char *out;
	bool ok;

	if (text == NULL)
		return (false);
	if ((at = plist_find(text, key, &vlen)) == NULL) {
		free(text);
		return (false);
	}
	if ((out = malloc(len - vlen + strlen(value) + 1)) == NULL)
		err(1, NULL);
	memcpy(out, text, (size_t)(at - text));
	memcpy(out + (at - text), value, strlen(value));
	memcpy(out + (at - text) + strlen(value), at + vlen,
	    len - (size_t)(at - text) - vlen + 1);
	ok = write_file(path, out, len - vlen + strlen(value));
	free(out);
	free(text);
	return (ok);
}

/*
 * The Info.plists of the project's targets, named the way agvtool reports
 * them: the INFOPLIST_FILE setting hung off the project bundle, so that a
 * path relative to the project directory reads as it was written.
 */
static void
info_plists(const char *project, const char *pbx_text, struct strv *out)
{
	struct strv files = { NULL, 0 };
	size_t i;

	pbx_values(pbx_text, "INFOPLIST_FILE", &files);
	for (i = 0; i < files.n; i++) {
		char *path;

		if (files.v[i][0] == '\0')
			continue;
		if (asprintf(&path, "%s/../%s", project, files.v[i]) < 0)
			err(1, NULL);
		if (!strv_has(out, path))
			strv_add(out, path);
		else
			free(path);
	}
}

static int
what_version(const char *project, const char *pbx_text, bool terse)
{
	struct strv v = { NULL, 0 };
	char *name = xstrdup(project);
	char *dot = strrchr(name, '.');

	if (dot != NULL)
		*dot = '\0';
	pbx_values(pbx_text, "CURRENT_PROJECT_VERSION", &v);
	if (v.n == 0) {
		if (!terse)
			printf("There does not seem to be a CURRENT_PROJECT_"
			    "VERSION key set for this project.  Add this key "
			    "to your target's expert build settings.\n\n");
		return (EX_NOMARKETING);
	}
	if (terse) {
		printf("%s\n", v.v[0]);
		return (0);
	}
	printf("Current version of project %s is: \n", name);
	printf("    %s\n\n", v.v[0]);
	return (0);
}

static int
set_version(const char *project, char *pbx_path, char *pbx_text,
    const char *value, bool all)
{
	char *name = xstrdup(project);
	char *dot = strrchr(name, '.');
	char *out, *out2;
	struct strv plists = { NULL, 0 };
	int changed, changed2;
	size_t i;

	if (dot != NULL)
		*dot = '\0';
	printf("Setting version of project %s to: \n", name);
	printf("    %s.\n\n", value);

	out = pbx_set(pbx_text, "CURRENT_PROJECT_VERSION", value, &changed);
	out2 = pbx_set(out, "DYLIB_CURRENT_VERSION", value, &changed2);
	free(out);
	if (!write_file(pbx_path, out2, strlen(out2))) {
		warn("%s", pbx_path);
		free(out2);
		return (1);
	}

	if (!all) {
		free(out2);
		return (0);
	}
	printf("Also setting CFBundleVersion key (assuming it exists)\n\n");
	printf("Updating CFBundleVersion in Info.plist(s)...\n\n");
	info_plists(project, out2, &plists);
	for (i = 0; i < plists.n; i++) {
		if (plist_set(plists.v[i], "CFBundleVersion", value))
			printf("Updated CFBundleVersion in \"%s\" to %s\n\n",
			    plists.v[i], value);
	}
	printf("\n");
	free(out2);
	return (0);
}

static int
what_marketing_version(const char *project, const char *pbx_text, int terse)
{
	struct strv plists = { NULL, 0 };
	size_t i;
	int found = 0;

	if (terse == 0) {
		printf("No marketing version number "
		    "(CFBundleShortVersionString) found for Jambase "
		    "targets.\n\n");
		printf("Looking for marketing version in native targets...\n");
	}
	info_plists(project, pbx_text, &plists);
	if (plists.n == 0) {
		if (terse == 0)
			printf("No native targets with Info.plist files "
			    "found.\n\n");
		return (EX_NOMARKETING);
	}
	if (terse == 0)
		printf("Looking for marketing version "
		    "(CFBundleShortVersionString) in native targets...\n\n");
	for (i = 0; i < plists.n; i++) {
		size_t vlen;
		const char *at;
		char *text = read_file(plists.v[i], NULL);

		if (text == NULL)
			continue;
		at = plist_find(text, "CFBundleShortVersionString", &vlen);
		if (at != NULL) {
			found++;
			if (terse == 0)
				printf("Found CFBundleShortVersionString of "
				    "\"%.*s\" in \"%s\" \n", (int)vlen, at,
				    plists.v[i]);
			else if (terse == 1)
				printf("\"%s\"=%.*s\n", plists.v[i], (int)vlen,
				    at);
			else
				printf("%.*s\n", (int)vlen, at);
		}
		free(text);
	}
	return (found > 0 ? 0 : EX_NOMARKETING);
}

static int
new_marketing_version(const char *project, const char *pbx_text,
    const char *value)
{
	struct strv plists = { NULL, 0 };
	char *name = xstrdup(project);
	char *dot = strrchr(name, '.');
	size_t i;

	if (dot != NULL)
		*dot = '\0';
	printf("Setting CFBundleShortVersionString of project %s to: \n", name);
	printf("    %s.\n\n", value);
	printf("Updating CFBundleShortVersionString in Info.plist(s)...\n\n");
	info_plists(project, pbx_text, &plists);
	for (i = 0; i < plists.n; i++) {
		if (plist_set(plists.v[i], "CFBundleShortVersionString", value))
			printf("Updated CFBundleShortVersionString in \"%s\" "
			    "to %s\n", plists.v[i], value);
	}
	return (0);
}

/* The next whole number above the current one: 54 becomes 55, 234.6 235. */
static char *
bumped(const char *current)
{
	double v = strtod(current, NULL);
	char *out;

	if (asprintf(&out, "%.0f", floor(v) + 1) < 0)
		err(1, NULL);
	return (out);
}

int
main(int argc, char *argv[])
{
	const char *op = NULL, *arg = NULL;
	char *project, *pbx_path, *pbx_text;
	bool all = false, terse = false, terse1 = false;
	int i, rc;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "-noscm") == 0 || strcmp(a, "-usecvs") == 0 ||
		    strcmp(a, "-usesvn") == 0 || strcmp(a, "-bytag") == 0 ||
		    strcmp(a, "-notbytag") == 0)
			continue;
		if (strcmp(a, "-all") == 0) {
			all = true;
			continue;
		}
		if (strcmp(a, "-terse") == 0) {
			terse = true;
			continue;
		}
		if (strcmp(a, "-terse1") == 0) {
			terse1 = true;
			continue;
		}
		if (op == NULL)
			op = a;
		else if (arg == NULL)
			arg = a;
	}

	if (op == NULL) {
		printf("Operation specifier required.\n");
		usage();
		return (EX_NOOPERATION);
	}
	if (strcmp(op, "help") == 0) {
		usage();
		printf("\nFor more information type \"man agvtool\".\n");
		return (0);
	}
	if (strcmp(op, "tag") == 0 || strcmp(op, "submit") == 0) {
		printf("The %s operation is supported only if CVS or SVN "
		    "support is enabled.\n", op);
		usage();
		return (EX_NOSCM);
	}

	if (strcmp(op, "what-version") != 0 && strcmp(op, "vers") != 0 &&
	    strcmp(op, "next-version") != 0 && strcmp(op, "bump") != 0 &&
	    strcmp(op, "new-version") != 0 &&
	    strcmp(op, "what-marketing-version") != 0 &&
	    strcmp(op, "mvers") != 0 &&
	    strcmp(op, "new-marketing-version") != 0) {
		printf("Unrecognized operation specifier \"%s\".\n", op);
		usage();
		return (EX_BADOPERATION);
	}

	if ((project = find_project()) == NULL) {
		printf("There are no Xcode project files in this directory.  "
		    "agvtool needs a project to operate.\n");
		return (EX_NOPROJECT);
	}
	if (asprintf(&pbx_path, "%s/project.pbxproj", project) < 0)
		err(1, NULL);
	if ((pbx_text = read_file(pbx_path, NULL)) == NULL) {
		printf("There are no Xcode project files in this directory.  "
		    "agvtool needs a project to operate.\n");
		return (EX_NOPROJECT);
	}

	if (strcmp(op, "what-version") == 0 || strcmp(op, "vers") == 0)
		rc = what_version(project, pbx_text, terse);
	else if (strcmp(op, "next-version") == 0 || strcmp(op, "bump") == 0) {
		struct strv v = { NULL, 0 };
		char *next;

		pbx_values(pbx_text, "CURRENT_PROJECT_VERSION", &v);
		next = bumped(v.n > 0 ? v.v[0] : "0");
		rc = set_version(project, pbx_path, pbx_text, next, all);
		free(next);
	} else if (strcmp(op, "new-version") == 0) {
		if (arg == NULL) {
			printf("Operation specifier required.\n");
			usage();
			return (EX_NOOPERATION);
		}
		rc = set_version(project, pbx_path, pbx_text, arg, all);
	} else if (strcmp(op, "new-marketing-version") == 0) {
		if (arg == NULL) {
			printf("Operation specifier required.\n");
			usage();
			return (EX_NOOPERATION);
		}
		rc = new_marketing_version(project, pbx_text, arg);
	} else
		rc = what_marketing_version(project, pbx_text,
		    terse1 ? 2 : (terse ? 1 : 0));

	return (rc);
}
