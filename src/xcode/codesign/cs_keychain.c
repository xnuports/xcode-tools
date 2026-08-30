/*
 * cs_keychain.c -- sign with an identity from the keychain.
 *
 * The rest of this tool builds its CMS with OpenSSL, from a certificate
 * and key it can read out of a file.  A keychain identity cannot be
 * handled that way: the private key of an Apple-issued signing identity
 * is normally non-extractable, so there is nothing for OpenSSL to load.
 * Security.framework's CMSEncoder signs through the key rather than with
 * a copy of it, which is what Apple's own codesign does.
 *
 * Identities are matched on the certificate's common name, by substring,
 * so the usual spellings work:
 *
 *	codesign -s "Apple Development"
 *	codesign -s "Apple Development: someone@example.com (TEAMID)"
 *
 * A note on the SPI declared below.  The Apple code-signing hash-agility
 * attribute is what lets a signature carry the cdhashes list, and
 * Security exposes the attribute *flags* publicly while keeping the
 * setter private, so there is no public way to supply the value.  It is
 * declared here rather than included, and looked up at runtime, so an OS
 * that ever drops it degrades to a signature without hash agility rather
 * than failing to launch.
 *
 * Copyright (c) 2026 Sunneva N. Mariu
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/CMSEncoder.h>

#include <openssl/x509.h>

#include "codesign.h"

typedef OSStatus (*set_agility_fn)(CMSEncoderRef, CFDataRef);

static int
cfstr_to_buf(CFStringRef s, char *buf, size_t len)
{
	if (s == NULL || buf == NULL || len == 0)
		return -1;
	buf[0] = '\0';
	return CFStringGetCString(s, buf, (CFIndex)len, kCFStringEncodingUTF8)
	    ? 0 : -1;
}

/*
 * Read the subject name out of a keychain certificate.
 *
 * Security's own accessors return the subject as a nested property
 * tree; the certificate is public data, so it is simpler -- and
 * consistent with the file-based identities -- to hand the DER to
 * OpenSSL and share cs_blob.c's extraction.  Only the certificate is
 * copied out; the private key never leaves the keychain.
 */
static void
copy_cert_names(SecCertificateRef cert, char *team_id, size_t team_len,
    char *cert_cn, size_t cn_len)
{
	CFDataRef der;
	const unsigned char *p;
	X509 *x = NULL;

	if (team_id != NULL && team_len > 0)
		snprintf(team_id, team_len, "notset");
	if (cert_cn != NULL && cn_len > 0)
		cert_cn[0] = '\0';

	if ((der = SecCertificateCopyData(cert)) == NULL)
		return;

	p = CFDataGetBytePtr(der);
	x = d2i_X509(NULL, &p, (long)CFDataGetLength(der));
	if (x != NULL) {
		cert_copy_names(x, team_id, team_len, cert_cn, cn_len);
		X509_free(x);
	}
	CFRelease(der);
}

/*
 * Match on the certificate's common name, taking the first identity
 * whose name contains the string given.  An exact match wins over a
 * partial one, so a full "Apple Development: ... (TEAMID)" selects that
 * identity even when a shorter name would also match.
 */
