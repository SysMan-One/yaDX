#!/usr/bin/env python3
#
#  mk_aaareadme.py -- make AAAREADME.TXT (plain text for VMS) from README.md
#
#  __MODULE__	YADX_MK_AAAREADME
#  __IDENT__	X-02-05ECO13
#  __REV__	02.05.13
#
#  AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#  MODIFICATION HISTORY:
#
#	26-SEP-2026	CLD	X-02-05ECO4: initial version (after yaDCX's).
#	26-SEP-2026	CLD	X-02-05ECO5: ident only.
#	28-SEP-2026	CLD	X-02-05ECO6: ident only.
#	28-SEP-2026	CLD	X-02-05ECO7: ident only.
#	21-OCT-2026	CLD	X-02-05ECO11: ident only (GPL v2 text in COPYING).
#	21-OCT-2026	CLD	X-02-05ECO12: ident only (Thanks, trademarks).
#	21-OCT-2026	CLD	X-02-05ECO13: ident only (input line keys).
#
#  Usage:	python3 tools/mk_aaareadme.py		(from the top directory)
#
#  README.md is the source, never edit AAAREADME.TXT by hand.  Tables become
#  indented lists (the header row is dropped), code blocks are indented by 4,
#  headings are underlined, typographic dashes are replaced by ASCII.  The
#  header block below (IDENT, history) is kept here and must be bumped with
#  the project.
#
import re, textwrap
src=open('README.md',encoding='utf-8').read().splitlines()
out=[]; code=False; hdrrow=True
def clean(s):
	s=s.replace('`','').replace('**','').replace('—','--').replace('→','->').replace('«','"').replace('»','"')
	return re.sub(r'(?<![\w.])\*(?=\S)(.+?)(?<=\S)\*(?!\w)',r'\1',s)
for l in src:
	if l.startswith('```'): code=not code; continue
	if code: out.append(('    '+l.replace('\t','  ')).rstrip()); continue
	if l.startswith('|'):
		cells=[clean(c.strip()) for c in l.strip().strip('|').split('|')]
		if set(''.join(cells))<=set('-'): continue
		if hdrrow: hdrrow=False; continue
		out+=textwrap.wrap(cells[0],74,initial_indent='  ',subsequent_indent='    ')
		out+=textwrap.wrap(' -- '.join(cells[1:]),76,initial_indent='        ',subsequent_indent='        ')
		continue
	hdrrow=True
	if l.startswith('#'):
		t=clean(l.lstrip('#').strip())
		out+=['',t.upper() if l.startswith('# ') else t,('=' if l.startswith('# ') else '-')*len(t)]; continue
	if l.strip()=='---': out+=['','*'*78]; continue
	out.append(clean(l))
txt=re.sub(r'\n{3,}','\n\n','\n'.join(out)).strip()
hdr="""AAAREADME.TXT -- yaDX, DX full-screen file manager for OpenVMS

__MODULE__  AAAREADME
__IDENT__   X-02-05ECO13
__REV__     02.05.13

AUTHORS:    StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
            -- editors of DX by Chau-Kuang Hung (1989-1992) and
            Jerome Lauret (1994-1999), see Thanks.

MODIFICATION HISTORY:

    26-SEP-2026  CLD  X-02-05ECO3: made from README.md; replaces the
                      1999 AAAREADME (kept as README_1999.TXT) and README.TXT.
    26-SEP-2026  CLD  X-02-05ECO4: made by tools/mk_aaareadme.py.
    26-SEP-2026  CLD  X-02-05ECO5: Home/End/Ins keys.
    28-SEP-2026  CLD  X-02-05ECO6: Home/End in the directory tree and in Find.
    28-SEP-2026  CLD  X-02-05ECO7: ident only (directory tree fix).
    21-OCT-2026  CLD  X-02-05ECO11: License -- yaDX is under the GPL v2+
                      from COPYRIGHT (1999), full text in COPYING.
    21-OCT-2026  CLD  X-02-05ECO12: Thanks -- Tom Steppe (DIFF.C),
                      where the sources come from; trademarks line.
    21-OCT-2026  CLD  X-02-05ECO13: Keys -- the input line keys
                      (Home/End added).

Made from README.md.  The text is UTF-8: the Russian part needs a UTF-8
terminal, or read README.md on the git side.

"""
open('AAAREADME.TXT','w',encoding='utf-8').write(hdr+txt+'\n')
