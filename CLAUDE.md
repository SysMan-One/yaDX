# CLAUDE.md

The yaDX rules for all AI agents are in AGENTS.md (human version: CONTRIBUTING.md).

@AGENTS.md

Below are the maintainer's general rules (from `Rules-for-coding-for-AI.txt`). Where yaDX differs, AGENTS.md wins;
the differences are marked **yaDX:**.

## When we start a new project

- Think the task through before writing code; draw diagrams where they help.
- Work out the project structure in as much detail as possible.
- Talk to the maintainer in Russian.

## New C code

- CMake if the project has more than one file; no identifiers with `$` inside CMake files.
  **yaDX:** no CMake, the build is `DX_BUILD.COM`.
- Lines up to 192 columns (not 80). Tabs only, width 8, no spaces.
- Whitesmiths braces (brace on its own line, indented with the block). A routine's outer braces are not indented.
  Struct braces at column 0, members indented one tab; struct tags end in `_t`:

```c
typedef struct modbus_pdu_t {
	uint64_t	seq;
} MODBUS$PDU;
```

- Routine layout:

```c
static uint32_t	s_cmd_set_log_level (
	const char	*a_arg
	)
{
}
```

- Prefixes: `g_` global, `l_` local, `s_` static (also static routines), `a_` input parameter, `m_` bit mask,
  `v_` bit field in a structure.
- Macros and constants use `$` as the word separator, VMS style: `MODULE$K_CONST`, `$UTIL_MACRO`.
- Logging only through `$LOG` with an explicit `STS$K_*`. **yaDX:** its own `$LOG(status)` in `GLOBAL.H`, no `STS$K_*`.
- Describe every routine in the VMS style: inputs, outputs, explicit, implicit, returns.
  Comment in detail but to the point — not half a screen above a constant or a one-line routine.
- Every file starts with `__MODULE__`, `__IDENT__`, `__REV__`. `__IDENT__` is VMS style `X-aa-bbECOcc` (a small bug fix
  is an ECO); `__REV__` is the Unix form `aa.bb.cc` made from it. Keep both exact. The product ident is the head module's.
- A revision history in the file header, as in OpenVMS.
- No braces around a single statement under `if`. A space inside the parentheses of a condition:
  `if ( !a_name->dsc$w_length || a_name->dsc$w_length > YADCX$K_NAMESZ )`.
- Declarations first, at column 0, grouped by type; prefer `<stdint.h>` types (`uint32_t`, `be32`) so the size is explicit.
- Work with lengths and sizes; do not call `strlen()` again on the same string.
- A routine returns a status; values go out through output parameters. Check every status on its low bit.
- Loop counters in the loop: `for (int i = 0; i < n; i++)`. Ordered constants are an `enum`.
- On a C RTL error, print the return value or `errno` / `rte_errno`, so the cause is not hidden.
- No `$` in shell/bash environment variable names on Unix systems.
- Author: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan); history mark `RRL`.
  **yaDX:** the original authors stay in every header; `CLD` for what Claude designed and wrote itself,
  `RRL` for a change made the way the maintainer said (their idea or advice). If you are not sure which, ask.
- Archives: `<package>-<ident>.tar.gz`.
- User guides in two languages, Russian and English, written for beginners in simple English; include troubleshooting
  and "what to do if you see this message".
- Use the utility routines library as much as possible; log in the VMS style with `$PUTMSG`/FAO from the StarLet
  packages and HELP. **yaDX:** not for VMS code — see below.
- In explanations (not in code or code comments) point at `module:line`.

## Code for VMS

- As little C RTL as possible; use the VMS libraries (`LIB$`, `SMG$`, `STR$`, `SYS$`, `CLI$`, `LBR$`).
  StarLet libraries and StarLet HELP are not used in VMS code.
- Command parsing: `CLI$` and a `.CLD`.
- Messages: a `.MSG` file and `$GETMSG`/`$PUTMSG`.
- Help: `.HLP` → `.HLB`.
- Build with DCL (a `.COM` procedure). **yaDX:** `@DX_BUILD`.
