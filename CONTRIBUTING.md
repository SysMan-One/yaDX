# Contributing to yaDX

*Русский текст — ниже, после английского.*

Thank you for helping. yaDX is DX 2.5 — code written by Chau-Kuang Hung and
Jérôme Lauret in 1989-1999 — edited in 2026 in the house style of StarLet
Squad. Two things follow from that:

- **New code** follows the house style below. It is not optional.
- **Old code** keeps its own layout. Do not reformat it, do not re-indent it,
  do not "modernize" it. Change only what your fix needs. Never remove the
  original authors' names, copyrights or notes.

AI agents: read [`AGENTS.md`](AGENTS.md) as well.

## 1. Before you start

- You need OpenVMS Alpha (V8.3 is the reference) and DEC C / HP C (V7.1 is
  the reference). HP C V7.1 has **no `<stdint.h>`**: use `<inttypes.h>`.
- Build with `@DX_BUILD` (`@DX_BUILD DEBUG` for `DX_DEBUG.EXE`). All 9
  modules must compile with **no** `%CC-I-`, `%CC-W-`, `%CC-E-`, `%CC-F-` and
  no `%LINK-` messages. Today the count is zero; keep it zero.
- Read the build output yourself. The summary line `Compiled OK: 9  Failed: 0`
  is a help, not a proof.
- There is no automatic test. After a change, run DX on a VT-type terminal
  and try what you changed, plus the basics: move in the list, open a menu
  (W, DO), move in a submenu with the arrows, leave with KP0, CTRL/Z and
  Window → eXit. Use `KEYCODE` to see what your terminal sends.

## 2. Files and headers

Every C module starts with a header: program name, `Original Author`,
`Editor`, description, and the revision history, then `__MODULE__`,
`__IDENT__`, `__REV__`:

```c
/*   Original Author       : C. K. Hung                                   */
/*   Editor                : StarLet Squad and Ruslan R. Laishev (RRL)  */
...
/*  26-Sep-2026 CLD ECO3: DO / keypad "-" / PF4 / "-" open/close the menu;  */

#define __MODULE__	"FILER"
#define __IDENT__	"X-02-05ECO3"
#define __REV__		"02.05.03"
```

- `Original Author` stays as it is — always. The 2026 editors are named in a
  separate `Editor` line.
- In the history, sign with **your own** three-letter initials. `RRL` is the
  maintainer's own mark — never use it. `CLD` marks changes that Claude
  designed and wrote itself; a change Claude made on the maintainer's advice
  is signed `RRL`.
  Never rewrite someone else's history line.
- DCL procedures carry the same header as `$!` comments (see `DX_BUILD.COM`).
- License: yaDX is under the GNU GPL, version 2 or any later version
  (`COPYRIGHT`, full text in `COPYING`). By sending a change you agree that it
  goes under the same terms. A **new** file gets a copyright line and the GPL
  notice in its header (see `KEYCODE.C`); an old file keeps its original
  notice untouched.

### IDENT and REV

- `__IDENT__` is VMS style: `X-aa-bb`, a bug fix adds `ECOcc`: `X-02-05ECO4`.
- `__REV__` is the Unix form made from it: `aa.bb.cc` → `02.05.04`.
- Bump **both** together in every file you change, and add a history line.
- The product version is the one of the head module `DX.C`.
- Archives are named `<package>-<ident>.tar.gz`: `yaDX-X-02-05ECO4.tar.gz`.

## 3. Formatting (new code)

- **Tabs only**, tab width 8. No spaces for indentation or alignment.
- Lines up to **192** columns. When you touch an old expression cut for 80
  columns, join it up to 192.
- **Whitesmiths** braces: the brace goes on its own line, indented to the
  level of the block. The outer braces of a routine are **not** indented.
  Structure braces at column 0, members indented.
- A single statement under `if`/`else`/`for` takes **no** braces.
- Declarations first, at column 0, grouped by type. In new code use the
  `<stdint.h>` types (`uint32_t`), so the size is visible.
