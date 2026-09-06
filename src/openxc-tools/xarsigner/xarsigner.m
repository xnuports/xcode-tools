/*
 * xarsigner -- put a detached signature into a xar archive.
 *
 * An Installer package is a xar archive whose heap begins with the SHA-1 of
 * its own compressed table of contents; a signature over those twenty bytes
 * is what makes the package trusted.  Signing therefore needs the digest
 * before the archive exists, and the archive before the digest can be
 * signed, so the work is split in two:
 *
 *	--simulate  rewrite the table of contents as it will be once signed,
 *		    reserving room in the heap for the signatures, and print
 *		    the digest that reservation produces.
 *	--sign	    do the same again and drop the signature bytes, made
 *		    elsewhere from that digest, into the room reserved.
 *
 * The two runs agree because the reservations are fixed sizes and the
 * certificates -- which do go into the table of contents -- are given to
 * both.  That is what lets the private key live somewhere this tool never
 * sees.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#import <Foundation/Foundation.h>

#include <CommonCrypto/CommonDigest.h>

#include <err.h>
#include <getopt.h>
#include <sysexits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/*
 * The heap room each signature style gets, whatever the signature actually
 * measures.  Fixed, so that --simulate can reserve it without having seen a
 * signature; anything longer is refused and anything shorter is zero filled.
 */
#define RSA_SLOT	256
#define CMS_SLOT	9216

#define XAR_MAGIC	0x78617221U	/* 'xar!' */
#define XAR_HEADER_SIZE	28

struct xar_header {
	uint32_t	magic;
	uint16_t	size;
	uint16_t	version;
	uint64_t	toc_len_compressed;
	uint64_t	toc_len_uncompressed;
	uint32_t	cksum_alg;
};

static const char *progname = "xarsigner";

static void
fail(NSString *fmt, ...)
{
	va_list ap;
	NSString *m;

	va_start(ap, fmt);
	m = [[NSString alloc] initWithFormat:fmt arguments:ap];
	va_end(ap);
	fprintf(stderr, "%s: %s\n", progname, [m UTF8String]);
}

/*
 * What Apple report for anything that is not a readable xar: an error out of
 * their own domain with nothing to say for itself.
 */
static NSError *
not_an_archive(void)
{
	return ([NSError errorWithDomain:@"XARSignerErrorDomain" code:2
	    userInfo:nil]);
}

static void
usage(FILE *out)
{
	fprintf(out, "Usage:\n");
	fprintf(out, "%s --simulate --input-archive <path> ...\n", progname);
	fprintf(out, "%s --sign --input-archive <path> --output-archive "
	    "<path> ...\n", progname);
	fprintf(out, "\nShared Options:\n");
	fprintf(out, "--input-archive, -f           The path to the archive "
	    "to sign or re-sign. (Required.)\n");
	fprintf(out, "--cms, -c                     Use CMS signing mode. "
	    "Multiple signing modes may be used if multiple signatures are "
	    "desired.\n");
	fprintf(out, "--cms-certificate, -t         The path to a certificate "
	    "in x509 DER/PEM format that is part of the CMS signature's "
	    "chain. May be passed multiple times.\n");
	fprintf(out, "--rsa, -r                     Use RSA signing mode. "
	    "Multiple signing modes may be used if multiple signatures are "
	    "desired.\n");
	fprintf(out, "--rsa-certificate, -T         The path to a certificate "
	    "in x509 DER/PEM format that is part of the RSA signature's "
	    "chain. May be passed multiple times.\n");
	fprintf(out, "--help, -h                    Display this help "
	    "output.\n");
	fprintf(out, "\nSigning Options:\n");
	fprintf(out, "--output-archive, -o          The path to write the "
	    "newly signed/re-signed archive to. (Required.)\n");
	fprintf(out, "--cms-signature, -C           The path to the CMS "
	    "signature data to embed. (Required if --cms is passed.)\n");
	fprintf(out, "--rsa-signature, -R           The path to the RSA "
	    "signature data to embed. (Required if --rsa is passed.)\n\n");
}

