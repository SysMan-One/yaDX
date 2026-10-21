/* Program Name            : DX.C                                       */
/*   Original Author       : Chau-Kuang Hung				*/
/*   Date                  : 7-12-89					*/
/* 	Chau-Kuang Hung (chung@us.oracle.com)                           */
/* 	Oracle Corporation                                              */
/* 	300 Oracle Parkway                                              */
/* 	BOX 659304                                                      */
/* 	Redwood Shores, CA 94065                                        */
/*                                                                      */
/*   Editor                 : StarLet Squad and Ruslan R. Laishev (RRL) */
/*   Program Description   :                                            */
/* 	DX is a VAX/VMS utility for displaying files on a VT100 or      */
/* 	upper type terminal.  It can be used to delete, edit, or        */
/* 	rename files in the current directory.  DX can also create a    */
/* 	hardcopy listing of all the files displayed.                    */
/*                         :                                            */
/* References                                                           */
/*   Files open for Input  :                                            */
/*   Files open for Output :                                            */
/*   Modules Referenced    :                                            */
/*									*/
/* Revision History follows                                             */
/*  04-Jun-1999 JL Modified for CLI and MSG interface			*/
/*		   Modified for (weak) error handler			*/
/*  13-Sep-2026 RRL a_/l_/$LOG naming convention; project-wide file      */
/*		    consolidation 34 -> 9 modules (see PLAN.TXT)	*/
/*  25-Sep-2026 RRL ECO2: __NEW_STARLET prototypes, types fixed (0 CC diags)  */
/*                        SMG$ ids -> unsigned int; &CONST args as &(unsigned int) CONST;  */
/*                        itemlist field types; unused l_original_dst removed  */
/*  26-Sep-2026 CLD ECO3: DO / keypad "-" / PF4 / "-" open the top menu;  */
/*                        submenu arrows fixed (DX_KEY_TOUPPER, codes > 255);  */
/*                        no CRTL stderr on exit; ident X-02-05ECO3    */
/*  26-Sep-2026 CLD ECO4: product ident for DX_BUILD.COM fixes (no code change)  */
/*  26-Sep-2026 CLD ECO5: SIGALRM unification; Home/End/Ins keys        */
/*  28-Sep-2026 CLD ECO6: Home/End in the directory tree and in Find      */
/*  28-Sep-2026 CLD ECO7: directory tree shows subdirectories again       */
/*  28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ in the headers of  */
/*                        .H/.MSG/.CLD/.HLP/.COM/.MMS files (no code)     */
/*  28-Sep-2026 RRL ECO9: DX_BUILD.COM gives DX.EXE the image ident      */
/*                        (IDENTIFICATION= from this __IDENT__)          */
/*  28-Sep-2026 CLD ECO10: product ident bump for FILER.C smg$ update-    */
/*                        batching in the submenus (no code change here) */
/*  21-Oct-2026 CLD ECO11: product ident bump: GPL v2 text in COPYING,    */
/*                        README License section (no code change here)   */
/*  21-Oct-2026 CLD ECO12: product ident bump: FILERSTA.C uses <fh2def.h>, */
/*                        FCHDEF.H dropped; README Thanks, trademarks     */
/*  21-Oct-2026 CLD ECO13: product ident bump: INQUIRE.C input line --   */
/*                        key codes >= 256, Home/End, TAB update        */
/*  21-Oct-2026 CLD ECO14: product ident bump: FILERSTA.C Status page in  */
/*                        one smg$ display update                        */

#define __MODULE__	"DX"
#define __IDENT__	"X-02-05ECO14"
#define __REV__		"02.05.14"
/*
** ..........................................................................
**
** DISTRIBUTION AND COPYRIGHT:
**
** Copyright (C) 1989-1992 Chau-Kuang Hung
** Copyright 1994-1999 (c) J�r�me Lauret <jlauret@mail.chem.sunysb.edu>
**
** This program is free software; permission is granted to any
** individual or institution to use, copy, or redistribute this
** software so long as it is not sold for profit, provided this
** copyright notice is retained.
**
** DISCLAIMER:
**
** This software and its documentation are provided "AS IS" and
** without any expressed or implied warranties whatsoever.
** No warranties as to performance, merchantability, or fitness
** for a particular purpose exist.
**
** In no event shall any person or organization of people be
** held responsible for any direct, indirect, consequential
** or inconsequential damages or lost profits.
**
** ..........................................................................
**/

#define	__NEW_STARLET	1
#include	<starlet.h>
#include	<lib$routines.h> 
#include	<str$routines.h> 
#include	<cli$routines.h>
#include	<climsgdef.h>
#include	<iodef.h>
#include	<jpidef.h>
#include	<libvmdef.h>
#include	<lnmdef.h>
#include	<rmsdef.h>
#include	<stsdef.h>
#include	<ttdef.h>
#include	<fibdef.h>
#include	<trmdef.h>

#include "global.h"
#include "direct.h"
#include "dx.h"
#include "filer.h"
#include "fileropt.h"
#include "filerque.h"
#include "inquire.h"

 
 
/* ****  INTERNAL FUNCTION PROTOTYPING *** */
 