- A space inside the parentheses of a condition:
  `if ( !a_name->dsc$w_length || a_name->dsc$w_length > MODULE$K_NAMESZ )`.
- A loop counter is declared in the loop: `for (int i = 0; i < n; i++)`.
  A set of ordered constants is an `enum`.
- Routine parameters one per line, the closing `)` on its own line:

```c
static uint32_t	s_cmd_set_log_level (
	const char	*a_arg
	)
{
}
```

- Structures and unions:

```c
typedef struct modbus_pdu_t {
	uint64_t	seq;
} MODBUS$PDU;
```

A new routine, as `s_filer_scroll_off()` in `FILER.C`:

```c
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	s_filer_scroll_off() moves the physical cursor to column 1 of the
**	pasteboard's last row and writes a one-<LF> record to SYS$OUTPUT, ...
**
**  INPUT PARAMETERS:		None.
**  OUTPUT PARAMETERS:		None.
**  IMPLICIT INPUTS:		cntrl_info_block.pasteboard_id, ...
**  IMPLICIT OUTPUTS:		The terminal: cursor position and one line of scroll.
**
**  RETURNS VALUE:
**
**	Condition value of the first failing call (already reported via
**	$LOG), else SS$_NORMAL.
**--
**/
static unsigned int	s_filer_scroll_off(void)
{
unsigned int	l_rc;
$DESCRIPTOR(l_lf, "\n");

	if (!(1 & (l_rc = smg$set_physical_cursor(&cntrl_info_block.pasteboard_id, &cntrl_info_block.pasteboard_rows, &1))))
		return	$LOG(l_rc);

	if (!(1 & (l_rc = lib$put_output(&l_lf))))
		return	$LOG(l_rc);

	return	SS$_NORMAL;
}
```

(The real block in `FILER.C` has each section on its own lines. The sample
was written before the `if ( … )` spacing rule.)

## 4. Names

| What | Rule | Example |
|---|---|---|
| global variable | `g_` | `g_count` |
| local variable | `l_` | `l_rc`, `l_keystroke` |
| static variable or routine | `s_` | `s_find_cache_head`, `s_filer_scroll_off()` |
| routine input parameter | `a_` | `a_status`, `a_sig` |
| bit mask / bit field | `m_` / `v_` | `m_list`, `v_verify` |
| constant, macro | `MODULE$K_NAME`, `$MACRO` | `$LOG` |
| structure tag / type | `name_t` / `MODULE$NAME` | |
| message code | `DXM__NAME` (in `DX_MSG.MSG`) | `DXM__PGWID` |

The old exported routines keep their names (`filer_copy`, `dxscr_pb_exit`,
`navigator`...). Do not rename them.

## 5. Code rules

- **Every status is checked on its low bit**: `if (!(1 & (l_rc = ...)))`.
- A failed RTL or system call is reported with **`$LOG(status)`**
  (`GLOBAL.H`): it signals `%DX-F-SFC` with the line and module, and gives
  the status back, so `return $LOG(l_rc);` works.
- New routines return a status; results go through output parameters.
- **No `printf` family** anywhere (`printf`, `sprintf`, `fprintf`...).
  Format with `SYS$FAO`/`LIB$FAO` (`!%D`, `!%T` for VMS times), write to the
  screen with `smg$put_chars` and friends, to SYS$OUTPUT with
  `LIB$PUT_OUTPUT`. No writes to `stderr`.
- New messages go into `DX_MSG.MSG` and into the **"Messages and what to
  do"** tables of README.md, in both languages.
- `#define __NEW_STARLET 1` at the top of every module, then `<starlet.h>`,
  `<lib$routines.h>`, `<str$routines.h>`, the other system headers, and only
  then the project `"*.h"`. Fix types **in the declarations** (SMG ids are
  `unsigned int`), not with casts at every call.
