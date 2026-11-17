/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <gio/gio.h>
G_BEGIN_DECLS

typedef struct {
	const char *id;
	const char *display_name;
	const char *binary;
} TermacAgent;

const TermacAgent *termac_agent_find (const char *id);
const TermacAgent *termac_agent_default (void);
const TermacAgent *const *termac_agent_all (gsize *count);
char *termac_agent_format_attachment (const TermacAgent *agent, const char *path);

gboolean termac_agent_is_installed (const TermacAgent *agent);

typedef enum {
	TERMAC_AGENT_AVAILABLE,
	TERMAC_AGENT_MISSING,
	TERMAC_AGENT_DIRECTORY_MISSING,
} TermacAgentAvailability;

/* Queries use the target folder and interactive shell PATH.
 * Startup jobs are isolated and cleaned up on completion/timeout/cancellation. */
void termac_agent_check_async (const TermacAgent *agent, const char *cwd,
                              GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
TermacAgentAvailability termac_agent_check_finish (GAsyncResult *result, GError **error);
char *termac_agent_build_launch_command (const TermacAgent *agent);

G_END_DECLS
