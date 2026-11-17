# P0 Reliability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove, with automated tests that drive the real app, that the PTY path survives vim, mouse tracking, dead-key/IME input, large and hostile pastes, independent sessions and session restore, and that tab churn does not grow memory.

**Architecture:** Every check is a new case in `tests/test-ui.c` (the existing Xvfb + XTest suite that runs the real `TermacApplication`), using small programs available on any dev box (`vi`, `python3`, `cat`, `wc`). Failures found by a test are fixed in `src/` in the same task. Leak work is a script plus a suppressions file, not a test case.

**Tech Stack:** C, GTK 4, libadwaita, libghostty-vt, Meson, GLib `g_test`, X11 XTest, valgrind 3.27.1.

**Spec:** [PLANO_IMPLEMENTACAO.md](../../PLANO_IMPLEMENTACAO.md), section "Prioridades — 13/11/2026", item P0. P1–P3 get their own plans.

## Global Constraints

- Language and toolkit: C, GTK 4, libadwaita >= 1.8, libghostty-vt at the revision pinned in `subprojects/ghostty.wrap`, Zig `0.16.0`.
- Tests run as `GSETTINGS_BACKEND=memory GDK_BACKEND=x11 xvfb-run -a build/tests/test-ui -p /ui/<name>`; the whole suite is `meson test -C build`.
- One tab is one session; tab bar hidden with one tab, visible with more.
- The test session may hold user work: tests never touch the real `~/.local/state/termac`; the UI suite already isolates XDG directories, keep it that way.
- Code style: tabs, GNU-ish braces as in the existing files, `g_autofree`/`g_autoptr` for ownership.
- Do not mark a plan item done without the evidence the test gives. A native build is not proof of Builder or Flatpak behaviour.
- Commits end with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

Inputs the spec implies but a happy-path test would miss, most likely first. Each line is pinned by a test in the task named in brackets.

1. Paste larger than the 1 MiB write queue (`WRITE_QUEUE_LIMIT` in `src/termac-terminal.c`) is silently truncated; expected: every byte arrives. [Task 4]
2. Pasted text containing the bracketed-paste end marker `ESC[201~` must not end the paste early (paste injection). [Task 4]
3. A corrupt or truncated `session-v3.gvariant` must not crash startup; expected: one fresh tab. [Task 6]
4. Resizing the window while vim runs must change the PTY size (`LINES`/`COLUMNS` via `stty size`) and redraw. [Task 1]
5. Closing a tab while vim is the foreground job must leave no orphan process. [Task 1]

## File Structure

- Modify `tests/test-ui.c`: new test cases and two helpers (`screen_contains`, `wait_for_screen`). One file stays the home of UI tests, matching the current layout.
- Modify `src/termac-terminal.c`: only if a test proves a defect (Task 4 backpressure is the expected one).
- Create `tools/valgrind-churn.sh` and `tools/valgrind.supp`: leak run and suppressions.
- Modify `PLANO_IMPLEMENTACAO.md`: results, per `AGENTS.md`.

---

### Task 1: Shared screen helpers and vim in the PTY

**Files:**
- Modify: `tests/test-ui.c` (helpers near `type_text`, test after `test_tui_alternate_screen`, registration in `main`)

**Interfaces:**
- Produces:
  - `static gboolean screen_contains (TermacTerminal *terminal, const char *needle)` — TRUE when any viewport row text contains `needle`; rows via `termac_vt_viewport_row_text (vt, columns, row, &text)`; use the grid height the widget reports (add a `guint termac_terminal_get_rows (TermacTerminal *)` accessor to `src/termac-terminal.[ch]` if none exists).
  - `static gboolean wait_for_screen (TermacTerminal *terminal, const char *needle, guint timeout_ms)` — `pump (100)` loop until `screen_contains` or timeout.
  - `static void run_in_terminal (TermacWindow *window, TermacTerminal *terminal, const char *command)` — focus, `type_text`, `Return`.