- Keep the DEC C idioms: `&LIB$M_CLI_CTRLY`, `&1` — the address of a
  constant. If `__NEW_STARLET` wants a type, write
  `&(unsigned int) LIB$M_CLI_CTRLY`. Do not replace them by variables.
- Use the Run-Time Library and system services (`LIB$`, `SMG$`, `STR$`,
  `SYS$`) instead of writing your own. Use as little C RTL as you can in new
  code: commands through `CLI$` and a `.CLD`, messages through a `.MSG` and
  `$GETMSG`/`$PUTMSG`, help through a `.HLP` → `.HLB`.
- **No StarLet libraries in VMS code** (utility routines, their `$LOG` and
  `$PUTMSG` wrappers, their HELP) — they are for non-VMS projects. yaDX has
  its own `$LOG(status)` in `GLOBAL.H`.
- Work with lengths (descriptors, counted strings); do not call `strlen()`
  twice on the same string.
- When a C RTL call fails, report what it returned and `errno`/`vaxc$errno`,
  so the cause is not hidden.
- Comment in detail but to the point: every new routine gets the VMS block
  (`FUNCTIONAL DESCRIPTION`, `INPUT/OUTPUT PARAMETERS`, `IMPLICIT
  INPUTS/OUTPUTS`, `RETURNS VALUE`). Do not write half a screen above a
  constant or a one-line routine.

## 6. Traps already met — do not step into them again

- **SMG key codes are bigger than 255** (DOWN = 275, KP0 = 260, DO = 296).
  `_toupper()`, `toupper()`, `isalnum()` are defined only for 0..255: in DEC C
  `_toupper(c)` is `(toupper)(c)` and turns a key code into garbage. Use
  `DX_KEY_TOUPPER()` / `DX_KEY_ISALNUM()` from `GLOBAL.H`, and do not
  `strchr()` a key code ≥ 256 in a string of letters.
- **`smg$begin/end_display_update` only around content writes**
  (`smg$put_chars`, `smg$erase_display`...). Never around
  `smg$paste_virtual_display`, `smg$repaste_...`, `smg$unpaste_...`,
  `smg$get_pasting_info`, `smg$create_...`, `smg$delete_virtual_display` —
  it crashed with `%DX-F-SFC`.
- **`LIB$PUT_OUTPUT` of an empty record** sends only `<CR>` to the terminal,
  no line feed. For a scroll, send a record holding `"\n"`.
- **No mass renaming by regular expressions.** `\bfab\b` also hits
  `fab$l_fna` (`$` is a word boundary), `->field`, `.field`, field
  declarations and string literals. Rename by hand, then grep for
  `->l_`, `->a_`, `.l_`, `.a_`, `l_xxx$`.
- **DCL: an assignment resets `$SEVERITY`.** `X = $STATUS` followed by
  `Y = $SEVERITY` gives 1. Take the severity from the saved status:
  `Y = X .AND. 7`.
- **Terminal programs differ.** The grey keypad `-` may come as KP `-`, as
  PF4 or as a plain `-`. Check with `KEYCODE` before you "fix" a key.
  Home, End and Ins reach DX as E1, E4, E2 only when the terminal sends
  `ESC [1~`, `ESC [4~`, `ESC [2~`; the xterm codes give 511 (unknown).
- **Years have 4 digits.** DX once showed "126" for 2026.
- **`lib$find_file` writes no EOS.** It fills the result with blanks up to
  the descriptor length. Put `EOS` into the extra last byte of the buffer
  before any `strchr()`/`strrchr()`, or they run into the next variables on
  the stack (the directory tree lost all its subdirectories this way).

## 7. Documentation

- Two READMEs: `README.md` (source, English + Russian) and `AAAREADME.TXT`
  (for VMS). **Never edit `AAAREADME.TXT` by hand**: change README.md and run
  `python3 tools/mk_aaareadme.py`.
- User documentation is written for beginners: short sentences, simple
  English words; the Russian part says the same thing.
