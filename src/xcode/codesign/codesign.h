/*
 * codesign.h - shared definitions for codesign.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CODESIGN_H
#define CODESIGN_H

#include <stdint.h>
#include <stddef.h>

#define CODESIGN_VERSION "0.1.0 (compat 16.0)"

#define CSMAGIC_CODEDIRECTORY           0xfade0c02
#define CSMAGIC_EMBEDDED_SIGNATURE      0xfade0cc0
#define CSMAGIC_EMBEDDED_ENTITLEMENTS   0xfade7171
#define CSMAGIC_EMBEDDED_DER_ENTITLEMENTS 0xfade7172
#define CSMAGIC_BLOBWRAPPER             0xfade0b01
#define CSMAGIC_REQUIREMENT             0xfade0c00
#define CSMAGIC_REQUIREMENTS            0xfade0c01

#define CSSLOT_CODEDIRECTORY            0
#define CSSLOT_REQUIREMENTS             2
#define CSSLOT_ENTITLEMENTS           0x05
#define CSSLOT_DER_ENTITLEMENTS       0x07
#define CSSLOT_ALTERNATE_CODEDIRECTORIES 0x1000
#define CSSLOT_SIGNATURESLOT          0x10000

#define CS_HASHTYPE_SHA1                1
#define CS_HASHTYPE_SHA256              2
#define CS_SHA1_LEN                     20
#define CS_SHA256_LEN                   32
#define CS_CDHASH_LEN                   20

#define CS_CD_VERSION                   0x20400

#define CS_ADHOC                        0x00000002
#define CS_HARD                         0x00000100
#define CS_KILL                         0x00000200
#define CS_RESTRICT                     0x00000800
#define CS_ENFORCEMENT                  0x00001000
#define CS_REQUIRE_LV                   0x00002000
#define CS_RUNTIME                      0x00010000
#define CS_LINKER_SIGNED                0x00020000

#define CS_EXECSEG_MAIN_BINARY          0x1
#define CS_EXECSEG_ALLOW_UNSIGNED       0x10
#define CS_ALLOW_UNSIGNED               0x00000020

#define kReqOpTrue                      1
#define kReqOpIdent                     2
#define kReqOpAppleGenericAnchor        15
#define kReqOpAnd                       6
#define kReqOpCertField                11
#define kReqOpCertGeneric              14
#define kReqMatchEqual                  1
#define kReqMatchExists                 0
#define kSecDesignatedRequirementType   3

#define OID_CDHASHES                    "1.2.840.113635.100.9.1"
#define OID_CDHASHES2                   "1.2.840.113635.100.9.2"

#define CSB_MAX_BLOBS 64

#define LC_CODE_SIGNATURE               0x0000001D
#define LC_SEGMENT_64                   0x00000019
#define LC_SEGMENT                      0x00000001

#define MH_MAGIC                        0xfeedface
#define MH_CIGAM                        0xcefaedfe
#define MH_MAGIC_64                     0xfeedfacf
#define MH_CIGAM_64                     0xcffaedfe

#define MH_EXECUTE                      0x02
#define MH_DYLIB                        0x06
#define MH_BUNDLE                       0x08

#define CSMAGIC_REQUIREMENT             0xfade0c00

#define HASH_CHUNK_LEN                  65536

struct cs_blob_index {
	uint32_t type;
	uint32_t offset;
};

struct cs_superblob_hdr {
	uint32_t magic;
	uint32_t length;
	uint32_t count;
};

/* In-memory CodeDirectory for construction (host endian internally) */
struct code_directory {
	uint32_t magic;
	uint32_t version;
	uint32_t flags;
	uint32_t hash_offset;
	uint32_t ident_offset;
	uint32_t n_special_slots;
	uint32_t n_code_slots;
	uint32_t code_limit;
	uint8_t  hash_size;
	uint8_t  hash_type;
	uint8_t  platform;
	uint8_t  page_size;
	uint32_t scatter_offset;
	uint32_t team_offset;
	uint64_t code_limit64;
	uint64_t exec_seg_base;
	uint64_t exec_seg_limit;
	uint64_t exec_seg_flags;
	const char *identifier;
	const char *team_id;
	const uint8_t *special_hash;
	const uint8_t *code_hash;
};


struct cs_blob_entry {
	uint32_t type;
	size_t   data_len;
	const uint8_t *data;
};

struct superblob_builder {
	struct cs_blob_entry blobs[CSB_MAX_BLOBS];
	int  count;
	size_t total_len;
};

/* cs_blob.c */
void  be_write32(uint8_t *p, uint32_t v);
uint32_t be_read32(const uint8_t *p);
void  be_write64(uint8_t *p, uint64_t v);
uint64_t be_read64(const uint8_t *p);