static uint16_t
be16(const uint8_t *p)
{
	return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t
be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t
be64(const uint8_t *p)
{
	return ((uint64_t)be32(p) << 32) | be32(p + 4);
}

static void
put16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)v;
}

static void
put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static void
put64(uint8_t *p, uint64_t v)
{
	put32(p, (uint32_t)(v >> 32));
	put32(p + 4, (uint32_t)v);
}

static NSData *
inflate_toc(NSData *in, size_t expected)
{
	NSMutableData *out = [NSMutableData dataWithLength:expected];
	uLongf len = (uLongf)expected;

	if (uncompress([out mutableBytes], &len, [in bytes],
	    (uLong)[in length]) != Z_OK || len != expected)
		return (nil);
	return (out);
}

static NSData *
deflate_toc(NSData *in)
{
	uLongf cap = compressBound((uLong)[in length]);
	NSMutableData *out = [NSMutableData dataWithLength:cap];

	if (compress2([out mutableBytes], &cap, [in bytes],
	    (uLong)[in length], Z_BEST_COMPRESSION) != Z_OK)
		return (nil);
	[out setLength:cap];
	return (out);
}

/* A certificate on disk is either DER already or PEM around DER. */
static NSData *
load_certificate(NSString *path)
{
	NSData *raw = [NSData dataWithContentsOfFile:path];
	NSString *text;
	NSRange begin, end;

	if (raw == nil)
		return (nil);
	text = [[NSString alloc] initWithData:raw encoding:NSUTF8StringEncoding];
	if (text == nil)
		return (raw);
	begin = [text rangeOfString:@"-----BEGIN CERTIFICATE-----"];
	if (begin.location == NSNotFound)
		return (raw);
	end = [text rangeOfString:@"-----END CERTIFICATE-----"];
	if (end.location == NSNotFound)
		return (raw);
	text = [text substringWithRange:NSMakeRange(NSMaxRange(begin),
	    end.location - NSMaxRange(begin))];
	return ([[NSData alloc] initWithBase64EncodedString:text
	    options:NSDataBase64DecodingIgnoreUnknownCharacters]);
}

/*
 * <KeyInfo><X509Data><X509Certificate>...  The certificates are base64 in
 * the same seventy-six column wrapping, with the CRLF line endings the
 * signature schema calls for, that the rest of the toolchain uses.
 */
static NSXMLElement *
key_info(NSArray<NSData *> *certs)
{
	NSXMLElement *info = [NSXMLElement elementWithName:@"KeyInfo"];
	NSXMLElement *data = [NSXMLElement elementWithName:@"X509Data"];

	[info addAttribute:[NSXMLNode attributeWithName:@"xmlns"
	    stringValue:@"http://www.w3.org/2000/09/xmldsig#"]];
	for (NSData *c in certs) {
		NSString *b64 = [c base64EncodedStringWithOptions:
		    NSDataBase64Encoding76CharacterLineLength |
		    NSDataBase64EncodingEndLineWithCarriageReturn |
		    NSDataBase64EncodingEndLineWithLineFeed];

		[data addChild:[NSXMLElement elementWithName:@"X509Certificate"
		    stringValue:b64]];
	}
	[info addChild:data];
	return (info);
}

static NSXMLElement *
signature_node(NSString *name, NSString *style, uint64_t offset,
    uint64_t size, NSArray<NSData *> *certs)
{
	NSXMLElement *e = [NSXMLElement elementWithName:name];

	[e addAttribute:[NSXMLNode attributeWithName:@"style"
	    stringValue:style]];
	[e addChild:[NSXMLElement elementWithName:@"offset"
	    stringValue:[NSString stringWithFormat:@"%llu", offset]]];
	[e addChild:[NSXMLElement elementWithName:@"size"
	    stringValue:[NSString stringWithFormat:@"%llu", size]]];
	[e addChild:key_info(certs)];
	return (e);
}