- If you change keys or menus, update `DX.HLP` too (it becomes `HELP DX`).
- Everything about the original authors stays: the headers, `COPYRIGHT`,
  `COPYING`, `README_1999.TXT`, `HISTORY.TXT`, the Thanks and License sections.

## 8. Sending a change

- Before a big change (a new module, a new subsystem), write down the plan
  and the file layout first — with a diagram if it helps — and agree it with
  the maintainer.
- One logical change per commit. Message: `yaDX <IDENT>: what and why`, in
  English.
- Say what you built and tried, on which VMS version and compiler.
- When you explain a change, point at `MODULE:line` (for example
  `FILER.C:5790`).

---

# Как помочь проекту yaDX

Спасибо за помощь. yaDX — это DX 2.5, код, который написали Chau-Kuang Hung
и Jérôme Lauret в 1989-1999 годах, отредактированный в 2026 году в стиле
StarLet Squad. Отсюда два правила:

- **Новый код** пишется в стиле, описанном ниже. Это обязательно.
- **Старый код** сохраняет своё оформление. Не переформатируйте его, не
  меняйте отступы, не «осовременивайте». Меняйте только то, что нужно для
  исправления. Никогда не удаляйте имена авторов оригинала, копирайты и их
  заметки.

AI-агентам: прочитайте также [`AGENTS.md`](AGENTS.md). Образцы кода — в
английской части выше.

## 1. Перед началом

- Нужны OpenVMS Alpha (эталон — V8.3) и DEC C / HP C (эталон — V7.1). В HP C
  V7.1 **нет `<stdint.h>`**: используйте `<inttypes.h>`.
- Сборка: `@DX_BUILD` (`@DX_BUILD DEBUG` — `DX_DEBUG.EXE`). Все 9 модулей
  должны компилироваться **без** `%CC-I-`, `%CC-W-`, `%CC-E-`, `%CC-F-` и без
  `%LINK-`. Сейчас их ноль — так и должно остаться.
- Читайте вывод сборки сами. Строка `Compiled OK: 9  Failed: 0` — подсказка,
  а не доказательство.
- Автоматических тестов нет. После изменения запустите DX на терминале типа
  VT и проверьте изменённое и основное: ходьбу по списку, открытие меню (W,
  DO), стрелки в подменю, выход по KP0, CTRL/Z и Window → eXit. Что шлёт ваш
  терминал — покажет `KEYCODE`.

## 2. Файлы и шапки

- Каждый C-модуль начинается с шапки: имя программы, `Original Author`,
  `Editor`, описание, история изменений, затем `__MODULE__`, `__IDENT__`,
  `__REV__`.
- `Original Author` остаётся как есть — всегда. Редакторы 2026 года указаны
  отдельной строкой `Editor`.
- В истории подписывайтесь **своими** тремя буквами. `RRL` — метка
  сопровождающего, её не используйте. `CLD` — то, что Claude придумал и
  написал сам; изменение, сделанное Claude по совету сопровождающего,
  подписывается `RRL`.
  Чужие строки истории не переписывайте.
- DCL-процедуры несут такую же шапку в комментариях `$!` (см. `DX_BUILD.COM`).
- Лицензия: yaDX распространяется по GNU GPL версии 2 или любой более поздней
  (`COPYRIGHT`, полный текст в `COPYING`). Присылая изменение, вы соглашаетесь,
  что оно выходит на тех же условиях. **Новый** файл получает в шапке строку
  копирайта и уведомление GPL (см. `KEYCODE.C`); у старого файла исходное
  уведомление остаётся нетронутым.
- `__IDENT__` — стиль VMS: `X-aa-bb`, исправление ошибки добавляет `ECOcc`.
  `__REV__` — Unix-форма: `aa.bb.cc`. Меняйте **оба** вместе в каждом
  изменённом файле и добавляйте строку в историю. Версия продукта — версия
  головного модуля `DX.C`.
