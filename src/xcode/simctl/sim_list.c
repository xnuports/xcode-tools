/*
 * sim_list - simctl list subcommand implementation.
 *
 * Lists devices, device types, runtimes, or pairs from the
 * CoreSimulator framework data on disk.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

#include "simctl.h"

const char *state_names[] = {
    [CS_STATE_UNKNOWN]  = "Unknown",
    [CS_STATE_SHUTDOWN] = "Shutdown",
    [CS_STATE_BOOTED]   = "Booted",
    [CS_STATE_CREATING] = "Creating",
    [CS_STATE_DELETING] = "Deleting",
};

/*
 * Read a string value from a plist file using plutil(8).
 * CoreSimulator plists are in binary format; plutil converts
 * to the -p text output format which we parse line-by-line.
 */
static char *
plist_read_string(const char *plist_path, const char *key)
{
	char cmd[2048];
	snprintf(cmd, sizeof cmd, "/usr/bin/plutil -p \"%s\" 2>/dev/null",
	    plist_path);

	FILE *f = popen(cmd, "r");
	if (f == NULL)
		return NULL;

	char line[1024];
	char *result = NULL;
	char key_search[128];
	snprintf(key_search, sizeof key_search, "\"%s\"", key);

	while (fgets(line, sizeof line, f) != NULL) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;

		if (strstr(p, key_search) != NULL) {
			char *v = strstr(p, "=>");
			if (v) {
				v += 2;
				while (*v == ' ' || *v == '\t') v++;
				if (*v == '"') {
					v++;
					char *end = strchr(v, '"');
					if (end) {
						*end = '\0';
						size_t len = strlen(v);
						result = malloc(len + 1);
						if (result)
							strcpy(result, v);
					}
				}
			}
			break;
		}
	}
	pclose(f);
	return result;
}

/* Read an integer value from a plist file */
static int
plist_read_int(const char *plist_path, const char *key)
{
	char cmd[2048];
	snprintf(cmd, sizeof cmd, "/usr/bin/plutil -p \"%s\" 2>/dev/null",
	    plist_path);

	FILE *f = popen(cmd, "r");
	if (f == NULL)
		return -1;

	char line[1024];
	char key_search[128];
	snprintf(key_search, sizeof key_search, "\"%s\"", key);
	int val = -1;

	while (fgets(line, sizeof line, f) != NULL) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;

		if (strstr(p, key_search) != NULL) {
			char *v = strstr(p, "=>");
			if (v) {
				v += 2;
				while (*v == ' ' || *v == '\t') v++;
				val = atoi(v);
			}
			break;
		}
	}
	pclose(f);
	return val;
}

/*
 * Read a boolean value from a plist file.
 * Returns 1 if true, 0 if false, -1 if not found.
 */
static int
plist_read_bool(const char *plist_path, const char *key)
{
	char cmd[2048];
	snprintf(cmd, sizeof cmd, "/usr/bin/plutil -p \"%s\" 2>/dev/null",
	    plist_path);

	FILE *f = popen(cmd, "r");
	if (f == NULL)
		return -1;

	char line[1024];
	char key_search[128];
	snprintf(key_search, sizeof key_search, "\"%s\"", key);
	int val = -1;

	while (fgets(line, sizeof line, f) != NULL) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;

		if (strstr(p, key_search) != NULL) {
			if (strstr(p, "=> true") != NULL ||
			    strstr(p, "=>true") != NULL)
				val = 1;
			else if (strstr(p, "=> false") != NULL ||
			    strstr(p, "=>false") != NULL)
				val = 0;
			break;
		}
	}
	pclose(f);
	return val;
}

/* Find the home directory */
static const char *
home_dir(void)
{
	static char path[1024];
	const char *h = getenv("HOME");
	if (h == NULL) {
		struct passwd *pw = getpwuid(getuid());
		if (pw)
			h = pw->pw_dir;
	}
	if (h != NULL) {
		snprintf(path, sizeof path, "%s", h);
		return path;
	}
	return ".";
}

/* Extract username from home path */
static const char *
home_user(void)
{
	static char user[256];
	const char *h = home_dir();
	const char *slash = strrchr(h, '/');
	if (slash && slash[1] != '\0') {
		snprintf(user, sizeof user, "%s", slash + 1);
		return user;
	}
	return "user";
}

