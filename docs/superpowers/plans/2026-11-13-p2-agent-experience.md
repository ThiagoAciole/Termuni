# P2 Agent Experience Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The user explicitly picks Claude Code or Codex CLI, each CLI's specifics live in one adapter module, and image attachments are validated against both.

**Architecture:** A small adapter table in `src/termac-agent.c` (id, display name, binary, how to format an attached image path, how to build the launch command). The terminal and window call the adapter instead of hard-coding quoting. A GSettings key `agent` stores the choice; Preferences > Attachments shows it. A new action opens the chosen agent in a new tab. Adding a third agent later means adding one table row.

**Tech Stack:** C, GLib (`g_find_program_in_path`, `GSettings`), libadwaita (`AdwComboRow`), existing `termac-attachments`.

**Spec:** [PLANO_IMPLEMENTACAO.md](../../../PLANO_IMPLEMENTACAO.md), "Prioridades — 13/11/2026", item P2. Product decision in [PDR.Arquiteture.md](../../../PDR.Arquiteture.md): Claude Code and Codex CLI first, architecture extensible, OpenCode out of the MVP.

## Global Constraints

- MVP agents: `claude` (Claude Code) and `codex` (Codex CLI). Hermes, Gemini, OpenCode are out of the MVP validation.
- CLI-specific text or flags exist only in `src/termac-agent.c`. No other file may contain the strings `claude` or `codex` as program names (a test greps for this).
- Known behaviour, already confirmed by the user on 12/11/2026: dragging a PNG attaches it in both CLIs (Codex shows `[Image #1]`); Ctrl+V with an image on the clipboard works in Claude Code when `wl-clipboard` is installed.
- Attachment files live in `$XDG_CACHE_HOME/termac/attachments/<session-id>/`, max 20 MiB, 8000 px, PNG or JPEG.
- Never invent a CLI flag: Task 6 reads `--help` of the installed binaries before any flag is used.
- Strings are translatable (`_()`), and `po/pt_BR.po` keeps 0 fuzzy.
- Commits end with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

1. Attachment path with spaces, quotes, `$`, or non-ASCII characters is typed into the prompt intact. [Task 1]
2. The saved agent is not installed (CLI removed after choosing it): the UI says so and attachments still work as plain paths. [Tasks 2, 3]
3. `agent` setting holds an unknown value (hand-edited or from a newer version): fall back to `claude`, no crash. [Task 3]
4. Attaching an image in a tab that runs a plain shell (no agent) still inserts a quoted path. [Task 4]
5. Launching the agent in a folder that no longer exists, or when the binary is not on `PATH`: a visible message, no empty tab left behind. [Task 5]

## File Structure

- Create `src/termac-agent.c`, `src/termac-agent.h`: adapters and registry.
- Create `tests/test-agent.c`: unit tests (no GTK).
- Modify `src/termac-terminal.c` (the quoting in `terminal_attach_texture`) and `src/termac-window.c` (`window_attach_stored`): use the adapter.
- Modify `data/thiago.aciole.termac.gschema.xml.in`: key `agent`.
- Modify `src/termac-application.c`: Preferences row; `src/termac-window.c` + `src/termac-window.ui`: `win.launch-agent`.
- Modify `src/meson.build`, `tests/meson.build`, `tests/test-ui.c`, `po/pt_BR.po`, `PLANO_IMPLEMENTACAO.md`.

---

### Task 1: Adapter table and attachment formatting

**Files:**
- Create: `src/termac-agent.h`, `src/termac-agent.c`, `tests/test-agent.c`
- Modify: `src/meson.build`, `tests/meson.build`

**Interfaces:**
- Produces:
  - `typedef struct { const char *id; const char *display_name; const char *binary; } TermacAgent;`
  - `const TermacAgent *termac_agent_find (const char *id)` — NULL for unknown ids.
  - `const TermacAgent *termac_agent_default (void)` — the `claude` entry.
  - `const TermacAgent *const *termac_agent_all (gsize *count)`.
  - `char *termac_agent_format_attachment (const TermacAgent *agent, const char *path)` — text to type into the prompt for an image (caller frees); wraps `termac_attachments_quote (path)` plus a trailing space for both agents today.