- Архивы: `<пакет>-<ident>.tar.gz`, например `yaDX-X-02-05ECO4.tar.gz`.

## 3. Оформление (нового кода)

- **Только табуляция**, ширина 8. Никаких пробелов для отступов.
- Строки до **192** колонок. Если правите старое выражение, порезанное под
  80 колонок, — сворачивайте его до 192.
- Скобки **Whitesmiths**: скобка на своей строке с отступом блока; внешние
  скобки тела рутины **без** отступа; скобки структуры в колонке 0.
- Одиночный оператор под `if`/`else`/`for` — **без** скобок.
- Объявления в начале, с колонки 0, сгруппированы по типу. В новом коде —
  типы из `<stdint.h>` (`uint32_t`), чтобы размер был виден.
- Пробел внутри скобок условия:
  `if ( !a_name->dsc$w_length || a_name->dsc$w_length > MODULE$K_NAMESZ )`.
- Счётчик цикла объявляется в цикле: `for (int i = 0; i < n; i++)`.
  Упорядоченные константы — `enum`.
- Параметры рутины — по одному на строке, `)` на отдельной строке;
  структуры — `typedef struct имя_t {` … `} MODULE$ИМЯ;` (примеры — в
  английской части, раздел 3).

## 4. Имена

Префиксы: `g_` — глобальные, `l_` — локальные, `s_` — статические переменные
и рутины, `a_` — входные параметры, `m_`/`v_` — битовые маски/поля.
Константы и макросы — `MODULE$K_ИМЯ`, `$МАКРОС`; типы — `имя_t` /
`MODULE$ИМЯ`; коды сообщений — `DXM__ИМЯ` в `DX_MSG.MSG`. Старые
экспортируемые рутины (`filer_copy`, `dxscr_pb_exit`, `navigator`...) не
переименовывать.

## 5. Правила кода

- **Каждый статус проверяется по младшему биту**: `if (!(1 & (l_rc = ...)))`.
- Неудачный вызов RTL или системного сервиса — через **`$LOG(status)`**
  (`GLOBAL.H`): сигналит `%DX-F-SFC` со строкой и модулем и возвращает
  статус, поэтому работает `return $LOG(l_rc);`.
- Новые рутины возвращают статус, результаты — через выходные параметры.
- **Никакого семейства `printf`** (`printf`, `sprintf`, `fprintf`...).
  Форматирование — `SYS$FAO`/`LIB$FAO` (`!%D`, `!%T` для времени VMS), на
  экран — `smg$put_chars` и т.п., в SYS$OUTPUT — `LIB$PUT_OUTPUT`. Ничего в
  `stderr`.
- Новые сообщения — в `DX_MSG.MSG` и в таблицы «Сообщения и что делать»
  README.md на обоих языках.
- `#define __NEW_STARLET 1` в начале каждого модуля, затем `<starlet.h>`,
  `<lib$routines.h>`, `<str$routines.h>`, прочие системные заголовки, и
  только потом проектные `"*.h"`. Типы чинить **в объявлениях** (id SMG —
  `unsigned int`), а не кастами в каждом вызове.
- Сохранять идиомы DEC C: `&LIB$M_CLI_CTRLY`, `&1` — адрес константы. Если
  `__NEW_STARLET` требует тип — `&(unsigned int) LIB$M_CLI_CTRLY`. Не
  заменять на переменные.
- По максимуму использовать RTL и системные сервисы (`LIB$`, `SMG$`, `STR$`,
  `SYS$`). C RTL в новом коде — по минимуму: команды через `CLI$` и `.CLD`,
  сообщения через `.MSG` и `$GETMSG`/`$PUTMSG`, справка через `.HLP` → `.HLB`.
- **Никаких библиотек StarLet в коде для VMS** (utility routines, их обёртки
  `$LOG` и `$PUTMSG`, их HELP) — они для проектов не под VMS. В yaDX свой
  `$LOG(status)` в `GLOBAL.H`.