/* Extract OS display name from runtime identifier.
 * e.g., com.apple.CoreSimulator.SimRuntime.iOS-26-5 -> "iOS 26.5" */
static void
runtime_os_display(const char *rt, char *out, size_t outsz)
{
	const char *prefix = "com.apple.CoreSimulator.SimRuntime.";
	out[0] = '\0';
	if (strncmp(rt, prefix, strlen(prefix)) != 0)
		return;
	const char *rest = rt + strlen(prefix);
	int major, minor;
	if (sscanf(rest, "iOS-%d-%d", &major, &minor) == 2)
		snprintf(out, outsz, "iOS %d.%d", major, minor);
	else if (sscanf(rest, "tvOS-%d-%d", &major, &minor) == 2)
		snprintf(out, outsz, "tvOS %d.%d", major, minor);
	else if (sscanf(rest, "watchOS-%d-%d", &major, &minor) == 2)
		snprintf(out, outsz, "watchOS %d.%d", major, minor);
	else if (sscanf(rest, "xrOS-%d-%d", &major, &minor) == 2)
		snprintf(out, outsz, "xrOS %d.%d", major, minor);
	else if (sscanf(rest, "visionOS-%d-%d", &major, &minor) == 2)
		snprintf(out, outsz, "visionOS %d.%d", major, minor);
	else
		snprintf(out, outsz, "%s", rest);
}

/* --- list devices --- */

