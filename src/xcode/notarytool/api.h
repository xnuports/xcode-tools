/*
 * notarytool - open source reimplementation of Apple's notarytool(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef API_H
#define API_H

#include <stddef.h>

#define NOTARY_API_BASE "https://appstoreconnect.apple.com/notary/v2/"
#define NOTARY_STATUS_SUBMITTED "Submitted"
#define NOTARY_STATUS_IN_PROGRESS "In Progress"
#define NOTARY_STATUS_ACCEPTED "Accepted"
#define NOTARY_STATUS_INVALID "Invalid"
#define NOTARY_STATUS_REJECTED "Rejected"
#define NOTARY_POLL_INTERVAL 15

/*
 * Submission status response from the API.
 */
struct submission_status {
	char *id;           /* submission UUID */
	char *status;       /* "Submitted", "In Progress", "Accepted", etc. */
	char *submission_name;
	char *upload_url;   /* S3 presigned URL for file upload */
	char *log_url;      /* developer log URL */
	char *sha256;
	int done;           /* 1 if terminal status (Accepted/Invalid/Rejected) */
};

/*
 * Credential set for API authentication.
 */
struct creds {
	enum { CREDS_KEY, CREDS_ASP } type;
	/* For CREDS_KEY: */
	char *key_path;
	char *key_id;
	char *issuer_id;
	char *team_id;
	/* For CREDS_ASP: */
	char *apple_id;
	char *app_specific_password;
	/* For CREDS_PROFILE: */
	char *profile_name;
	char *keychain_path;
};

/*
 * Authenticate and retrieve a JWT for API requests.
 * Returns a malloc'd JWT string, or NULL on failure.
 */
char *api_authenticate(struct creds *c);

/*
 * Create a submission: POST to /submissions, returns the submission
 * object. Caller must free all fields via submission_status_free.
 */
struct submission_status *api_submit(struct creds *c,
    const char *file_path, const char *content_type,
    int use_s3_ta, int force);

/*
 * Get the status of an existing submission.
 */
struct submission_status *api_get_status(struct creds *c,
    const char *submission_id);

/*
 * Download the notarization log JSON for a submission.
 * Returns malloc'd JSON string, or NULL on failure.
 */
char *api_get_log(struct creds *c, const char *submission_id);

/*
 * Get submission history for the team.
 * Returns malloc'd JSON string, or NULL.
 */
char *api_get_history(struct creds *c);

/*
 * Poll for submission completion. Blocks until the submission reaches
 * a terminal status or the timeout is reached. Returns 0 on success.
 */
int api_wait(struct creds *c, const char *submission_id,
    long timeout_seconds, int verbose);

/*
 * Free a submission_status struct.
 */
void submission_status_free(struct submission_status *s);

/*
 * Free a creds struct.
 */
void creds_free(struct creds *c);

#endif /* API_H */