- [ ] **Step 1: Write the failing tests** in `tests/test-agent.c`: `/agent/find` (`claude`, `codex` found; `"nope"` and NULL give NULL; `termac_agent_default ()->id` is `"claude"`), `/agent/format-plain` (`/tmp/a.png` becomes `'/tmp/a.png' ` or whatever `termac_attachments_quote` returns, plus space), `/agent/format-hostile` (paths `"/tmp/it's here.png"`, `"/tmp/$HOME `x`.png"`, `"/tmp/ação.png"`: assert the formatted text, run through `/bin/sh -c "printf %s <formatted>"`, prints the original path exactly).
- [ ] **Step 2: Run** `meson test -C build agent`. Expected: FAIL (missing sources).
- [ ] **Step 3: Implement** the table (two rows: `claude`/"Claude Code"/`claude`, `codex`/"Codex CLI"/`codex`) and the four functions. Add `test('agent', ...)` linking `termac-agent.c`, `termac-attachments.c`, `termac-util.c`.
- [ ] **Step 4: Run** `meson test -C build agent`. Expected: PASS.
- [ ] **Step 5: Commit** `feat: add the agent adapter table`.

### Task 2: Detecting installed agents

**Files:**
- Modify: `src/termac-agent.c`, `src/termac-agent.h`, `tests/test-agent.c`

**Interfaces:**
- Produces: `gboolean termac_agent_is_installed (const TermacAgent *agent)` — `g_find_program_in_path (agent->binary) != NULL`, honouring the process `PATH` at call time.

- [ ] **Step 1: Write the failing test** `/agent/installed`: create a temp dir, write an executable `claude` script in it (mode 0755), `g_setenv ("PATH", dir, TRUE)`; assert claude installed, codex not; remove the script, assert claude not installed.
- [ ] **Step 2: Run** `meson test -C build agent`. Expected: FAIL (function missing).
- [ ] **Step 3: Implement** and declare it.
- [ ] **Step 4: Run.** Expected: PASS.
- [ ] **Step 5: Commit** `feat: detect installed agents`.

### Task 3: The choice in settings and Preferences

**Files:**
- Modify: `data/thiago.aciole.termac.gschema.xml.in`, `src/termac-application.c`, `tests/test-ui.c`, `po/pt_BR.po`

**Interfaces:**
- Consumes: `termac_agent_all`, `termac_agent_find`, `termac_agent_default`, `termac_agent_is_installed`.
- Produces: `const TermacAgent *termac_application_get_agent (TermacApplication *self)` — setting value resolved through `termac_agent_find`, falling back to `termac_agent_default ()`.

- [ ] **Step 1: Write the failing test** `/ui/agent-choice`: open Preferences, find the `AdwComboRow` titled "Agent" on the Attachments page, select Codex CLI, assert `g_settings_get_string (settings, "agent")` is `"codex"` and `termac_application_get_agent (app)->id` is `"codex"`; then `g_settings_set_string (settings, "agent", "bogus")` and assert the getter returns the `claude` entry.
- [ ] **Step 2: Run** `... -p /ui/agent-choice`. Expected: FAIL (no row, no key).
- [ ] **Step 3: Implement.** Schema key `agent`, type `s`, default `'claude'`, with summary. Add the combo row to the Attachments page, listing every agent; mark not-installed entries with a subtitle "Not installed" (translatable). Selecting an entry writes the setting. The getter resolves and falls back.
- [ ] **Step 4: Run** `meson test -C build`. Expected: all OK; run `ninja -C build termac-update-po`, translate the new strings ("Agent", "Not installed", row subtitle), `msgfmt --check --statistics po/pt_BR.po` shows 0 fuzzy.
- [ ] **Step 5: Commit** `feat: choose the agent in Preferences`.

### Task 4: Attachments go through the adapter

**Files:**
- Modify: `src/termac-terminal.c` (`terminal_attach_texture`), `src/termac-window.c` (`window_attach_stored`), `src/termac-terminal.h`

**Interfaces:**
- Consumes: `termac_agent_format_attachment`, `termac_application_get_agent`.
- Produces: `void termac_terminal_set_agent (TermacTerminal *self, const TermacAgent *agent)` — terminals receive the current agent from the window whenever the setting changes and at creation.

