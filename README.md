# yaDX — DX, a full-screen file manager for OpenVMS

*Русский текст — ниже, после английского.*

Version X-02-05ECO14 (02.05.14). Editors: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan).
Original authors: Chau-Kuang Hung (1989-1992) and Jérôme Lauret (1994-1999) — see Thanks.

## What it is

DX (Directory eXtension) shows your directory on a VT100-type terminal as a
list of files. You move over the list with the arrow keys and work with the
files through pull-down menus and one-key commands: type, edit, copy, delete,
rename, purge, set protection, print, compare, run.

DX can also:

- open up to seven windows on the same or on different directories;
- show the whole directory tree, and let you walk it;
- sort and filter the list (by name, type, size, date; by date ranges, size
  ranges, exclude patterns);
- work on many marked files at once ("multiple mode").

DX is a VMS program: it draws the screen with SMG$ and works with files
through RMS and QIO. It needs no Unix-style tools.

yaDX is the 2026 edition of the old DX 2.5 sources. The sources were
cleaned up (VMS naming, 34 files merged into 9, `__NEW_STARLET` prototypes,
all `printf` replaced by `SYS$FAO`), some bugs were fixed, and a few keys
were added. The behaviour stays the same as DX 2.5.

## Files

| File | What it is |
|---|---|
| `DX.C` | main module: start, command line, terminal checks |
| `FILER.C` | the file window: command loop, keypad keys, windows, menus |
| `FILERCPY.C` | copy, delete, rename, purge, append, compare |
| `FILERSTA.C` | file status, type, edit, print |
| `FILEROPT.C` | Option and Customize menus: run a DCL command, launch, protection, find, settings |
| `FILERQUE.C` | directory scan, file cache, sorting, filters |
| `DIRECT.C` | the directory tree window |
| `INQUIRE.C` | input line editing, yes/no questions, shared SMG$ helpers |
| `DIFF.C` | file compare |
| `*.H` | headers; `GLOBAL.H` is shared by all modules |
| `DX_MSG.MSG` | messages (facility `DX`, prefix `DXM__`) |
| `DX_CLI.CLD`, `DXCLI.H` | command line syntax used inside DX |
| `DX.CLD`, `DX.COM` | the `DX` command for DCL |
| `DX.HLP` | help text, built into `DXHELP.HLB` |
| `DX_BUILD.COM`, `DX_MODULES.LIS` | the build procedure and its module list |
| `DX_IA64.OPT` | link options for Itanium |
| `DESCRIP.MMS` | MMS/MMK build file (not tested, use `DX_BUILD.COM`) |
| `DX$STARTUP.DAT` | a sample settings file |
| `KEYCODE.C` | a small tool: shows the code of the key you press |
| `AAAREADME.TXT` | this guide as plain text, for VMS users (made by `tools/mk_aaareadme.py`) |
| `CONTRIBUTING.md`, `AGENTS.md` | rules for changing the code: for people, for AI agents |
| `README_1999.TXT`, `HISTORY.TXT` | the original DX 2.5 readme and history (1999) |
| `COPYRIGHT` | the DX 2.5 copyright and license (see License) |
| `COPYING` | the GNU General Public License, version 2 (full text) |

## Ready-to-run kit

You do not have to build DX yourself. Every release on Codeberg has a kit:
https://codeberg.org/SysManOne/yaDX/releases

