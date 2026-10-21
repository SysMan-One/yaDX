# AGENTS.md — instructions for AI coding agents

This file is for AI agents (Claude, Codex, Cursor, Aider, …) working on
yaDX. Humans: see [`CONTRIBUTING.md`](CONTRIBUTING.md) — it is the full
version of the same rules, with examples. When the two differ,
CONTRIBUTING.md wins; tell the maintainer.

## What this project is

yaDX is **DX 2.5**, a full-screen file manager for **OpenVMS** (SMG$ screen,
RMS/QIO files), written by Chau-Kuang Hung (1989-1992) and Jérôme Lauret
(1994-1999), edited in 2026 by StarLet Squad and Ruslan R. Laishev. User
docs: `README.md`.

| File | Role |
|---|---|
| `DX.C` | **head module** — its `__IDENT__` is the product version; start, CLI, terminal checks |
| `FILER.C` | file window: main key loop `DX_process_loop()`, keypad, windows, menus (`navigator()`, `select_from_submenu()`) |
| `FILERCPY.C`, `FILERSTA.C`, `FILEROPT.C` | file commands: copy/delete/rename…; status/type/edit/print; Option/Customize menus |
| `FILERQUE.C` | directory scan, cache, sort, filters |
| `DIRECT.C` | directory tree window |
| `INQUIRE.C` | input line, yes/no, shared SMG$ helpers (`dxscr_*`, `dxscr_pb_exit()`) |
| `DIFF.C` | file compare |
| `GLOBAL.H` | shared: `$LOG`, `DX_KEY_TOUPPER/ISALNUM`, limits, `cntrl_info_block` |
| `DX_MSG.MSG`, `DX_CLI.CLD`, `DX.CLD` | messages `DXM__*`; internal CLI; DCL verb |
| `DX_BUILD.COM`, `DX_MODULES.LIS` | the build; module list |
| `DX.HLP` | `HELP DX` text → `DXHELP.HLB` |
| `KEYCODE.C` | stand-alone tool: shows SMG key codes |
| `tools/mk_aaareadme.py` | makes `AAAREADME.TXT` from `README.md` |

## Build and test

The code only builds on OpenVMS (HP C V7.1, OpenVMS Alpha V8.3 reference).
There is no Linux build and no unit-test framework — do not invent one.

```
$ @DX_BUILD                       ! 9 modules; there must be NO %CC-I/W/E/F- and NO %LINK- lines
```

Do not trust the `Compiled OK:` summary alone — read the output. Then run
DX on a VT-type terminal and try the change plus: list movement, W and DO
menus, arrows in a submenu, exit by KP0 / CTRL/Z / Window → eXit.

If you can not reach an OpenVMS system, say so plainly in your report: code
you have not compiled on VMS is **untested**, never claim otherwise.

## Hard rules (a change that breaks one is wrong even if it works)

Old code vs new code
- The DX 2.5 code keeps its layout. **Never reformat, re-indent or
  "modernize" it**; change only what the fix needs.
- **Never remove or shorten anything about the original authors**: headers
  (`Original Author`, Copyright), `COPYRIGHT`, `COPYING`, `README_1999.TXT`,
  `HISTORY.TXT`, the Thanks/License sections, `DXVERSION`.
- License: GNU GPL v2 or later (`COPYRIGHT`, text in `COPYING`). A **new** file gets
  `Copyright 2026 (c) StarLet Squad and Ruslan R. Laishev` and the GPL notice in its header
  (copy `KEYCODE.C`); never add or change a license line in an old file.
- Keep DEC C idioms: `&LIB$M_CLI_CTRLY`, `&1` (address of a constant); if a type
  is needed write `&(unsigned int) CONST`. Do not port VMS/DEC C idioms to ISO C.

Formatting (new code)
- Tabs only, width 8. Lines up to 192 columns; join old 80-column cuts you touch.
- Whitesmiths braces; a routine's outer braces are not indented; struct braces at column 0.
- No braces around a single statement under `if`/`else`/`for`. Declarations first, column 0.
- Copy the layout of `s_filer_scroll_off()` in `FILER.C` (it predates the `if ( … )` spacing and the routine template).
- Routine template: the VMS comment block, then one parameter per line and `)` on its own line:
  `static uint32_t s_name (` / `<tab>const char<tab>*a_arg` / `<tab>)` / `{`.
- Struct/union: `typedef struct name_t {` … `} MODULE$NAME;` with the members indented one tab.
- Group the declarations by type. In new code use `<stdint.h>` types (`uint32_t`) so the size is visible.
- A space inside the parentheses of a condition: `if ( !a_name->dsc$w_length || a_name->dsc$w_length > MODULE$K_NAMESZ )`.
- Loop counter inside the loop: `for (int i = 0; i < n; i++)`. Use an `enum` for a set of ordered constants.
- Comment what is not obvious. Do not write half a screen above a constant or a one-line routine.

Names
- `g_` global, `l_` local, `s_` static (also static routines), `a_` input parameter, `m_`/`v_` bit mask/field.
- Messages `DXM__NAME`; constants `MODULE$K_NAME`; types `name_t` + `MODULE$NAME`.
- Do not rename the old exported routines. **Never rename by regex** (see Traps).

Behaviour
- Check every status on its low bit. Report a failed RTL/system call with `$LOG(status)`
  (`GLOBAL.H`; it returns the status: `return $LOG(l_rc);`).