/*
 * Every heap offset in the table of contents is measured from the end of the
 * table, so making room for the signatures moves all of them along.  The
 * checksum sits at zero and stays there.
 */
static void
shift_offsets(NSXMLElement *toc, long long delta)
{
	NSArray *nodes = [toc nodesForXPath:@".//data/offset" error:NULL];

	if (delta == 0)
		return;
	for (NSXMLElement *o in nodes) {
		long long v = [[o stringValue] longLongValue];

		[o setStringValue:[NSString stringWithFormat:@"%lld",
		    v + delta]];
	}
}

int
main(int argc, char *argv[])
{
@autoreleasepool {
	static struct option opts[] = {
		{ "sign",		no_argument,		NULL, 's' },
		{ "simulate",		no_argument,		NULL, 'S' },
		{ "input-archive",	required_argument,	NULL, 'f' },
		{ "output-archive",	required_argument,	NULL, 'o' },
		{ "cms",		no_argument,		NULL, 'c' },
		{ "rsa",		no_argument,		NULL, 'r' },
		{ "cms-certificate",	required_argument,	NULL, 't' },
		{ "rsa-certificate",	required_argument,	NULL, 'T' },
		{ "cms-signature",	required_argument,	NULL, 'C' },
		{ "rsa-signature",	required_argument,	NULL, 'R' },
		{ "help",		no_argument,		NULL, 'h' },
		{ NULL,			0,			NULL, 0 }
	};
	NSMutableArray<NSData *> *cms_certs = [NSMutableArray array];
	NSMutableArray<NSData *> *rsa_certs = [NSMutableArray array];
	NSString *input = nil, *output = nil;
	NSString *cms_sig_path = nil, *rsa_sig_path = nil;
	NSData *archive, *toc_z, *toc, *cms_sig = nil, *rsa_sig = nil;
	NSXMLDocument *doc;
	NSXMLElement *toc_el, *anchor = nil;
	NSMutableData *out;
	struct xar_header h;
	const uint8_t *base;
	uint64_t old_sig_total = 0, new_sig_total = 0, off;
	unsigned char digest[CC_SHA1_DIGEST_LENGTH];
	bool sign = false, simulate = false, use_cms = false, use_rsa = false;
	int ch, i;

	while ((ch = getopt_long(argc, argv, "sSf:o:crt:T:C:R:h", opts,
	    NULL)) != -1) {
		NSString *arg = optarg ?
		    [NSString stringWithUTF8String:optarg] : nil;
		NSData *cert;

		switch (ch) {
		case 's':
			sign = true;
			break;
		case 'S':
			simulate = true;
			break;
		case 'f':
			input = arg;
			break;
		case 'o':
			output = arg;
			break;
		case 'c':
			use_cms = true;
			break;
		case 'r':
			use_rsa = true;
			break;
		case 't':
		case 'T':
			if ((cert = load_certificate(arg)) == nil) {
				fail(@"%s certificate data not found at %s",
				    ch == 't' ? "CMS" : "RSA", optarg);
				return (EX_NOINPUT);
			}
			[(ch == 't' ? cms_certs : rsa_certs) addObject:cert];
			break;
		case 'C':
			cms_sig_path = arg;
			break;
		case 'R':
			rsa_sig_path = arg;
			break;
		case 'h':
			usage(stdout);
			return (0);
		default:
			usage(stderr);
			return (EX_USAGE);
		}
	}

	if (!sign && !simulate) {
		fail(@"Either --sign or --simulate must be specified.");
		return (EX_USAGE);
	}
	if (input == nil) {
		fail(@"--input-archive is a required option.");
		return (EX_USAGE);
	}
	if (sign && output == nil) {
		fail(@"--output-archive is a required option in --sign mode.");
		return (EX_USAGE);
	}
	/*
	 * Signing nothing still produces an archive, just an unsigned one.
	 * Simulating nothing has nothing to print, so it is an error.
	 */
	if (!use_cms && !use_rsa) {
		if (!sign) {
			fprintf(stderr, "No signature types selected.\n");
			return (EX_NOINPUT);
		}
		fprintf(stderr, "Warning: No signature type specified. Output "
		    "archive will have no signatures.\n");
	}
	if (use_cms && cms_certs.count == 0)
		fprintf(stderr, "Warning: No CMS certificates specified. For "
		    "signature to validate, identical --cms-certificate "
		    "arguments must be passed in both --simulate and --sign "
		    "modes.\n");
	if (use_rsa && rsa_certs.count == 0)
		fprintf(stderr, "Warning: No RSA certificates specified. For "
		    "signature to validate, identical --rsa-certificate "
		    "arguments must be passed in both --simulate and --sign "
		    "modes.\n");

	if (sign) {
		if (use_cms) {
			cms_sig = cms_sig_path == nil ? nil :
			    [NSData dataWithContentsOfFile:cms_sig_path];
			if (cms_sig == nil) {
				fail(@"CMS signature data not found at %s",
				    cms_sig_path == nil ? "(null)" :
				    [cms_sig_path UTF8String]);
				return (EX_NOINPUT);
			}
		}
		if (use_rsa) {
			rsa_sig = rsa_sig_path == nil ? nil :
			    [NSData dataWithContentsOfFile:rsa_sig_path];
			if (rsa_sig == nil) {
				fail(@"RSA signature data not found at %s",
				    rsa_sig_path == nil ? "(null)" :
				    [rsa_sig_path UTF8String]);
				return (EX_NOINPUT);
			}
		}
		if ((cms_sig != nil && cms_sig.length > CMS_SLOT) ||
		    (rsa_sig != nil && rsa_sig.length > RSA_SLOT)) {
			fail(@"Failed to sign archive: the signature is larger "
			    "than the space reserved for it.");
			return (EX_SOFTWARE);
		}
	}

	if (![[NSFileManager defaultManager] fileExistsAtPath:input]) {
		NSError *err = [NSError errorWithDomain:NSCocoaErrorDomain
		    code:NSFileNoSuchFileError
		    userInfo:@{ NSFilePathErrorKey: input }];

		fail(@"Failed to %s archive: %@", sign ? "sign" :
		    "simulate signing of", [err localizedDescription]);
		return (EX_SOFTWARE);
	}
	{
		NSError *err = nil;

		archive = [NSData dataWithContentsOfFile:input options:0
		    error:&err];
		if (archive == nil) {
			fail(@"Failed to %s archive: %@", sign ? "sign" :
			    "simulate signing of", [err localizedDescription]);
			return (EX_SOFTWARE);
		}
	}
	if (archive.length < XAR_HEADER_SIZE) {
		fail(@"Failed to %s archive: %@", sign ? "sign" :
		    "simulate signing of", [not_an_archive()
		    localizedDescription]);
		return (EX_SOFTWARE);
	}
	base = [archive bytes];
	h.magic = be32(base);
	h.size = be16(base + 4);
	h.version = be16(base + 6);
	h.toc_len_compressed = be64(base + 8);
	h.toc_len_uncompressed = be64(base + 16);
	h.cksum_alg = be32(base + 24);
	if (h.magic != XAR_MAGIC || h.size < XAR_HEADER_SIZE ||
	    h.size + h.toc_len_compressed > archive.length) {
		fail(@"Failed to %s archive: %@", sign ? "sign" :
		    "simulate signing of", [not_an_archive()
		    localizedDescription]);
		return (EX_SOFTWARE);
	}
	toc_z = [archive subdataWithRange:NSMakeRange(h.size,
	    (NSUInteger)h.toc_len_compressed)];
	toc = inflate_toc(toc_z, (size_t)h.toc_len_uncompressed);
	if (toc == nil) {
		fail(@"Failed to %s archive: the table of contents is corrupt",
		    sign ? "sign" : "simulate signing of");
		return (EX_SOFTWARE);
	}

	doc = [[NSXMLDocument alloc] initWithData:toc options:0 error:NULL];
	if (doc == nil ||
	    (toc_el = [[[doc rootElement] elementsForName:@"toc"]
	    firstObject]) == nil) {
		fail(@"Failed to %s archive: the table of contents is corrupt",
		    sign ? "sign" : "simulate signing of");
		return (EX_SOFTWARE);
	}
	[doc setStandalone:YES];

	/*
	 * Any signatures already there are dropped, and the room they took is
	 * remembered so the heap offsets can be moved by the difference.
	 */
	for (NSXMLElement *e in [toc_el nodesForXPath:
	    @"signature | x-signature" error:NULL]) {
		NSXMLElement *sz = [[e elementsForName:@"size"] firstObject];

		if (sz != nil)
			old_sig_total += (uint64_t)[[sz stringValue]
			    longLongValue];
		[e detach];
	}

	if (use_rsa)
		new_sig_total += RSA_SLOT;
	if (use_cms)
		new_sig_total += CMS_SLOT;

	anchor = [[toc_el elementsForName:@"checksum"] firstObject];
	off = CC_SHA1_DIGEST_LENGTH;
	i = anchor == nil ? 0 : (int)[anchor index] + 1;
	if (use_rsa) {
		[toc_el insertChild:signature_node(@"signature", @"RSA", off,
		    RSA_SLOT, rsa_certs) atIndex:i++];
		off += RSA_SLOT;
	}
	if (use_cms) {
		[toc_el insertChild:signature_node(@"x-signature", @"CMS", off,
		    CMS_SLOT, cms_certs) atIndex:i++];
		off += CMS_SLOT;
	}
	shift_offsets(toc_el, (long long)new_sig_total -
	    (long long)old_sig_total);

	toc = [doc XMLDataWithOptions:0];
	if ((toc_z = deflate_toc(toc)) == nil) {
		fail(@"Failed to %s archive: could not compress the table of "
		    "contents", sign ? "sign" : "simulate signing of");
		return (EX_SOFTWARE);
	}
	CC_SHA1([toc_z bytes], (CC_LONG)[toc_z length], digest);

	if (!sign) {
		for (i = 0; i < CC_SHA1_DIGEST_LENGTH; i++)
			printf("%02x", digest[i]);
		printf("\n");
		return (0);
	}

	out = [NSMutableData dataWithLength:XAR_HEADER_SIZE];
	{
		uint8_t *p = [out mutableBytes];

		put32(p, XAR_MAGIC);
		put16(p + 4, XAR_HEADER_SIZE);
		put16(p + 6, h.version);
		put64(p + 8, (uint64_t)[toc_z length]);
		put64(p + 16, (uint64_t)[toc length]);
		put32(p + 24, h.cksum_alg);
	}
	[out appendData:toc_z];
	[out appendBytes:digest length:sizeof(digest)];
	if (use_rsa) {
		[out appendData:rsa_sig];
		[out increaseLengthBy:RSA_SLOT - rsa_sig.length];
	}
	if (use_cms) {
		[out appendData:cms_sig];
		[out increaseLengthBy:CMS_SLOT - cms_sig.length];
	}
	{
		NSUInteger skip = (NSUInteger)(h.size + h.toc_len_compressed +
		    CC_SHA1_DIGEST_LENGTH + old_sig_total);

		if (skip < archive.length)
			[out appendData:[archive subdataWithRange:
			    NSMakeRange(skip, archive.length - skip)]];
	}
	{
		NSError *err = nil;

		if (![out writeToFile:output options:0 error:&err]) {
			fail(@"Failed to sign archive: %@",
			    [err localizedDescription]);
			return (EX_SOFTWARE);
		}
	}
	return (0);
}
}