The kit (`yaDX-<version>.EXE`) is one self-extracting file made by
[yaDCX](https://codeberg.org/SysManOne/yaDCX). It holds a BACKUP saveset with
the sources and with `DX.EXE` and `DXHELP.HLB` already built. Copy it to VMS
in **binary** mode, then:

```
$ RUN yaDX-X-02-05ECO13.EXE       ! the file you downloaded; checks CRC, writes DX_V29.BCK
$ BACKUP DX_V29.BCK/SAVE_SET [...] ! makes the subdirectory [.DX]
$ @[.DX]DX                        ! defines the DX command (see Install)
```

The release page says which version is inside and has all the steps.

## Build

You need DEC C / HP C on OpenVMS Alpha (tested with HP C V7.1 on OpenVMS
V8.3). MMS or MMK is not needed.

```
$ SET DEFAULT dev:[dir]           ! the directory with the sources
$ @DX_BUILD                       ! builds DX.EXE and DXHELP.HLB
```

`DX_BUILD` works in its own directory (you can also run it as
`@dev:[dir]DX_BUILD` from anywhere) and gives your default directory back
at the end. It compiles the 9 modules, the messages and the command table,
links `DX.EXE` and builds `DXHELP.HLB`. At the end it prints
`Compiled OK: 9   Failed: 0`. Also look at the output yourself: there must
be no `%CC-E-`, `%CC-F-`, `%LINK-E-` or `%LINK-F-` lines.

For a debug build: `$ @DX_BUILD DEBUG`.

## Install

```
$ @dev:[dir]DX                    ! DX.COM: defines DX$HOME and the DX command
```

`DX.COM` points the logical name `DX$HOME` to its own directory and adds the
`DX` verb (from `DX.CLD`). Keep `DX.EXE` and `DXHELP.HLB` in that directory.
Put the `@DX` line into your `LOGIN.COM` to have DX every time.

Other ways:

- a foreign command: `$ DX :== $dev:[dir]DX.EXE`
- for everybody: `$ DEFINE/SYSTEM DX$HOME dev:[dir]` and add `DX.CLD` to
  `DCLTABLES`.

DX looks for its help library in this order: the logical name `DX$HELPLIB`
(full file name), then `DX$HOME:DXHELP.HLB`, then `SYS$HELP:DXHELP.HLB`.

Settings are read from `DX$STARTUP.DAT` in the current directory, then from
`SYS$LOGIN:`. Another file: `/INITIAL_FILE=file`.

## Run

```
$ DX                              ! the current directory
$ DX [.SUB]*.COM,*.TXT            ! only these files
$ DX /SORT_BY=DATE /DESCENDING
$ DX /SINCE=YESTERDAY /MODIFIED
```

All qualifiers: `$ HELP DX` (after DXHELP.HLB is in a help library) or the
HELP key inside DX.

## Keys

| Key | What it does |
|---|---|
| arrows | move over the list; in a menu: move over items and menus |
| RETURN | on a directory: go into it; in a menu: run the item |
| a letter (W, A, E, S, F, O, C) | open that menu |
| DO, keypad `-`, PF4, `-` | open the menu (Window first); press again to close it |
| keypad 0 | cancel; in the file window: leave DX, back to the start directory |
| CTRL/Z, CTRL/E | leave DX, back to the start directory |
| Window → eXit | leave DX, stay in the directory you are in now |
| keypad 1 / HELP | help |
| keypad 2 | direction forward / backward |
| keypad 3 / 4 | find / find next |
| keypad 5 / 6, Home / End | first / last file (in the tree: directory) |
| keypad 7, Ins | mark / unmark a file |
| keypad 8, PREV, NEXT | page up / down |
| keypad 9 | single / multiple mode |
| TAB | next window |
| CTRL/W | repaint the screen |

In the text viewers (Action → Type, Action → Find): arrows — page up / down,
Home / End — the first / the last page, keypad 0 — back to the list.

In an input line (file names, filters, Find): CTRL/H or Home — to the start,
CTRL/E or End — to the end, CTRL/A — insert / overwrite, CTRL/U — clear the
line, CTRL/J — delete the word on the left, up / down arrows — earlier
inputs, keypad 0 — cancel. Other function keys do nothing there.

Your terminal program may send other codes for some keys. Build and run
`KEYCODE` to see what DX gets:

```
$ CC KEYCODE
$ LINK KEYCODE
$ RUN KEYCODE                     ! press keys; CTRL/Z to stop
```

## Messages and what to do

| Message | What to do |
|---|---|
| `%DX-F-BADPARSE` | the command line is wrong. Check the qualifiers and the file name, then type the command again |
| `%DX-F-SFC` | a system call failed inside DX (line and module are in the text). If it repeats, send the full message with the line and module |
| `%DX-E-NANSITERM` | the output is not an ANSI terminal. Run DX from a terminal, not from a batch job or a file |
| `%DX-E-NONVT100CRT` | the terminal is not VT100-type. `$ SET TERMINAL/DEVICE_TYPE=VT100` (or `/INQUIRE`) |
| `%DX-E-PGLEN` | the screen is too short. Make the window taller or `$ SET TERMINAL/PAGE=24` (or more) |
| `%DX-E-PGWID` | the screen is too narrow. Make the window wider or `$ SET TERMINAL/WIDTH=80` (or more) |
| `%DX-E-OPENIN` | a file cannot be opened. Check that it exists and that you may read it |
| `%DX-W-CLIPAR` | a qualifier or parameter is wrong; the text says which |
| `%DX-I-ERRMSG` | the text of the system error that came before |

More: `$ HELP/MESSAGE DXM__code`, or the file `DX_MSG.MSG`.

## Troubleshooting

- **"Help library not found" inside DX.** Put `DXHELP.HLB` into `DX$HOME:`
  or `SYS$HELP:`, or `$ DEFINE DX$HELPLIB dev:[dir]DXHELP.HLB`.
- **Home, End or Ins do nothing.** DX gets them as the VT keys Find, Select
  and Insert Here (`ESC [1~`, `ESC [4~`, `ESC [2~`). Set your terminal program
  to send these (PuTTY: Terminal → Keyboard → "Home and End keys: Standard").
  The xterm codes (`ESC [H`, `ESC [F`) are unknown to SMG$ — `KEYCODE` shows 511.
- **Keypad keys do nothing.** Your terminal program may send them as normal
  characters. Turn on "application keypad" mode in it, or check with
  `KEYCODE` what is sent.
- **After CTRL/Y the terminal has no echo.** DX sets up a CTRL/Y handler that
  gives the terminal back. If you still see this: `$ SET TERMINAL/INQUIRE`,
  and please report it.
- **`@DX_BUILD` says Failed: 1 or more.** Look for the `%CC-E-` lines just
  above that summary; they name the file and the line.

## Thanks

DX is the work of its original authors. Almost all of the logic in these
sources is theirs, and this edition keeps their names, copyrights and notes
in every file. Thank you both for a real, useful VMS tool that is still worth
running more than thirty years later.

**Chau-Kuang Hung** — wrote DX (1989-1992) while
at Oracle Corporation, 300 Oracle Parkway, Redwood Shores, CA
(`chung@us.oracle.com`, also `chung@appseq.us.oracle.com`). The windows,
the pull-down menus, the directory tree, the filters, multiple mode, the
keypad — the whole design of DX comes from him.

**Jérôme Lauret** — kept DX alive from 14-Apr-1994 to 1999
(`jlauret@mail.chem.sunysb.edu`):

- DX V2.4: the port to OpenVMS AXP 1.0;
- DX V2.5 (03-Jun-1999): a rewrite and clean-up — the message facility
  (`DX_MSG.MSG`), the CLI interface (`DX_CLI.CLD`), a better error handler,
  `__FIB` usage, the MMS build (`DESCRIP.MMS`, `GEN_MSGH.COM`), and builds
  on OpenVMS 6.2 VAX and Alpha (VAX C V3.2, DEC C V6.0).

The "About DX" command still says: *"DX Version 2.5 03-Jun-1999, by C.K. Hung &
J. Lauret"*. Their own words are kept too: `README_1999.TXT` (the DX 2.5
readme), `HISTORY.TXT`, and the "Warranty" and "Problems_and_Comments"
topics in `HELP DX`.

