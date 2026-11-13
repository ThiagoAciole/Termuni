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

G_END_DECLS