- [ ] **Step 1: Write the failing tests** in `tests/test-ui.c`: extend `test_attachment_sequence` expectations rather than adding a case: with agent `claude` and again with `codex`, drop a PNG and assert the typed text (read the screen with the P0 helper `screen_contains`, or the attachment list) equals `termac_agent_format_attachment` for that agent. Add `/ui/attach-without-agent-binary`: with `PATH` emptied, attaching still inserts the quoted path (Review Focus 4).
- [ ] **Step 2: Run** `... -p /ui/attachment-sequence`. Expected: FAIL only if the terminal still quotes directly; otherwise PASS and the replacement below keeps it green.
- [ ] **Step 3: Replace** the two direct `termac_attachments_quote` calls with `termac_agent_format_attachment`, delete the duplicated `g_strconcat (quoted, " ")`, wire `termac_terminal_set_agent` from the window (`notify` of the setting).
- [ ] **Step 4: Add the guard test.** In `tests/test-agent.c` or a script run by `meson test`: `grep -rEn '"(claude|codex)"' src | grep -v termac-agent.c` must output nothing; wire it as `test('agent-names-contained', find_program('sh'), args: [...])`.
- [ ] **Step 5: Run** `meson test -C build`. Expected: all OK. Commit `refactor: format attachments through the agent adapter`.

### Task 5: Open the agent in a tab

**Files:**
- Modify: `src/termac-window.c`, `src/termac-window.ui` (menu entry), `src/termac-application.c` (editable shortcut row, default `<control><shift>a`), `tests/test-ui.c`, `po/pt_BR.po`

**Interfaces:**
- Consumes: `termac_application_get_agent`, `termac_agent_is_installed`, `termac_terminal_insert_text`.
- Produces: window action `win.launch-agent` and `void termac_window_launch_agent (TermacWindow *self)`.

- [ ] **Step 1: Write the failing tests:**
  - `/ui/launch-agent`: put an executable `claude` script on `PATH` that prints `AGENT-STARTED` and sleeps; activate `launch-agent`; assert a new tab exists and `wait_for_screen (terminal, "AGENT-STARTED", 5000)` is TRUE.
  - `/ui/launch-agent-missing`: empty `PATH`; activate the action; assert tab count is unchanged and a toast with "not installed" text is shown (check via the toast overlay helper or a `message` signal).
- [ ] **Step 2: Run both.** Expected: FAIL (action missing).
- [ ] **Step 3: Implement.** The action opens a tab in the current terminal's cwd, then types `<binary>` plus Enter into the shell (so the user's shell environment applies); it never builds argument lists beyond the binary. If the binary is not installed, show a toast and do nothing. Add the menu entry and the editable shortcut with the same pattern as the others (add it to `editable_shortcuts[]` and to the help overlay `src/shortcuts-dialog.ui`).
- [ ] **Step 4: Run** `meson test -C build`. Expected: all OK. Update translations (0 fuzzy).
- [ ] **Step 5: Commit** `feat: open the chosen agent in a new tab`.

### Task 6: Real CLI validation (manual, recorded)

**Files:**
- Create: `docs/checklists/agents-manual.md`
- Modify: `PLANO_IMPLEMENTACAO.md`

- [ ] **Step 1: Read the CLIs.** Run `claude --help` and `codex --help` and record in the checklist whether either takes an image flag, and its exact name. If one does, add an optional field to `TermacAgent` in a follow-up commit only if the flag is needed for launching with attachments; otherwise leave the table as is.
- [ ] **Step 2: Write the checklist** (each line has a result column): drag a PNG into each CLI; Ctrl+V with an image on the clipboard in each CLI (needs `wl-clipboard`, see P1 Task 4); a JPEG; an image with a space in its name; large image near the 20 MiB limit shows the error message; Codex shows `[Image #N]`; Claude Code shows the attachment.
- [ ] **Step 3: A person runs it** against the real CLIs (needs accounts and network). Record dates, versions (`claude --version`, `codex --version`) and results in the plan.
- [ ] **Step 4: Commit** `docs: agent validation checklist and results`.

### Task 7: Record results

- [ ] **Step 1:** Update `PLANO_IMPLEMENTACAO.md` under "Prioridades — 13/11/2026": adapters done, tests that prove it, manual validation status. Update the stale "Estado atual" row for marco 4 to match.
- [ ] **Step 2: Commit** `docs: record P2 results`.

## Self-Review

- **Spec coverage:** explicit selection (T3), adapters per CLI (T1, T2, T4), attachment validation (T6, plus automated format tests), no CLI flags in the UI (T4 guard test).
- **Type consistency:** `TermacAgent`, `termac_agent_find`, `termac_agent_default`, `termac_agent_all`, `termac_agent_is_installed`, `termac_agent_format_attachment` are defined in T1/T2 and used unchanged in T3–T5. `termac_terminal_set_agent` is introduced in T4 and not used earlier.
- **Both agents currently format identically.** That is deliberate: the table exists so a difference found in Task 6 lands in one file with a test.
- **Out of scope:** per-agent completion events, OpenCode, Hermes, Gemini, terminal profiles.