void
list_devices(int json_mode, int verbose)
{
	char dev_root[1024];
	snprintf(dev_root, sizeof dev_root, "%s/%s",
	    home_dir(), CS_DEVICES_DIR);

	DIR *dir = opendir(dev_root);
	if (dir == NULL) {
		fprintf(stderr, "simctl: no CoreSimulator devices directory\n");
		return;
	}

	/* Collect devices into arrays for sorting */
	char *names[256];
	char *udids[256];
	char *device_types[256];
	char *runtimes[256];
	int states[256];
	char *states_str[256];
	int is_avail[256];
	int count = 0;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		char plist_path[1024];
		snprintf(plist_path, sizeof plist_path,
		    "%s/%s/device.plist", dev_root, entry->d_name);

		struct stat st;
		if (stat(plist_path, &st) != 0)
			continue;
		if (!S_ISREG(st.st_mode))
			continue;

		char *name = plist_read_string(plist_path, "name");
		char *udid_str = plist_read_string(plist_path, "UDID");
		char *runtime = plist_read_string(plist_path, "runtime");
		char *dt = plist_read_string(plist_path, "deviceType");
		int state = plist_read_int(plist_path, "state");
		int deleted = plist_read_bool(plist_path, "isDeleted");

		if (name == NULL || udid_str == NULL) {
			free(name);
			free(udid_str);
			free(runtime);
			free(dt);
			continue;
		}

		/* Skip deleted devices unless verbose */
		if (deleted == 1 && !verbose) {
			free(name);
			free(udid_str);
			free(runtime);
			free(dt);
			continue;
		}

		if (count < 256) {
			names[count] = name;
			udids[count] = udid_str;
			runtimes[count] = runtime ? runtime : strdup("");
			device_types[count] = dt ? dt : strdup("");
			states[count] = state;
			is_avail[count] = (deleted == 0);
			states_str[count] = strdup(state_names[
			    state >= 0 && state < 5 ? state : 0]);
			count++;
		} else {
			free(name);
			free(udid_str);
			free(runtime);
			free(dt);
		}
	}
	closedir(dir);

	/* Sort by runtime, then name */
	for (int i = 0; i < count - 1; i++) {
		for (int j = 0; j < count - i - 1; j++) {
			int swap = 0;
			if (strcmp(runtimes[j], runtimes[j + 1]) > 0)
				swap = 1;
			else if (strcmp(runtimes[j], runtimes[j + 1]) == 0 &&
			    strcmp(names[j], names[j + 1]) > 0)
				swap = 1;
			if (swap) {
				char *t;
				t = names[j]; names[j] = names[j + 1];
				    names[j + 1] = t;
				t = udids[j]; udids[j] = udids[j + 1];
				    udids[j + 1] = t;
				t = runtimes[j]; runtimes[j] = runtimes[j + 1];
				    runtimes[j + 1] = t;
				t = device_types[j]; device_types[j] =
				    device_types[j + 1];
				    device_types[j + 1] = t;
				t = states_str[j]; states_str[j] =
				    states_str[j + 1];
				    states_str[j + 1] = t;
				int ti = states[j]; states[j] = states[j + 1];
				    states[j + 1] = ti;
				int tia = is_avail[j]; is_avail[j] =
				    is_avail[j + 1];
				    is_avail[j + 1] = tia;
			}
		}
	}

	if (json_mode) {
		printf("{\n  \"devices\" : {\n");

		int printed_runtime = 0;
		for (int i = 0; i < count; i++) {
			if (runtimes[i] == NULL ||
			    runtimes[i][0] == '\0')
				continue;

			/* Only print each runtime once */
			int seen = 0;
			for (int j = 0; j < i; j++) {
				if (runtimes[j] &&
				    strcmp(runtimes[j], runtimes[i]) == 0) {
					seen = 1;
					break;
				}
			}
			if (seen)
				continue;

			if (printed_runtime)
				printf(",\n");
			printf("  \"%s\" : [\n", runtimes[i]);
			printed_runtime = 1;

			int first = 1;
			for (int j = 0; j < count; j++) {
				if (runtimes[j] == NULL ||
				    strcmp(runtimes[j], runtimes[i]) != 0)
					continue;
				if (!first)
					printf(",\n");
				first = 0;
				printf("    {\n");
				printf("      \"dataPath\" : \"%s/%s/data\",\n",
				    dev_root, udids[j]);
				printf("      \"dataPathSize\" : 0,\n");
				printf("      \"logPath\" : "
				    "\"/Users/%s/Library/Logs/CoreSimulator/%s\",\n",
				    home_user(), udids[j]);
				printf("      \"udid\" : \"%s\",\n", udids[j]);
				printf("      \"isAvailable\" : %s,\n",
				    is_avail[j] ? "true" : "false");
				printf("      \"deviceTypeIdentifier\" : "
				    "\"%s\",\n",
				    device_types[j][0] ? device_types[j] :
				    "com.apple.CoreSimulator.SimDeviceType.Unknown");
				printf("      \"state\" : \"%s\",\n",
				    states_str[j]);
				printf("      \"name\" : \"%s\"\n", names[j]);
				printf("    }");
			}
			printf("\n  ]");
		}
		printf("\n  }\n}\n");
	} else {
		/* Non-JSON output, grouped by OS version */
		for (int i = 0; i < count; i++) {
			if (runtimes[i] == NULL ||
			    runtimes[i][0] == '\0')
				continue;

			/* Only print header once per runtime */
			int seen = 0;
			for (int j = 0; j < i; j++) {
				if (runtimes[j] &&
				    strcmp(runtimes[j], runtimes[i]) == 0) {
					seen = 1;
					break;
				}
			}
			if (seen)
				continue;

			/* Extract OS display name from runtime identifier */
			char os_display[64];
			runtime_os_display(runtimes[i], os_display, sizeof os_display);

			printf("-- %s --\n",
			    os_display[0] ? os_display : "Unknown");

			for (int j = 0; j < count; j++) {
				if (runtimes[j] == NULL ||
				    strcmp(runtimes[j], runtimes[i]) != 0)
					continue;
				printf("    %s (%s) (%s) \n",
				    names[j], udids[j], states_str[j]);
			}
		}
	}

	for (int i = 0; i < count; i++) {
		free(names[i]);
		free(udids[i]);
		free(runtimes[i]);
		free(device_types[i]);
		free(states_str[i]);
	}
}

/* --- list device types --- */

/* Infer product family from device name */
static const char *
product_family(const char *name)
{
	if (strncmp(name, "iPhone", 6) == 0)
		return "iPhone";
	if (strncmp(name, "iPad", 4) == 0)
		return "iPad";
	if (strncmp(name, "Apple TV", 8) == 0)
		return "Apple TV";
	if (strncmp(name, "Apple Watch", 11) == 0)
		return "Apple Watch";
	if (strncmp(name, "Apple Vision", 12) == 0)
		return "Apple Vision";
	return "Other";
}