- [ ] **Step 1: Write the failing test `test_vim_edit_resize_close`** (registered as `/ui/vim-session`)
  - Create a temp file path with `g_build_filename (g_get_tmp_dir (), "termac-vim-XXXXXX")`, run `vi <path>`.
  - `wait_for_screen (t, "~", 3000)`, assert `termac_vt_alternate_screen (vt)`.
  - Type `ihello-vim`, `Escape`, then `:wq` + `Return`. Wait for `termac_vt_alternate_screen` to be FALSE. `g_file_get_contents` on the path and assert it equals `"hello-vim\n"`.
  - Resize focus: run `vi` again, then resize the window with `gtk_window_set_default_size (GTK_WINDOW (window), 900, 500)` plus `pump (800)`; quit vim with `:q!`; run `stty size`; assert the printed rows differ from the rows before the resize.
  - Close case: run `vi` again, `close-tab` after opening a second tab, `pump (1500)`, assert `child_process_count ()` equals the count before vim started.

- [ ] **Step 2: Run to verify it fails**

Run: `meson compile -C build && GSETTINGS_BACKEND=memory GDK_BACKEND=x11 xvfb-run -a build/tests/test-ui -p /ui/vim-session`
Expected: FAIL to compile first (helpers/accessor missing), then fail or pass on behaviour. A pass on the first run is acceptable evidence; record it.

- [ ] **Step 3: Implement the helpers and, if needed, `termac_terminal_get_rows`; fix any defect the test exposes** (likely areas: SIGWINCH on resize in `termac-terminal.c`, SIGHUP on close).

- [ ] **Step 4: Run the test and the full suite**

Run: `meson test -C build`
Expected: 7 of 7 OK.

- [ ] **Step 5: Commit**

```bash
git add tests/test-ui.c src/termac-terminal.c src/termac-terminal.h
git commit -m "test: drive vi through edit, resize and tab close"
```

### Task 2: Mouse tracking reaches the program

**Files:**
- Modify: `tests/test-ui.c` (helpers `mouse_button`, test, registration)

**Interfaces:**
- Consumes: `mouse_move (GtkWidget *window, int x, int y)` (exists), `wait_for_screen`, `run_in_terminal` from Task 1.
- Produces: `static void mouse_button (guint button, gboolean press)` using `fake_button_event`, same style as `test_mouse_selection_copy`.

- [ ] **Step 1: Write the failing test `test_mouse_tracking_report`** (`/ui/mouse-tracking`)
  - Run `python3 -c "import sys,tty;tty.setraw(0);sys.stdout.write('\x1b[?1000h\x1b[?1006h');sys.stdout.flush();d=sys.stdin.buffer.read1(32);sys.stdout.write('GOT:'+repr(d));sys.stdout.flush()"`.
  - Wait until `termac_vt_mouse_tracking (vt)` is TRUE.
  - Click at pixel (40, 60) inside the terminal: assert the screen eventually contains `GOT:b'\x1b[<0;` (SGR press, button 0).
  - Second run of the same program, then Shift+click: assert `termac_vt_mouse_tracking` is TRUE yet the screen does **not** contain `GOT:` within 1000 ms, and `termac_vt_selection_text` is available after a Shift+drag (Shift bypasses reporting, see `terminal_mouse_reporting`).

- [ ] **Step 2: Run** `... -p /ui/mouse-tracking`. Expected: first run FAIL until helpers exist.
- [ ] **Step 3: Implement `mouse_button`; fix any report defect found** (coordinates, release event, scroll wheel encoding in `termac-terminal.c`).
- [ ] **Step 4: Run the test, then `meson test -C build`.** Expected: PASS, 7 of 7.
- [ ] **Step 5: Commit** `test: verify mouse reports and shift bypass`.

### Task 3: Dead keys and Unicode entry through the input method

**Files:**
- Modify: `tests/test-ui.c`

**Interfaces:**
- Consumes: `send_keysym_full (KeySym, gboolean ctrl, gboolean shift)`, `wait_for_screen`.