void  sbb_init(struct superblob_builder *b);
int   sbb_add(struct superblob_builder *b, uint32_t type,
              const uint8_t *data, size_t len);
int   sbb_emit(struct superblob_builder *b, uint8_t *out, size_t *out_len);

size_t build_code_directory(struct code_directory *cd,
                            uint8_t *out, size_t out_len);
size_t build_requirements_blob(const char *bundle_id,
                               const char *cert_cn,
                               uint8_t *out, size_t out_len);
size_t build_entitlements_xml(const char *plist_xml,
                              uint8_t *out, size_t out_len);
size_t build_entitlements_der(const char *plist_xml,
                              uint8_t *out, size_t out_len);
int    build_cms_signature(const uint8_t *cd_hash_sha1,
                           size_t cd_hash_len,
                           const char *cdhashes_plist,
                           const char *cd_sha256_hex,
                           const char *cert_file, const char *key_file,
                           const char *p12_file, const char *key_password,
                           uint8_t *out, size_t *out_len,
                           char *team_id_out, size_t team_id_len,
                           char *cert_cn_out, size_t cert_cn_len);
int    load_identity_info(const char *cert_file, const char *key_file,
                           const char *p12_file, const char *key_password,
                           char *team_id_out, size_t team_id_len,
                           char *cert_cn_out, size_t cert_cn_len);
int    build_adhoc_wrapper(uint8_t *out, size_t *out_len);
void   sha1_raw(const uint8_t *data, size_t len, uint8_t *out);
void   sha256_raw(const uint8_t *data, size_t len, uint8_t *out);

/* cs_macho.c */
struct arch_info {
	const uint8_t *base;
	size_t size;
	uint32_t cputype;
	uint32_t cpusubtype;
	int is_64;
	int is_le;
	uint32_t file_type;
	uint32_t flags;
	const uint8_t *code_start;
	uint64_t code_limit;
	uint64_t text_vmaddr;
	uint64_t text_vmsize;
	uint64_t text_fileoff;
	uint64_t text_filesize;
	uint64_t text_exec_flags;
	uint32_t dataoff;
	uint32_t datasize;
	uint64_t linkedit_fileoff;
	uint64_t linkedit_filesize;
};

struct macho_file {
	int is_fat;
	int n_archs;
	const uint8_t *data;
	size_t size;
	struct arch_info archs[16];
};

int macho_parse(const uint8_t *data, size_t size, struct macho_file *mf);
int macho_has_codesig(struct arch_info *ai);
const char *macho_arch_name(uint32_t cputype);
int  macho_is_le(struct arch_info *ai);
uint32_t macho_file_type(struct arch_info *ai);
uint32_t macho_cputype(struct arch_info *ai);
uint64_t macho_code_limit(struct arch_info *ai);
uint32_t macho_dataoff(struct arch_info *ai);
uint32_t macho_datasize(struct arch_info *ai);
uint64_t macho_exec_seg_flags(struct arch_info *ai);
uint32_t macho_header_size(struct arch_info *ai);
long   macho_find_codesig_lc(struct arch_info *ai);
int    macho_update_codesig_lc(struct arch_info *ai, uint32_t new_dataoff,
    uint32_t new_datasize, uint8_t *file_buf, size_t file_sz);
int    macho_update_linkedit_seg(struct arch_info *ai, uint32_t sig_dataoff,
    uint32_t sig_datasize);

/* cs_sign.c */
int sign_macho(const char *path, const char *identity, int force,
               int adhoc, const char *identifier,
               const char *entitlements_file, uint32_t cd_flags,
               uint32_t page_size_log2, const char *cert_file,
               const char *key_file, const char *p12_file,
               const char *key_password, const char *req_str);
int sign_bundle(const char *path, const char *identity, int force,
                int adhoc, const char *identifier,
                const char *entitlements_file, uint32_t cd_flags,
                uint32_t page_size_log2, const char *cert_file,
                const char *key_file, const char *p12_file,
                const char *key_password, const char *req_str);
int remove_signature(const char *path);

/* cs_verify.c */
int verify_code(const char *path, int verbose);
int display_code(const char *path, int verbose,
                 const char *entitlements_out, const char *requirements_out,
                 const char *cert_prefix, int der, int xml, int all_archs);

/* cs_file.c */
uint8_t *cs_read_file(const char *path, size_t *size);
int  cs_write_file(const char *path, const uint8_t *data, size_t size);
int  cs_file_exists(const char *path);
int  cs_file_size(const char *path, size_t *size);
int  cs_is_directory(const char *path);
char *cs_basename(const char *path);

/* cs_sign.c */
char *find_bundle_executable(const char *bundle_path);
char *find_bundle_identifier(const char *bundle_path);

/* Global state */
extern int g_verbose;
extern int g_continue_on_error;

#endif