void
list_devicetypes(int json_mode)
{
	char buf[1024];
	snprintf(buf, sizeof buf, "%s/DeviceTypes", CS_PROFILES_DIR);

	DIR *dir = opendir(buf);
	if (dir == NULL) {
		fprintf(stderr, "simctl: no device types directory\n");
		return;
	}

	struct dirent *entry;
	int count = 0;
	char *names[512];
	char *ids[512];
	char *bundles[512];
	const char *families[512];

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		if (strstr(entry->d_name, ".simdevicetype") == NULL)
			continue;

		char dt_path[1024];
		snprintf(dt_path, sizeof dt_path, "%s/%s", buf,
		    entry->d_name);

		char info_path[1024];
		snprintf(info_path, sizeof info_path,
		    "%s/Contents/Info.plist", dt_path);

		struct stat st;
		if (stat(info_path, &st) != 0 || !S_ISREG(st.st_mode))
			continue;

		char *name = plist_read_string(info_path, "CFBundleName");
		char *id = plist_read_string(info_path, "CFBundleIdentifier");

		if (name && id && count < 512) {
			names[count] = name;
			ids[count] = id;
			bundles[count] = strdup(dt_path);
			families[count] = product_family(name);
			count++;
		} else {
			free(name);
			free(id);
		}
	}
	closedir(dir);

	/* Sort by family, then name */
	for (int i = 0; i < count - 1; i++) {
		for (int j = 0; j < count - i - 1; j++) {
			int swap = 0;
			int fc = strcmp(families[j], families[j + 1]);
			if (fc > 0)
				swap = 1;
			else if (fc == 0 && strcmp(names[j], names[j + 1]) > 0)
				swap = 1;
			if (swap) {
				char *t;
				t = names[j]; names[j] = names[j + 1];
				    names[j + 1] = t;
				t = ids[j]; ids[j] = ids[j + 1];
				    ids[j + 1] = t;
				t = bundles[j]; bundles[j] = bundles[j + 1];
				    bundles[j + 1] = t;
				const char *tf = families[j];
				    families[j] = families[j + 1];
				    families[j + 1] = tf;
			}
		}
	}

	if (json_mode) {
		printf("{\n  \"devicetypes\" : [\n");
		for (int i = 0; i < count; i++) {
			printf("    {\n");
			printf("      \"productFamily\" : \"%s\",\n",
			    families[i]);
			printf("      \"bundlePath\" : \"%s\",\n", bundles[i]);
			printf("      \"maxRuntimeVersion\" : 4294967295,\n");
			printf("      \"maxRuntimeVersionString\" : "
			    "\"65535.255.255\",\n");
			printf("      \"identifier\" : \"%s\",\n", ids[i]);
			printf("      \"modelIdentifier\" : \"\",\n");
			printf("      \"minRuntimeVersionString\" : "
			    "\"1.0.0\",\n");
			printf("      \"minRuntimeVersion\" : 65536,\n");
			printf("      \"name\" : \"%s\"\n", names[i]);
			printf("    }%s\n", i < count - 1 ? "," : "");
		}
		printf("  ]\n}\n");
	} else {
		for (int i = 0; i < count; i++) {
			printf("%s (%s)\n", names[i], ids[i]);
		}
	}

	for (int i = 0; i < count; i++) {
		free(names[i]);
		free(ids[i]);
		free(bundles[i]);
	}
}

/* --- list runtimes --- */