**Tom Steppe** — the file compare in `DIFF.C` is based on his article
"File Comparison Algorithms", *Dr. Dobb's Journal* #131 (September 1987),
pp. 54-60. The `DIFF.C` header names him as the author.

Where the sources come from: yaDX was made from the original DX V2.5
package in Hunter Goatley's FILESERV collection (package DX, 13-APR-2000,
`dx.zip`), unchanged there since 2000:
https://vms.process.com/scripts/fileserv/fileserv.com?DX
(file https://vms.process.com/ftp/vms-freeware/fileserv/dx.zip, SHA-256
`431cd8b4f93d325d95e6cec63b97109976d59505c5b108509b628f7eb9f9b4e2`).
Anyone can compare it with this edition. Thanks to Hunter Goatley for
keeping it.

The 2026 edition (yaDX) was edited by StarLet Squad and Ruslan R. Laishev:
they are editors of this code, not its authors. The file headers say this
too ("Original Author" and "Editor").

## License

```
Copyright (C) 1989-1992 Chau-Kuang Hung
Copyright 1994-1999 (c) Jérôme Lauret <jlauret@mail.chem.sunysb.edu>
```

The original terms (in `DX.C` and in `HELP DX`, topic Warranty): *"This
program is free software; permission is granted to any individual or
institution to use, copy, or redistribute this software so long as it is
not sold for profit, provided this copyright notice is retained."* The
software is provided "AS IS", without any warranty.

