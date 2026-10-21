/*
**++
**  FACILITY:  KEYCODE.C
**
**  MODULE DESCRIPTION:
**
**      Tiny stand-alone helper for DX: shows, for every key pressed, the
**	SMG$ terminator code that SMG$READ_KEYSTROKE returns for it (the
**	very value DX's keyboard loops test against SMG$K_TRM_xxx) and
**	the key's SMG$ name from SMG$KEYCODE_TO_NAME (DO, PF4, KP0, MINUS,
**	E5, ...).  Meant for finding out which VT key a PC keyboard key
**	actually sends through a given terminal emulator.  CTRL/Z shows
**	its own code and exits (SMG$READ_KEYSTROKE reports it as SMG$_EOF).
**
**	Build and run:
**
**	    $ CC KEYCODE
**	    $ LINK KEYCODE
**	    $ RUN KEYCODE
**
**  AUTHORS:
**	Ruslan R. Laishev (RRL)
**
**  COPYRIGHT:
**	Copyright 2026 (c) StarLet Squad and Ruslan R. Laishev.
**	This program is free software; you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation; either version 2 of the License, or
**	(at your option) any later version.  See the file COPYING.
**
**  CREATION DATE:  26-Sep-2026
**
**
**  MODIFICATION HISTORY:
**
**	26-Sep-2026  RRL  Initial version
**	21-Oct-2026  CLD  ECO1: GPL v2+ license notice (see COPYING)
**--
*/

#define __MODULE__	"KEYCODE"
#define __IDENT__	"X-01-00ECO1"
#define __REV__		"01.00.01"


/*
**
**  INCLUDE FILES
**
*/
#define	__NEW_STARLET	1
#include	<starlet.h>
#include	<lib$routines.h>
#include	<smg$routines.h>
#include	<smgdef.h>
#include	<smgmsg.h>
#include	<descrip.h>
#include	<ssdef.h>

#include <string.h>


/*
**
**  MACRO DEFINITIONS
**
*/

#define	KEYCODE_NAMELEN		31	/**  Longest SMG$ key name we keep  **/

/**  Signal a failed RTL/system service call, like DX's $LOG()  **/
#define $LOG(status)		(lib$signal(status), (status))


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	keycode_name() fills a_name with a printable name for key code
**	a_code: the SMG$ name when SMG$KEYCODE_TO_NAME knows the code
**	(function/keypad/editing keys, CTRL keys, ...), otherwise the
**	character itself in quotes for printable ASCII/DEC MCS, or
**	"(no SMG$ name)".
**
**  INPUT PARAMETERS:
**
**	a_code  -  SMG$ terminator code from SMG$READ_KEYSTROKE.
**
**  OUTPUT PARAMETERS:
**
**	a_name  -  ASCIZ name, at most KEYCODE_NAMELEN chars.
**
**  RETURNS VALUE:
**
**	None.
**
**--
*/
static void	keycode_name(unsigned short a_code, char *a_name)
{
    struct dsc$descriptor_s l_name_desc;
    unsigned short l_code = a_code;
    int l_len;
    unsigned int l_rc;

    l_name_desc.dsc$w_length  = KEYCODE_NAMELEN;
    l_name_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
    l_name_desc.dsc$b_class   = DSC$K_CLASS_S;
    l_name_desc.dsc$a_pointer = a_name;

    l_rc = smg$keycode_to_name(&l_code, &l_name_desc);
    if (1 & l_rc)
    {	    /**  Fixed-length result comes back blank-padded -- trim it  **/
	for (l_len = KEYCODE_NAMELEN; l_len > 0 && a_name[l_len-1] == ' '; l_len--)
	    ;
	a_name[l_len] = '\0';
    }
    else if ((a_code >= 32 && a_code < 127) || (a_code >= 160 && a_code < 256))
    {	    /**  Plain character key  **/
	a_name[0] = '"';
	a_name[1] = (char) a_code;
	a_name[2] = '"';
	a_name[3] = '\0';
    }
    else
	strcpy(a_name, "(no SMG$ name)");
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	main() sets up an SMG$ pasteboard, a scrolling virtual display
**	and a virtual keyboard exactly the way DX does, then loops: read
**	one keystroke, print "decimal  %Xhex  name" for it, until CTRL/Z.
**
**  RETURNS VALUE:
**
**	SS$_NORMAL.
**
**--
*/
int	main(void)
{
    unsigned int l_pasteboard_id;
    unsigned int l_keyboard_id;
    unsigned int l_display_id;
    int l_rows, l_cols;
    unsigned short l_code;
    char l_name[KEYCODE_NAMELEN+1];
    char l_line[128];
    struct dsc$descriptor_s l_line_desc;
    unsigned short l_len;
    unsigned int l_rc;
    static $DESCRIPTOR (l_title, " KEYCODE -- press any key to see its SMG$ code and name, CTRL/Z to exit");
    static $DESCRIPTOR (l_fao_ctrl, " !5UW   %X!4XW   !AZ");

    if (!(1 & (l_rc = smg$create_pasteboard(&l_pasteboard_id, 0, &l_rows, &l_cols, 0, 0, 0))))
	return $LOG(l_rc);

    if (!(1 & (l_rc = smg$create_virtual_display(&l_rows, &l_cols, &l_display_id, 0, 0, 0))))
	return $LOG(l_rc);

    if (!(1 & (l_rc = smg$paste_virtual_display(&l_display_id, &l_pasteboard_id, &1, &1, 0))))
	return $LOG(l_rc);

    if (!(1 & (l_rc = smg$create_virtual_keyboard(&l_keyboard_id, 0, 0, 0, 0))))
	return $LOG(l_rc);

    if (!(1 & (l_rc = smg$put_line(&l_display_id, &l_title, 0, &(unsigned int) SMG$M_REVERSE, 0, 0, 0, 0))))
	$LOG(l_rc);

    l_line_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
    l_line_desc.dsc$b_class   = DSC$K_CLASS_S;
    l_line_desc.dsc$a_pointer = l_line;

    do
    {
	/**  CTRL/Z comes back as SMG$_EOF (with code SMG$K_TRM_CTRLZ) --
	**   an expected status here, not an error  **/
	l_rc = smg$read_keystroke(&l_keyboard_id, &l_code, 0, 0, 0, 0, 0);
	if (l_rc == SMG$_EOF)
	    l_code = SMG$K_TRM_CTRLZ;
	else if (!(1 & l_rc))
	{
	    $LOG(l_rc);
	    break;
	}

	keycode_name(l_code, l_name);

	l_line_desc.dsc$w_length = sizeof(l_line) - 1;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_line_desc,
				  (unsigned long) l_code, (unsigned long) l_code, l_name))))
	{
	    $LOG(l_rc);
	    continue;
	}
	l_line_desc.dsc$w_length = l_len;

	if (!(1 & (l_rc = smg$put_line(&l_display_id, &l_line_desc, 0, 0, 0, 0, 0, 0))))
	    $LOG(l_rc);

    } while (l_code != SMG$K_TRM_CTRLZ);

    /**  Leave the last screen as it is and put the cursor below it  **/
    smg$delete_virtual_keyboard(&l_keyboard_id);
    smg$delete_pasteboard(&l_pasteboard_id, &0);

    return SS$_NORMAL;
}