static SecIdentityRef
find_identity(const char *name)
{
	const void *k[] = { kSecClass, kSecReturnRef, kSecMatchLimit };
	const void *v[] = { kSecClassIdentity, kCFBooleanTrue, kSecMatchLimitAll };
	CFDictionaryRef query;
	CFArrayRef items = NULL;
	SecIdentityRef exact = NULL, partial = NULL;
	CFIndex i;

	if (name == NULL || *name == '\0')
		return NULL;

	query = CFDictionaryCreate(NULL, k, v, 3,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (query == NULL)
		return NULL;

	if (SecItemCopyMatching(query, (CFTypeRef *)&items) != errSecSuccess ||
	    items == NULL) {
		CFRelease(query);
		return NULL;
	}
	CFRelease(query);

	for (i = 0; i < CFArrayGetCount(items); i++) {
		SecIdentityRef id = (SecIdentityRef)CFArrayGetValueAtIndex(items, i);
		SecCertificateRef cert = NULL;
		CFStringRef cn = NULL;
		char buf[512];

		if (SecIdentityCopyCertificate(id, &cert) != errSecSuccess)
			continue;
		if (SecCertificateCopyCommonName(cert, &cn) == errSecSuccess &&
		    cn != NULL && cfstr_to_buf(cn, buf, sizeof(buf)) == 0) {
			if (strcmp(buf, name) == 0 && exact == NULL)
				exact = (SecIdentityRef)CFRetain(id);
			else if (strstr(buf, name) != NULL && partial == NULL)
				partial = (SecIdentityRef)CFRetain(id);
		}
		if (cn != NULL)
			CFRelease(cn);
		CFRelease(cert);
	}

	CFRelease(items);

	if (exact != NULL) {
		if (partial != NULL)
			CFRelease(partial);
		return exact;
	}
	return partial;
}

/*
 * Attach the issuing certificates.
 *
 * The encoder embeds only what it can reach on its own, which stops at
 * the intermediate; a verifier that cannot reach an anchor from what is
 * embedded reports no authority and rejects the signature.  Evaluating
 * the leaf against a basic X.509 policy produces the chain the system
 * itself would build, and every certificate in it is added.
 */
static void
add_supporting_certs(CMSEncoderRef enc, SecCertificateRef leaf)
{
	SecPolicyRef policy;
	SecTrustRef trust = NULL;
	CFArrayRef chain = NULL;
	CFErrorRef err = NULL;

	if ((policy = SecPolicyCreateBasicX509()) == NULL)
		return;

	if (SecTrustCreateWithCertificates(leaf, policy, &trust) == errSecSuccess) {
		/*
		 * The chain is what is wanted here, not the verdict: an
		 * evaluation that fails (an expired or untrusted leaf)
		 * still yields the certificates that were found.
		 */
		(void)SecTrustEvaluateWithError(trust, &err);
		if (err != NULL)
			CFRelease(err);

		chain = SecTrustCopyCertificateChain(trust);
		if (chain != NULL) {
			CFIndex n = CFArrayGetCount(chain);

			/*
			 * The encoder has already embedded the signer and
			 * the issuer directly above it; adding those again
			 * leaves a duplicate in the certificate set.  What
			 * is missing is the rest of the chain up to the
			 * anchor.
			 */
			for (CFIndex i = 2; i < n; i++) {
				SecCertificateRef c =
				    (SecCertificateRef)CFArrayGetValueAtIndex(chain, i);

				if (c != NULL)
					CMSEncoderAddSupportingCerts(enc, c);
			}
			CFRelease(chain);
		}
		CFRelease(trust);
	}
	CFRelease(policy);
}

int
keychain_identity_exists(const char *name)
{
	SecIdentityRef id = find_identity(name);

	if (id == NULL)
		return 0;
	CFRelease(id);
	return 1;
}

int
keychain_identity_info(const char *name, char *team_id, size_t team_len,
    char *cert_cn, size_t cn_len)
{
	SecIdentityRef id = find_identity(name);
	SecCertificateRef cert = NULL;

	if (id == NULL)
		return -1;

	if (SecIdentityCopyCertificate(id, &cert) != errSecSuccess) {
		CFRelease(id);
		return -1;
	}

	copy_cert_names(cert, team_id, team_len, cert_cn, cn_len);

	CFRelease(cert);
	CFRelease(id);
	return 0;
}

int
keychain_cms_sign(const char *name,
    const uint8_t *content, size_t content_len,
    const char *cdhashes_plist,
    uint8_t *out, size_t *out_len,
    char *team_id, size_t team_len, char *cert_cn, size_t cn_len)
{
	SecIdentityRef id = NULL;
	SecCertificateRef cert = NULL;
	CMSEncoderRef enc = NULL;
	CFDataRef encoded = NULL;
	/*
	 * The signing time is what a verifier evaluates the certificate
	 * chain against; without it the chain cannot be judged and the
	 * signature is reported as having no authority.
	 */
	CMSSignedAttributes attrs = kCMSAttrSigningTime;
	int ret = -1;

	if ((id = find_identity(name)) == NULL) {
		fprintf(stderr, "codesign: no identity found matching '%s'\n", name);
		return -1;
	}

	if (SecIdentityCopyCertificate(id, &cert) == errSecSuccess)
		copy_cert_names(cert, team_id, team_len, cert_cn, cn_len);

	if (CMSEncoderCreate(&enc) != errSecSuccess) {
		fprintf(stderr, "codesign: could not create CMS encoder\n");
		goto out;
	}
	if (CMSEncoderAddSigners(enc, id) != errSecSuccess) {
		fprintf(stderr, "codesign: identity '%s' cannot be used to sign\n", name);
		goto out;
	}
	if (cert != NULL)
		add_supporting_certs(enc, cert);

	/*
	 * The signature is detached: what it covers is the CodeDirectory
	 * blob, which is carried in the superblob rather than inside the
	 * CMS.  SHA-1 is the encoder's default and is not what codesign
	 * reads, so the digest is set explicitly.
	 */
	CMSEncoderSetHasDetachedContent(enc, true);
	if (CMSEncoderSetSignerAlgorithm(enc,
	    kCMSEncoderDigestAlgorithmSHA256) != errSecSuccess) {
		fprintf(stderr, "codesign: could not select SHA-256 for signing\n");
		goto out;
	}

	/*
	 * Hash agility, through the SPI described at the top of this
	 * file.  Only version 1 is emitted: it takes the cdhashes
	 * property list directly, whereas the version 2 dictionary is
	 * keyed by an algorithm identifier this code has no documented
	 * mapping for, and a wrong key produces an attribute that makes
	 * the signature unreadable -- Security faults on it rather than
	 * ignoring it.  A signature without the version 2 attribute
	 * verifies; one with a malformed attribute does not.
	 */
	{
		set_agility_fn set_v1 = (set_agility_fn)dlsym(RTLD_DEFAULT,
		    "CMSEncoderSetAppleCodesigningHashAgility");

		if (set_v1 != NULL && cdhashes_plist != NULL) {
			CFDataRef d = CFDataCreate(NULL,
			    (const UInt8 *)cdhashes_plist,
			    (CFIndex)strlen(cdhashes_plist));

			if (d != NULL) {
				if (set_v1(enc, d) == errSecSuccess)
					attrs |= kCMSAttrAppleCodesigningHashAgility;
				CFRelease(d);
			}
		}
	}

	if (attrs != kCMSAttrNone)
		CMSEncoderAddSignedAttributes(enc, attrs);

	if (CMSEncoderUpdateContent(enc, content, content_len) != errSecSuccess) {
		fprintf(stderr, "codesign: CMS content update failed\n");
		goto out;
	}
	if (CMSEncoderCopyEncodedContent(enc, &encoded) != errSecSuccess ||
	    encoded == NULL) {
		fprintf(stderr, "codesign: CMS encoding failed for identity '%s'\n", name);
		goto out;
	}

	{
		CFIndex len = CFDataGetLength(encoded);

		/* Same blob header every superblob member carries. */
		if (*out_len < (size_t)len + 8) {
			fprintf(stderr, "codesign: CMS signature too large\n");
			goto out;
		}
		be_write32(out, CSMAGIC_BLOBWRAPPER);
		be_write32(out + 4, (uint32_t)(len + 8));
		memcpy(out + 8, CFDataGetBytePtr(encoded), (size_t)len);
		*out_len = (size_t)len + 8;
	}

	ret = 0;
out:
	if (encoded != NULL)
		CFRelease(encoded);
	if (enc != NULL)
		CFRelease(enc);
	if (cert != NULL)
		CFRelease(cert);
	if (id != NULL)
		CFRelease(id);
	return ret;
}