- **No `printf` family, no `stderr`**: `SYS$FAO`/`LIB$FAO`, `smg$put_chars`, `LIB$PUT_OUTPUT`.
- `#define __NEW_STARLET 1` first in every module, system headers before project ones.
  Keep **0 diagnostics**; fix types in declarations, not with casts at each call.
- Prefer RTL/system services (`LIB$`, `SMG$`, `STR$`, `SYS$`) over hand-written code.
  Use as little C RTL as you can in new code. Commands: `CLI$` + `.CLD`. Messages: `.MSG` + `$GETMSG`/`$PUTMSG`.
  Help: `.HLP` → `.HLB`. Build: DCL (`@DX_BUILD`).
- A routine returns a condition value. Results go out through output parameters.
- Work with lengths (descriptors, counted strings). Do not call `strlen()` twice on the same string.
- When a C RTL call fails, report what it returned and `errno`/`vaxc$errno`, so the cause is not hidden.
- yaDX is VMS code: **no StarLet libraries** (utility routines, their `$LOG`/`$PUTMSG`, their HELP). yaDX has its own
  `$LOG(status)` in `GLOBAL.H` (no `STS$K_*`) and the standard VMS help: `DX.HLP` → `DXHELP.HLB`, `HELP/MESSAGE`.

Headers and versions
- `__IDENT__` `X-aa-bb[ECOcc]` ↔ `__REV__` `aa.bb.cc`. Bump **both** in every file you change, add a history line.
  A bug fix bumps the ECO number. Product version = `DX.C`.
- Every source file carries `__MODULE__`/`__IDENT__`/`__REV__`: `#define` in `.C`; in `.H` as text in the header comment
  (a `#define` there would redefine the `.C` one); `!`/`$!` comment header in `.MSG`, `.CLD`, `.HLP`, `.COM`, `.MMS`
  (plus the native `.Ident` / `Ident` in `.MSG` / `.CLD`). No header: `DX_MSG.H` (generated), `GEN_MSGH.COM`,
  and the data files `DX_MODULES.LIS`, `DX_IA64.OPT`, `DX$STARTUP.DAT`.
- Sign history lines with **your own** initials (Claude uses `CLD`). `RRL` is the maintainer's mark: contributors and
  their agents **never use it**. Only an agent working for the maintainer signs `RRL`, and only for a change the maintainer
  told it how to make; its own work (designed and written from scratch) gets its own initials.
  Never rewrite someone else's history line.
- Archives: `yaDX-<IDENT>.tar.gz`.

Documentation
- New or changed message → update the "Messages and what to do" tables in README.md, English **and** Russian.
- New or changed key/menu → update `DX.HLP` and the Keys tables in README.md.
- Never edit `AAAREADME.TXT` by hand; regenerate it with `python3 tools/mk_aaareadme.py`.
- User docs are for beginners: simple English, the Russian part says the same.

## Traps (each one cost a debugging session)

1. SMG key codes are > 255 (DOWN 275, KP0 260, DO 296). DEC C `_toupper(c)` is `(toupper)(c)`, undefined
   above 255 → garbage. Use `DX_KEY_TOUPPER()`/`DX_KEY_ISALNUM()`; never `strchr()` a code ≥ 256.
2. `smg$begin/end_display_update` only around content writes; never around paste/repaste/unpaste,
   `smg$get_pasting_info`, create/delete virtual display (crashed with `%DX-F-SFC`).
3. `LIB$PUT_OUTPUT` of an empty record sends only `<CR>` — no scroll. Send `"\n"`.
4. Regex renaming breaks `fab$l_fna` (`$` is a word boundary), `->field`, `.field`, field
   declarations and string literals. Rename by hand; grep `->l_ ->a_ .l_ .a_ l_xxx$` afterwards.
5. DCL: an assignment resets `$SEVERITY` — `S = $STATUS` then `V = $SEVERITY` gives 1. Use `V = S .AND. 7`.
6. Terminal emulators send the grey keypad `-` as KP `-`, PF4 or plain `-`. Check with `KEYCODE` first.
   Home/End/Ins arrive as E1/E4/E2 only in VT mode (`ESC[1~`/`ESC[4~`/`ESC[2~`); xterm `ESC[H`/`ESC[F` → 511 (unknown).
7. Build logs captured over ssh contain NUL bytes — `grep` calls them binary; use `tr -d '\r\000' | grep -a`.
8. DCL has no command separator: over `ssh host "cmd"` one command per call — put sequences into a `.COM` file.
9. Years have 4 digits (DX once printed "126" for 2026).
10. `lib$find_file` (and other RTL calls with a static descriptor) blank-pads the result and writes **no EOS**.
    Put `buf[MAXFILESPEC] = EOS` in the extra byte before any `strchr()`/`strrchr()` on it — otherwise they
    run into the next stack variables (the directory tree once lost all subdirectories this way).

## How to report

- Talk to the maintainer in Russian; code, comments and commits are in English.
- Before a big change (a new module, a new subsystem) write down the plan and the file layout
  first. Draw a diagram if it helps. Wait for the maintainer to say yes.
- Explain changes with `MODULE:line` references.
- State exactly what was built and run, where, and what the output was. If something failed or was skipped, say so.
- Commits: `yaDX <IDENT>: what and why`, in English.