void
list_runtimes(int json_mode)
{
	DIR *vdir = opendir(CS_VOLUMES_DIR);
	if (vdir == NULL) {
		fprintf(stderr, "simctl: no CoreSimulator volumes\n");
		return;
	}

	char *names[256];
	char *versions[256];
	char *buildvs[256];
	char *ids[256];
	int count = 0;

	struct dirent *ventry;
	while ((ventry = readdir(vdir)) != NULL) {
		if (ventry->d_name[0] == '.')
			continue;

		/* Volume dir name is like iOS_23F73 */
		char vol_name[512];
		snprintf(vol_name, sizeof vol_name, "%s/%s",
		    CS_VOLUMES_DIR, ventry->d_name);

		/* Extract build version from dir name */
		char *buildver = NULL;
		char *underscore = strchr(ventry->d_name, '_');
		if (underscore) {
			buildver = strdup(underscore + 1);
		}

		/* Find the .simruntime bundle */
		char rt_search[512];
		snprintf(rt_search, sizeof rt_search,
		    "%s/Library/Developer/CoreSimulator/Profiles/Runtimes",
		    vol_name);

		DIR *rdir = opendir(rt_search);
		if (rdir == NULL) {
			free(buildver);
			continue;
		}

		struct dirent *rentry;
		while ((rentry = readdir(rdir)) != NULL) {
			if (rentry->d_name[0] == '.')
				continue;
			if (strstr(rentry->d_name, ".simruntime") == NULL)
				continue;

			char rt_path[512];
			snprintf(rt_path, sizeof rt_path,
			    "%s/%s", rt_search, rentry->d_name);

			char info_path[1024];
			snprintf(info_path, sizeof info_path,
			    "%s/Contents/Info.plist", rt_path);

			struct stat st;
			if (stat(info_path, &st) != 0 || !S_ISREG(st.st_mode))
				continue;

			char *rt_name = plist_read_string(info_path,
			    "CFBundleName");
			char *rt_id = plist_read_string(info_path,
			    "CFBundleIdentifier");

			if (rt_name && rt_id && count < 256) {
				if (buildver == NULL)
					buildver = strdup("");

				/* Extract version number from CFBundleName
				 * e.g., "iOS 26.5" -> "26.5" */
				char version[64];
				char *sp = strchr(rt_name, ' ');
				if (sp)
					snprintf(version, sizeof version,
					    "%s", sp + 1);
				else
					snprintf(version, sizeof version,
					    "%s", rt_name);

				names[count] = rt_name;
				versions[count] = strdup(version);
				buildvs[count] = buildver;
				ids[count] = rt_id;
				buildver = NULL; /* transfer ownership */
				count++;
			} else {
				free(rt_name);
				free(rt_id);
			}
		}
		closedir(rdir);
		free(buildver);
	}
	closedir(vdir);

	/* Sort by build version for deterministic output */
	for (int i = 0; i < count - 1; i++) {
		for (int j = 0; j < count - i - 1; j++) {
			if (strcmp(buildvs[j], buildvs[j + 1]) > 0) {
				char *t;
				t = names[j]; names[j] = names[j + 1];
				    names[j + 1] = t;
				t = versions[j]; versions[j] = versions[j + 1];
				    versions[j + 1] = t;
				t = buildvs[j]; buildvs[j] = buildvs[j + 1];
				    buildvs[j + 1] = t;
				t = ids[j]; ids[j] = ids[j + 1];
				    ids[j + 1] = t;
			}
		}
	}

	if (json_mode) {
		printf("{\n  \"runtimes\" : [\n");
		for (int i = 0; i < count; i++) {
			printf("    {\n");
			printf("      \"isAvailable\" : true,\n");
			printf("      \"version\" : \"%s\",\n", versions[i]);
			printf("      \"isInternal\" : false,\n");
			printf("      \"buildversion\" : \"%s\",\n", buildvs[i]);
			printf("      \"supportedArchitectures\" : [\n");
			printf("        \"arm64\"\n");
			printf("      ],\n");
			printf("      \"supportedDeviceTypes\" : [],\n");
			printf("      \"bundlePath\" : \"\",\n");
			printf("      \"identifier\" : \"%s\"\n", ids[i]);
			printf("    }%s\n", i < count - 1 ? "," : "");
		}
		printf("  ]\n}\n");
	} else {
		for (int i = 0; i < count; i++) {
			printf("%s (%s - %s) - %s\n",
			    names[i], versions[i], buildvs[i], ids[i]);
		}
	}

	for (int i = 0; i < count; i++) {
		free(names[i]);
		free(versions[i]);
		free(buildvs[i]);
		free(ids[i]);
	}
}

/* --- list pairs --- */

void
list_pairs(int json_mode)
{
	if (json_mode)
		printf("{\n  \"pairs\" : {\n\n  }\n}\n");
	else
		printf("");
}