In 1999 Jérôme Lauret put DX 2.5 under the GNU General Public License,
version 2 or (at your option) any later version (the file `COPYRIGHT`).
**yaDX is distributed under these GPL terms.** The full text of the GPL
version 2 is in the file `COPYING`. The original notice above is kept as it
was written.

The 2026 changes and the new files (for example `KEYCODE.C`) are:

```
Copyright 2026 (c) StarLet Squad and Ruslan R. Laishev
```

and are under the same GNU GPL, version 2 or any later version.

yaDX is free: it is not sold, and the kits are free too.

Please keep these copyright notices in any copy.

OpenVMS, VMS, VAX and Alpha are trademarks of their owners. yaDX is not
made, supported or endorsed by them.

---

# yaDX — DX, полноэкранный файловый менеджер для OpenVMS

Версия X-02-05ECO14 (02.05.14). Редакция: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan).
Авторы оригинала: Chau-Kuang Hung (1989-1992) и Jérôme Lauret (1994-1999) — см. Благодарности.

## Что это

DX (Directory eXtension) показывает каталог на терминале типа VT100 в виде
списка файлов. По списку ходят стрелками, а с файлами работают через
выпадающие меню и команды на одну клавишу: просмотр, правка, копирование,
удаление, переименование, purge, защита, печать, сравнение, запуск.

Ещё DX умеет:

- открыть до семи окон на один или на разные каталоги;
- показать всё дерево каталогов и ходить по нему;
- сортировать и фильтровать список (по имени, типу, размеру, дате;
  по диапазону дат и размеров, по шаблонам исключения);
- работать сразу со многими отмеченными файлами (режим multiple).

DX — программа для VMS: экран рисует через SMG$, с файлами работает через
RMS и QIO. Unix-инструменты ему не нужны.

yaDX — редакция 2026 года старых исходников DX 2.5. Исходники приведены в
порядок (VMS-именование, 34 файла сведены в 9, прототипы `__NEW_STARLET`,
все `printf` заменены на `SYS$FAO`), исправлены ошибки, добавлено несколько
клавиш. Поведение осталось как у DX 2.5.

## Файлы

| Файл | Что это |
|---|---|
| `DX.C` | головной модуль: старт, командная строка, проверка терминала |
| `FILER.C` | файловое окно: цикл команд, клавиши keypad, окна, меню |
| `FILERCPY.C` | копирование, удаление, переименование, purge, append, сравнение |
| `FILERSTA.C` | статус файла, просмотр, правка, печать |
| `FILEROPT.C` | меню Option и Customize: команда DCL, запуск, защита, поиск, настройки |
| `FILERQUE.C` | чтение каталога, кэш файлов, сортировка, фильтры |
| `DIRECT.C` | окно дерева каталогов |
| `INQUIRE.C` | ввод строки, вопросы да/нет, общие помощники SMG$ |
| `DIFF.C` | сравнение файлов |
| `*.H` | заголовки; `GLOBAL.H` общий для всех модулей |
| `DX_MSG.MSG` | сообщения (facility `DX`, префикс `DXM__`) |
| `DX_CLI.CLD`, `DXCLI.H` | синтаксис командной строки внутри DX |
| `DX.CLD`, `DX.COM` | команда `DX` для DCL |
| `DX.HLP` | текст справки, собирается в `DXHELP.HLB` |
| `DX_BUILD.COM`, `DX_MODULES.LIS` | процедура сборки и список модулей |
| `DX_IA64.OPT` | опции линковки для Itanium |
| `DESCRIP.MMS` | сборка через MMS/MMK (не проверялась, используйте `DX_BUILD.COM`) |
| `DX$STARTUP.DAT` | пример файла настроек |
| `KEYCODE.C` | маленькая утилита: показывает код нажатой клавиши |
| `AAAREADME.TXT` | это руководство простым текстом, для пользователей VMS (делает `tools/mk_aaareadme.py`) |
| `CONTRIBUTING.md`, `AGENTS.md` | правила изменения кода: для людей, для ИИ-агентов |
| `README_1999.TXT`, `HISTORY.TXT` | оригинальные readme и история DX 2.5 (1999) |
| `COPYRIGHT` | копирайт и лицензия DX 2.5 (см. Лицензия) |
| `COPYING` | GNU General Public License версии 2 (полный текст) |

