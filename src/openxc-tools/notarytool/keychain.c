/*
 * keychain - credential storage for notarytool store-credentials.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Uses the macOS Security framework to store/retrieve App Store Connect
 * API key and Apple ID credentials by profile name. Credentials are
 * stored as generic keychain items with attributes for each auth field.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>

#include "keychain.h"

#define kService "com.apple.notarytool.credentials"

int
keychain_store(const char *profile_name, const char *key_path,
    const char *key_id, const char *issuer_id, const char *team_id,
    const char *apple_id, const char *app_specific_password,
    const char *keychain_path)
{
	(void)keychain_path;

	/*
	 * Store credentials as a generic password item. We encode the
	 * credential fields into the password data as a simple
	 * key=value format, separated by newlines.
	 */
	char data[4096];
	int offset = 0;

	if (key_path)
		offset += snprintf(data + offset, sizeof data - offset,
		    "key_path=%s\n", key_path);
	if (key_id)
		offset += snprintf(data + offset, sizeof data - offset,
		    "key_id=%s\n", key_id);
	if (issuer_id)
		offset += snprintf(data + offset, sizeof data - offset,
		    "issuer=%s\n", issuer_id);
	if (team_id)
		offset += snprintf(data + offset, sizeof data - offset,
		    "team_id=%s\n", team_id);
	if (apple_id)
		offset += snprintf(data + offset, sizeof data - offset,
		    "apple_id=%s\n", apple_id);
	if (app_specific_password)
		offset += snprintf(data + offset, sizeof data - offset,
		    "asp=%s\n", app_specific_password);

	CFStringRef sv = CFStringCreateWithCString(NULL, kService,
	    kCFStringEncodingUTF8);
	CFStringRef ac = CFStringCreateWithCString(NULL, profile_name,
	    kCFStringEncodingUTF8);

	/* Delete any existing item with the same service + account */
	const void *del_keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
	const void *del_vals[] = {
	    kSecClassGenericPassword, sv, ac };
	CFDictionaryRef del_query = CFDictionaryCreate(NULL,
	    del_keys, del_vals, 3,
	    &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	if (del_query != NULL) {
		SecItemDelete(del_query);
		CFRelease(del_query);
	}

	/* Add the new item */
	CFDataRef pd = CFDataCreate(NULL, (const UInt8 *)data, (CFIndex)offset);
	const void *add_keys[] = {
	    kSecClass, kSecAttrService, kSecAttrAccount,
	    kSecValueData, kSecAttrGeneric };
	const void *add_vals[] = {
	    kSecClassGenericPassword, sv, ac, pd, ac };
	CFDictionaryRef query = CFDictionaryCreate(NULL,
	    add_keys, add_vals, 5,
	    &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);

	OSStatus status = noErr;
	if (query != NULL) {
		status = SecItemAdd(query, NULL);
		CFRelease(query);
	}

	CFRelease(sv);
	CFRelease(ac);
	CFRelease(pd);

	return (status == errSecSuccess) ? 0 : -1;
}

static char *
find_field(const char *data, const char *field)
{
	char pattern[256];
	snprintf(pattern, sizeof pattern, "%s=", field);
	const char *p = strstr(data, pattern);
	if (p == NULL)
		return NULL;
	p += strlen(pattern);
	const char *eol = strchr(p, '\n');
	size_t len = eol ? (size_t)(eol - p) : strlen(p);
	char *val = malloc(len + 1);
	if (val == NULL)
		return NULL;
	memcpy(val, p, len);
	val[len] = '\0';
	return val;
}

int
keychain_retrieve(const char *profile_name, const char *keychain_path,
    char **key_path, char **key_id, char **issuer_id, char **team_id,
    char **apple_id, char **app_specific_password)
{
	(void)keychain_path; /* keychain path is handled by the system */

	CFStringRef sv = CFStringCreateWithCString(NULL, kService,
	    kCFStringEncodingUTF8);
	CFStringRef ac = CFStringCreateWithCString(NULL, profile_name,
	    kCFStringEncodingUTF8);

	const void *keys[] = {
	    kSecClass, kSecAttrService, kSecAttrAccount,
	    kSecReturnData, kSecMatchLimit };
	const void *vals[] = {
	    kSecClassGenericPassword, sv, ac,
	    kCFBooleanTrue, kSecMatchLimitOne };
	CFDictionaryRef query = CFDictionaryCreate(NULL,
	    keys, vals, 5,
	    &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);

	if (query == NULL) {
		CFRelease(sv);
		CFRelease(ac);
		return -1;
	}

	CFTypeRef result = NULL;
	OSStatus status = SecItemCopyMatching(query, &result);
	CFRelease(query);

	if (status != errSecSuccess || result == NULL) {
		CFRelease(sv);
		CFRelease(ac);
		return -1;
	}

	CFDataRef data = (CFDataRef)result;
	const UInt8 *bytes = CFDataGetBytePtr(data);
	CFIndex len = CFDataGetLength(data);
	char *flat = malloc(len + 1);
	memcpy(flat, bytes, len);
	flat[len] = '\0';
	CFRelease(result);
	CFRelease(sv);
	CFRelease(ac);

	*key_path = find_field(flat, "key_path");
	*key_id = find_field(flat, "key_id");
	*issuer_id = find_field(flat, "issuer");
	*team_id = find_field(flat, "team_id");
	*apple_id = find_field(flat, "apple_id");
	*app_specific_password = find_field(flat, "asp");

	free(flat);
	return 0;
}