- [ ] **Step 1: Write the failing test `test_dead_key_and_unicode`** (`/ui/input-method`)
  - Run `cat` (echoes typed input). Send `XK_dead_acute`, then `XK_a`: assert screen contains `á`.
  - Send `XK_dead_tilde`, `XK_n` (`ñ`); send `XK_Return`.
  - Ctrl+Shift+U sequence: `send_keysym_full (XK_u, TRUE, TRUE)`, type `1f600`, `XK_space`: assert screen contains `😀`.
  - Assert the shell still behaves: `XK_d` with Ctrl ends `cat`, `has_foreground_job` FALSE.
  - `XkbKeycode` for dead keys may be absent on the Xvfb keymap; if `XKeysymToKeycode` returns 0, load a keymap with `setxkbmap -layout br -variant abnt2` inside the test environment via `g_spawn_command_line_sync` and note it in the test comment.

- [ ] **Step 2: Run** `... -p /ui/input-method`. Expected: FAIL or PASS; record which.
- [ ] **Step 3: Check preedit handling.** `grep -n preedit src/termac-terminal.c` found no handler in the earlier review. If the test shows composed text only appears after commit, add a `preedit-changed` handler that draws the pending text at the cursor in `termac-terminal.c` and a second assertion that the screen is unchanged until commit. If composed input already works end to end, record "preedit not drawn" as a known gap in the plan instead of building it.
- [ ] **Step 4: Run test and full suite.** Expected: 7 of 7.
- [ ] **Step 5: Commit** `test: cover dead keys and unicode entry`.

### Task 4: Paste integrity and backpressure

**Files:**
- Modify: `tests/test-ui.c`, `src/termac-terminal.c` (the `WRITE_QUEUE_LIMIT` check near line 213), `src/termac-vt.c` (`termac_vt_encode_paste`)

**Interfaces:**
- Consumes: `termac_terminal_insert_text (TermacTerminal *, const char *)`, `termac_terminal_paste`, `termac_vt_encode_paste (const char *, gsize, gboolean, gsize *)`.
- Produces: queued writes that never drop bytes; `terminal_queue_bytes` keeps its signature but returns without discarding when the queue is full by pausing the producer (see Step 3).

- [ ] **Step 1: Write three failing tests** (all in `tests/test-ui.c`, plus one in `tests/test-vt.c`)
  - `test_vt.c` `/vt/paste-strips-end-marker`: `termac_vt_encode_paste ("a\x1b[201~b", 7, TRUE, &n)` must contain exactly one `ESC[201~` (the trailing one) and no `ESC` from the payload.
  - `/ui/paste-multiline`: run `cat > /tmp/termac-paste-out`, set clipboard text `"one\ntwo\nthree"` with `gdk_clipboard_set_text`, `termac_terminal_paste`, end with Ctrl+D, assert the file equals the text (newlines kept).
  - `/ui/paste-large`: run `wc -c`, paste a 2 MiB string of `x` plus Ctrl+D, `wait_for_screen (t, "2097152", 20000)`.

- [ ] **Step 2: Run** `meson test -C build vt` and `... -p /ui/paste-large`. Expected: marker test and large test FAIL on the current code (truncation at 1 MiB).
- [ ] **Step 3: Fix.** Make `termac_vt_encode_paste` drop `ESC` bytes (and C0 controls other than `\n`, `\t`) from the payload when bracketed. For the queue, stop discarding: when the queue exceeds the limit, keep the remainder in a per-terminal `GBytes` backlog and drain it from the PTY writable callback; `terminal_send_input` returns after queueing the first chunk.
- [ ] **Step 4: Run** `meson test -C build`. Expected: 7 of 7, `paste-large` within 20 s.
- [ ] **Step 5: Commit** `fix: never drop pasted bytes and strip the paste end marker`.

### Task 5: Independent sessions

**Files:**
- Modify: `tests/test-ui.c`

- [ ] **Step 1: Write `test_sessions_are_isolated`** (`/ui/sessions-isolated`)
  - New tab. In tab 2 run `export TERMAC_ISOLATION=two; cd /usr`; in tab 1 run `echo "[$TERMAC_ISOLATION]"` and `pwd`.
  - Assert tab 1 shows `[]` and not `/usr`; assert tab 2's `termac_terminal_get_cwd` becomes `/usr` after OSC 7 or the shell integration update, and tab 1's does not.
  - Type `isolated-input` into tab 2 without Return; assert tab 1's screen does not contain it; `termac_terminal_get_session_id` of both differ.