## Готовый кит

Собирать DX самому не обязательно. У каждого выпуска на Codeberg есть кит:
https://codeberg.org/SysManOne/yaDX/releases

Кит (`yaDX-<версия>.EXE`) — один самораспаковывающийся файл, сделанный
[yaDCX](https://codeberg.org/SysManOne/yaDCX). В нём сейвсет BACKUP с
исходниками и уже собранными `DX.EXE` и `DXHELP.HLB`. Скопируйте его на VMS в
**двоичном** режиме, затем:

```
$ RUN yaDX-X-02-05ECO13.EXE       ! скачанный файл; проверит CRC, запишет DX_V29.BCK
$ BACKUP DX_V29.BCK/SAVE_SET [...] ! создаст подкаталог [.DX]
$ @[.DX]DX                        ! определит команду DX (см. Установка)
```

На странице выпуска написано, какая версия внутри, и все шаги по порядку.

## Сборка

Нужен DEC C / HP C на OpenVMS Alpha (проверено: HP C V7.1, OpenVMS V8.3).
MMS или MMK не нужны.

```
$ SET DEFAULT dev:[dir]           ! каталог с исходниками
$ @DX_BUILD                       ! собирает DX.EXE и DXHELP.HLB
```

`DX_BUILD` работает в своём каталоге (можно запускать и как
`@dev:[dir]DX_BUILD` откуда угодно) и в конце возвращает ваш текущий
каталог. Он компилирует 9 модулей, сообщения и таблицу команд, линкует
`DX.EXE` и собирает `DXHELP.HLB`. В конце пишет `Compiled OK: 9   Failed: 0`.
Посмотрите и сам вывод: там не должно быть строк `%CC-E-`, `%CC-F-`,
`%LINK-E-`, `%LINK-F-`.

Отладочная сборка: `$ @DX_BUILD DEBUG`.

## Установка

```
$ @dev:[dir]DX                    ! DX.COM: задаёт DX$HOME и команду DX
```

`DX.COM` направляет логическое имя `DX$HOME` на свой каталог и добавляет
глагол `DX` (из `DX.CLD`). `DX.EXE` и `DXHELP.HLB` держите в этом каталоге.
Строку `@DX` можно добавить в `LOGIN.COM`.

Другие способы:

- foreign-команда: `$ DX :== $dev:[dir]DX.EXE`
- для всех: `$ DEFINE/SYSTEM DX$HOME dev:[dir]` и `DX.CLD` в `DCLTABLES`.

Библиотеку справки DX ищет так: логическое имя `DX$HELPLIB` (полное имя
файла), затем `DX$HOME:DXHELP.HLB`, затем `SYS$HELP:DXHELP.HLB`.

Настройки читаются из `DX$STARTUP.DAT` в текущем каталоге, затем из
`SYS$LOGIN:`. Другой файл: `/INITIAL_FILE=файл`.

## Запуск

```
$ DX                              ! текущий каталог
$ DX [.SUB]*.COM,*.TXT            ! только эти файлы
$ DX /SORT_BY=DATE /DESCENDING
$ DX /SINCE=YESTERDAY /MODIFIED
```

Все квалификаторы: `$ HELP DX` (когда DXHELP.HLB подключён к справке) или
клавиша HELP внутри DX.

## Клавиши

| Клавиша | Что делает |
|---|---|
| стрелки | ходить по списку; в меню — по пунктам и между меню |
| RETURN | на каталоге — войти в него; в меню — выполнить пункт |
| буква (W, A, E, S, F, O, C) | открыть это меню |
| DO, keypad `-`, PF4, `-` | открыть меню (сначала Window); ещё раз — закрыть |
| keypad 0 | отмена; в файловом окне — выход из DX с возвратом в исходный каталог |
| CTRL/Z, CTRL/E | выход из DX с возвратом в исходный каталог |
| Window → eXit | выход из DX, остаться в текущем каталоге |
| keypad 1 / HELP | справка |
| keypad 2 | направление вперёд / назад |
| keypad 3 / 4 | поиск / искать дальше |
| keypad 5 / 6, Home / End | первый / последний файл (в дереве — каталог) |
| keypad 7, Ins | отметить / снять отметку |
| keypad 8, PREV, NEXT | страница вверх / вниз |
| keypad 9 | режим single / multiple |
| TAB | следующее окно |
| CTRL/W | перерисовать экран |

В просмотре текста (Action → Type, Action → Find): стрелки — страница вверх /
вниз, Home / End — первая / последняя страница, keypad 0 — назад к списку.

В строке ввода (имена файлов, фильтры, Find): CTRL/H или Home — в начало,
CTRL/E или End — в конец, CTRL/A — вставка / замена, CTRL/U — очистить
строку, CTRL/J — удалить слово слева, стрелки вверх / вниз — прежний ввод,
keypad 0 — отмена. Остальные функциональные клавиши там ничего не делают.

Эмулятор терминала может посылать для некоторых клавиш другие коды.
Соберите и запустите `KEYCODE`, чтобы увидеть, что получает DX:

```
$ CC KEYCODE
$ LINK KEYCODE
$ RUN KEYCODE                     ! нажимайте клавиши; CTRL/Z — выход
```

## Сообщения и что делать

| Сообщение | Что делать |
|---|---|
| `%DX-F-BADPARSE` | ошибка в командной строке. Проверьте квалификаторы и имя файла, наберите команду снова |
| `%DX-F-SFC` | внутри DX не прошёл системный вызов (строка и модуль — в тексте). Если повторяется, пришлите сообщение целиком |
| `%DX-E-NANSITERM` | вывод идёт не на ANSI-терминал. Запускайте DX с терминала, не из batch и не в файл |
| `%DX-E-NONVT100CRT` | терминал не типа VT100. `$ SET TERMINAL/DEVICE_TYPE=VT100` (или `/INQUIRE`) |
| `%DX-E-PGLEN` | экран слишком низкий. Увеличьте окно или `$ SET TERMINAL/PAGE=24` (или больше) |
| `%DX-E-PGWID` | экран слишком узкий. Увеличьте окно или `$ SET TERMINAL/WIDTH=80` (или больше) |
| `%DX-E-OPENIN` | файл не открывается. Проверьте, что он есть и что его можно читать |
| `%DX-W-CLIPAR` | неверный квалификатор или параметр; какой — написано в тексте |
| `%DX-I-ERRMSG` | текст системной ошибки к предыдущему сообщению |

Подробнее: `$ HELP/MESSAGE DXM__код` или файл `DX_MSG.MSG`.

## Если что-то не так

- **Внутри DX: "Help library not found".** Положите `DXHELP.HLB` в
  `DX$HOME:` или `SYS$HELP:`, или `$ DEFINE DX$HELPLIB dev:[dir]DXHELP.HLB`.
- **Home, End или Ins ничего не делают.** DX получает их как VT-клавиши Find,
  Select и Insert Here (`ESC [1~`, `ESC [4~`, `ESC [2~`). Настройте эмулятор
  на эти коды (PuTTY: Terminal → Keyboard → "Home and End keys: Standard").
  Коды xterm (`ESC [H`, `ESC [F`) SMG$ не знает — `KEYCODE` покажет 511.
- **Клавиши keypad ничего не делают.** Эмулятор шлёт их как обычные
  символы. Включите в нём режим "application keypad" или посмотрите через
  `KEYCODE`, что приходит.
- **После CTRL/Y у терминала нет эха.** DX ставит обработчик CTRL/Y, который
  возвращает терминал в норму. Если всё же случилось:
  `$ SET TERMINAL/INQUIRE` — и сообщите, пожалуйста.
- **`@DX_BUILD` пишет Failed: 1 или больше.** Найдите строки `%CC-E-` над
  сводкой — в них файл и номер строки.

## Благодарности

DX — труд его авторов. Почти вся логика в этих исходниках — их, и эта
редакция сохраняет их имена, копирайты и заметки в каждом файле. Спасибо
обоим за настоящий, полезный инструмент для VMS, которым стоит пользоваться
и спустя больше тридцати лет.

**Chau-Kuang Hung** — написал DX (1989-1992),
работая в Oracle Corporation, 300 Oracle Parkway, Redwood Shores, CA
(`chung@us.oracle.com`, также `chung@appseq.us.oracle.com`). Окна,
выпадающие меню, дерево каталогов, фильтры, режим multiple, keypad — весь
замысел DX принадлежит ему.

**Jérôme Lauret** — сопровождал DX с 14-Apr-1994 по 1999 год
(`jlauret@mail.chem.sunysb.edu`):

- DX V2.4: перенос на OpenVMS AXP 1.0;
- DX V2.5 (03-Jun-1999): переработка и чистка — facility сообщений
  (`DX_MSG.MSG`), интерфейс CLI (`DX_CLI.CLD`), улучшенный обработчик
  ошибок, работа через `__FIB`, сборка через MMS (`DESCRIP.MMS`,
  `GEN_MSGH.COM`), сборка на OpenVMS 6.2 VAX и Alpha (VAX C V3.2,
  DEC C V6.0).

Команда "About DX" по-прежнему говорит: *"DX Version 2.5 03-Jun-1999, by C.K. Hung &
J. Lauret"*. Их собственные тексты тоже сохранены: `README_1999.TXT`
(readme DX 2.5), `HISTORY.TXT`, разделы "Warranty" и "Problems_and_Comments"
в `HELP DX`.

**Tom Steppe** — сравнение файлов в `DIFF.C` сделано по его статье
"File Comparison Algorithms", *Dr. Dobb's Journal* №131 (сентябрь 1987),
с. 54-60. В шапке `DIFF.C` он указан как автор.

Откуда исходники: yaDX сделан из оригинального пакета DX V2.5 из
коллекции FILESERV Hunter Goatley (пакет DX, 13-APR-2000, `dx.zip`), где он
лежит без изменений с 2000 года:
https://vms.process.com/scripts/fileserv/fileserv.com?DX
(файл https://vms.process.com/ftp/vms-freeware/fileserv/dx.zip, SHA-256
`431cd8b4f93d325d95e6cec63b97109976d59505c5b108509b628f7eb9f9b4e2`).
Любой может сравнить его с этой редакцией. Спасибо Hunter Goatley за то,
что он его хранит.

Редакция 2026 года (yaDX): StarLet Squad and Ruslan R. Laishev — редакторы
этого кода, а не его авторы. Так же это указано в шапках файлов ("Original
Author" и "Editor").

## Лицензия

```
Copyright (C) 1989-1992 Chau-Kuang Hung
Copyright 1994-1999 (c) Jérôme Lauret <jlauret@mail.chem.sunysb.edu>
```

Исходные условия (в `DX.C` и в `HELP DX`, раздел Warranty): *"This program
is free software; permission is granted to any individual or institution to
use, copy, or redistribute this software so long as it is not sold for
profit, provided this copyright notice is retained."* — свободно
использовать, копировать и распространять, не продавать ради прибыли,
сохранять уведомление об авторских правах. Программа поставляется "как
есть", без каких-либо гарантий.

В 1999 году Jérôme Lauret выпустил DX 2.5 на условиях GNU General Public
License версии 2 или (по вашему выбору) любой более поздней (файл
`COPYRIGHT`). **yaDX распространяется на этих условиях GPL.** Полный текст
GPL версии 2 — в файле `COPYING`. Исходное уведомление выше сохранено в том
виде, в каком оно было написано.

Изменения 2026 года и новые файлы (например, `KEYCODE.C`):

```
Copyright 2026 (c) StarLet Squad and Ruslan R. Laishev
```

распространяются на тех же условиях GNU GPL версии 2 или любой более поздней.

yaDX бесплатен: он не продаётся, киты тоже бесплатны.

Пожалуйста, сохраняйте эти уведомления об авторских правах в любых копиях.

OpenVMS, VMS, VAX и Alpha — товарные знаки их владельцев. yaDX не
создан, не поддерживается и не одобрен ими.
