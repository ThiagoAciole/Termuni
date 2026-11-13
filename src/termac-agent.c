/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "termac-agent.h"
#include "termac-attachments.h"

static const TermacAgent registry[] = {
	{ "claude", "Claude Code", "claude" },
	{ "codex", "Codex CLI", "codex" },
};
static const TermacAgent *const agents[] = { &registry[0], &registry[1] };

const TermacAgent *
termac_agent_find (const char *id)
{
	for (guint i = 0; i < G_N_ELEMENTS (agents); i++)
		if (g_strcmp0 (id, agents[i]->id) == 0)
			return agents[i];
	return NULL;
}

const TermacAgent *termac_agent_default (void) { return agents[0]; }

const TermacAgent *const *
termac_agent_all (gsize *count)
{
	if (count != NULL) *count = G_N_ELEMENTS (agents);
	return agents;
}

char *
termac_agent_format_attachment (const TermacAgent *agent, const char *path)
{
	g_autofree char *quoted = termac_attachments_quote (path);
	/* Both initial adapters accept a quoted path. NULL is the plain-shell case. */
	(void) agent;
	return g_strconcat (quoted, " ", NULL);
}

gboolean
termac_agent_is_installed (const TermacAgent *agent)
{
	g_autofree char *program = NULL;
	if (agent == NULL) return FALSE;
	program = g_find_program_in_path (agent->binary);
	return program != NULL;
}