- Работать на длинах (дескрипторы, строки со счётчиком); не вызывать
  `strlen()` дважды для одной строки.
- При ошибке C RTL выводить, что вернул вызов, и `errno`/`vaxc$errno`, чтобы
  причина не терялась.
- Комментарии подробные, но по делу: у каждой новой рутины блок VMS. Не
  писать полэкрана перед константой или однострочной рутиной.

## 6. Грабли, на которые уже наступали

- **Коды клавиш SMG больше 255** (DOWN = 275, KP0 = 260, DO = 296).
  `_toupper()`, `toupper()`, `isalnum()` определены только для 0..255; в DEC
  C `_toupper(c)` — это `(toupper)(c)`, и код клавиши превращается в мусор.
  Используйте `DX_KEY_TOUPPER()` / `DX_KEY_ISALNUM()` из `GLOBAL.H` и не
  ищите `strchr()` код ≥ 256 в строке букв.
- **`smg$begin/end_display_update` — только вокруг записи содержимого.**
  Никогда вокруг paste/repaste/unpaste, `smg$get_pasting_info`, create/delete
  virtual display — это падало с `%DX-F-SFC`.
- **`LIB$PUT_OUTPUT` пустой записи** шлёт на терминал только `<CR>`, без
  перевода строки. Для прокрутки — запись из `"\n"`.
- **Никаких массовых переименований регулярными выражениями.** `\bfab\b`
  задевает `fab$l_fna` (`$` — граница слова), `->поле`, `.поле`, объявления
  полей и строковые литералы. Переименовывать вручную, потом grep на `->l_`,
  `->a_`, `.l_`, `.a_`, `l_xxx$`.
- **DCL: присваивание сбрасывает `$SEVERITY`.** После `X = $STATUS` строка
  `Y = $SEVERITY` даст 1. Брать из сохранённого статуса: `Y = X .AND. 7`.
- **Эмуляторы терминала разные.** Серый `-` на цифровой клавиатуре приходит
  как KP `-`, как PF4 или как обычный `-`. Проверяйте `KEYCODE`, прежде чем
  «чинить» клавишу.
  Home, End и Ins приходят в DX как E1, E4, E2, только если терминал шлёт
  `ESC [1~`, `ESC [4~`, `ESC [2~`; коды xterm дают 511 (неизвестная клавиша).
- **Год — 4 цифры.** Однажды DX показывал «126» вместо 2026.
- **`lib$find_file` не ставит EOS.** Результат добивается пробелами до длины
  дескриптора. Перед `strchr()`/`strrchr()` положите `EOS` в последний лишний
  байт буфера, иначе поиск уйдёт в соседние переменные стека (так дерево
  каталогов потеряло все подкаталоги).

## 7. Документация

- `README.md` — источник (английский + русский), `AAAREADME.TXT` — для VMS.
  **`AAAREADME.TXT` руками не править**: правьте README.md и запускайте
  `python3 tools/mk_aaareadme.py`.
- Руководство пользователя — для начинающих, простыми словами.
- Меняете клавиши или меню — обновите и `DX.HLP` (из него `HELP DX`).
- Всё об авторах оригинала сохраняется: шапки, `COPYRIGHT`, `COPYING`,
  `README_1999.TXT`, `HISTORY.TXT`, разделы «Благодарности» и «Лицензия».

## 8. Как прислать изменение

- Перед крупным изменением (новый модуль, новая подсистема) сначала
  опишите план и структуру файлов — если нужно, с диаграммой — и согласуйте
  с сопровождающим.
- Один логический шаг — один коммит. Сообщение по-английски:
  `yaDX <IDENT>: что и зачем`.
- В описании — что собрано и проверено, на какой версии VMS и каком
  компиляторе.
- В пояснениях ссылайтесь на `МОДУЛЬ:строка` (например `FILER.C:5790`).
