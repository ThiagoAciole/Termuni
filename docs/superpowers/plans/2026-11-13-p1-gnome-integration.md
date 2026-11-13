# P1 GNOME Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Termac builds and runs as a Flatpak (GNOME Builder flow), no longer depends on the build directory at run time, and is checked for icon, accessibility and interface scaling.

**Architecture:** First remove the hard dependency on `BUILD_SCHEMA_DIR` (without it, no installed copy can work), then make the manifest build and run, then fix what the sandbox breaks (shell location, clipboard tools), then add automated accessibility and scaling checks. Parts that only a person can judge (Builder UI, Orca, Wayland fractional scale) get a written checklist and are recorded as unverified until done.

**Tech Stack:** Meson, Flatpak (`flatpak-builder`, `org.gnome.Sdk` 50), GSettings, GTK 4 accessibility API, Xvfb.

**Spec:** [PLANO_IMPLEMENTACAO.md](../../../PLANO_IMPLEMENTACAO.md), "Prioridades — 13/11/2026", item P1. Also [PDR.Arquiteture.md](../../../PDR.Arquiteture.md), "Riscos e pontos de atenção".

## Global Constraints

- Profiles `development` (`thiago.aciole.Termac.Devel`) and `default` (`thiago.aciole.Termac`) keep their IDs, schemas and icons; resources keep the path `/thiago/aciole/termac`.
- Runtime: `org.gnome.Platform` 50 (user decision, recorded in the plan), Zig `0.16.0` pinned with the tarball SHA-256 already in `thiago.aciole.termac.json`.
- Libadwaita >= 1.8 (`AdwShortcutsDialog`).
- Wayland is the main target; X11 stays as `fallback-x11`.
- Sandbox permissions are minimal and each one is justified in the manifest commit message.
- Do not claim Builder or Flatpak behaviour from a native build. Record exactly what ran.
- Commits end with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

1. App started with no `build/` directory next to it (installed prefix, Flatpak) must still load settings; today `src/termac-application.c` loads the schema only from `BUILD_SCHEMA_DIR` and returns with `self->settings == NULL`, so every `g_settings_*` call that follows is a crash risk. [Task 1]
2. Manifest `source` is `file:///home/thiago/Projects/termac`, a git URL: uncommitted work is not in the Flatpak build. Expected: the build uses the working tree. [Task 2]
3. Shell inside the sandbox is the sandbox's, not the host's: `$HOME` tools, `ls ~`, user dotfiles may be missing. Expected: a decision, written down, not an accident. [Task 3]
4. Image paste to Claude Code depends on `wl-paste`; the GNOME runtime may not ship it. [Task 4]
5. Every icon-only button and menu button has an accessible name. [Task 6]

## File Structure

- Create `src/termac-settings.c`, `src/termac-settings.h`: one function that finds and opens the application schema.
- Create `tests/test-settings.c`: unit test for it.
- Modify `src/termac-application.c`: use it (replaces lines around `g_settings_schema_source_new_from_directory`).
- Modify `src/meson.build`, `tests/meson.build`: add the new source and test; add a HiDPI variant of the UI test.
- Modify `thiago.aciole.termac.json`: sources, permissions, wl-clipboard module.
- Modify `tests/test-ui.c`: accessibility audit test.
- Create `docs/checklists/gnome-manual.md`: manual Builder, Orca and Wayland scaling checklist.
- Modify `PLANO_IMPLEMENTACAO.md`: results.

---

### Task 1: Settings schema without the build directory

**Files:**
- Create: `src/termac-settings.c`, `src/termac-settings.h`, `tests/test-settings.c`
- Modify: `src/termac-application.c`, `src/meson.build`, `tests/meson.build`

**Interfaces:**
- Produces: `GSettings *termac_settings_new (const char *application_id, const char *fallback_schema_dir)`. Looks the schema up in the default source first; only when it is missing and `fallback_schema_dir` is non-NULL and exists, loads it from there. Returns NULL (never aborts) when neither works, after a `g_warning`.

- [ ] **Step 1: Write the failing test** `tests/test-settings.c`, name `/settings/installed-schema`, `/settings/build-dir-fallback`, `/settings/missing`:
  - Installed: set `GSETTINGS_SCHEMA_DIR` to the compiled schema dir (`g_test_build_filename (G_TEST_BUILT, "../data", NULL)`), call with fallback `"/nonexistent"`, assert non-NULL and `g_settings_get_uint (s, "font-size")` equals the schema default.
  - Fallback: unset `GSETTINGS_SCHEMA_DIR`, pass the data dir as fallback, assert non-NULL.
  - Missing: neither available, assert NULL and no crash (`g_test_expect_message` for the warning).