- [ ] **Step 2: Run** `... -p /ui/sessions-isolated`. Expected: PASS (existing behaviour); record as evidence.
- [ ] **Step 3: Fix any defect found.** If none, no source change.
- [ ] **Step 4: `meson test -C build`.** Expected: 7 of 7.
- [ ] **Step 5: Commit** `test: prove tabs do not share state`.

### Task 6: Session restore round-trip and bad files

**Files:**
- Modify: `tests/test-ui.c`, `src/termac-application.c` (only if a bad file crashes)

**Interfaces:**
- Consumes: the v3 format `a(iiiba(ss))` documented in `src/termac-application.c`, the `test_session_v2_migration` pattern for closing windows and reactivating the shared app (close a `g_list_copy` of the window list, never iterate the live list).

- [ ] **Step 1: Write two tests**
  - `/ui/session-restore-roundtrip`: open 3 tabs, set titles with `rename-tab` action, `termac_application_save_session_now`, close all windows, `g_application_activate`, assert `tab_count == 3`, same selected index, same cwd values.
  - `/ui/session-corrupt-file`: write `"garbage"` to `session-v3.gvariant`, close windows, activate, assert exactly one window with one tab and the app still alive; repeat with a 0-byte file and a valid GVariant of the wrong type.
- [ ] **Step 2: Run** each with `-p`. Expected: round trip PASS; corrupt-file may FAIL.
- [ ] **Step 3: Fix** `load_sessions()` to return NULL on any parse or type mismatch (`g_variant_is_of_type` before use) and log with `g_warning`.
- [ ] **Step 4: `meson test -C build`.** Expected: 7 of 7.
- [ ] **Step 5: Commit** `fix: start with a fresh tab when the session file is bad`.

### Task 7: Leak run under churn

**Files:**
- Create: `tools/valgrind-churn.sh`, `tools/valgrind.supp`
- Modify: `src/termac-terminal.c` only if the run points at Termac code

- [ ] **Step 1: Write `tools/valgrind-churn.sh`**: builds if needed, then `GSETTINGS_BACKEND=memory GDK_BACKEND=x11 xvfb-run -a valgrind --leak-check=full --show-leak-kinds=definite --error-exitcode=2 --suppressions=tools/valgrind.supp --log-file=<dir>/valgrind-%p.log build/tests/test-ui -p /ui/tab-churn` and prints the `definitely lost` summary and the top Termac frames.
- [ ] **Step 2: Run it** and compare two runs of different length (set the churn iterations through `TERMAC_CHURN_ROUNDS`, default 10, in `test_tab_churn`; run with 10 and with 40). Expected evidence: definite loss from `termac_terminal_init` and `termac_terminal_set_font_size` stays constant (Pango font cache) rather than scaling with rounds.
- [ ] **Step 3: Classify.** If loss scales with rounds, trace the allocation stack and fix in `termac-terminal.c`. If it is constant, put a `tools/valgrind.supp` entry for the Pango metrics frames and the libadwaita `aligned_alloc` mismatch; keep entries minimal.
- [ ] **Step 4: Re-run the script.** Expected: exit 0 with the suppressions, no invalid read/write reports.
- [ ] **Step 5: Update `PLANO_IMPLEMENTACAO.md`** under "Prioridades — 13/11/2026" with results of Tasks 1–7 (what passed, what was fixed, what is still unverified such as real IME engines and Wayland), then commit:

```bash
git add tools/valgrind-churn.sh tools/valgrind.supp tests/test-ui.c PLANO_IMPLEMENTACAO.md
git commit -m "test: leak run under tab churn and record P0 results"
```

## Self-Review

- **Spec coverage:** vim (T1), IME (T3), mouse tracking (T2), paste (T4), independent sessions (T5), restore (T6), leaks (T7). Reproducible-failure fixes live inside each task.
- **Not covered, stated plainly:** real IME engines (ibus, fcitx) and Wayland-specific behaviour need manual runs; Xvfb cannot prove them. Record this in Task 7 Step 5.
- **Type consistency:** helpers `screen_contains`, `wait_for_screen`, `run_in_terminal` are defined in Task 1 and used by Tasks 2–5 with the same signatures.
- **Open decision flagged in Task 3 Step 3:** whether preedit drawing is built now or recorded as a gap, decided by what the test shows.