static int	    DX_initialized(void);
static int	    parse_cli_parameters(void);
static int	    parse_cli_qualifiers(void);
static int	    trnlnm_1(const char *, int *, int *);
 
 
/*
**  FUNCTIONAL DESCRIPTION:
**
**      MAIN() is the main driver routine
**
**/
 
 
 
 
main()
{
 
    /*
    **	Initialize DX for execution
    **/
 
    DX_initialized();
 
    /*
    **	Find all the files in the directory.  The status returned from
    **	the function calls are not checked since the only reason to
    **	fail is run out of memory, which is very unlikely for the first
    **	call.
    **/
    find_direct_file();
    find_exclude_file(
	&DX_CURRENT_DIRECTORY.exclude_filelist,
	DX_CURRENT_DIRECTORY.cur_filter,
	DX_CURRENT_DIRECTORY.sortby,
	DX_CURRENT_DIRECTORY.sortorder);
    find_non_direct_file();
 
    /*
    **	Process commands until user exits
    **/
 
    DX_process_loop();
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Initialization routine that fills the global data
**	CNTRL_INFO_BLOCK. Every fields in CNTRL_INFO_BLOCK will be
**	assigned a value except DIR_DX, which will be assigned in
**	PARSE_CLI_QUALIFIERS().
**
**--
**/
static int	DX_initialized()
{
    static unsigned long l_type_of_terminal;
 
    struct SMG$R_ATTRIBUTE_INFO_BLOCK l_pasteboard_info_table;
    int l_rows,
	     l_status_rightmost;
    unsigned long l_rc;
 
    /*
    **	Initially, there is only one winodw.
    **	The window number is 0.
    **/
 
    cntrl_info_block.windows = 1;
    cntrl_info_block.cur_win = 0;
 
    /*
    **	Parse command-line parameters.  Abort DX if invalid directory
    **  or file specification.
    **/
 
    cntrl_info_block.dir_dx[0].cur_dir[0] = EOS;
    cntrl_info_block.dir_dx[0].par_dir[0] = EOS;
    if (parse_cli_parameters() == DX__ERROR)
	exit(DXM__BADPARSE);
 
    /*
    **	Set up smg$ environment and verify that the display
    **	device is valid (must be vt-100 or up).
    **/
 
    if (!(smg$create_pasteboard (
		&cntrl_info_block.pasteboard_id,
		$DESCR ("SYS$OUTPUT"),
		0,
		0,
		0,
		&l_type_of_terminal) & 1))
	exit(DXM__NANSITERM);
 
    if (l_type_of_terminal != SMG$K_VTTERMTABLE)
	dxscr_pb_exit(DXM__NANSITERM);

    if (!(1 & (l_rc = smg$get_pasteboard_attributes (
		&cntrl_info_block.pasteboard_id,
		(unsigned int *) &l_pasteboard_info_table,
		&(unsigned int) SMG$C_PASTEBOARD_INFO_BLOCK))))
        $LOG(l_rc);

    if (l_pasteboard_info_table.SMG$B_DEV_TYPE < TT$_VT100)
	dxscr_pb_exit(DXM__NONVT100CRT);

    if (l_pasteboard_info_table.SMG$R_ROW_OVERLAY.SMG$B_NUM_ROWS <
	    CMDS_RSV_LNS+DIRINFO_RSV_LNS+4)
	dxscr_pb_exit(DXM__PGLEN);
 
    cntrl_info_block.pasteboard_rows =
	(unsigned short) l_pasteboard_info_table.SMG$R_ROW_OVERLAY.SMG$B_NUM_ROWS;
    cntrl_info_block.pasteboard_width =	    /**  Use the terminal's actual
						**   width, capped at
						**   DX_MAX_PB_WIDTH (255, the
						**   MAXFILESPEC-wide line
						**   buffers) -- was hardwired
						**   to 80, then capped at 132;
						**   every fixed buffer sized
						**   against this field has
						**   been checked against the
						**   cap, see FILER.C:
						**   put_general_cmds())  **/
	min (DX_MAX_PB_WIDTH, l_pasteboard_info_table.SMG$W_NUM_COLUMNS);
    if (cntrl_info_block.pasteboard_width < 76)
	dxscr_pb_exit(DXM__PGWID);

    /*
    **	CTRL/Y must restore the terminal (echo, line mode) instead of
    **	leaving it in SMG$'s full-screen no-echo mode -- see
    **	dxscr_enable_ctrly_cleanup()/dxscr_ctrly_ast() (DXSCR.C).
    **/
    dxscr_enable_ctrly_cleanup();

    /*
    **	Create a virtual keyboard to accept user input
    **/
 
    if (!(1 & (l_rc = smg$create_virtual_keyboard (
	    &cntrl_info_block.keyboard_id,
	    $DESCR ("SYS$INPUT"),
	    0,
	    0,
	    0))))
        $LOG(l_rc);
 
    /*
    **	Create COMMANDS_DISPLAY to display menus and function keys.
    **	The virtual display is as large as the pasteboard.
    **/
 
    if (!(1 & (l_rc = smg$create_virtual_display (
		&cntrl_info_block.pasteboard_rows,
		&cntrl_info_block.pasteboard_width,
		&cntrl_info_block.commands_display.id,
		&SMG$M_TRUNC_ICON,
		0,
		0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$paste_virtual_display (
                 &cntrl_info_block.commands_display.id,
                 &cntrl_info_block.pasteboard_id,
                 &COMMANDS_PBD_ROW,
                 &COMMANDS_PBD_COLUMN,
                 0))))
        $LOG(l_rc);
 
    cntrl_info_block.commands_display.rows = cntrl_info_block.pasteboard_rows;
    cntrl_info_block.commands_display.width = cntrl_info_block.pasteboard_width;
    cntrl_info_block.commands_display.beg_y = COMMANDS_PBD_ROW;
    cntrl_info_block.commands_display.beg_x = COMMANDS_PBD_COLUMN;
 
    /*
    **	Create STATUS_DISPLAY to show error messages.
    **/
 
    l_status_rightmost = cntrl_info_block.pasteboard_width-10;
			    /**  Reserve 10 columns for clock  **/
    if (!(1 & (l_rc = smg$create_virtual_display (
		&STATUS_BOTTOM,
		&l_status_rightmost,
		&cntrl_info_block.status_display.id,
		&SMG$M_TRUNC_ICON,
		0,
		0))))
        $LOG(l_rc);
 
    cntrl_info_block.status_display.rows = STATUS_BOTTOM;
    cntrl_info_block.status_display.width = l_status_rightmost;
    cntrl_info_block.status_display.beg_y = STATUS_PBD_ROW;
    cntrl_info_block.status_display.beg_x = STATUS_PBD_COLUMN;
 
    /*
    **	Use default settings for the user preference.  They will be
    **	overwritten by the command-line options or the initial file.
    **/
 
    use_default("", "", "");
 
    /*
    **	Define all other settings except for the directory equivalent
    **	names and directory file id.  Use one window initially.
    **/
 
    /**  Create a virtual display to show directory-related information  **/
 
    l_rows = cntrl_info_block.pasteboard_rows-3;
    if (!(1 & (l_rc = smg$create_virtual_display (
		&l_rows,
		&cntrl_info_block.pasteboard_width,
		&cntrl_info_block.dir_dx[0].dir_info_display.id,
		&SMG$M_TRUNC_ICON,
		0,
		0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$paste_virtual_display (
                 &DX_CURRENT_DIRECTORY.dir_info_display.id,
                 &cntrl_info_block.pasteboard_id,
                 &SINGLE_WINDOW_DIR_INFO_PBD_ROW,
                 &SINGLE_WINDOW_DIR_INFO_PBD_COLUMN,
                 0))))
        $LOG(l_rc);
 
    cntrl_info_block.dir_dx[0].dir_info_display.rows =
	cntrl_info_block.pasteboard_rows-3;
    cntrl_info_block.dir_dx[0].dir_info_display.width =
	cntrl_info_block.pasteboard_width;
    cntrl_info_block.dir_dx[0].dir_info_display.beg_y =
	SINGLE_WINDOW_DIR_INFO_PBD_ROW;
    cntrl_info_block.dir_dx[0].dir_info_display.beg_x =
	SINGLE_WINDOW_DIR_INFO_PBD_COLUMN;
 
    cntrl_info_block.dir_dx[0].form = long_form;
    cntrl_info_block.dir_dx[0].sortby = sortname;
    cntrl_info_block.dir_dx[0].sortorder = ascending;
    cntrl_info_block.dir_dx[0].filemode = single;
    cntrl_info_block.dir_dx[0].direction = advance;
    cntrl_info_block.dir_dx[0].exitn = 0;
 
    cntrl_info_block.dir_dx[0].dir_filelist = NULL;
    cntrl_info_block.dir_dx[0].non_dir_filelist = NULL;
    cntrl_info_block.dir_dx[0].exclude_filelist = NULL;
    cntrl_info_block.dir_dx[0].cur_file = NULL;
 
    strcpy(cntrl_info_block.dir_dx[0].search_pattern, "*");
 
    cntrl_info_block.dir_dx[0].tot_files = 0;
    cntrl_info_block.dir_dx[0].tot_blocks = 0;
 
    cntrl_info_block.dir_dx[0].sel_files = 0;
    cntrl_info_block.dir_dx[0].sel_blocks = 0;
 
    cntrl_info_block.dir_dx[0].dpro = 0Xffff;
 
    cntrl_info_block.dir_dx[0].subdir = 0;
 
    cntrl_info_block.dir_dx[0].cur_filter.date = DX_CREDATE;
    convert_date_string (
	"31-DEC-2857 00:00:00.00",
	&cntrl_info_block.dir_dx[0].cur_filter.before);
    convert_date_string (
	"17-NOV-1858 00:00:00.00",
	&cntrl_info_block.dir_dx[0].cur_filter.since);
    cntrl_info_block.dir_dx[0].cur_filter.maximum = 1073741823;
    cntrl_info_block.dir_dx[0].cur_filter.minimum = 0;
 
    /*
    **	Parse command-line qualifiers
    **/
 
    parse_cli_qualifiers();
 
    /*
    **	Trap broadcast messages
    **/
 
    if (!(1 & (l_rc = smg$set_broadcast_trapping (
                 &cntrl_info_block.pasteboard_id,
                 broadcast_routine,
                 0))))
        $LOG(l_rc);
 
    /*
    **	Initilaize filer and directory info buffer
    **/
 
    initialize_filer_cache();
    initialize_direct_cache();
 
    /*
    **	Return normal status
    **/
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      PARSE_CLI_PARAMETERS() gets command-line parameters
**	from the foreign command.
**
**--
**/
static int	parse_cli_parameters()
{
unsigned short int l_len;
char l_original_cwd[MAXFILESPEC+1];
struct {
	short buflen;
	short itemcode;
	void *buf;
	unsigned short *reslen;
} l_itmlist [] = {
	{ MAXFILESPEC, LNM$_STRING, l_original_cwd, &l_len },
	{ 0, 0, 0, 0 }
	};
 
char l_curdir[MAXFILESPEC+1];
$DESCRIPTOR (l_curdir_descrip, l_curdir);
 
char l_resultant_string[MAXFILESPEC+1];
$DESCRIPTOR(l_resultant_string_descrip, l_resultant_string);
 
char l_lognm[MAXFILESPEC+1];		    /**  LOG NAME(BEFORE TRANSLATED) **/
$DESCRIPTOR (l_lognm_descrip, l_lognm);
unsigned long l_rc;
 
char l_default_filespec[MAXFILESPEC+1];
char l_errmsg[MAXFILESPEC+1];
 
#include "dxcli.h"
 
    /*
    **  Get current working directory (use full vms filespec.)
    **/
 
    strcpy(l_lognm, "SYS$DISK");
    LENGTH(l_lognm_descrip) = strlen(l_lognm);
    if (!(1 & (l_rc = sys$trnlnm (
		 0,
		 $DESCR ("LNM$FILE_DEV"),
		 &l_lognm_descrip,
		 0,
		 l_itmlist))))
        $LOG(l_rc);
    strncpy (cntrl_info_block.original_cwd, l_original_cwd, l_len);
    cntrl_info_block.original_cwd[l_len] = EOS;
    if (!(1 & (l_rc = sys$setddir (
		0,
		&l_len,
		&l_resultant_string_descrip))))
        $LOG(l_rc);
    l_resultant_string[l_len] = EOS;
    strcat(cntrl_info_block.original_cwd, l_resultant_string);
 
    /**  Get command-line string  **/
 
    if (!(1 & (l_rc = lib$get_foreign (
                 &l_resultant_string_descrip,
                 0,
                 &l_len,
                 0))))
        $LOG(l_rc);
 
    strcpy(l_default_filespec, "*.*;*");
 
    if (l_len == 0)
    {  	/**  DX was invoked by 'RUN' command or a command without parameters **/
 
	{
	    static $DESCRIPTOR (l_fao_ctrl, "!AZ*.*;*");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_fao_len;

	    l_out_desc.dsc$w_length  = sizeof(l_resultant_string) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_resultant_string;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
				      cntrl_info_block.original_cwd))))
		$LOG(l_rc);
	    l_resultant_string[l_fao_len] = EOS;
	}
	if (insert_filespec_list(
		&cntrl_info_block.dir_dx[0].cur_filter.name_filespec_list,
		l_default_filespec,
		l_resultant_string,
		l_errmsg) == DX__ERROR)
	{
	    lib$signal(DXM__OPENIN,1,l_default_filespec);
	    lib$signal(DXM__ERRMSG,1,l_errmsg);
	    exit(SS$_NORMAL);
	}
	set_cwd_and_par(l_default_filespec);
    } else { /**  DX was invoked by a command with at least one qualifiers	     **/
 
	int l_count = 0;
 
	/*
	**  Process parameter "P1 (FILE_SPEC)"
	**/
 
	str$concat(&l_resultant_string_descrip,&command,
		   &l_resultant_string_descrip);
	cli$dcl_parse(&l_resultant_string_descrip,&DX_CLI);
 
	while (cli$get_value(
		    $DESCR("FILE_SPEC"),
		    &l_resultant_string_descrip,
		    &l_len) & 1)
	{
	    l_resultant_string[l_len] = EOS;
	    if (insert_filespec_list(
		    &cntrl_info_block.dir_dx[0].cur_filter.name_filespec_list,
		    l_default_filespec,
		    l_resultant_string,
		    l_errmsg) != DX__NORMAL)
	    {
		lib$signal(DXM__OPENIN,1,l_default_filespec);
		lib$signal(DXM__ERRMSG,1,l_errmsg);
		exit(SS$_NORMAL);
	    }
	    if (l_count++ == 0)
		set_cwd_and_par(l_default_filespec);
	}
    }
 
    /*
    **	Set default directory to target directory
    **/
 
    if (setddir(cntrl_info_block.dir_dx[0].cur_dir, l_errmsg) == DX__ERROR)
    {
	lib$signal(DXM__CLIPAR,1,l_errmsg);
	return DX__ERROR;
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      PARSE_CLI_QUALIFIERS() gets command-line qualifiers
**	from the foreign command.
**
**--
**/
static int	parse_cli_qualifiers()
{
char l_resultant_string[MAXFILESPEC+1];
unsigned short int l_len;
$DESCRIPTOR(l_resultant_string_descrip, l_resultant_string);
unsigned long l_rc;
char l_dat_tim_string[256];
$DESCRIPTOR(l_dat_tim_string_descrip, l_dat_tim_string);
char l_initial_file[MAXFILESPEC+1];
char l_errmsg[256];
unsigned int l_status;
char l_default_filespec[MAXFILESPEC+1];
 
#include "dxcli.h"
 
    if (!(1 & (l_rc = lib$get_foreign (
                 &l_resultant_string_descrip,
                 0,
                 &l_len,
                 0))))
        $LOG(l_rc);
 
    if (l_len == 0)  	/**  DX was invoked by 'RUN' command or a foreign    **/
    {			/**  without any command-line options		     **/
	if (restore_setting(DEFAULT_STARTUP, "init", l_errmsg) != DX__NORMAL)
	{
	    {
		static $DESCRIPTOR (l_fao_ctrl, "SYS$LOGIN:!AZ");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_fao_len;

		l_out_desc.dsc$w_length  = sizeof(l_initial_file) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_initial_file;

		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
					  DEFAULT_STARTUP))))
		    $LOG(l_rc);
		l_initial_file[l_fao_len] = EOS;
	    }
	    restore_setting(l_initial_file, "init", l_errmsg);
	}
    } else {		    /**  Invoked from the foreign command   **/
	/*
	**  Process qualifier "INITIAL_FILE"
	**/
 
	str$concat(&l_resultant_string_descrip,&command,
		   &l_resultant_string_descrip);
	cli$dcl_parse(&l_resultant_string_descrip,&DX_CLI);
 
 
	l_status = cli$present($DESCR("INITIAL_FILE"));
	if (cli$present($DESCR("INITIAL_FILE")) == CLI$_PRESENT)
	{
	    cli$get_value(
		    $DESCR("INITIAL_FILE"),
		    &l_resultant_string_descrip,
		    &l_len);
	    l_resultant_string[l_len] = EOS;
	    restore_setting(l_resultant_string, "init", l_errmsg);
	}
        else if (restore_setting(DEFAULT_STARTUP, "init", l_errmsg) != DX__NORMAL)
        {
	    {
		static $DESCRIPTOR (l_fao_ctrl, "SYS$LOGIN:!AZ");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_fao_len;

		l_out_desc.dsc$w_length  = sizeof(l_initial_file) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_initial_file;

		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
					  DEFAULT_STARTUP))))
		    $LOG(l_rc);
		l_initial_file[l_fao_len] = EOS;
	    }
	    restore_setting(l_initial_file, "init", l_errmsg);
        }
 
	/*
	**  Process qualifier "EXIT"
	**/
 
	l_status = cli$present($DESCR("EXIT"));
	if (l_status == CLI$_PRESENT || l_status == CLI$_LOCPRES)
            cntrl_info_block.dir_dx[0].exitn = 1;
 
	/*
	**  Process qualifier "ASCENDING"
	**/
 
	l_status = cli$present($DESCR("ASCENDING"));
	if (l_status == CLI$_PRESENT || l_status == CLI$_LOCPRES ||
	    l_status == CLI$_DEFAULTED)
            cntrl_info_block.dir_dx[0].sortorder = ascending;
 
	/*
	**  Process qualifier "DESCENDING"
	**/
 
	l_status = cli$present($DESCR("DESCENDING"));
	if (l_status == CLI$_LOCPRES || l_status == CLI$_PRESENT)
            cntrl_info_block.dir_dx[0].sortorder = descending;
 
	/*
	**  Process qualifier "DETAIL"
	**/
 
	l_status = cli$present($DESCR("DETAIL"));
	if (l_status == CLI$_LOCPRES || l_status == CLI$_PRESENT ||
	    (l_status == CLI$_DEFAULTED &&
		!cntrl_info_block.user_pref.use_short_form))
            cntrl_info_block.dir_dx[0].form = long_form;
 
	/*
	**  Process qualifier "BRIEF"
	**/
 
	l_status = cli$present($DESCR("BRIEF"));
	if (l_status == CLI$_LOCPRES || l_status == CLI$_PRESENT)
            cntrl_info_block.dir_dx[0].form = short_form;
 
	/*
	**  Process qualifier "BEFORE"
	**/
 
	l_status = cli$present($DESCR("BEFORE"));
	if (l_status == CLI$_PRESENT)
	{
	    cli$get_value(
		    $DESCR("BEFORE"),
		    &l_resultant_string_descrip,
		    &l_len);
	    l_resultant_string[l_len] = EOS;
	    strcpy(l_dat_tim_string, l_resultant_string);
 
	    convert_date_string (
		l_dat_tim_string,
		&cntrl_info_block.dir_dx[0].cur_filter.before);
	}
 
	/*
	**  Process qualifier "SINCE"
	**/
 
	l_status = cli$present($DESCR("SINCE"));
	if (l_status == CLI$_PRESENT)
	{
	    cli$get_value(
		    $DESCR("SINCE"),
		    &l_resultant_string_descrip,
		    &l_len);
	    l_resultant_string[l_len] = EOS;
	    strcpy(l_dat_tim_string, l_resultant_string);
 
	    convert_date_string (
		 l_dat_tim_string,
		 &cntrl_info_block.dir_dx[0].cur_filter.since);
	}
 
	/*
	**  Process qualifier "CREATED"
	**/
 
	l_status = cli$present($DESCR("CREATED"));
	if (l_status == CLI$_LOCPRES ||
	    l_status == CLI$_PRESENT ||
	    (l_status == CLI$_DEFAULTED &&
		cntrl_info_block.user_pref.display_date != DX_REVDATE &&
		cntrl_info_block.user_pref.display_date != DX_EXPDATE &&
		cntrl_info_block.user_pref.display_date != DX_BAKDATE))
            cntrl_info_block.dir_dx[0].cur_filter.date = DX_CREDATE;
 
	/*
	**  Process qualifier "MODIFIED"
	**/
 
	l_status = cli$present($DESCR("MODIFIED"));
	if (l_status == CLI$_LOCPRES || l_status == CLI$_PRESENT)
            cntrl_info_block.dir_dx[0].cur_filter.date = DX_REVDATE;
 
	/*
	**  Process qualifier "EXPIRED"
	**/
 
	l_status = cli$present($DESCR("EXPIRED"));
	if (l_status == CLI$_LOCPRES || l_status == CLI$_PRESENT)
            cntrl_info_block.dir_dx[0].cur_filter.date = DX_EXPDATE;
 
	/*
	**  Process qualifier "BACKUP"
	**/
 
	l_status = cli$present($DESCR("BACKUP"));
	if (l_status == CLI$_LOCPRES || l_status == CLI$_PRESENT)
            cntrl_info_block.dir_dx[0].cur_filter.date = DX_BAKDATE;
 
	/*
	**  Process qualifier "SORT_BY"
	**/
 
	cli$get_value($DESCR("SORT_BY"), &l_resultant_string_descrip, &l_len);
	l_resultant_string[l_len] = EOS;
        if (!strncmp (l_resultant_string, "NAME", l_len))
        {
            cntrl_info_block.dir_dx[0].sortby = sortname;
        }
        else if (!strncmp (l_resultant_string, "TYPE", l_len))
        {
            cntrl_info_block.dir_dx[0].sortby = sorttype;
        }
        else if (!strncmp (l_resultant_string, "SIZE", l_len))
        {
            cntrl_info_block.dir_dx[0].sortby = sortsize;
        }
        else if (!strncmp (l_resultant_string, "DATE", l_len))
        {
            switch (cntrl_info_block.dir_dx[0].cur_filter.date)
            {
                case DX_CREDATE:
                    cntrl_info_block.dir_dx[0].sortby = sortcredate;
		    break;
                case DX_REVDATE:
                    cntrl_info_block.dir_dx[0].sortby = sortrevdate;
		    break;
                case DX_EXPDATE:
                    cntrl_info_block.dir_dx[0].sortby = sortexpdate;
		    break;
                case DX_BAKDATE:
                    cntrl_info_block.dir_dx[0].sortby = sortbakdate;
		    break;
                default:
                    break;
            }
        }
 
	/*
	**  Process qualifier "MAXIMUM"
	**/
 
	if (cli$present($DESCR("MAXIMUM")) == CLI$_PRESENT)
	{
	    cli$get_value(
		    $DESCR("MAXIMUM"),
		    &l_resultant_string_descrip,
		    &l_len);
	    l_resultant_string[l_len] = EOS;
	    cntrl_info_block.dir_dx[0].cur_filter.maximum =
		atoi(l_resultant_string);
	}
 
	/*
	**  Process qualifier "MINIMUM"
	**/
 
	if (cli$present($DESCR("MINIMUM")) == CLI$_PRESENT)
	{
	    cli$get_value(
		    $DESCR("MINIMUM"),
		    &l_resultant_string_descrip,
		    &l_len);
	    l_resultant_string[l_len] = EOS;
	    cntrl_info_block.dir_dx[0].cur_filter.minimum =
		atoi(l_resultant_string);
	}
 
	/*
	**  Process qualifier "EXCLUDE"
	**/
 
	if (cli$present($DESCR("EXCLUDE")) == CLI$_PRESENT)
	{
	    strcpy(l_default_filespec, "*.*;*");
	    while (cli$get_value(
			$DESCR("EXCLUDE"),
			&l_resultant_string_descrip,
			&l_len) & 1)
	    {
		l_resultant_string[l_len] = EOS;
		insert_filespec_list(
		    &cntrl_info_block.dir_dx[0].cur_filter.exclude_filespec_list,
		    l_default_filespec,
		    l_resultant_string,
		    l_errmsg);
	    }
	}
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	trnlnm() fully translates a VMS logical name (or a plain device/
**	directory string with none) down to every equivalence-name string
**	it expands to, queuing each one into a_logicalQ.  It seeds a_workingQ
**	with a single entry holding a_original, then repeatedly dequeues an
**	entry and hands it to trnlnm_1(), which either re-queues one or more
**	further-translated candidates onto that same working queue (if the
**	name is itself a logical) or, once nothing more can be translated,
**	moves the final string onto a_logicalQ.  The loop ends when the
**	working queue drains.
**
**  INPUT PARAMETERS:
**
**	a_original  -  the logical name (or device/directory string) to
**		       translate.
**
**  OUTPUT PARAMETERS:
**
**	a_logicalQ  -  VMS self-relative queue header (2 longwords); on
**		       return holds one struct logical_queue_entry_tag per
**		       fully-translated equivalence name.  Caller owns and
**		       must eventually free (via lib$free_vm) both the
**		       queue entries and their equivalent_name buffers.
**
**  IMPLICIT INPUTS:
**
**	None.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL always (an lib$get_vm failure inside the loop returns
**	$LOG(l_rc) instead, propagating the VMS condition value).
**
**--
**/
int	trnlnm(a_original, a_logicalQ)
const char *a_original;
int *a_logicalQ;
{
    struct logical_queue_entry_tag *l_e;
    static _align (QUADWORD) int l_workingQ[2];
    int l_allocate_size;
    unsigned long l_rc;
 
    l_workingQ[0] = l_workingQ[1] = 0;
 
    /**  Allocate dynamic memory for a queue entry  **/
 
    l_allocate_size = sizeof (struct logical_queue_entry_tag);
    if (!(1 & (l_rc = lib$get_vm (
                 &l_allocate_size,
                 &l_e,
                 0))))
        return $LOG(l_rc);
 
    /*
    **  Allocate dynamic memory for the content of a queue entry.
    **	The size is the string length plus EOS.
    **/
 
    l_allocate_size = strlen(a_original)+1;
    if (!(1 & (l_rc = lib$get_vm (
                 &l_allocate_size,
                 &l_e->equivalent_name,
                 0))))
        return $LOG(l_rc);
    strcpy(l_e->equivalent_name, a_original);
 
    _INSQTI (l_e, l_workingQ);	    /**  Insert the first entry to queue  **/
    while (_REMQHI (l_workingQ, &l_e) != 3)
    {
	trnlnm_1(l_e->equivalent_name, l_workingQ, a_logicalQ);

	/**  trnlnm_1() always copies l_e->equivalent_name into whatever
	**   new entry/entries it queues -- it never adopts this buffer,
	**   so it (and the dequeued entry itself) are always ours to
	**   free here.  **/
	l_allocate_size = strlen(l_e->equivalent_name)+1;
	if (!(1 & (l_rc = lib$free_vm (
		     &l_allocate_size,
		     &l_e->equivalent_name,
		     0))))
	    $LOG(l_rc);
	l_allocate_size = sizeof (struct logical_queue_entry_tag);
	if (!(1 & (l_rc = lib$free_vm (
		     &l_allocate_size,
		     &l_e,
		     0))))
	    $LOG(l_rc);
    }

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	trnlnm_1() performs one translation step for trnlnm().  It strips
**	any trailing ":..." portion off a_original and asks sys$trnlnm how
**	many equivalence names (LNM$_MAX_INDEX) that root logical has.  If
**	sys$trnlnm fails, a_original is not itself a translatable logical --
**	it is treated as a final answer: any extra leading '[' characters
**	(from repeated rooted-logical translation) are collapsed to one,
**	and the resulting string is queued straight onto a_logicalQ.
**	Otherwise, each of the 0..l_maxindex equivalence names (LNM$_INDEX)
**	is fetched, has the stripped ":..." suffix reattached, and is
**	queued onto a_workingQ so trnlnm()'s caller loop will translate it
**	(recursively, in case it is itself a logical) before it is treated
**	as final.
**
**  INPUT PARAMETERS:
**
**	a_original  -  the logical name (or terminal string) to translate
**		       one level further.
**
**  OUTPUT PARAMETERS:
**
**	a_workingQ  -  queue receiving further-translatable equivalence
**		       names (VMS self-relative queue header).
**	a_logicalQ  -  queue receiving names determined final (not
**		       themselves logicals).
**
**  IMPLICIT INPUTS:
**
**	None.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL on the "final name" path; DX__NORMAL after queuing all
**	equivalence names on the translated path; $LOG(l_rc) (a VMS
**	condition value) if lib$get_vm fails while allocating a queue entry.
**
**--
**/
static int	trnlnm_1(a_original, a_workingQ, a_logicalQ)
const char *a_original;
int *a_workingQ;
int *a_logicalQ;
{
    char l_before[256],
	 l_after[256],
	 l_final[256];
    char l_postfix[256];
    $DESCRIPTOR (l_before_dst, l_before);
    unsigned int l_status;
    int l_maxindex;
    int l_index;
    unsigned short l_retlen;
    unsigned short l_dummy;
    unsigned long l_rc;
    struct {
	short buflen;
	short itemcode;
	void *buf;
	unsigned short *reslen;
    } l_itmlist_1 [] =
	{
	    {sizeof(int), LNM$_MAX_INDEX, &l_maxindex, &l_retlen},
	    {0, 0, 0, 0}
	},
      l_itmlist_2 [] =
	{
	    {sizeof (int), LNM$_INDEX, &l_index, &l_dummy},
	    {255, LNM$_STRING, l_after, &l_retlen},
	    {0, 0, 0, 0}
	};
 
    char *l_cp, *l_cp1;
    int l_allocate_size;
    struct logical_queue_entry_tag *l_e;
 
    /*  Find a logical name to be translate */
 
    strcpy(l_before, a_original);
    if ((l_cp = strchr(l_before, ':')) != 0)
	*l_cp = EOS;
 
    /* Get max index */
 
    LENGTH(l_before_dst) = strlen(l_before);
    if ((l_status = sys$trnlnm (
			 &(unsigned int) LNM$M_CASE_BLIND,
			 $DESCR ("LNM$FILE_DEV"),
			 &l_before_dst,
			 0,
			 l_itmlist_1)) != SS$_NORMAL)
    {	/**  No more translation  **/
	if ((l_cp = strchr(a_original, '[')) != 0)
	{   /**  Remove all '[' except the first one  **/
	    l_cp++;
	    while ((l_cp1 = strchr(l_cp, '[')) != 0)
		memmove (l_cp1-1, l_cp1+1, strlen(l_cp1+1)+1);
	}
 
	l_allocate_size = sizeof (struct logical_queue_entry_tag);
	if (!(1 & (l_rc = lib$get_vm (
		     &l_allocate_size,
		     &l_e,
		     0))))
	    return $LOG(l_rc);
	l_allocate_size = strlen(a_original)+1;
	if (!(1 & (l_rc = lib$get_vm (
		     &l_allocate_size,
		     &l_e->equivalent_name,
		     0))))
	    return $LOG(l_rc);
	strcpy(l_e->equivalent_name, a_original);
	_INSQTI (l_e, a_logicalQ);
	return DX__NORMAL;
    }
 
    for (l_index = 0; l_index <= l_maxindex; l_index++)
    {
	/*  Find a logical name to be translate */
	strcpy(l_before, a_original);
	if ((l_cp = strchr(l_before, ':')) == 0)
	    l_postfix[0] = EOS;
	else
	{
	    strcpy(l_postfix, l_cp);
	    *l_cp = EOS;
	}
	LENGTH(l_before_dst) = strlen(l_before);
 
	/* Get i-th equivalent string */
	sys$trnlnm (
		 &(unsigned int) LNM$M_CASE_BLIND,
		 $DESCR ("LNM$FILE_DEV"),
		 &l_before_dst,
		 0,
		 l_itmlist_2);
	l_after[l_retlen] = EOS;
 
	/*  Post to result */
	if ((l_cp = strchr(l_after, ':')) == 0)
	{
	    strcpy(l_final, l_after);
	    strcat(l_final, l_postfix);
	}
	else if (l_postfix[0] == ':')
	{
	    strcpy(l_final, l_after);
	    strcat(l_final, l_postfix+1);
	}
	else
	{
	    strcpy(l_final, l_after);
	    strcat(l_final, l_postfix);
	}
 
	l_allocate_size = sizeof (struct logical_queue_entry_tag);
	if (!(1 & (l_rc = lib$get_vm (
		     &l_allocate_size,
		     &l_e,
		     0))))
	    return $LOG(l_rc);
	l_allocate_size = strlen(l_final)+1;
	if (!(1 & (l_rc = lib$get_vm (
		     &l_allocate_size,
		     &l_e->equivalent_name,
		     0))))
	    return $LOG(l_rc);
	strcpy(l_e->equivalent_name, l_final);
 
	_INSQTI (l_e, a_workingQ);
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	insert_dids() appends one new w_dids_tag entry (an equivalence
**	name together with the directory-id triple and I/O channel it
**	resolves to) to the tail of the singly-linked list rooted at
**	*a_l, recursing down the "next" chain until it finds the NULL
**	tail.
**
**  INPUT PARAMETERS:
**
**	a_s  -  equivalence name string to store in the new node.
**	a_d  -  directory-id triple (3 words) to store in the new node.
**	a_c  -  I/O channel number to store in the new node.
**
**  OUTPUT PARAMETERS:
**
**	a_l  -  address of the list head pointer (or, on recursive calls,
**		of some node's "next" field); updated in place to point at
**		the newly-allocated node once the tail is reached.
**
**  IMPLICIT INPUTS:
**
**	None.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success; $LOG(l_rc) (a VMS condition value) if
**	lib$get_vm fails to allocate the new node.
**
**--
**/
int	insert_dids(struct w_dids_tag **a_l,char *a_s, unsigned short int a_d[3],
		unsigned short int a_c)
{
    struct w_dids_tag *l_m;
    int l_allocate_size;
    unsigned long l_rc;
 
    if (*a_l != NULL)
        insert_dids(&((*a_l)->next), a_s, a_d, a_c);
    else
    {
	l_allocate_size = sizeof (struct w_dids_tag);
	if (!(1 & (l_rc = lib$get_vm (
		     &l_allocate_size,
		     &l_m,
		     0))))
	    return $LOG(l_rc);
	strcpy(l_m->equivalent_name, a_s);
	l_m->did[0] = a_d[0];
	l_m->did[1] = a_d[1];
	l_m->did[2] = a_d[2];
	l_m->chan = a_c;
	l_m->next = (struct w_dids_tag *) NULL;
	*a_l = l_m;
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	set_cwd_and_par() sets the current window's current and parent
**	directory strings from a_s.  The current directory is everything
**	in a_s up through (and including) the first ']'.  The parent
**	directory is derived by dropping the last subdirectory level (by
**	turning "[...NAME]" into "[...-]" and looking that up with
**	lib$find_file, "-" being the VMS syntax for "one level up") --
**	left empty if that lookup fails (e.g. already at a top-level/
**	rooted directory).
**
**  INPUT PARAMETERS:
**
**	a_s  -  a filespec whose directory portion (up to the first ']')
**		becomes the new current directory.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	None.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.cur_dir and DX_CURRENT_DIRECTORY.par_dir (i.e.
**	cntrl_info_block.dir_dx[cntrl_info_block.cur_win].{cur_dir,par_dir})
**	are both (re)written.
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	set_cwd_and_par(a_s)
char *a_s;
{
    static unsigned int l_context;
 
    size_t l_len;
    char l_wild[MAXFILESPEC+1];
    $DESCRIPTOR (l_wild_descrip, l_wild);
    char l_filespec[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descrip, l_filespec);
    char *l_cp;
 
    /*
    **	Set current woking directory
    **/
 
    l_len = strcspn (a_s, "]")+1;
    if (l_len > MAXFILESPEC)
    {
        /**  No ']' in a_s (or a l_filespec longer than the buffer) --
        **   cap l_len so the EOS write below stays inside cur_dir[]
        **   (cur_dir[MAXFILESPEC+1]).  **/
	l_len = MAXFILESPEC;
    }
    strncpy(DX_CURRENT_DIRECTORY.cur_dir, a_s, l_len);
    DX_CURRENT_DIRECTORY.cur_dir[l_len] = EOS;
 
    /*
    **  Set parent directory
    **/
 
    DX_CURRENT_DIRECTORY.par_dir[0] = EOS;
    strcpy(l_wild, DX_CURRENT_DIRECTORY.cur_dir);
    l_wild[strlen(l_wild)-1] = EOS;
    strcat(l_wild, ".-]*.*;*");
    LENGTH(l_wild_descrip) = strlen(l_wild);
    if (lib$find_file (
	    &l_wild_descrip,
	    &l_filespec_descrip,
	    &l_context,
	    0,
	    0,
	    0,
	    0) & 1)
    {
	if ((l_cp = strchr(l_filespec, ']')) != NULL)
	{
	    *(l_cp+1) = EOS;
	    strcpy(DX_CURRENT_DIRECTORY.par_dir, l_filespec);
	}
    }
    lib$find_file_end (
	&l_context);
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	get_chan() assigns (sys$assign) an I/O channel to the given device
**	name, for later ACP QIO use against files on that device.
**
**  INPUT PARAMETERS:
**
**	a_nam  -  device name string to assign a channel to.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	None.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	The assigned channel number (0 if sys$assign failed -- the failure
**	itself is only logged via $LOG(), not otherwise reported to the
**	caller).
**
**--
**/
unsigned short get_chan(a_nam)
char *a_nam;
{
    struct dsc$descriptor_s l_rslbuf =
	{strlen(a_nam), DSC$K_DTYPE_T, DSC$K_CLASS_S, a_nam};
    unsigned long l_rc;
    unsigned short l_chan;
 
    if (!(1 & (l_rc = sys$assign(
		    &l_rslbuf, &l_chan, 0, 0))))
        $LOG(l_rc);
    return l_chan;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	insert_filespec_list() resolves one user-supplied filespec (a_s,
**	defaulted against a_dna) to its device/directory/name/type/version
**	parts (sys$parse), confirms that directory is actually reachable
**	(lib$find_file, tolerating RMS$_FNF -- "no files found" is fine,
**	directory access failing is not), then finds every physical
**	directory that device/directory logical name is equivalent to
**	(trnlnm()), assigning each one an I/O channel (get_chan()) and
**	recording it (insert_dids()).  The whole thing -- the bare
**	name/type/version plus the list of (equivalent name, did, chan)
**	triples -- is then appended as one entry to *a_l via
**	insert_filespec_list$1().  This is how a single command-line
**	filespec that resolves through a logical name spanning several
**	physical directories becomes one merged filer-window entry.
**
**  INPUT PARAMETERS:
**
**	a_dna  -  default filespec used by sys$parse to fill in any part
**		  a_s leaves unspecified (e.g. "*.*;*").
**	a_s    -  the filespec to resolve (may be partial).
**
**  OUTPUT PARAMETERS:
**
**	a_l       -  address of the filespec_list_tag list head; a new
**		     entry is appended via insert_filespec_list$1().
**	a_dna     -  overwritten with the resolved device+directory+name+
**		     type+version on success, or with whatever partial
**		     string caused the failure on error.
**	a_errmsg  -  filled in with an RMS/VMS error message if the
**		     function returns DX__ERROR.
**
**  IMPLICIT INPUTS:
**
**	None.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success; DX__ERROR if sys$parse fails, if the
**	directory is inaccessible for a reason other than "no files
**	found", or if a later sys$parse on one of the equivalence names
**	fails.
**
**--
**/
int	insert_filespec_list(a_l, a_dna, a_s, a_errmsg)
struct filespec_list_tag **a_l;
char *a_dna;
char *a_s;
char *a_errmsg;
{
    static _align (QUADWORD) int l_logicalQ[2];
    static unsigned int l_context;
 
    struct filespec_list_tag l_temp_entry;
    char l_dev_dir[MAXFILESPEC+1];
    char l_expand_name[NAM$C_MAXRSS];
    struct logical_queue_entry_tag *l_e;
    unsigned int l_status;
    struct FAB l_fab;
    struct NAM l_nam;
    char l_fab_dna[MAXFILESPEC+1];
    int l_number_of_bytes;
    unsigned long l_rc;
 
    char l_wild[MAXFILESPEC+1];
    $DESCRIPTOR (l_wild_descrip, l_wild);
    char l_devname[MAXFILESPEC+1];
    char *l_cp;
    char l_filespec[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descrip, l_filespec);
    unsigned short int l_len;
    char l_errmsg1[MAXFILESPEC+1];
    $DESCRIPTOR (l_errmsg1_descrip, l_errmsg1);
 
    l_logicalQ[0] = l_logicalQ[1] = 0;
 
    /**  Initialize FAB and NAM blocks  **/
 
    l_fab = cc$rms_fab;
    l_nam = cc$rms_nam;
 
    l_nam.nam$l_esa = l_expand_name;
    l_nam.nam$b_ess = NAM$C_MAXRSS;
 
    /**  Get directory fid, name, and equivalent name  **/
    l_fab.fab$l_fna = a_s;
    l_fab.fab$b_fns = strlen(a_s);
    strcpy(l_fab_dna, a_dna);
    l_fab.fab$l_dna = l_fab_dna;
    l_fab.fab$b_dns = strlen(l_fab_dna);
    l_fab.fab$l_nam = &l_nam;
 
    l_status = sys$parse(&l_fab, 0, 0);
    if (!(l_status & 1))
    {
        lib$sys_getmsg (
            &l_status,
            &l_len,
            &l_errmsg1_descrip,
            0,
            0);
	l_errmsg1[l_len] = EOS;
	strcpy(a_errmsg, l_errmsg1);
	strcpy(a_dna, a_s);
	return DX__ERROR;
    }
 
    /*
    **	Is this directory accessible?
    **/
 
    strncpy(l_wild, l_expand_name, l_nam.nam$b_esl);
    l_wild[l_nam.nam$b_esl] = EOS;
    LENGTH(l_wild_descrip) = strlen(l_wild);
    l_status = lib$find_file (
		&l_wild_descrip,
		&l_filespec_descrip,
		&l_context,
		0,
		0,
		0,
		0);
    lib$find_file_end (
	&l_context);
    if (l_status != RMS$_FNF && !(l_status & 1))
    {
        lib$sys_getmsg (
            &l_status,
            &l_len,
            &l_errmsg1_descrip,
            0,
            0);
	l_errmsg1[l_len] = EOS;
	strcpy(a_errmsg, l_errmsg1);
	strcpy (a_dna, l_wild);
	return DX__ERROR;
    }
 
    strncpy(
	a_dna,
	l_nam.nam$l_dev,
	l_nam.nam$b_dev+l_nam.nam$b_dir+l_nam.nam$b_name+l_nam.nam$b_type+l_nam.nam$b_ver);
    a_dna[l_nam.nam$b_dev+l_nam.nam$b_dir+l_nam.nam$b_name+l_nam.nam$b_type+l_nam.nam$b_ver] = EOS;
    l_temp_entry.w_dids = NULL;
 
    strncpy (
	l_temp_entry.name,
	l_nam.nam$l_name,
	l_nam.nam$b_name+l_nam.nam$b_type+l_nam.nam$b_ver);
    l_temp_entry.name[l_nam.nam$b_name+l_nam.nam$b_type+l_nam.nam$b_ver] = EOS;
 
    strncpy (l_dev_dir, l_nam.nam$l_dev, l_nam.nam$b_dev+l_nam.nam$b_dir);
    l_dev_dir[l_nam.nam$b_dev+l_nam.nam$b_dir] = EOS;
 
    /*
    **	Translate current directory logical name to equivalent name(s).
    **	The directory may contains more than one physical directories.
    **/
 
    trnlnm(l_dev_dir, l_logicalQ);
    while (_REMQHI (l_logicalQ, &l_e) != 3)
    {
	l_number_of_bytes = strlen(l_e->equivalent_name)+1;
        if ((l_cp = strstr (l_e->equivalent_name, ".000000]")) != NULL)
	{
	    *l_cp++ = ']';
	    *l_cp = EOS;
	}
	l_fab.fab$l_fna = l_e->equivalent_name;
	l_fab.fab$b_fns = strlen(l_e->equivalent_name);
	l_fab.fab$l_nam = &l_nam;
 
	l_status = sys$parse(&l_fab, 0, 0);
        if (!(l_status & 1))
        {
	    lib$sys_getmsg (
		&l_status,
		&l_len,
		&l_errmsg1_descrip,
		0,
		0);
	    l_errmsg1[l_len] = EOS;
	    strcpy(a_errmsg, l_errmsg1);
	    strcpy(a_dna, l_e->equivalent_name);
	    return DX__ERROR;
        }
 
	/*
	**  Get an i/o channel for each physical device
	**/
 
	strcpy(l_devname, l_e->equivalent_name);
	l_cp = strchr(l_devname, ':');
	*++l_cp = EOS;
 
	insert_dids (
	    &l_temp_entry.w_dids,
	    l_e->equivalent_name,
	    l_nam.nam$w_did,
	    get_chan(l_devname));
        if (!(1 & (l_rc = lib$free_vm (
                     &l_number_of_bytes,
                     &l_e->equivalent_name,
                     0))))
            $LOG(l_rc);
    }
    insert_filespec_list$1(a_l, l_temp_entry);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	insert_filespec_list$1() appends one filespec_list_tag entry
**	(by value, already fully built by insert_filespec_list()) to the
**	tail of the singly-linked list rooted at *a_l, recursing down the
**	"next" chain until it finds the NULL tail -- the same tail-append
**	pattern as insert_dids().
**
**  INPUT PARAMETERS:
**
**	a_e  -  the already-populated entry (name plus its w_dids list) to
**		copy into a newly-allocated node.
**
**  OUTPUT PARAMETERS:
**
**	a_l  -  address of the list head pointer (or, on recursive calls,
**		of some node's "next" field); updated in place to point at
**		the newly-allocated node once the tail is reached.
**
**  IMPLICIT INPUTS:
**
**	None.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success; $LOG(l_rc) (a VMS condition value) if
**	lib$get_vm fails to allocate the new node.
**
**--
**/
int	insert_filespec_list$1(a_l, a_e)
struct filespec_list_tag **a_l;
struct filespec_list_tag a_e;
{
    struct filespec_list_tag *l_p;
    int l_allocate_size;
    unsigned long l_rc;
 
    if (*a_l != (struct filespec_list_tag *)NULL)
	insert_filespec_list$1(&((*a_l)->next), a_e);
    else
    {
	l_allocate_size = sizeof (struct filespec_list_tag);
        if (!(1 & (l_rc = lib$get_vm (
                     &l_allocate_size,
                     &l_p,
                     0))))
            return $LOG(l_rc);
	strcpy(l_p->name, a_e.name);
	l_p->w_dids = a_e.w_dids;
	l_p->next = (struct filespec_list_tag *)NULL;
	*a_l = l_p;
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	broadcast_routine() is DX's OpenVMS broadcast-message handler
**	(installed via smg$set_broadcast_trapping() in DX_initialized()).
**	It fetches the pending message text (smg$get_broadcast_message),
**	skips any leading non-printable framing characters, pastes up a
**	full-screen overlay display showing the message and a "Press
**	RETURN to continue ..." prompt, cancels any keyboard input already
**	queued, waits for one line of input, then tears the overlay down
**	and restores the cursor to wherever it was on the previously
**	active display.  Return status from smg$get_broadcast_message is
**	deliberately not checked with the usual $LOG() pattern, since a
**	non-SS$_NORMAL status here (e.g. the user disabled broadcasts)
**	is an expected, silent no-op rather than an error.
**
**  INPUT PARAMETERS:
**
**	None.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.pasteboard_id, .pasteboard_rows, .pasteboard_width,
**	.keyboard_id.
**
**  IMPLICIT OUTPUTS:
**
**	None (all display state it creates is torn down again before
**	returning).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	    broadcast_routine()
{
    unsigned int l_status;
    char l_message[BROADMSGLENG+1];
    $DESCRIPTOR (l_message_descrip, l_message);
    unsigned short l_message_length;
    unsigned short l_message_type;
    unsigned int l_prev_display_id, l_display_id;
    int l_start_row, l_start_column, l_col;
    char *l_prompt = "Press RETURN to continue ...";
    $DESCRIPTOR (l_prompt_descrip, l_prompt);
    char l_resultant[512];
    $DESCRIPTOR (l_resultant_descrip, l_resultant);
    int l_i;
    unsigned long l_rc;
 
    /*
    **	Do not use 'check' to verify the return status
    **	(in case of user disables broadcast message)
    **/
 
    l_status = smg$get_broadcast_message (
		  &cntrl_info_block.pasteboard_id,
		  &l_message_descrip,
		  &l_message_length,
		  &l_message_type);
 
    if (l_status == SS$_NORMAL)
    {
	l_message[l_message_length] = EOS;
	for (l_i = 0;  l_i < l_message_length && !isprint ((int) l_message[l_i]);  l_i++)
	{
;
	}
	POINTER (l_message_descrip) += l_i;
	LENGTH(l_message_descrip) = l_message_length - l_i;
 
	if (!(1 & (l_rc = smg$find_cursor_display (
		  &cntrl_info_block.pasteboard_id,
		  &l_prev_display_id,
		  0,
		  0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$return_cursor_pos (
		  &l_prev_display_id,
		  &l_start_row,
		  &l_start_column))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$create_virtual_display (
		     &cntrl_info_block.pasteboard_rows,
		     &cntrl_info_block.pasteboard_width,
		     &l_display_id,
		     &SMG$M_DISPLAY_CONTROLS,
		     0,
		     0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$paste_virtual_display (
		     &l_display_id,
		     &cntrl_info_block.pasteboard_id,
		     &1,
		     &1,
		     0))))
	    $LOG(l_rc);
	/*  14-Sep-2026 RRL: batch the message + prompt into one flush --
	**  NOT including the paste above (smg$begin/end_display_update is
	**  only valid around content writes to an already-pasted display,
	**  not around the paste operation itself; wrapping a paste broke
	**  a similar case in signal_err(), %DX-F-SFC in live testing) --
	**  ended BEFORE fgets() below, since the message/prompt must
	**  actually be visible while it blocks waiting for RETURN.  **/
	if (!(1 & (l_rc = smg$begin_display_update(&l_display_id))))
	    $LOG(l_rc);
	smg$put_chars (
		 &l_display_id,
		 &l_message_descrip,
		 0,
		 0,
		 0,
		 &SMG$M_BOLD,
		 0,
		 0);
	l_col = cntrl_info_block.pasteboard_width - strlen(l_prompt);
	LENGTH(l_prompt_descrip) = strlen(l_prompt);
	if (!(1 & (l_rc = smg$put_chars (
		     &l_display_id,
		     &l_prompt_descrip,
		     &cntrl_info_block.pasteboard_rows,
		     &l_col,
		     0,
		     &SMG$M_BOLD,
		     0,
		     0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$end_display_update(&l_display_id))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$cancel_input (
		     &cntrl_info_block.keyboard_id))))
	    $LOG(l_rc);
	/**  gets() has no bound and can overrun "l_resultant" -- fgets()
	**   with an explicit size limit reads (and discards, same as
	**   before) at most one line up to the buffer's capacity.  **/
	fgets (l_resultant, sizeof (l_resultant), stdin);

	if (!(1 & (l_rc = smg$erase_display (
		     &l_display_id,
		     0,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$delete_virtual_display (
		     &l_display_id))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$set_cursor_abs (
		  &l_prev_display_id,
		  &l_start_row,
		  &l_start_column))))
	    $LOG(l_rc);
    }
    return DX__NORMAL;
}