- [ ] **Step 2: Run** `meson test -C build settings`. Expected: FAIL (target and source do not exist).
- [ ] **Step 3: Implement** `termac_settings_new` with `g_settings_schema_source_get_default ()` then `g_settings_schema_source_lookup (..., TRUE)`. In `termac-application.c` replace the inline lookup with it, passing `BUILD_SCHEMA_DIR`, and make every use of `self->settings` tolerate NULL (grep `self->settings` and guard; use built-in defaults when NULL).
- [ ] **Step 4: Run** `meson test -C build`. Expected: all suites OK including `settings`.
- [ ] **Step 5: Install check.** `meson install -C build --destdir /tmp/termac-root` then, with `GSETTINGS_SCHEMA_DIR=/tmp/termac-root/usr/local/share/glib-2.0/schemas` (run `glib-compile-schemas` there first), `xvfb-run -a /tmp/termac-root/usr/local/bin/termac` for 8 s with the `build/` directory renamed; expected: no "Could not load settings schema" warning. Restore `build/` afterwards.
- [ ] **Step 6: Commit** `fix: load the settings schema from the installed location first`.

### Task 2: Flatpak build from the working tree

**Files:**
- Modify: `thiago.aciole.termac.json`
- Create: `docs/checklists/gnome-manual.md` (Builder section)

- [ ] **Step 1: Failing check.** `flatpak-builder --user --force-clean --install-deps-from=flathub build-flatpak thiago.aciole.termac.json`. Record the first failure verbatim in the plan (expected: git source ignores uncommitted work, or Zig/ghostty fetch problem).
- [ ] **Step 2: Change the `termac` module source** from `{"type":"git","url":"file:///..."}` to `{"type":"dir","path":"."}` so the Builder and the CLI both use the working tree.
- [ ] **Step 3: Permissions.** Keep `--socket=wayland`, `--socket=fallback-x11`, `--device=dri`, `--share=ipc`. Remove `--share=network` from `finish-args` (runtime does not need it; the build still gets it through `build-args`). Add nothing else yet; Task 3 may add one.
- [ ] **Step 4: Build and run.** Same command, then `flatpak run thiago.aciole.Termac.Devel`. Expected: window opens, `echo ok` works in the shell, no GLib-CRITICAL on the console. Record the output.
- [ ] **Step 5: Write the Builder checklist** in `docs/checklists/gnome-manual.md`: open the folder in GNOME Builder, pick the manifest, Build and Run, confirm same behaviour. Mark as **not verified** in the plan until a person runs it.
- [ ] **Step 6: Commit** `build: build the Flatpak from the working tree and drop runtime network`.

### Task 3: Where the shell runs inside the sandbox

**Files:**
- Modify: `thiago.aciole.termac.json`, `src/termac-terminal.c` (shell spawn), `PDR.Arquiteture.md`

- [ ] **Step 1: Observe.** In the Flatpak run from Task 2 type `echo $SHELL; ls ~; which git claude codex` and record the result.
- [ ] **Step 2: Decide with the user** between (a) sandbox shell with `--filesystem=home` or (b) host shell via `flatpak-spawn --host` with `--talk-name=org.freedesktop.Flatpak`. Recommended default: (b), because Claude Code and Codex live on the host and the user expects their own shell; it widens the sandbox on purpose, state that in the manifest commit.
- [ ] **Step 3: Implement the choice.** For (b): in the spawn code, when `g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS)`, exec `flatpak-spawn --host --watch-bus --directory=<cwd> <shell>` instead of the shell. Keep the PTY semantics (`forkpty` child execs this command).
- [ ] **Step 4: Test** with `flatpak run`, then `/ui/typing-and-interrupt` equivalent by hand: `sleep 30` then two Ctrl+C returns the prompt; `pwd` shows the tab's cwd; closing the tab leaves no `sleep` on the host (`pgrep sleep`).
- [ ] **Step 5: Document** the decision in `PDR.Arquiteture.md` under "Decisões técnicas" and commit `feat: run the shell on the host from the Flatpak`.

### Task 4: Clipboard tools for image paste

**Files:**
- Modify: `thiago.aciole.termac.json`

- [ ] **Step 1: Check.** `flatpak run --command=sh thiago.aciole.Termac.Devel -c 'command -v wl-paste'` (and `xclip` for X11). Expected: missing.
- [ ] **Step 2: If missing, add a `wl-clipboard` module** (meson, pinned release tarball with `sha256`) before `termac`. If Task 3 chose the host shell, the shell sees the host's `wl-paste` instead and this module is only needed for the sandbox variant; record which applies.
- [ ] **Step 3: Verify** by pasting an image with Ctrl+V into Claude Code from the Flatpak build; expected "Image attached to the prompt" and the CLI shows the attachment. Record as manual evidence.
- [ ] **Step 4: Commit** `build: ship wl-clipboard for image paste in the sandbox`.

### Task 5: Icon, desktop entry and metadata on the installed app

**Files:**
- Modify: `data/*.in` only if a check fails

- [ ] **Step 1: Run** `meson test -C build` (desktop-file-validate and appstreamcli tests already exist) and `appstreamcli validate --pedantic` on the generated metainfo. Expected: no errors.
- [ ] **Step 2: Check the installed names.** After the Flatpak install: `ls /var/lib/flatpak/exports/share/applications` or the user equivalent, the `.desktop` file and the icon `thiago.aciole.Termac.Devel` must exist; `gtk4-icon-browser` or `gtk-launch thiago.aciole.Termac.Devel` starts the app.
- [ ] **Step 3: Fix** missing keys (`Comment`, `StartupWMClass` equal to the application ID) in the desktop template if validation or the dock shows the wrong icon on Wayland.
- [ ] **Step 4: Record** in the checklist that dock/app-switcher icon appearance is a visual check (not verified until looked at). Commit `data: fix desktop entry and metadata findings` only if something changed.

### Task 6: Accessibility audit test

**Files:**
- Modify: `tests/test-ui.c`

**Interfaces:**
- Consumes: `find_widget_of_type`, `collect_buttons_with_data`-style tree walking helpers (add `static void collect_widgets_of_type (GtkWidget *root, GType type, GPtrArray *out)`).
- Produces: nothing for later tasks.

- [ ] **Step 1: Write the failing test** `/ui/accessibility-labels`: for the main window and the Preferences window, walk every `GtkButton` and `GtkMenuButton`; for each assert it has a visible label child text, or a non-empty `tooltip-text`, or an accessible label set with `gtk_accessible_update_property`. Also assert the terminal widget role is `GTK_ACCESSIBLE_ROLE_TERMINAL` via `gtk_accessible_get_accessible_role`.
- [ ] **Step 2: Run** `... -p /ui/accessibility-labels`. Expected: FAIL naming the unlabeled buttons (print each one's type and `icon-name`).
- [ ] **Step 3: Fix** by setting a translated accessible label on each (`_()` strings, then `ninja termac-update-po` and translate in `po/pt_BR.po`, `msgfmt --check --statistics` must report 0 fuzzy).
- [ ] **Step 4: Run** `meson test -C build`. Expected: all OK.
- [ ] **Step 5: Commit** `fix: give icon buttons accessible names`.

### Task 7: Screen reader, honestly scoped

**Files:**
- Modify: `docs/checklists/gnome-manual.md`, `PLANO_IMPLEMENTACAO.md`

- [ ] **Step 1: Write the Orca checklist:** launch under Orca; tab strip announces tab titles; new-tab/overview buttons are spoken; Preferences rows are spoken; typing echo. Mark each item with a result column left empty.
- [ ] **Step 2: Feasibility spike (read-only, no code):** check `gtk_accessible_text_*` availability in the installed GTK (`pkg-config --modversion gtk4`, grep `gtkaccessibletext.h`), and note whether terminal contents (viewport rows via `termac_vt_viewport_row_text`) could be exposed through it. Write a 5-line finding in the plan: feasible or not, estimated size, risks. Do not implement it here; it becomes its own plan if wanted.
- [ ] **Step 3: Commit** `docs: add screen reader checklist and findings`.

### Task 8: Interface scaling

**Files:**
- Modify: `tests/meson.build`, `tests/test-ui.c`

- [ ] **Step 1: Add the failing variant.** In `tests/meson.build` add `test('ui-hidpi', ..., env: ['GSETTINGS_BACKEND=memory', 'GDK_BACKEND=x11', 'GDK_SCALE=2', 'GDK_DPI_SCALE=1.25'])` running the same executable, with timeout 240.
- [ ] **Step 2: Add test `/ui/grid-fits-widget`:** for `scale` of the terminal (`gtk_widget_get_scale_factor`), assert `columns * cell_width <= widget_width` and `rows * cell_height <= widget_height` using the grid accessors (`termac_terminal_get_rows` from the P0 plan, plus a `termac_terminal_get_columns` accessor added here), and that after `win.zoom-in` the grid shrinks while still fitting.
- [ ] **Step 3: Run** `meson test -C build ui ui-hidpi`. Expected: either PASS or FAIL showing clipped or overflowing grid; fix drawing/size computation in `src/termac-terminal.c` (cell metrics are in logical pixels; verify the Cairo context is not double-scaled).
- [ ] **Step 4: Manual items** in the checklist: Wayland at 125%, 150%, 200% fractional scale (GNOME Settings > Displays), text sharp, selection aligned with text, no blur. Mark as not verified until done.
- [ ] **Step 5: Commit** `test: run the UI suite at scale 2 and fix grid sizing findings`.

### Task 9: Record results

- [ ] **Step 1:** Update `PLANO_IMPLEMENTACAO.md` under "Prioridades — 13/11/2026" with: what ran (commands and outcomes), what was fixed, what is still manual and unverified (Builder UI, Orca, fractional scale, dock icon). Also update the stale "Estado atual" table rows for marcos 0 and 6 to match.
- [ ] **Step 2: Commit** `docs: record P1 results`.

## Self-Review

- **Spec coverage:** Builder (T2, checklist), Flatpak (T2–T4), sandbox permissions (T2, T3), icon (T5), accessibility (T6, T7), scaling (T8).
- **Type consistency:** `termac_settings_new` is defined in T1 and used only there; `termac_terminal_get_rows` comes from the P0 plan (Task 1 there). If P0 is not done first, add it here.
- **Dependencies:** Task 3 asks the user to choose between host shell and sandbox shell; stop there if no answer.
- **Not automatable:** GNOME Builder UI, Orca speech, Wayland fractional scaling. They stay open in the plan until a person records them.
