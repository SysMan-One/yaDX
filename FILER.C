/* Program Name            : FILER.C                                    */
/*   Original Author       : C. K. Hung					*/
/*   Editor                : StarLet Squad and Ruslan R. Laishev (RRL)  */
/*   Date                  : 7-12-89					*/
/*   Program Description   : Main filer driver, windows, GOLD-key       */
/*        keypad commands and menu navigation -- consolidated           */
/*        14-Sep-2026 from FILER.C + FILERWIN.C + FILERKPD.C +          */
/*        MENUNAVI.C.  It accepts user command to perform functions     */
/*        such as delete, edit, or rename files in the current          */
/*        directory.                                                    */
/*                         :                                            */
/* Revision History follows                                             */
/*   03-Jun-1999 Fixed bug with PF1					*/
/*		 Weak /EXIT support					*/
/*   13/14-Sep-2026 RRL a_/l_/$LOG naming convention; consolidated       */
/*		 FILERWIN.C+FILERKPD.C+MENUNAVI.C into this file        */
/*  25-Sep-2026 RRL ECO2: __NEW_STARLET prototypes, types fixed (0 CC diags)  */
/*                        SIGALRM handlers -> void f(int); SMG$ ids -> unsigned int  */
/*                        PgUp/PgDn: one jump scroll, not N 1-line scrolls  */
/*  26-Sep-2026 CLD ECO3: DO / keypad "-" / PF4 / "-" open/close the menu;  */
/*                        submenu arrows fixed (DX_KEY_TOUPPER, codes > 255);  */
/*                        exit NEL via s_filer_scroll_off(), not stderr "\033E"  */
/*  26-Sep-2026 CLD ECO5: SIGALRM "Working..." handlers -> dxscr_working_start()  */
/*                        Home/End/Ins (E1/E4/E2) = keypad 5/6/7 in the file list  */
/*  28-Sep-2026 CLD ECO7: find file: EOS after lib$find_file result       */
/*  28-Sep-2026 CLD ECO10: submenu paint/highlight-move batched with       */
/*                        smg$begin/end_display_update (create_submenu(), */
/*                        select_from_submenu()) -- less flicker opening  */
/*                        a submenu and moving inside it                  */

#define __MODULE__	"FILER"
#define __IDENT__	"X-02-05ECO10"
#define __REV__		"02.05.10"
 
 
#define	__NEW_STARLET	1
#include	<starlet.h>
#include	<lib$routines.h>
#include	<str$routines.h>
#include	<libdef.h>
#include	<signal.h>
#include	<strdef.h>
#include	<time.h>

#include "global.h"
#include "direct.h"
#include "diff.h"
#include "dx.h"
#include "filer.h"
#include "filercpy.h"
#include "filersta.h"
#include "fileropt.h"
#include "filerque.h"
#include "inquire.h"
 
/*
**	GLOBAL DECLARATIONS
**/
 
struct menutype filer_menu[] = {
    {
	" Window ", GEN_CMDS_ROW, 1,
	{
	    "Split window", "Delete window", "One window", MENU_SEPARATOR,
	    "Next window", "Previous window", MENU_SEPARATOR,
	    "eXit", NULL
	},
	{
	    filer_split_win, filer_delete_win, filer_one_win, NULL,
	    filer_next_win, filer_prev_win, NULL,
	    (int (*)()) filer_exitset	/**  void: never returns  **/
	},
    },
    {
	" Action ", GEN_CMDS_ROW, 9,
	{
	    "Type", "Edit", "Delete", "purGe",
	    "Rename...", "Copy...", "Append...", "Set protection...",
	    "Print...", "Show", "Find...", "Run...",
	    "Compare...", NULL
	},
	{
	    filer_type, filer_edt, filer_delete, filer_purge,
	    filer_rename, filer_copy, filer_append, filer_protection,
	    filer_print, filer_status, filer_find, filer_launch,
	    filer_diff
	},
    },
    {
	" Execute ", GEN_CMDS_ROW, 17,
	{
	    "Command", "VMS Gateway", NULL
	},
	{
	    filer_vms_command, filer_vms_gateway
	},
    },
    {
	" Sort ", GEN_CMDS_ROW, 26,
	{
	    "by Name...", "by Type...", "by Date...",
	    "by Size...", "by Marked...", NULL
	},
	{
	    filer_sort_name, filer_sort_type, filer_sort_date,
	    filer_sort_size, filer_sort_mark
	}
    },
    {
	" Filter ", GEN_CMDS_ROW, 32,
	{
	    "Name...", "Exclude...",
	    "Since...", "Before...",
	    "maXimum...", "Minimum...", NULL
	},
	{
	    filer_filter_name, filer_filter_exclude,
	    filer_filter_since, filer_filter_before,
	    filer_filter_maximum, filer_filter_minimum
	},
    },
    {
	" Option ", GEN_CMDS_ROW, 40,
	{
	    "Direct tree", "Update", "Mkdir...",
	    "show Buffer", "Write...", "Goto...", MENU_SEPARATOR,
	    "Short form", "Long form", MENU_SEPARATOR,
	    "Include all", "Exclude all", "Re-include all", MENU_SEPARATOR,
	    "About DX", NULL
	},
	{
	    filer_direct, filer_update, filer_mkdir,
	    filer_show_buffer, filer_write, filer_goto, NULL,
	    filer_short_form, filer_long_form, NULL,
	    filer_mark_all, filer_unmark_all, filer_remark_all, NULL,
	    filer_about
	},
    },
    {
	" Customize ", GEN_CMDS_ROW, 48,
	{
	    "Editor...", "Display with date...", "Type file...",
	    "Auto update...", "Delete confirm...", "Purge confirm...",
	    "Show clock...", "Flash clock colon...", MENU_SEPARATOR,
	    "Save settings...", "Restore settings...", "Use defaults", NULL
	},
	{
	    custom_editor, custom_display_date, custom_type,
	    custom_auto_update, custom_delete, custom_purge,
	    custom_display_clock, custom_update_in_second, NULL,
	    custom_save_setting, custom_restore_setting, custom_use_default
	},
    },
    NULL
};
 
/*
**
**	INTERNAL FUNCTION PROTOTYPING
**
**/
 
    static unsigned int		s_filer_scroll_off(void);
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	s_filer_scroll_off() moves the physical cursor to column 1 of the
**	pasteboard's last row and writes a one-<LF> record to SYS$OUTPUT, so
**	the screen scrolls up one line and the DCL prompt that follows the
**	image exit starts on a clean bottom line under the DX screen, which
**	is left in place.  Used by the KP0/CTRL-Z/CTRL-E exits of
**	DX_process_loop(); replaces the former fputs("\033E", stderr).
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
**	cntrl_info_block.pasteboard_id, cntrl_info_block.pasteboard_rows.
**
**  IMPLICIT OUTPUTS:
**
**	The terminal: cursor position and one line of scroll.
**
**  RETURNS VALUE:
**
**	Condition value of the first failing smg$set_physical_cursor() or
**	lib$put_output() call (already reported via $LOG), else SS$_NORMAL.
**
**--
**/
static unsigned int	s_filer_scroll_off(void)
{
unsigned int	l_rc;
$DESCRIPTOR(l_lf, "\n");

	if (!(1 & (l_rc = smg$set_physical_cursor(&cntrl_info_block.pasteboard_id, &cntrl_info_block.pasteboard_rows, &1))))
		return	$LOG(l_rc);

	/*
	**  A record holding one <LF>: at the last row it scrolls one line,
	**  and the record's trailing <CR> returns to column 1 -- same as
	**  ESC E (NEL).  An empty record is not enough: seen live, the
	**  terminal then gets only the <CR>, no scroll.
	**/
	if (!(1 & (l_rc = lib$put_output(&l_lf))))
		return	$LOG(l_rc);

	return	SS$_NORMAL;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	DX_process_loop() is the main filer command loop.  It builds the
**	single-letter valid-command string from filer_menu[], paints the
**	general-command line and PF-key line (skipped if this window is
**	exiting via /EXIT), lays out and pastes the FILER virtual display,
**	writes the current directory's files to it, and (unless /EXIT
**	applies, in which case it restores the original directory and exits
**	immediately) enters an infinite read-keystroke loop: an upper-cased
**	alphanumeric key matching a general-menu letter is handed to
**	navigator() to run that menu's command; anything else is dispatched
**	by a big switch on the raw keystroke code covering exit (KP0,
**	CTRL/Z, CTRL/E), screen refresh (CTRL/W), next window (HT), help
**	(KP1, HELP), direction toggle (KP2), find first/next (KP3/KP4),
**	move to top/bottom (KP5/KP6), mark/unmark (KP7), page up/down
**	(KP8, E5, E6), single/multiple mode toggle (KP9), arrow-key
**	movement, and change-directory (CR, via filer_filter$1() with
**	criterion "Name").  Any key with no definition here rings the bell
**	and reports its symbolic name via smg$keycode_to_name().
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
**	filer_menu[] (the general-command table), cntrl_info_block (current
**	window, pasteboard/keyboard ids, user preferences), the current
**	window's DX_CURRENT_DIRECTORY/DX_CURRENT_FILE state.
**
**  IMPLICIT OUTPUTS:
**
**	The current window's FILER virtual display (created, painted, and
**	pasted), DX_CURRENT_DIRECTORY.direction/filemode (toggled by KP2/
**	KP9), and whatever the dispatched command (navigator() or the
**	switch cases) itself changes.
**
**  RETURNS VALUE:
**
**	Whatever the CR case's filer_filter$1() call returns, if that path
**	is taken (this is the only case that actually returns out of the
**	function instead of looping); the loop itself never falls through,
**	since every other path either continues the loop or calls exit()
**	directly.
**
**--
**/
int	DX_process_loop()
{
int l_number_of_items;
char *l_c1, *l_c2;
struct menutype *l_mp;
char l_valid_cmdstr[MAXCOMMANDSITEMNUM+1];
 
int l_row, l_col;
unsigned short l_keystroke;
unsigned int l_status;
int l_choice_no;
char l_keyname[15];
$DESCRIPTOR (l_keyname_descrip, l_keyname);
char l_errmsg[MAXFILESPEC+1];	/**  Error message buf       **/
int l_test;
unsigned long l_rc;
 
    /*
    **	Get valid commands string
    **/
 
    l_number_of_items = 0;
    l_c1 = l_valid_cmdstr;
    for (l_mp = filer_menu; l_mp->itmstr; l_mp++)
	for (l_c2 = l_mp->itmstr; *l_c2; l_c2++)
	    if (isupper (*l_c2)) {
		*l_c1++ = *l_c2;
		l_number_of_items++;
		break;
	    }
    *l_c1 = EOS;
 
    /*
    **	Write status and command lines
    **/
 
    if(!cntrl_info_block.dir_dx[cntrl_info_block.cur_win].exitn){
     put_general_cmds(filer_menu);
     dxscr_tick_clock(cntrl_info_block.commands_display.id);
     put_filer_pfs(FILER_PFS, cntrl_info_block.commands_display.id);
    }
 
    /*
    **	Compute the position of each file
    **/
 
    DX_CURRENT_DIRECTORY.dir_filelist->beg_y =
	DX_CURRENT_DIRECTORY.dir_filelist->beg_x = 1;
    l_row = l_col = 1;
    compute_filer_begin_xy (
	DX_CURRENT_DIRECTORY.dir_filelist, &l_row, &l_col, cntrl_info_block.cur_win);
    DX_CURRENT_DIRECTORY.non_dir_filelist->beg_y =
	DX_CURRENT_DIRECTORY.dir_filelist->backward->beg_y;
    DX_CURRENT_DIRECTORY.non_dir_filelist->beg_x =
	DX_CURRENT_DIRECTORY.dir_filelist->backward->beg_x;
    compute_filer_begin_xy (
	DX_CURRENT_DIRECTORY.non_dir_filelist, &l_row, &l_col, cntrl_info_block.cur_win);
 
    /*
    **	Create FILER virtual display
    **/
 
    DX_CURRENT_DIRECTORY.filer_display.view_rows =
	 cntrl_info_block.pasteboard_rows-CMDS_RSV_LNS-DIRINFO_RSV_LNS;
    DX_CURRENT_DIRECTORY.filer_display.view_width =
	cntrl_info_block.pasteboard_width;
    DX_CURRENT_DIRECTORY.filer_display.view_beg_y = 1;
    DX_CURRENT_DIRECTORY.filer_display.view_beg_x = 1;
    DX_CURRENT_DIRECTORY.filer_display.rows =
	max (DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y,
	     DX_CURRENT_DIRECTORY.filer_display.view_rows);
    DX_CURRENT_DIRECTORY.filer_display.width =
	cntrl_info_block.pasteboard_width;
    DX_CURRENT_DIRECTORY.filer_display.beg_y = FILER_PBD_ROW;
    DX_CURRENT_DIRECTORY.filer_display.beg_x = FILER_PBD_COLUMN;
    if (!(1 & (l_rc = smg$create_virtual_display (
		&DX_CURRENT_DIRECTORY.filer_display.rows,
		&DX_CURRENT_DIRECTORY.filer_display.width,
		&DX_CURRENT_DIRECTORY.filer_display.id,
		0,
		0,
		0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$create_viewport (
		&DX_CURRENT_DIRECTORY.filer_display.id,
		&DX_CURRENT_DIRECTORY.filer_display.view_beg_y,
		&DX_CURRENT_DIRECTORY.filer_display.view_beg_x,
		&DX_CURRENT_DIRECTORY.filer_display.view_rows,
		&DX_CURRENT_DIRECTORY.filer_display.view_width))))
        $LOG(l_rc);
 
    /*
    **	Set current file pointer.  Make sure at least one file exists
    **	in the directory file list
    **/
 
    DX_CURRENT_DIRECTORY.cur_file =
	(DX_CURRENT_DIRECTORY.dir_filelist->forward !=
	    DX_CURRENT_DIRECTORY.dir_filelist?
		DX_CURRENT_DIRECTORY.dir_filelist->forward :
		DX_CURRENT_DIRECTORY.non_dir_filelist->forward);
 
    /*
    **	Write directory title and statistics
    **/
 
    if(!cntrl_info_block.dir_dx[cntrl_info_block.cur_win].exitn){
     put_filer_title(cntrl_info_block.cur_win);
     put_filer_stat(cntrl_info_block.cur_win);
 
     /*
     **	Write directory and page info
     **/
 
     /*
     div_p = div (cntrl_info_block.dir_dx[0].non_dir_filelist->backward->beg_y,
		    cntrl_info_block.dir_dx[0].filer_display.view_rows);
     pages = (div_p.rem == 0? div_p.quot : div_p.quot+1);
     all_dirs = (strcmp(cntrl_info_block.dir_dx[0].dir_filelist->forward->fn, "[-]")?
	cntrl_info_block.dir_dx[0].subdir : cntrl_info_block.dir_dx[0].subdir+1);
     sprintf(pages_str, "Total of %d %s, %d %s read",
	all_dirs, (all_dirs > 1? "directories" : "directory"),
	pages, (pages > 1? "pages" : "page"));
     signal_err(pages_str, silence);
     */
    }
 
    /*
    **  Write all the files to screen
    **/
 
    write_to_filer(
	cntrl_info_block.dir_dx[cntrl_info_block.cur_win].dir_filelist);
    write_to_filer(
	cntrl_info_block.dir_dx[cntrl_info_block.cur_win].non_dir_filelist);
 
    if(!cntrl_info_block.dir_dx[cntrl_info_block.cur_win].exitn)
		highlight_filer_current_file();
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
		&DX_CURRENT_DIRECTORY.filer_display.id,
		&cntrl_info_block.pasteboard_id,
		&DX_CURRENT_DIRECTORY.filer_display.beg_y,
		&DX_CURRENT_DIRECTORY.filer_display.beg_x,
		0))))
        $LOG(l_rc);
 
    /* Exit immediatly. Will do better later	*/
    if(cntrl_info_block.dir_dx[cntrl_info_block.cur_win].exitn){
	setddir(cntrl_info_block.original_cwd, l_errmsg);
	if (!(1 & (l_rc = smg$set_physical_cursor (
		 &cntrl_info_block.pasteboard_id,
		 &cntrl_info_block.pasteboard_rows,
		 &1))))
	    $LOG(l_rc);
	exit(SS$_NORMAL);
    }
 
 
 
    /*
    **	Loop until exited by user
    **/
 
 
    while (1)
    {
	/*
	**  Read next command.  Clear error message line if any.
	**/
 
	int l_timeout = cntrl_info_block.user_pref.update_in_second? 1 : 60;
 
	do {
	    l_status = smg$read_keystroke (
				&cntrl_info_block.keyboard_id,
				&l_keystroke,
				0,
				&l_timeout,
				0,
				0,
				0);
	    dxscr_tick_clock(cntrl_info_block.commands_display.id);
	} while (l_status == SS$_TIMEOUT ||
		 l_keystroke == SMG$K_TRM_CANCELLED);
 
	if (!(1 & (l_rc = smg$get_pasting_info (
		  &cntrl_info_block.status_display.id,
		  &cntrl_info_block.pasteboard_id,
		  &l_status,
		  0,
		  0))))
	    $LOG(l_rc);
 
	if (l_status == SMG$M_DISPLAY_PASTED)
	    if (!(1 & (l_rc = smg$unpaste_virtual_display (
		      &cntrl_info_block.status_display.id,
		      &cntrl_info_block.pasteboard_id))))
	        $LOG(l_rc);
 
	/*
	**  Do job according to key pressed
	**/
 
	/* PF1 = 256 will crash DX on Alpha */
	if( l_keystroke != 256){
		l_test = DX_KEY_ISALNUM(l_keystroke) &&
	    		(l_c1 = strchr(l_valid_cmdstr, (char) DX_KEY_TOUPPER(l_keystroke))) != NULL;
	} else {
		l_test = 0;
	}
 
	if (l_test){
	    l_choice_no = (int) (l_c1-l_valid_cmdstr);
	    l_keystroke = navigator(
			    filer_menu,
			    &l_choice_no,
			    l_number_of_items,
			    1);
	} else if (l_keystroke == SMG$K_TRM_DO ||
		   l_keystroke == SMG$K_TRM_MINUS ||
		   l_keystroke == SMG$K_TRM_PF4 ||
		   l_keystroke == '-') {
	    /*  25-Sep-2026 RRL: DO key -- straight into the top menu bar on
	    **  its first category (Window), submenu open at once; LEFT/RIGHT
	    **  then move between categories and KP0 cancels, exactly as
	    **  after typing a category letter.  26-Sep-2026 RRL: also keypad
	    **  "-" -- handier than DO (Shift+F6) on a PC keyboard.  A PC's
	    **  Num "-" arrives as VT KP "-" in some emulators and as PF4 in
	    **  others (NumLock / * - = PF1..PF4), so both are accepted --
	    **  and as a plain ASCII "-" in yet others (seen live with
	    **  KEYCODE: 45 %X002D), so that too; "-" had no filer meaning
	    **  (only letters/digits are menu hotkeys here).  **/
	    l_choice_no = 0;
	    l_keystroke = navigator(
			    filer_menu,
			    &l_choice_no,
			    l_number_of_items,
			    0);
	} else {
	    switch (l_keystroke)
	    {
	    /**  Exit without changing directory  **/
	    case SMG$K_TRM_KP0:
	    case SMG$K_TRM_CTRLZ:
	    case SMG$K_TRM_CTRLE:
		setddir(cntrl_info_block.original_cwd, l_errmsg);

		/**  Scroll screen up 1 line  **/
		s_filer_scroll_off();

		/*
		**  NEVER RETURN!!!
		**/
		exit(SS$_NORMAL);

	    case SMG$K_TRM_CTRLW:    /**  Refresh screen  **/
                if (!(1 & (l_rc = smg$repaint_screen (
			     &cntrl_info_block.pasteboard_id))))
                    $LOG(l_rc);
		break;
 
 
	    case SMG$K_TRM_HT:	    /**  Next window  **/
		filer_next_win();
		break;
 
 
	    /**  Help command  **/
	    case SMG$K_TRM_KP1:
		filer_help("DX");
		break;
	    case SMG$K_TRM_HELP:
		filer_help("DX");
		break;
 
 
	    case SMG$K_TRM_KP2:	    /**  Forward/backward switch  **/
 
		if (DX_CURRENT_DIRECTORY.direction == advance)
		{
		    DX_CURRENT_DIRECTORY.direction = backup;
		} else {
		    DX_CURRENT_DIRECTORY.direction = advance;
		}
		put_filer_title(cntrl_info_block.cur_win);
		break;
 
	    case SMG$K_TRM_KP3:	    /**  Find first command  **/
		filer_find_first();
		break;
 
	    case SMG$K_TRM_KP4:	    /**  Find next command  **/
		filer_find_next();
		break;
 
	    /*
	    **  26-Sep-2026 CLD: PC Home/End/Ins come as the VT editing keys
	    **  E1 (Find), E4 (Select), E2 (Insert Here) -- same as KP5/KP6/KP7.
	    **/
	    case SMG$K_TRM_KP5:	    /**  Move to top  **/
	    case SMG$K_TRM_E1:	    /**  Home  **/
		unhighlight_filer_current_file();
		filer_top();
		break;
 
	    case SMG$K_TRM_KP6:	    /**  Move to bottom  **/
	    case SMG$K_TRM_E4:	    /**  End  **/
		unhighlight_filer_current_file();
		filer_bottom();
		break;
 
	    case SMG$K_TRM_KP7:	    /**  Include/exclude switch  **/
	    case SMG$K_TRM_E2:	    /**  Ins  **/
		if (DX_CURRENT_FILE->state != ready)
		    filer_mark(DX_CURRENT_FILE, 0);
		else
		    filer_unmark(DX_CURRENT_FILE, 0);
		break;
 
 
	    /**  Page up/down  **/
	    case SMG$K_TRM_KP8:
		unhighlight_filer_current_file();
		if (cntrl_info_block.dir_dx[cntrl_info_block.cur_win].direction == advance)
		    filer_page_down();
		else
		    filer_page_up();
		break;
	    case SMG$K_TRM_E5:
		unhighlight_filer_current_file();
		filer_page_up();
		break;
	    case SMG$K_TRM_E6:
		unhighlight_filer_current_file();
		filer_page_down();
		break;
 
 
 
 
	    case SMG$K_TRM_KP9:	    /**  Single/multiple switch  **/
		if (DX_CURRENT_DIRECTORY.filemode == single)
		{	/**  Enter MULTIPLE mode  **/
		    DX_CURRENT_DIRECTORY.filemode = multiple;
		}
		else
		{	/**  Single mode  **/
		    DX_CURRENT_DIRECTORY.filemode = single;
		}
		put_filer_title(cntrl_info_block.cur_win);
		break;
 
	    case SMG$K_TRM_UP:		/**  Move up  **/
		unhighlight_filer_current_file();
		filer_up();
		break;
 
	    case SMG$K_TRM_DOWN:	/**  Move down  **/
		unhighlight_filer_current_file();
		filer_down();
		break;
 
	    case SMG$K_TRM_LEFT:	/**  Move left  **/
		unhighlight_filer_current_file();
		filer_left();
		break;
 
	    case SMG$K_TRM_RIGHT:	/**  Move right  **/
		unhighlight_filer_current_file();
		filer_right();
		break;
 
	    case SMG$K_TRM_CR:		/**  Change directory  **/
                if (DX_CURRENT_FILE->fn[strlen(DX_CURRENT_FILE->fn)-1] != ']')
		{
                    if (!(1 & (l_rc = smg$ring_bell (
                                 &DX_CURRENT_DIRECTORY.filer_display.id,
                                 0))))
                        $LOG(l_rc);
		}
		else
                {
		    char buf[MAXFILESPEC+1];
		    char *cp;
		    void (*cstat)(int);
 
		    /*
		    **	Output 'working...' message if total processing time
		    **	is longer than 3 seconds.  Needs to disable broadcast
		    **	message first to avoid interference.
		    **/
#ifndef DEBUG
		    if (!(1 & (l_rc = smg$disable_broadcast_trapping (
				 &cntrl_info_block.pasteboard_id))))
		        $LOG(l_rc);
 
		    if (!(1 & (l_rc = dxscr_working_start(cntrl_info_block.commands_display.id, 2, 0, cntrl_info_block.commands_display.id, &cstat))))
			$LOG(l_rc);
#endif
		    find_full_path_name(
			DX_CURRENT_FILE->fn, buf, cntrl_info_block.cur_win);
		    if ((cp = strchr(buf, ']')) != NULL)
			*cp = '.';
		    cp = strrchr(buf, '.');
		    *cp++ = ']';
		    *cp = EOS;
                    if (filer_filter$1(buf, "Name", l_errmsg) == DX__ERROR)
                        signal_err(l_errmsg, bell);
		    else
		    {
			if (!(1 & (l_rc = smg$erase_chars (
				     &cntrl_info_block.commands_display.id,
				     &10,
				     &2,
				     &1))))
			    $LOG(l_rc);
		    }
 
		    /*
		    **	Set the signal back to system default
		    **/
#ifndef DEBUG
		    alarm(0);
		    signal(SIGALRM, cstat);
 
		    if (!(1 & (l_rc = smg$set_broadcast_trapping (
				 &cntrl_info_block.pasteboard_id,
				 broadcast_routine,
				 0))))
		        $LOG(l_rc);
#endif
                }
		break;
 
	    default:			/**  Key with no definition  **/
                memset (l_keyname, ' ', 15);
        	smg$keycode_to_name (
                   &l_keystroke,
                   &l_keyname_descrip);
 
                l_keyname[strcspn (l_keyname, " ")] = EOS;
                {
                    static $DESCRIPTOR (l_fao_ctrl, "Key '!AZ' currently has no definition");
                    struct dsc$descriptor_s l_out_desc;
                    unsigned short int l_len;

                    l_out_desc.dsc$w_length  = sizeof(l_errmsg) - 1;
                    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
                    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
                    l_out_desc.dsc$a_pointer = l_errmsg;

                    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_keyname))))
                        $LOG(l_rc);
                    l_errmsg[l_len] = EOS;
                }
		signal_err(l_errmsg, bell);
	    }
 
	} /**  switch (l_keystroke)  **/
 
        /**  Reposition the cursor  **/
	highlight_filer_current_file();
    }
	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	put_general_cmds() paints the general-command line (the row of
**	menu category names -- " Window ", " Action ", etc.) on the
**	commands display, blanking the row first, then writing each
**	category's itmstr at its start_col with every uppercase (hotkey)
**	letter rendered bold+underlined and every other character plain.
**
**  INPUT PARAMETERS:
**
**	a_m  -  NULL-terminated array of menutype entries to paint (in
**		practice always filer_menu[]).
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.commands_display.id/pasteboard_width.
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.commands_display (the general-command row is
**	repainted).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	put_general_cmds(a_m)
struct menutype *a_m;
{
    char l_line[DX_MAX_PB_WIDTH+1];
			/**  Filled to cntrl_info_block.pasteboard_width via
			**   memset() below -- must stay >= the pasteboard
			**   width cap (DX_MAX_PB_WIDTH), see DX.C.
			**/
    $DESCRIPTOR (l_line_descrip, l_line);
    char l_str1[2];
    $DESCRIPTOR(l_str1_descrip, l_str1);
    struct menutype *l_p;
    int l_len;
/*    register long int l_i, l_j; */
    long int l_i,l_j;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = smg$begin_display_update (
	      &cntrl_info_block.commands_display.id))))
        $LOG(l_rc);
 
    memset (l_line, ' ', cntrl_info_block.pasteboard_width);
    LENGTH(l_line_descrip) = cntrl_info_block.pasteboard_width;
    if (!(1 & (l_rc = smg$put_chars (
	      &cntrl_info_block.commands_display.id,
	      &l_line_descrip,
	      &GEN_CMDS_ROW,
	      &GEN_CMDS_COLUMN,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    LENGTH(l_str1_descrip) = 1;
 
    for (l_p = a_m; l_p->itmstr; l_p++)
    {
	l_len = strlen(l_p->itmstr);
	for (l_i = 0, l_j = l_p->start_col; l_i < l_len; l_i++, l_j++)
	{
	    l_str1[0] = *(l_p->itmstr+l_i);
	    if (! isupper(l_str1[0]))
	    {
		if (!(1 & (l_rc = smg$put_chars (
			  &cntrl_info_block.commands_display.id,
			  &l_str1_descrip,
			  &GEN_CMDS_ROW,
			  &l_j,
			  0,
			  0,
			  0,
			  0))))
		    $LOG(l_rc);
	    }
	    else
	    {
		if (!(1 & (l_rc = smg$put_chars (
			  &cntrl_info_block.commands_display.id,
			  &l_str1_descrip,
			  &GEN_CMDS_ROW,
			  &l_j,
			  0,
			  &SMG$M_UNDERLINE,
			  &SMG$M_BOLD,
			  0))))
		    $LOG(l_rc);
	    }
	}
    }
 
    if (!(1 & (l_rc = smg$end_display_update (
	      &cntrl_info_block.commands_display.id))))
        $LOG(l_rc);
	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	write_time() writes the current time (HH:MM:SS, via
**	lib$date_time(), always 24-hour) into the upper-right corner of
**	the given display, blinking the seconds/minutes separator off on
**	odd seconds when update_in_second is on, then restores the
**	terminal's cursor to wherever it was before (found via
**	smg$find_cursor_display()/smg$return_cursor_pos()).
**
**  INPUT PARAMETERS:
**
**	a_time_display_id  -  SMG$ virtual display id to write the clock
**			       into.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.pasteboard_id/pasteboard_width/user_pref.
**	update_in_second.
**
**  IMPLICIT OUTPUTS:
**
**	None (the display written to is a_time_display_id, an explicit
**	parameter; the cursor is moved and then restored).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	write_time(a_time_display_id)
unsigned int a_time_display_id;
{
#define TIME_STR_LENGTH	    23
 
    unsigned int l_old_display_id;
    char l_time_str[TIME_STR_LENGTH];
    $DESCRIPTOR (l_time_descrip, l_time_str);
    int l_start_row, l_start_column;
    long int l_time_column;

    smg$find_cursor_display (
	      &cntrl_info_block.pasteboard_id,
	      &l_old_display_id,
	      0,
	      0);
 
    smg$return_cursor_pos (
	      &l_old_display_id,
	      &l_start_row,
	      &l_start_column);
 
    /*
    **	Get time information
    **/
 
    lib$date_time(&l_time_descrip);
 
    POINTER(l_time_descrip) = l_time_str+12;
    LENGTH(l_time_descrip) = 8;
 
    if (cntrl_info_block.user_pref.update_in_second &&
	strchr("13579", *(l_time_str+19)))
        *(l_time_str+14) = ' ';

    /*
    **	lib$date_time() already gives a 24-hour "HH:MM:SS" clock -- always
    **	display it as-is (was converted to a 12-hour AM/PM clock here,
    **	which also threw away the seconds digits to make room for " AM"/
    **	" PM"; removed by request, no reason to show a less precise,
    **	non-VMS-standard clock).
    **/

    /*
    **	Write the time information onto the upper right corner
    **	of the screen
    **/
 
    smg$begin_pasteboard_update (
	      &cntrl_info_block.pasteboard_id);
 
    l_time_column = cntrl_info_block.pasteboard_width-8;
    smg$put_chars (
    	&a_time_display_id,
    	&l_time_descrip,
    	&TIME_ROW,
    	&l_time_column,
    	0,
    	0,
    	0,
    	0);
 
    smg$set_cursor_abs (
	      &l_old_display_id,
	      &l_start_row,
	      &l_start_column);
 
    smg$end_pasteboard_update (
	      &cntrl_info_block.pasteboard_id);
	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	clear_time() blanks out the clock area (8 spaces) on the commands
**	display -- the counterpart to write_time(), used when the "show
**	clock" preference is turned off -- then restores the terminal
**	cursor to wherever it was before.
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
**	cntrl_info_block.pasteboard_id/pasteboard_width/commands_display.id.
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.commands_display (the clock area is blanked).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	clear_time()
{
    unsigned int l_old_display_id;
    int l_start_row, l_start_column;
    $DESCRIPTOR (l_blank_time, "        ");
    long int l_time_column;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = smg$find_cursor_display (
	      &cntrl_info_block.pasteboard_id,
	      &l_old_display_id,
	      0,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$return_cursor_pos (
	      &l_old_display_id,
	      &l_start_row,
	      &l_start_column))))
        $LOG(l_rc);
 
    /*
    **	Clear time display line
    **/
 
    if (!(1 & (l_rc = smg$begin_pasteboard_update (
	      &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    l_time_column = cntrl_info_block.pasteboard_width-8;
    if (!(1 & (l_rc = smg$put_chars (
	      &cntrl_info_block.commands_display.id,
	      &l_blank_time,
	      &TIME_ROW,
	      &l_time_column,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
	      &l_old_display_id,
	      &l_start_row,
	      &l_start_column))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$end_pasteboard_update (
	      &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	put_filer_pfs() paints the PF-key label line on the bottom row of
**	the given display: the whole a_pfs_def string in reverse video,
**	then the digits "1" through "9" and "0" (the PF-key numbers) drawn
**	bold over it at fixed 8-column spacing, and finally moves the
**	cursor to the bottom-right corner.
**
**  INPUT PARAMETERS:
**
**	a_pfs_def    -  the PF-key label string to display (e.g.
**			FILER_PFS or DIRTREE_PFS from filer.h/direct.h).
**	a_display_id -  SMG$ virtual display id to paint the line on.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.pasteboard_rows/pasteboard_width.
**
**  IMPLICIT OUTPUTS:
**
**	None (all output goes to the explicit a_display_id parameter).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	put_filer_pfs(a_pfs_def, a_display_id)
char *a_pfs_def;
unsigned int a_display_id;
{
    char l_filer_pfstr[81];
    $DESCRIPTOR(l_filer_pfstr_descrip, l_filer_pfstr);
 
    char l_str2[3];
    $DESCRIPTOR(l_str2_descrip, l_str2);
 
    long int l_i, l_j, l_f;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = smg$begin_display_update (
	      &a_display_id))))
        $LOG(l_rc);
 
    strcpy(l_filer_pfstr, a_pfs_def);
    LENGTH(l_filer_pfstr_descrip) = strlen(l_filer_pfstr);
    if (!(1 & (l_rc = smg$put_chars (
	      &a_display_id,
	      &l_filer_pfstr_descrip,
	      &cntrl_info_block.pasteboard_rows,
	      &1,
	      &SMG$M_ERASE_TO_EOL,
	      &SMG$M_REVERSE,
	      0,
	      0))))
        $LOG(l_rc);
 
    strcpy(l_str2, "1");
    LENGTH(l_str2_descrip) = 1;
    if (!(1 & (l_rc = smg$put_chars (
	      &a_display_id,
	      &l_str2_descrip,
	      &cntrl_info_block.pasteboard_rows,
	      &1,
	      0,
	      &SMG$M_BOLD,
	      0,
	      0))))
        $LOG(l_rc);
 
    LENGTH(l_str2_descrip) = 2;
    for (l_f = 2, l_i = 7; l_f <= 9; l_i += 8, l_f++) {
	{
	    static $DESCRIPTOR (l_fao_ctrl, "!2ZL");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;

	    l_out_desc.dsc$w_length  = sizeof(l_str2) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_str2;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, (unsigned long) l_f))))
	        $LOG(l_rc);
	    l_str2[l_len] = EOS;
	}
	l_j = l_i+1;
	if (!(1 & (l_rc = smg$put_chars (
		  &a_display_id,
		  &l_str2_descrip,
		  &cntrl_info_block.pasteboard_rows,
		  &l_j,
		  0,
		  &SMG$M_BOLD,
		  0,
		  0))))
	    $LOG(l_rc);
 
    }
    strcpy(l_str2, " 0");
    LENGTH(l_str2_descrip) = strlen(l_str2);
    l_j = l_i + 1;
    if (!(1 & (l_rc = smg$put_chars (
	      &a_display_id,
	      &l_str2_descrip,
	      &cntrl_info_block.pasteboard_rows,
	      &l_j,
	      0,
	      &SMG$M_BOLD,
	      0,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
	      &a_display_id,
	      &cntrl_info_block.pasteboard_rows,
	      &cntrl_info_block.pasteboard_width))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$end_display_update (
	      &a_display_id))))
        $LOG(l_rc);
	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	compute_filer_begin_xy() walks the linked list a_l and assigns
**	each entry's beg_y/beg_x screen position, updating *a_rowp/*a_colp
**	as it goes so a caller can chain another list (e.g. non-directory
**	files after directory files) starting where this one left off.  In
**	long form, every entry gets its own row.  In short form, entries
**	are packed left-to-right into columns sized to the file name's
**	length (1 to 4 times FILER_COLUMN_WIDTH), wrapping to the next row
**	once 4*FILER_COLUMN_WIDTH is exceeded or a wider name would not
**	fit in the columns already used on the current row.
**
**  INPUT PARAMETERS:
**
**	a_l    -  circular linked list (MARKER-headed) of fil_dx_tag
**		  entries to position; a_l itself is the list head/marker
**		  and is skipped.
**	a_win  -  window number, used to look up the current display form
**		  (long_form vs. short).
**
**  OUTPUT PARAMETERS:
**
**	a_rowp  -  in/out: starting row on entry, updated to the row past
**		   the last entry placed.
**	a_colp  -  in/out: starting column on entry, updated to the column
**		   past the last entry placed.
**	a_l's entries each have their beg_y/beg_x fields filled in.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.dir_dx[a_win].form.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	compute_filer_begin_xy(
			struct fil_dx_tag *a_l,
			int *a_rowp,
			int *a_colp,
			unsigned short int a_win)
{
    struct fil_dx_tag *l_p;
    int l_len;
 
    for (l_p = a_l->forward;  l_p != a_l;  l_p = l_p->forward)
    {
        if (cntrl_info_block.dir_dx[a_win].form == long_form)
        {
	    l_p->beg_y = *a_rowp;
	    l_p->beg_x = *a_colp;
            (*a_rowp)++;
        }
        else
        {
            l_len = strlen(l_p->fn);
            if (l_len <= FILER_COLUMN_WIDTH-2)
            {
		l_p->beg_y = *a_rowp;
		l_p->beg_x = *a_colp;
		*a_colp += FILER_COLUMN_WIDTH;
            }
            else if (l_len <= 2*FILER_COLUMN_WIDTH-2)
            {
                if (*a_colp > 2*FILER_COLUMN_WIDTH+1)
                {
                    (*a_rowp)++;
		    *a_colp = 1;
                }
		l_p->beg_y = *a_rowp;
		l_p->beg_x = *a_colp;
		*a_colp += 2*FILER_COLUMN_WIDTH;
            }
            else if (l_len <= 3*FILER_COLUMN_WIDTH-2)
            {
                if (*a_colp > FILER_COLUMN_WIDTH+1)
                {
                    (*a_rowp)++;
		    *a_colp = 1;
                }
		l_p->beg_y = *a_rowp;
		l_p->beg_x = *a_colp;
		*a_colp += 3*FILER_COLUMN_WIDTH;
            }
            else
            {
                if (*a_colp > 1)
                {
                    (*a_rowp)++;
		    *a_colp = 1;
                }
		l_p->beg_y = *a_rowp;
		l_p->beg_x = *a_colp;
		*a_colp += 4*FILER_COLUMN_WIDTH;
            }
            if (*a_colp > 4*FILER_COLUMN_WIDTH)
            {
                (*a_rowp)++;
		*a_colp = 1;
            }
        }
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      WRITE_TO_FILER() writes all the files to current FILER display
**
**--
**/
int write_to_filer(a_marker)
struct fil_dx_tag *a_marker;
{
    int l_rest_col;	/**  1st column after the file name field  **/
    struct fil_dx_tag *l_p;
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR(l_str_descrip, l_str);
    unsigned long l_rc;

    /*  14-Sep-2026 RRL: batch every smg$put_chars/smg$set_cursor_abs call in
    **  this loop into a single physical-terminal flush -- without this, a
    **  directory with many entries visibly redraws line by line instead of
    **  appearing all at once.  **/
    if (!(1 & (l_rc = smg$begin_display_update(&DX_CURRENT_DIRECTORY.filer_display.id))))
        $LOG(l_rc);

    for (l_p = a_marker->forward; l_p != a_marker; l_p = l_p->forward)
    {
	format_filer_entry(l_p, l_str, cntrl_info_block.cur_win);
	POINTER(l_str_descrip) = l_str;
        if (a_marker == DX_CURRENT_DIRECTORY.non_dir_filelist)
	{	/**  non-direcotry file  **/
	    LENGTH(l_str_descrip) = strlen(l_str);
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&l_p->beg_y,
			&l_p->beg_x,
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
	}
	else if (DX_CURRENT_DIRECTORY.form == short_form)
        {	/**  Directory file with short form  **/
	    LENGTH(l_str_descrip) = strlen(l_str);
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&l_p->beg_y,
			&l_p->beg_x,
			0,
			&SMG$M_BOLD,
			0,
			0))))
	        $LOG(l_rc);
	}
	else
	{	/**  Directory file with long form  **/
	    char *l_cp;
 
	    LENGTH(l_str_descrip) = 1;
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&l_p->beg_y,
			&1,
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
            if (!strcmp(l_p->fn, "[-]"))
            {
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    $DESCR ("[-]"),
			    &l_p->beg_y,
			    &2,
			    0,
			    &SMG$M_BOLD,
			    0,
			    0))))
		    $LOG(l_rc);
            }
            else
            {
                if (strlen(l_p->fn) <= FILER_NAME_WIDTH)
                {
		    POINTER(l_str_descrip) = l_p->fn;
		    LENGTH(l_str_descrip) = strlen(l_p->fn);
                }
                else
                {
		    POINTER(l_str_descrip) = l_str+1;
                    LENGTH(l_str_descrip) = FILER_NAME_WIDTH;
                }
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    &l_str_descrip,
			    &l_p->beg_y,
			    &2,
			    0,
			    &SMG$M_BOLD,
			    0,
			    0))))
		    $LOG(l_rc);
		l_cp = l_str+FILER_NAME_WIDTH+2;
		POINTER(l_str_descrip) = l_cp;
		LENGTH(l_str_descrip) = strlen(l_cp);
		l_rest_col = FILER_NAME_WIDTH+3;
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    &l_str_descrip,
			    &l_p->beg_y,
			    &l_rest_col,
			    0,
			    0,
			    0,
			    0))))
		    $LOG(l_rc);
            }
        }
 
	if (!(1 & (l_rc = smg$set_cursor_abs (
		    &DX_CURRENT_DIRECTORY.filer_display.id,
		    &l_p->beg_y,
		    &l_p->beg_x))))
	    $LOG(l_rc);
    }

    if (!(1 & (l_rc = smg$end_display_update(&DX_CURRENT_DIRECTORY.filer_display.id))))
        $LOG(l_rc);

	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FORMAT_FILER_ENTRY () format a string for FILER display
**
**--
**/
void	    format_filer_entry(struct fil_dx_tag *a_p,char *a_str,
		unsigned short int a_win)
{
    char l_spro[5],   /**  PROTECTION CODES  **/
	 l_opro[5],
	 l_gpro[5],
	 l_wpro[5];
    unsigned short int l_timbuf[7];
    char l_ch;
    char *l_cp;
    DATE_TIME l_date_time;
    char l_wday[12];	/**  "DD-MMM-YYYY" (11 chars) -- widened from the
			**   original char[9]; that only fit 2-digit years
			**   ("l_timbuf[0]-1900"), which broke once the year
			**   passed 1999 (e.g. 2026 -> 126, not 26).  **/
    char l_ch_str[2];	/**  l_ch as a 1-char ASCIZ string, for SYS$FAO's
			**   "!AZ" (which needs a string address, not a
			**   bare character value).  **/
    char l_namefld[DX_MAX_PB_WIDTH+1];
			/**  a_p->fn left-justified into the
			**   FILER_NAME_WIDTH-char name field used
			**   throughout this function's display lines
			**   ("~" marks truncation on the last char if the
			**   real name is longer; the field grows with the
			**   terminal width, 30 chars at 80 columns, 80
			**   at 132, 102 at 154) -- built once per branch via
			**   dxscr_ljust(), then handed to SYS$FAO() as a
			**   plain already-sized "!AZ" string, since FAO
			**   has no printf-style "%-N.Ns" width/precision
			**   directive of its own.  **/
    char l_msgfld[18];	/**  17-char fixed-width field for the "None
			**   specified"/"No backup listed" placeholder
			**   messages (same dxscr_ljust()-based approach as
			**   l_namefld, in place of the original
			**   "%-17.17s" idiom).  **/
    struct dsc$descriptor_s l_out_desc;
    unsigned short int l_len;
    unsigned long l_rc;

    a_str[0] = EOS;

    switch (a_p->state) {

	case ready:    l_ch = '*';	    break;
	case executed: l_ch = '+';	    break;
	case waiting:
	default:       l_ch = ' ';	    break;
    }
    l_ch_str[0] = l_ch;
    l_ch_str[1] = EOS;

    l_out_desc.dsc$w_length  = DX_MAX_PB_WIDTH;
					/**  Widest pasteboard (DX.C caps
					**   pasteboard_width at
					**   DX_MAX_PB_WIDTH = 255); every
					**   caller (FILER.C/FILERCPY.C/
					**   FILEROPT.C) passes at least
					**   char[MAXFILESPEC+1].  **/
    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
    l_out_desc.dsc$a_pointer = a_str;

    if (cntrl_info_block.dir_dx[a_win].form == short_form)
    {	    /**  Display in short form  **/
	static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ");

	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_ch_str, a_p->fn))))
	    $LOG(l_rc);
	a_str[l_len] = EOS;
    }
    else if (!strcmp(a_p->fn, "[-]"))
    {	    /**  Parent directory placeholder  **/
	strcpy(a_str, " [-]");
    }
    else
    {	    /**  Directory file or non-directory file  **/
	if (a_p->fpro == 0Xffff)
	{	/**  Insufficient privilege  **/
	    static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ    insufficient privilege for read operation");

	    if (strlen(a_p->fn) <= FILER_NAME_WIDTH)
		dxscr_ljust(l_namefld, a_p->fn, FILER_NAME_WIDTH);
	    else
	    {
		dxscr_ljust(l_namefld, a_p->fn, FILER_NAME_WIDTH-1);
		strcat(l_namefld, "~");
	    }
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_ch_str, l_namefld))))
		$LOG(l_rc);
	    a_str[l_len] = EOS;
	}
	else if (a_p->fpro == 0Xfffe)
	{	/**  No such file  **/
	    static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ    no such file");

	    if (strlen(a_p->fn) <= FILER_NAME_WIDTH)
		dxscr_ljust(l_namefld, a_p->fn, FILER_NAME_WIDTH);
	    else
	    {
		dxscr_ljust(l_namefld, a_p->fn, FILER_NAME_WIDTH-1);
		strcat(l_namefld, "~");
	    }
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_ch_str, l_namefld))))
		$LOG(l_rc);
	    a_str[l_len] = EOS;
	}
	else
	{	/**  Normal file  **/
	    /**  Processing file protection  **/
	    getpro(a_p->fpro, l_spro, l_opro, l_gpro, l_wpro);

	    /**  Processing file creation date or revision date  **/
            switch (cntrl_info_block.dir_dx[a_win].cur_filter.date)
            {
                case DX_CREDATE:
                    memcpy (&l_date_time, &a_p->cdat, sizeof (DATE_TIME));
		    break;
                case DX_REVDATE:
                    memcpy (&l_date_time, &a_p->rdat, sizeof (DATE_TIME));
		    break;
                case DX_EXPDATE:
                    memcpy (&l_date_time, &a_p->edat, sizeof (DATE_TIME));
		    break;
                case DX_BAKDATE:
                    memcpy (&l_date_time, &a_p->bdat, sizeof (DATE_TIME));
		    break;
                default:
		    /**  Should never happen  **/
		    return;
            }

	    /**  Processing date  **/
	    if (!(1 & (l_rc = sys$numtim (
			  l_timbuf,
			  (struct _generic_64 *) &l_date_time))))
	        $LOG(l_rc);

	    /*  14-Sep-2026 RRL: always show the plain "DD-MMM-YYYY" VMS
	    **  date here -- dropped the "Today"/"Yesterdy" relative-date
	    **  labels (and the convert_date_string()/lib$sub_times()
	    **  comparisons that fed them) per the user's direct request
	    **  ("убирай yesterday и подобное, чисто VMS дата и всё"), for
	    **  the same reason the "less than a week old -> weekday name"
	    **  case was dropped earlier: unwanted complexity, a plain date
	    **  is all that's needed.  **/
	    dxscr_vms_date_string(l_wday, &l_date_time);

	    if (strlen(a_p->fn) <= FILER_NAME_WIDTH)
		dxscr_ljust(l_namefld, a_p->fn, FILER_NAME_WIDTH);
	    else
	    {
		dxscr_ljust(l_namefld, a_p->fn, FILER_NAME_WIDTH-1);
		strcat(l_namefld, "~");
	    }
            if (l_timbuf[0] == 1858 && l_timbuf[1] == 11 && l_timbuf[2] == 17)
            {
                if (cntrl_info_block.dir_dx[a_win].cur_filter.date ==
		    DX_EXPDATE)
		{
		    static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ !AZ !5ZL   (!AZ,!AZ,!AZ,!AZ)");

		    dxscr_ljust(l_msgfld, "None specified", 17);
		    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
					       l_ch_str, l_namefld, l_msgfld,
					       (unsigned long) a_p->filesize,
					       l_spro, l_opro, l_gpro, l_wpro))))
			$LOG(l_rc);
		    a_str[l_len] = EOS;
		}
		else if (cntrl_info_block.dir_dx[a_win].cur_filter.date ==
			DX_BAKDATE)
		{
		    static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ !AZ !5ZL   (!AZ,!AZ,!AZ,!AZ)");

		    dxscr_ljust(l_msgfld, "No backup listed", 17);
		    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
					       l_ch_str, l_namefld, l_msgfld,
					       (unsigned long) a_p->filesize,
					       l_spro, l_opro, l_gpro, l_wpro))))
			$LOG(l_rc);
		    a_str[l_len] = EOS;
		}
		else
		{
		    static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ !AZ !2ZL:!2ZL:!2ZL !5ZL   (!AZ,!AZ,!AZ,!AZ)");

		    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
					       l_ch_str, l_namefld, l_wday,
					       (unsigned long) l_timbuf[3],
					       (unsigned long) l_timbuf[4],
					       (unsigned long) l_timbuf[5],
					       (unsigned long) a_p->filesize,
					       l_spro, l_opro, l_gpro, l_wpro))))
			$LOG(l_rc);
		    a_str[l_len] = EOS;
		}
            }
            else
            {
		static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ !AZ !2ZL:!2ZL:!2ZL !5ZL   (!AZ,!AZ,!AZ,!AZ)");

		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
					   l_ch_str, l_namefld, l_wday,
					   (unsigned long) l_timbuf[3],
					   (unsigned long) l_timbuf[4],
					   (unsigned long) l_timbuf[5],
					   (unsigned long) a_p->filesize,
					   l_spro, l_opro, l_gpro, l_wpro))))
		    $LOG(l_rc);
		a_str[l_len] = EOS;
            }
	}
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	setddir() changes the process's default device and directory to
**	a_dev_dir: it invalidates filer_find_first$2()'s file-membership
**	cache (since that cache is tied to the current directory/window),
**	parses a_dev_dir with sys$parse() to split it into device and
**	directory strings, redefines the SYS$DISK logical to the device
**	via lib$set_logical(), and sets the default directory via
**	sys$setddir().  The directory is set even if it does not actually
**	exist, matching the original behavior.
**
**  INPUT PARAMETERS:
**
**	a_dev_dir  -  device:[directory] specification to make current.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message if any step fails.
**
**  IMPLICIT INPUTS:
**
**	None.
**
**  IMPLICIT OUTPUTS:
**
**	The process's default device/directory and the SYS$DISK logical
**	name; filer_find_first$2()'s static file-membership cache
**	(invalidated).
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success; DX__ERROR if sys$parse(), lib$set_logical(),
**	or sys$setddir() fails (a_errmsg is set to a fixed "directory or
**	file specification invalid" message in every failure case).
**
**--
**/
int	    setddir(a_dev_dir, a_errmsg)
char *a_dev_dir;
char *a_errmsg;
{
    struct FAB l_fab;
    struct NAM l_nam;
    char l_expand_name[NAM$C_MAXRSS];
    char l_dev[MAXFILESPEC+1];
    $DESCRIPTOR (l_dev_descrip, l_dev);
    char l_dir[MAXFILESPEC+1];
    $DESCRIPTOR (l_dir_descrip, l_dir);
    unsigned int l_status;

    /**  The set of files filer_find_first$2() would scan is tied to
    **   the current directory/window -- discard its cache whenever
    **   either can change (setddir() is the single choke point for
    **   both: direct directory changes and every window switch).  **/
    invalidate_find_cache();

    /*
    **	Set default directory even if the directory specified
    **	does not exist.  First change the definition of
    **	SYS$DISK, and then set default directory.
    **/
 
    /**  Initialize FAB and NAM blocks  **/
 
    l_fab = cc$rms_fab;
    l_nam = cc$rms_nam;
 
    l_nam.nam$l_esa = l_expand_name;
    l_nam.nam$b_ess = NAM$C_MAXRSS;
 
    l_fab.fab$l_fna = a_dev_dir;
    l_fab.fab$b_fns = strlen(l_fab.fab$l_fna);
    l_fab.fab$l_nam = &l_nam;
 
    l_status = sys$parse(&l_fab, 0, 0);
    if (!(l_status & 1))
    {
	strcpy(a_errmsg, "directory or file specification invalid");
	return DX__ERROR;
    }
 
    strncpy (l_dev, l_nam.nam$l_dev, l_nam.nam$b_dev);
    l_dev[l_nam.nam$b_dev] = EOS;
 
    strncpy (l_dir, l_nam.nam$l_dir, l_nam.nam$b_dir);
    l_dir[l_nam.nam$b_dir] = EOS;
 
    /**  Change the definition of SYS$DISK  **/
    LENGTH(l_dev_descrip) = strlen(l_dev);
    l_status = lib$set_logical (
		  $DESCR ("SYS$DISK"),
		  &l_dev_descrip,
		  0,
		  0,
		  0);
    if (!(l_status & 1))
    {
	strcpy(a_errmsg, "directory or file specification invalid");
	return DX__ERROR;
    }
 
    /**  Set new default directory  **/
    LENGTH(l_dir_descrip) = strlen(l_dir);
    l_status = sys$setddir (
		&l_dir_descrip,
		0,
		0);
    if (!(l_status & 1))
    {
	strcpy(a_errmsg, "directory or file specification invalid");
	return DX__ERROR;
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	highlight_filer_current_file() reverse-videos DX_CURRENT_FILE's
**	entry on the FILER display and refreshes the "Pg N"/"Row N"
**	indicator on the directory-info line, unless the directory is
**	empty (DX_CURRENT_FILE pointing at the non-directory-file MARKER).
**	Non-directory entries and short-form directory entries are
**	rewritten in one smg$put_chars() call with SMG$M_REVERSE (plus
**	SMG$M_BOLD for directories); long-form directory entries are
**	rewritten in three pieces (state character, name, date/size/
**	protection) inside an smg$begin/end_display_update() bracket, with
**	the "[-]" parent-directory placeholder handled as a special case.
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
**	DX_CURRENT_FILE, DX_CURRENT_DIRECTORY (non_dir_filelist, form,
**	filer_display.view_rows), cntrl_info_block.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.dir_info_display and .filer_display (the
**	page/row indicator and the current file's entry are repainted).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	highlight_filer_current_file()
{
    int l_current_page;
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR(l_str_descrip, l_str);
    char l_page_str[81];
    $DESCRIPTOR(l_page_str_descrip, l_page_str);
    char l_row_str[81];
    $DESCRIPTOR(l_row_str_descrip, l_row_str);
    int l_row_beg_y;
    unsigned long l_rc;
 
    if (DX_CURRENT_FILE != DX_CURRENT_DIRECTORY.non_dir_filelist)
    {	    /**  Directory not empty  **/
	format_filer_entry(DX_CURRENT_FILE, l_str, cntrl_info_block.cur_win);
 
	l_current_page =
	  1+(DX_CURRENT_FILE->beg_y-1) /
	    DX_CURRENT_DIRECTORY.filer_display.view_rows;
	{
	    static $DESCRIPTOR (l_fao_ctrl, "Pg !SL");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;

	    l_out_desc.dsc$w_length  = sizeof(l_page_str) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_page_str;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, (unsigned long) l_current_page))))
	        $LOG(l_rc);
	    l_page_str[l_len] = EOS;
	}
	LENGTH(l_page_str_descrip) = strlen(l_page_str);

	{
	    static $DESCRIPTOR (l_fao_ctrl, "Row !SL");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;

	    l_out_desc.dsc$w_length  = sizeof(l_row_str) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_row_str;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, (unsigned long) DX_CURRENT_FILE->beg_y))))
	        $LOG(l_rc);
	    l_row_str[l_len] = EOS;
	}
	LENGTH(l_row_str_descrip) = strlen(l_row_str);
 
        if (!(1 & (l_rc = smg$begin_display_update (
                     &DX_CURRENT_DIRECTORY.dir_info_display.id))))
            $LOG(l_rc);
	if (!(1 & (l_rc = smg$draw_line (
		  &DX_CURRENT_DIRECTORY.dir_info_display.id,
		  &DX_CURRENT_DIRECTORY.dir_info_display.rows,
		  &1,
		  &DX_CURRENT_DIRECTORY.dir_info_display.rows,
		  &DX_CURRENT_DIRECTORY.dir_info_display.width,
		  0,
		  0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$put_chars (
		    &DX_CURRENT_DIRECTORY.dir_info_display.id,
		    &l_page_str_descrip,
		    &DX_CURRENT_DIRECTORY.dir_info_display.rows,
		    &2,
		    0,
		    &SMG$M_BOLD,
		    0,
		    0))))
	    $LOG(l_rc);
	l_row_beg_y = LENGTH(l_page_str_descrip)+4;
	if (!(1 & (l_rc = smg$put_chars (
		    &DX_CURRENT_DIRECTORY.dir_info_display.id,
		    &l_row_str_descrip,
		    &DX_CURRENT_DIRECTORY.dir_info_display.rows,
		    &l_row_beg_y,
		    0,
		    &SMG$M_BOLD,
		    0,
		    0))))
	    $LOG(l_rc);
        if (!(1 & (l_rc = smg$end_display_update (
                     &DX_CURRENT_DIRECTORY.dir_info_display.id))))
            $LOG(l_rc);
 
        if (DX_CURRENT_FILE->fn[ strlen(DX_CURRENT_FILE->fn)-1 ] != ']')
	{	/**  Current file is not a directory file  **/
	    LENGTH(l_str_descrip) = strlen(l_str);
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&DX_CURRENT_FILE->beg_y,
			&DX_CURRENT_FILE->beg_x,
			0,
			0,
			&SMG$M_REVERSE,
			0))))
	        $LOG(l_rc);
	}
	else if (DX_CURRENT_DIRECTORY.form == short_form)
        {	/**  Directory file with short form  **/
	    LENGTH(l_str_descrip) = strlen(l_str);
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&DX_CURRENT_FILE->beg_y,
			&DX_CURRENT_FILE->beg_x,
			0,
			&SMG$M_BOLD,
			&SMG$M_REVERSE,
			0))))
	        $LOG(l_rc);
	}
	else
	{	/**  Directory file with long form  **/
	    if (!(1 & (l_rc = smg$begin_display_update (
			 &DX_CURRENT_DIRECTORY.filer_display.id))))
	        $LOG(l_rc);
	    LENGTH(l_str_descrip) = 1;
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&DX_CURRENT_FILE->beg_y,
			&1,
			0,
			0,
			&SMG$M_REVERSE,
			0))))
	        $LOG(l_rc);
            if (!strcmp(l_str+1, "[-]"))
            {
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    $DESCR ("[-]"),
			    &DX_CURRENT_FILE->beg_y,
			    &2,
			    0,
			    &SMG$M_BOLD,
			    &SMG$M_REVERSE,
			    0))))
		    $LOG(l_rc);
            }
            else
            {
		int l_offset;
 
		POINTER(l_str_descrip) = l_str+1;
		LENGTH(l_str_descrip) = min (FILER_NAME_WIDTH, strlen(DX_CURRENT_FILE->fn));
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    &l_str_descrip,
			    &DX_CURRENT_FILE->beg_y,
			    &2,
			    0,
			    &SMG$M_BOLD,
			    &SMG$M_REVERSE,
			    0))))
		    $LOG(l_rc);
 
		l_offset = 1+LENGTH(l_str_descrip);
		POINTER(l_str_descrip) = l_str+l_offset;
		LENGTH(l_str_descrip) = strlen(l_str+l_offset);
		l_offset++;
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    &l_str_descrip,
			    &DX_CURRENT_FILE->beg_y,
			    &l_offset,
			    0,
			    0,
			    &SMG$M_REVERSE,
			    0))))
		    $LOG(l_rc);
            }
	    if (!(1 & (l_rc = smg$end_display_update (
			 &DX_CURRENT_DIRECTORY.filer_display.id))))
	        $LOG(l_rc);
        }
	if (!(1 & (l_rc = smg$set_cursor_abs (
		  &DX_CURRENT_DIRECTORY.filer_display.id,
		  &DX_CURRENT_FILE->beg_y,
		  &DX_CURRENT_FILE->beg_x))))
	    $LOG(l_rc);
    }
	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	unhighlight_filer_current_file() is the mirror image of
**	highlight_filer_current_file(): it rewrites DX_CURRENT_FILE's
**	entry in its normal (non-reverse-video) rendition -- plain for
**	non-directory entries, bold for short-form directory entries, and
**	the same three-piece write (with the "[-]" special case) for
**	long-form directory entries -- unless the directory is empty.
**	Unlike the highlight side, it does not touch the page/row
**	indicator.
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
**	DX_CURRENT_FILE, DX_CURRENT_DIRECTORY (non_dir_filelist, form),
**	cntrl_info_block.cur_win.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.filer_display (the current file's entry is
**	repainted without reverse video).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	unhighlight_filer_current_file()
{
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR(l_str_descrip, l_str);
    unsigned long l_rc;
 
    if (DX_CURRENT_FILE != DX_CURRENT_DIRECTORY.non_dir_filelist)
    {	    /**  Directory not empty  **/
	format_filer_entry(DX_CURRENT_FILE, l_str, cntrl_info_block.cur_win);
	LENGTH(l_str_descrip) = strlen(l_str);
        if (DX_CURRENT_FILE->fn[ strlen(DX_CURRENT_FILE->fn)-1 ] != ']')
	{	/**  Current file is not a directory file  **/
	    LENGTH(l_str_descrip) = strlen(l_str);
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&DX_CURRENT_FILE->beg_y,
			&DX_CURRENT_FILE->beg_x,
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
	}
	else if (DX_CURRENT_DIRECTORY.form == short_form)
        {	/**  Directory file with short form  **/
	    LENGTH(l_str_descrip) = strlen(l_str);
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&DX_CURRENT_FILE->beg_y,
			&DX_CURRENT_FILE->beg_x,
			0,
			&SMG$M_BOLD,
			0,
			0))))
	        $LOG(l_rc);
	}
	else
	{	/**  Directory file with long form  **/
	    if (!(1 & (l_rc = smg$begin_display_update (
			 &DX_CURRENT_DIRECTORY.filer_display.id))))
	        $LOG(l_rc);
	    LENGTH(l_str_descrip) = 1;
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&DX_CURRENT_FILE->beg_y,
			&1,
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
            if (!strcmp(l_str+1, "[-]"))
            {
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    $DESCR ("[-]"),
			    &DX_CURRENT_FILE->beg_y,
			    &2,
			    0,
			    &SMG$M_BOLD,
			    0,
			    0))))
		    $LOG(l_rc);
            }
            else
            {
		int l_offset;
 
		POINTER(l_str_descrip) = l_str+1;
		LENGTH(l_str_descrip) = min (FILER_NAME_WIDTH, strlen(DX_CURRENT_FILE->fn));
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    &l_str_descrip,
			    &DX_CURRENT_FILE->beg_y,
			    &2,
			    0,
			    &SMG$M_BOLD,
			    0,
			    0))))
		    $LOG(l_rc);
		l_offset = 1+LENGTH(l_str_descrip);
		POINTER(l_str_descrip) = l_str+l_offset;
		LENGTH(l_str_descrip) = strlen(l_str+l_offset);
		l_offset++;
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    &l_str_descrip,
			    &DX_CURRENT_FILE->beg_y,
			    &l_offset,
			    0,
			    0,
			    0,
			    0))))
		    $LOG(l_rc);
            }
	    if (!(1 & (l_rc = smg$end_display_update (
			 &DX_CURRENT_DIRECTORY.filer_display.id))))
	        $LOG(l_rc);
        }
	if (!(1 & (l_rc = smg$set_cursor_abs (
		  &cntrl_info_block.dir_dx[cntrl_info_block.cur_win].filer_display.id,
		  &DX_CURRENT_FILE->beg_y,
		  &DX_CURRENT_FILE->beg_x))))
	    $LOG(l_rc);
    }
	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      PUT_FILER_TITLE() outputs the header line
**
**--
**/
int	put_filer_title(unsigned short int a_win)
{
    char l_cwd_line[MAXFILESPEC+1];
    $DESCRIPTOR (l_cwd_line_descrip, l_cwd_line);
    char l_mod[10], l_dir[10];
    char l_namefld[MAXFILESPEC+1], l_dirfld[9], l_modfld[9];
    unsigned long l_rc;

    /**  Output cwd line  **/
    if (cntrl_info_block.dir_dx[a_win].filemode == single)
        strcpy(l_mod, "Single");
    else
        strcpy(l_mod, "Multiple");
    if (cntrl_info_block.dir_dx[a_win].direction == advance)
        strcpy(l_dir, "Forward");
    else
        strcpy(l_dir, "Backward");
    if (strlen(cntrl_info_block.dir_dx[a_win].cur_dir) > FILER_TITLE_DIR_WIDTH)
    {	    /**  Directory name truncated  **/
	dxscr_ljust(l_namefld, cntrl_info_block.dir_dx[a_win].cur_dir, FILER_TITLE_DIR_WIDTH-1);
	strcat(l_namefld, "~");
    }
    else
    {	    /**  Directory name fit  **/
	dxscr_ljust(l_namefld, cntrl_info_block.dir_dx[a_win].cur_dir, FILER_TITLE_DIR_WIDTH);
    }
    dxscr_ljust(l_dirfld, l_dir, 8);
    dxscr_ljust(l_modfld, l_mod, 8);
    {
	static $DESCRIPTOR (l_fao_ctrl, " !AZ | !AZ | !AZ ");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	l_out_desc.dsc$w_length  = sizeof(l_cwd_line) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_cwd_line;

	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_namefld, l_dirfld, l_modfld))))
	    $LOG(l_rc);
	l_cwd_line[l_len] = EOS;
    }
    LENGTH(l_cwd_line_descrip) = strlen(l_cwd_line);
    if (!(1 & (l_rc = smg$put_chars (
	      &cntrl_info_block.dir_dx[a_win].dir_info_display.id,
	      &l_cwd_line_descrip,
	      &FILER_DIRECT_ROW,
	      &FILER_DIRECT_COLUMN,
	      &SMG$M_ERASE_LINE,
	      &SMG$M_REVERSE,
	      0,
	      0))))
        $LOG(l_rc);
	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	put_filer_stat() paints the directory-statistics line for window
**	a_win: "total: <N>f/<N>b" and "selections: <N>f/<N>b" (file count/
**	block count), left- and right-justified on the same row with
**	enough spaces between them to fill FILER_RIGHTMOST, in bold.
**
**  INPUT PARAMETERS:
**
**	a_win  -  window number whose totals/selection counts to display.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.dir_dx[a_win] (tot_files/tot_blocks/sel_files/
**	sel_blocks), cntrl_info_block.dir_dx[a_win].dir_info_display
**	(implicitly, via FILER_STAT_ROW/FILER_STAT_COLUMN macros used in
**	the smg$put_chars() call below).
**
**  IMPLICIT OUTPUTS:
**
**	The directory-info display's statistics row is repainted.
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	put_filer_stat(unsigned short int a_win)
{
    char l_total_str[81], l_select_str[81];
    int l_spaces;
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR (l_str_descrip, l_str);
    unsigned long l_rc;
 
    /*
    **	Write statistics line
    **/
 
    /*  Put FILER statistics line  **/
    {
	static $DESCRIPTOR (l_fao_ctrl, "total: !SLf/!SLb");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	l_out_desc.dsc$w_length  = sizeof(l_total_str) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_total_str;

	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
				   cntrl_info_block.dir_dx[a_win].tot_files,
				   cntrl_info_block.dir_dx[a_win].tot_blocks))))
	    $LOG(l_rc);
	l_total_str[l_len] = EOS;
    }
    {
	static $DESCRIPTOR (l_fao_ctrl, "selections: !SLf/!SLb");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	l_out_desc.dsc$w_length  = sizeof(l_select_str) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_select_str;

	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
				   cntrl_info_block.dir_dx[a_win].sel_files,
				   cntrl_info_block.dir_dx[a_win].sel_blocks))))
	    $LOG(l_rc);
	l_select_str[l_len] = EOS;
    }
    l_spaces = max (1,
	FILER_RIGHTMOST-3-(strlen(l_total_str)+strlen(l_select_str)));

    l_str[0] = ' ';
    strcpy(l_str+1, l_total_str);
    memset (l_str+strlen(l_str), ' ', l_spaces);
    l_str[1+strlen(l_total_str)+l_spaces] = EOS;
    strcat(l_str, l_select_str);
 
    LENGTH(l_str_descrip) = strlen(l_str);
    if (!(1 & (l_rc = smg$put_chars (
		&cntrl_info_block.dir_dx[a_win].dir_info_display.id,
		&l_str_descrip,
		&FILER_STAT_ROW,
		&1,
	        &SMG$M_ERASE_TO_EOL,
		&SMG$M_BOLD,
		0,
		0))))
        $LOG(l_rc);
	return DX__NORMAL;
}
 
 

 
 

 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	find_full_path_name() expands the possibly-abbreviated display
**	name of a file/directory entry (a_fn -- what's actually shown in
**	the filer window, which may omit the device/directory when they
**	are implied by the current window) into a complete VMS file
**	specification, using the equivalence name recorded for the
**	window's directory.  Handles the "[-]" parent-directory
**	placeholder, directory entries (adding ".DIR;1"), and both
**	single- and multi-level subdirectory cases.
**
**  INPUT PARAMETERS:
**
**	a_fn   -  display name of the file or directory entry to expand.
**	a_win  -  window number whose directory equivalence name to use.
**
**  OUTPUT PARAMETERS:
**
**	a_buf  -  filled in with the fully-expanded file specification.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY (and cntrl_info_block.dir_dx[a_win] for the
**	non-directory, directory-not-included case) -- specifically
**	cur_filter.name_filespec_list->w_dids->equivalent_name.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	find_full_path_name(
		char *a_fn,
		char *a_buf,
		unsigned short int a_win)
{
    char *l_cp;
 
    if (!strcmp(a_fn, "[-]"))
    {	    /**  This is the parent directory placeholder  **/
	strcpy(a_buf, "[-]");
    }
    else if (a_fn[strlen(a_fn)-1] != ']')
    {	    /** This is a non-directory file  **/
        if (strchr(a_fn, ':') != NULL)
        {
            strcpy(a_buf, a_fn);
        }
        else if (a_fn[0] == '[')
	{	/**  Directory name included in the file name  **/
	    size_t l_len;
 
	    l_len = strcspn (
		DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list->w_dids->equivalent_name,
		"[");
            strncpy (a_buf,
		DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list->w_dids->equivalent_name,
		l_len);
	    a_buf[l_len] = EOS;
	    strcat (a_buf, a_fn);
	}
	else
        {	/**  Directory not included  **/
            strcpy(a_buf,
		cntrl_info_block.dir_dx[a_win].cur_filter.name_filespec_list->w_dids->equivalent_name);
	    strcat(a_buf, a_fn);
        }
    }
    else
    {	    /**  This is a directory file  **/
	/**  Get full directory name  **/
        if (strchr (a_fn, ':'))
        {	/**  Device name included in the file name  **/
            strcpy(a_buf, a_fn);
        }
        else if (*(strchr (a_fn, '[')+1) != '.')
	{	/**  More than one sub-directory  **/
	    size_t l_len;
 
	    l_len = strcspn (
		DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list->w_dids->equivalent_name,
		"[");
	    strncpy (
		a_buf,
		DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list->w_dids->equivalent_name,
		l_len);
	    a_buf[l_len] = EOS;
	    strcat(a_buf, a_fn);
	}
	else
	{	/**  Only one sub-directory  **/
	    strcpy (a_buf, DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list->w_dids->equivalent_name);
	    a_buf[ strlen(a_buf)-1 ] = EOS;	/**  Delete ']'  **/
	    strcat (a_buf, a_fn+1);
	}
 
	/**  Delete ']'  **/
	a_buf[strlen(a_buf)-1] = EOS;
 
	l_cp = strrchr(a_buf, '.');
	*l_cp = ']';	/**  Change the last '.' to ']'  **/
	strcat(a_buf, ".DIR;1");
    }
	return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	convert_date_string() converts a_date_string into a 64-bit VMS
**	binary date/time.  Three symbolic strings are handled specially
**	("TODAY"/"YESTERDAY"/"TOMORROW", each resolved against the current
**	date/time with the time-of-day fields zeroed out and correct
**	month/year rollover, including leap years); anything else is
**	parsed as a literal VMS date/time string via sys$bintim().
**
**  INPUT PARAMETERS:
**
**	a_date_string  -  "TODAY", "YESTERDAY", "TOMORROW", or a literal
**			  VMS date/time string.
**
**  OUTPUT PARAMETERS:
**
**	a_date_time  -  filled in with the resulting 64-bit binary
**			date/time.
**
**  IMPLICIT INPUTS:
**
**	None (reads the current date/time via sys$numtim()).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	convert_date_string(a_date_string, a_date_time)
char *a_date_string;
DATE_TIME *a_date_time;
{
    $DESCRIPTOR (l_date_string_descrip, a_date_string);
    int l_days_of_month[12] =
	{
	    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};
    struct
	{
	    unsigned short
		year, month, day, hour, minute, second, hundredth_of_second;
	} l_timbuf;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = sys$numtim (
                 (unsigned short *) &l_timbuf,
                 0))))
        $LOG(l_rc);
 
    if (l_timbuf.year % 400 == 0 ||
	(l_timbuf.year % 4 == 0 && l_timbuf.year % 100 != 0))
	l_days_of_month[1] = 29;
 
    if (!strcmp (a_date_string, "TODAY"))
    {
	l_timbuf.hour = l_timbuf.minute =
	    l_timbuf.second = l_timbuf.hundredth_of_second = 0;
    }
    else if (!strcmp (a_date_string, "YESTERDAY"))
    {
	l_timbuf.hour = l_timbuf.minute
	    = l_timbuf.second = l_timbuf.hundredth_of_second = 0;
        l_timbuf.day--;
        if (l_timbuf.day < 1)
        {
            l_timbuf.month--;
            if (l_timbuf.month > 0)
                l_timbuf.day = l_days_of_month[l_timbuf.month-1];
            else
            {
                l_timbuf.year--;
		l_timbuf.month = 12;
		l_timbuf.day = 31;
            }
        }
    }
    else if (!strcmp (a_date_string, "TOMORROW"))
    {
	l_timbuf.hour = l_timbuf.minute
	    = l_timbuf.second = l_timbuf.hundredth_of_second = 0;
        l_timbuf.day++;
        if (l_timbuf.day > l_days_of_month[l_timbuf.month-1] )
        {
            l_timbuf.day = 1;
	    l_timbuf.month++;
            if (l_timbuf.month > 12)
            {
                l_timbuf.month = 1;
		l_timbuf.year++;
            }
        }
    }
    else
    {
	LENGTH(l_date_string_descrip) = strlen(a_date_string);
        if (!(1 & (l_rc = sys$bintim (
                     &l_date_string_descrip,
                     (struct _generic_64 *) a_date_time))))
            $LOG(l_rc);
        if (!(1 & (l_rc = sys$numtim (
                     (unsigned short *) &l_timbuf,
                     (struct _generic_64 *) a_date_time))))
            $LOG(l_rc);
    }
 
    if (!(1 & (l_rc = lib$cvt_vectim (
                 (unsigned short *) &l_timbuf,
                 a_date_time))))
        $LOG(l_rc);
	return DX__NORMAL;
}



/* ---- (from FILERWIN.C -- window split/delete/one/next/prev/exit) ---- */

static struct fil_dx_tag *find_current_file(unsigned short int);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_split_win() creates a new window just after the current one
**	(refusing if MAXWINDOWS is already reached or the terminal isn't
**	tall enough): it shrinks every existing window's dir_info_display/
**	filer_display to make room, shifts the dir_dx[] array right to
**	open a slot, then builds the new window by copying the previous
**	window's filter/filelist state (copy_filespec_list()/
**	copy_filelist()), creating its virtual displays, and positioning
**	its current-file pointer at the same logical file the previous
**	window was on (find_current_file()).  The new window becomes the
**	current window.
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
**	cntrl_info_block (windows, cur_win, dir_dx[], pasteboard_rows/
**	width).
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.windows/cur_win/dir_dx[] (a new window is added
**	and made current); every window's virtual displays are resized/
**	repainted.
**
**  RETURNS VALUE:
**
**	DX__ERROR if there is no room for another window (MAXWINDOWS
**	reached, or the terminal isn't tall enough); DX__NORMAL otherwise.
**
**--
**/
int	filer_split_win()
{
    int l_total_rows,
	l_prev_win;
    unsigned short int l_windows;
    char l_errmsg[255];
    int l_i;
    int l_current_page;
    char l_row_str[81];
    $DESCRIPTOR(l_row_str_descrip, l_row_str);
    char l_page_str[81];
    $DESCRIPTOR(l_page_str_descrip, l_page_str);
    int l_row_beg_y;
    void (*l_cstat)(int);
    unsigned long l_rc;

    l_windows = cntrl_info_block.windows;
    if (l_windows >= MAXWINDOWS)
    {
	signal_err("Maximum number of windows exceeded", bell);
	return DX__ERROR;
    }
    else if (cntrl_info_block.pasteboard_rows <
	    CMDS_RSV_LNS+(l_windows+1)*(DIRINFO_RSV_LNS+4))
    {
	{
	    static $DESCRIPTOR (l_fao_ctrl, "Terminal page length too small for !UL windows");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;

	    l_out_desc.dsc$w_length  = sizeof(l_errmsg) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_errmsg;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, (unsigned long) (l_windows+1)))))
	        $LOG(l_rc);
	    l_errmsg[l_len] = EOS;
	}
        signal_err(l_errmsg, bell);
	return DX__ERROR;
    }

    for (l_i = cntrl_info_block.windows;  l_i >= 0;  l_i--)
    {
	if (l_i >= cntrl_info_block.cur_win+1)
	    cntrl_info_block.dir_dx[l_i] = cntrl_info_block.dir_dx[l_i-1];
    }

    /*
    **  Shrink the existing displays by reducing ROWS and BEG_Y fields
    **	in DIR_INFO_DISPLAY and FILER_DISPLAY, then do re-pasting.
    **/

    l_total_rows = 0;
    for (l_i = 0; l_i <= cntrl_info_block.windows;  l_i++)
    {
	cntrl_info_block.dir_dx[l_i].dir_info_display.rows =
	    (cntrl_info_block.pasteboard_rows - CMDS_RSV_LNS - l_total_rows) /
		(l_windows + 1 - l_i);
	cntrl_info_block.dir_dx[l_i].dir_info_display.beg_y =
	    CMDS_RSV_LNS + l_total_rows;
	l_total_rows += cntrl_info_block.dir_dx[l_i].dir_info_display.rows;
	cntrl_info_block.dir_dx[l_i].filer_display.view_rows =
	    cntrl_info_block.dir_dx[l_i].dir_info_display.rows - DIRINFO_RSV_LNS;
	cntrl_info_block.dir_dx[l_i].filer_display.beg_y =
	    2 + cntrl_info_block.dir_dx[l_i].dir_info_display.beg_y;

	if (l_i != cntrl_info_block.cur_win+1)
	{
	    /**  Update dir_info_display  **/
	    if (!(1 & (l_rc = smg$change_virtual_display (
			 &cntrl_info_block.dir_dx[l_i].dir_info_display.id,
			 &cntrl_info_block.dir_dx[l_i].dir_info_display.rows,
			 0,
			 0,
			 0,
			 0))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$move_virtual_display (
			 &cntrl_info_block.dir_dx[l_i].dir_info_display.id,
			 &cntrl_info_block.pasteboard_id,
			 &cntrl_info_block.dir_dx[l_i].dir_info_display.beg_y,
			 &cntrl_info_block.dir_dx[l_i].dir_info_display.beg_x,
			 0))))
	        $LOG(l_rc);

	    put_filer_title(l_i);
	    put_filer_stat(l_i);

	    l_current_page =
	      1+(cntrl_info_block.dir_dx[l_i].cur_file->beg_y-1) /
		cntrl_info_block.dir_dx[l_i].filer_display.view_rows;
	    {
		static $DESCRIPTOR (l_fao_ctrl, "Pg !SL");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_len;

		l_out_desc.dsc$w_length  = sizeof(l_page_str) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_page_str;

		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, (unsigned long) l_current_page))))
		    $LOG(l_rc);
		l_page_str[l_len] = EOS;
	    }
	    LENGTH(l_page_str_descrip) = strlen(l_page_str);

	    {
		static $DESCRIPTOR (l_fao_ctrl, "Row !SL");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_len;

		l_out_desc.dsc$w_length  = sizeof(l_row_str) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_row_str;

		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
					   (unsigned long) cntrl_info_block.dir_dx[l_i].cur_file->beg_y))))
		    $LOG(l_rc);
		l_row_str[l_len] = EOS;
	    }
	    LENGTH(l_row_str_descrip) = strlen(l_row_str);

	    if (!(1 & (l_rc = smg$begin_display_update (
			 &cntrl_info_block.dir_dx[l_i].dir_info_display.id))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$draw_line (
		      &cntrl_info_block.dir_dx[l_i].dir_info_display.id,
		      &cntrl_info_block.dir_dx[l_i].dir_info_display.rows,
		      &1,
		      &cntrl_info_block.dir_dx[l_i].dir_info_display.rows,
		      &cntrl_info_block.dir_dx[l_i].dir_info_display.width,
		      0,
		      0))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$put_chars (
			&cntrl_info_block.dir_dx[l_i].dir_info_display.id,
			&l_page_str_descrip,
			&cntrl_info_block.dir_dx[l_i].dir_info_display.rows,
			&2,
			0,
			&SMG$M_BOLD,
			0,
			0))))
	        $LOG(l_rc);
	    l_row_beg_y = LENGTH(l_page_str_descrip)+4;
	    if (!(1 & (l_rc = smg$put_chars (
			&cntrl_info_block.dir_dx[l_i].dir_info_display.id,
			&l_row_str_descrip,
			&cntrl_info_block.dir_dx[l_i].dir_info_display.rows,
			&l_row_beg_y,
			0,
			&SMG$M_BOLD,
			0,
			0))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$end_display_update (
			 &cntrl_info_block.dir_dx[l_i].dir_info_display.id))))
	        $LOG(l_rc);

	    /**  Update filer_display  **/
	    cntrl_info_block.dir_dx[l_i].filer_display.rows =
		max (cntrl_info_block.dir_dx[l_i].non_dir_filelist->backward->beg_y,
		     cntrl_info_block.dir_dx[l_i].filer_display.view_rows);
	    if (!(1 & (l_rc = smg$move_virtual_display (
			 &cntrl_info_block.dir_dx[l_i].filer_display.id,
			 &cntrl_info_block.pasteboard_id,
			 &cntrl_info_block.dir_dx[l_i].filer_display.beg_y,
			 &cntrl_info_block.dir_dx[l_i].filer_display.beg_x,
			 0))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$change_virtual_display (
			 &cntrl_info_block.dir_dx[l_i].filer_display.id,
			 &cntrl_info_block.dir_dx[l_i].filer_display.rows,
			 0,
			 0,
			 0,
			 0))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$change_viewport (
			 &cntrl_info_block.dir_dx[l_i].filer_display.id,
			 0,
			 0,
			 &cntrl_info_block.dir_dx[l_i].filer_display.view_rows,
			 0))))
	        $LOG(l_rc);
	    change_filer_viewport(l_i);
	}
    }

    highlight_filer_current_file();
    unhighlight_filer_current_file();

    /**  Assign the new display attributes  **/

    cntrl_info_block.windows++;
    l_prev_win = cntrl_info_block.cur_win;
    cntrl_info_block.cur_win++;

    /*
    **	New window will inherite most of the attributes from the original
    **	window.  Different attributes are modified follows.
    **/

    strcpy(DX_CURRENT_DIRECTORY.search_pattern, "*");

    /*
    **	Modify DIR_INFO virtual displays
    **/

    if (!(1 & (l_rc = smg$create_virtual_display (
                 &DX_CURRENT_DIRECTORY.dir_info_display.rows,
                 &cntrl_info_block.pasteboard_width,
                 &DX_CURRENT_DIRECTORY.dir_info_display.id,
                 &SMG$M_TRUNC_ICON,
                 0,
                 0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$paste_virtual_display (
                 &DX_CURRENT_DIRECTORY.dir_info_display.id,
                 &cntrl_info_block.pasteboard_id,
                 &DX_CURRENT_DIRECTORY.dir_info_display.beg_y,
                 &1,
                 0))))
        $LOG(l_rc);

    put_filer_title(cntrl_info_block.cur_win);
    put_filer_stat(cntrl_info_block.cur_win);
    if (!(1 & (l_rc = smg$draw_line (
	      &DX_CURRENT_DIRECTORY.dir_info_display.id,
	      &DX_CURRENT_DIRECTORY.dir_info_display.rows,
	      &1,
	      &DX_CURRENT_DIRECTORY.dir_info_display.rows,
	      &DX_CURRENT_DIRECTORY.dir_info_display.width,
	      0,
	      0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$set_cursor_abs (
                 &cntrl_info_block.commands_display.id,
                 &cntrl_info_block.commands_display.rows,
                 &cntrl_info_block.commands_display.width))))
        $LOG(l_rc);


    /*
    **	Output 'working...' message if total processing time
    **	is longer than 3 seconds.  Needs to disable broadcast message
    **	first because of interference.
    **/

#ifndef DEBUG
    if (!(1 & (l_rc = smg$disable_broadcast_trapping (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);

    if (!(1 & (l_rc = dxscr_working_start(cntrl_info_block.commands_display.id, 2, 0, cntrl_info_block.commands_display.id, &l_cstat))))
	$LOG(l_rc);
#endif

    /*
    **	Copy to new filespec lists -
    **	    NAME_FILESPEC_LIST, EXCLUDE_FILESPEC_LIST
    **/

    copy_filespec_list(
	cntrl_info_block.dir_dx[l_prev_win].cur_filter.name_filespec_list,
	&DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list);
    copy_filespec_list(
	cntrl_info_block.dir_dx[l_prev_win].cur_filter.exclude_filespec_list,
	&DX_CURRENT_DIRECTORY.cur_filter.exclude_filespec_list);

    /*
    **	Copy to new filelists -
    **	    NON_DIR_FILELIST, DIR_FILELIST, EXCLUDE_FILELIST
    **/

    copy_filelist(
	cntrl_info_block.dir_dx[l_prev_win].non_dir_filelist,
	&DX_CURRENT_DIRECTORY.non_dir_filelist,
	DX_CURRENT_DIRECTORY.sortby,
	DX_CURRENT_DIRECTORY.sortorder);

    copy_filelist(
	cntrl_info_block.dir_dx[l_prev_win].dir_filelist,
	&DX_CURRENT_DIRECTORY.dir_filelist,
	DX_CURRENT_DIRECTORY.sortby,
	DX_CURRENT_DIRECTORY.sortorder);

    copy_filelist(
	cntrl_info_block.dir_dx[l_prev_win].exclude_filelist,
	&DX_CURRENT_DIRECTORY.exclude_filelist,
	DX_CURRENT_DIRECTORY.sortby,
	DX_CURRENT_DIRECTORY.sortorder);

    /*
    **	Modify FILER virtual displays
    **/

    DX_CURRENT_DIRECTORY.filer_display.rows =
	max (DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y,
	     DX_CURRENT_DIRECTORY.filer_display.view_rows);

    if (!(1 & (l_rc = smg$create_virtual_display (
                 &DX_CURRENT_DIRECTORY.filer_display.rows,
                 &DX_CURRENT_DIRECTORY.filer_display.width,
                 &DX_CURRENT_DIRECTORY.filer_display.id,
                 0,
                 0,
                 0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$create_viewport (
		&DX_CURRENT_DIRECTORY.filer_display.id,
		&DX_CURRENT_DIRECTORY.filer_display.view_beg_y,
		&DX_CURRENT_DIRECTORY.filer_display.view_beg_x,
		&DX_CURRENT_DIRECTORY.filer_display.view_rows,
		&DX_CURRENT_DIRECTORY.filer_display.view_width))))
        $LOG(l_rc);

    /*
    **	Find current file pointer in the new window
    **/

    DX_CURRENT_FILE = find_current_file(l_prev_win);

    /*
    **	Write the new window
    **/

    write_to_filer(DX_CURRENT_DIRECTORY.dir_filelist);
    write_to_filer(DX_CURRENT_DIRECTORY.non_dir_filelist);
    change_filer_viewport(cntrl_info_block.cur_win);
    highlight_filer_current_file();

    if (!(1 & (l_rc = smg$paste_virtual_display (
		&DX_CURRENT_DIRECTORY.filer_display.id,
		&cntrl_info_block.pasteboard_id,
		&DX_CURRENT_DIRECTORY.filer_display.beg_y,
		&DX_CURRENT_DIRECTORY.filer_display.beg_x,
		0))))
        $LOG(l_rc);

    /*
    **	Set the signal back to system default
    **/

#ifndef DEBUG
    alarm(0);
    signal(SIGALRM, l_cstat);
    if (!(1 & (l_rc = smg$erase_chars (
    		 &cntrl_info_block.commands_display.id,
    		 &10,
    		 &2,
    		 &1))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$set_broadcast_trapping (
                 &cntrl_info_block.pasteboard_id,
                 broadcast_routine,
                 0))))
        $LOG(l_rc);
#endif

    return DX__NORMAL;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	find_current_file() locates, in the just-created new window's
**	freshly-copied dir_filelist/non_dir_filelist, the entry that
**	corresponds to a_prev_win's cur_file: it walks both windows' lists
**	in lockstep (they are structurally identical copies, made by
**	filer_split_win() via copy_filelist()) until it reaches the
**	position matching a_prev_win's cur_file, then returns the
**	new window's node at that same position.
**
**  INPUT PARAMETERS:
**
**	a_prev_win  -  window number the new window was split from.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.dir_dx[a_prev_win] (dir_filelist/non_dir_filelist/
**	cur_file), DX_CURRENT_DIRECTORY (the new window's freshly-copied
**	lists).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	Pointer to the new window's entry corresponding to a_prev_win's
**	current file; if no match is found (should not normally happen,
**	since the lists are copies of each other), returns
**	DX_CURRENT_DIRECTORY.non_dir_filelist (the "no current file"
**	sentinel used throughout this module).
**
**--
**/
static struct fil_dx_tag *find_current_file(
	unsigned short int a_prev_win)
{
    struct fil_dx_tag *l_p, *l_q;

    for (l_p = cntrl_info_block.dir_dx[a_prev_win].dir_filelist->forward,
	    l_q = DX_CURRENT_DIRECTORY.dir_filelist->forward;
	 l_p != cntrl_info_block.dir_dx[a_prev_win].non_dir_filelist;
	 l_p = l_p->forward, l_q = l_q->forward)
    {
        if (l_p == cntrl_info_block.dir_dx[a_prev_win].cur_file)
        {
            return l_q;
        }
	else if (l_p == cntrl_info_block.dir_dx[a_prev_win].dir_filelist)
	{
	    l_p = cntrl_info_block.dir_dx[a_prev_win].non_dir_filelist;
	    l_q = DX_CURRENT_DIRECTORY.non_dir_filelist;
	}
    }

    /**  Not found -- fall back to the "no current file" sentinel used
    **   throughout this module (see highlight/unhighlight_filer_
    **   current_file()) instead of returning a bogus (struct*)-1.  **/
    return DX_CURRENT_DIRECTORY.non_dir_filelist;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_delete_win() closes the current window (refusing if it's the
**	only one): it saves the window's state to free later, compacts the
**	dir_dx[] array by shifting every later window down one slot,
**	recomputes and applies each remaining window's new
**	dir_info_display/filer_display size and position (growing them to
**	fill the space freed up, scrolling in blank lines and adjusting
**	each window's viewport as needed), repositions each window's
**	cursor via change_filer_viewport(), and finally frees the closed
**	window's filter/filelist/display resources.
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
**	cntrl_info_block (windows, cur_win, dir_dx[], pasteboard_rows).
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.windows/cur_win/dir_dx[] (the current window is
**	removed and the array compacted); the remaining windows' virtual
**	displays are resized/repainted.
**
**  RETURNS VALUE:
**
**	DX__ERROR if this is the only window (nothing is closed);
**	DX__NORMAL otherwise.
**
**--
**/
int	filer_delete_win()
{
    int l_i, l_w;
    int l_num_of_scrolls;
    struct dir_dx_tag l_prev_dir_dx;
    int l_total_rows;
    unsigned long l_rc;

    if (cntrl_info_block.windows == 1)
    {
        signal_err("There is only one window", bell);
	return DX__ERROR;
    }

    l_prev_dir_dx = DX_CURRENT_DIRECTORY;

    for (l_w = cntrl_info_block.cur_win;  l_w < cntrl_info_block.windows-1;  l_w++)
    {
    	cntrl_info_block.dir_dx[l_w] = cntrl_info_block.dir_dx[l_w+1];
    }

    if (cntrl_info_block.cur_win == cntrl_info_block.windows-1)
    	cntrl_info_block.cur_win--;
    cntrl_info_block.windows--;

    l_total_rows = 0;
    for (l_w = 0; l_w < cntrl_info_block.windows; l_w++)
    {
	int l_display_rows;

	l_display_rows =
	 (cntrl_info_block.pasteboard_rows-CMDS_RSV_LNS-l_total_rows)/
	    (cntrl_info_block.windows-l_w);
	cntrl_info_block.dir_dx[l_w].dir_info_display.beg_y =
	    CMDS_RSV_LNS + l_total_rows;
	l_num_of_scrolls =
	    l_display_rows - cntrl_info_block.dir_dx[l_w].dir_info_display.rows;
	cntrl_info_block.dir_dx[l_w].dir_info_display.rows = l_display_rows;
	l_total_rows += cntrl_info_block.dir_dx[l_w].dir_info_display.rows;

	cntrl_info_block.dir_dx[l_w].filer_display.beg_y =
	    2 + cntrl_info_block.dir_dx[l_w].dir_info_display.beg_y;

	if (!(1 & (l_rc = smg$begin_pasteboard_update (
		     &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$move_virtual_display (
		     &cntrl_info_block.dir_dx[l_w].dir_info_display.id,
		     &cntrl_info_block.pasteboard_id,
		     &cntrl_info_block.dir_dx[l_w].dir_info_display.beg_y,
		     &cntrl_info_block.dir_dx[l_w].dir_info_display.beg_x,
		     0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$change_virtual_display (
		     &cntrl_info_block.dir_dx[l_w].dir_info_display.id,
		     &cntrl_info_block.dir_dx[l_w].dir_info_display.rows,
		     0,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$move_virtual_display (
		     &cntrl_info_block.dir_dx[l_w].filer_display.id,
		     &cntrl_info_block.pasteboard_id,
		     &cntrl_info_block.dir_dx[l_w].filer_display.beg_y,
		     &cntrl_info_block.dir_dx[l_w].filer_display.beg_x,
		     0))))
	    $LOG(l_rc);

	/**  Adjust filer_display size  **/
	if (cntrl_info_block.dir_dx[l_w].filer_display.rows <
	    cntrl_info_block.dir_dx[l_w].dir_info_display.rows - 3)
	{
	    cntrl_info_block.dir_dx[l_w].filer_display.rows =
		cntrl_info_block.dir_dx[l_w].dir_info_display.rows - 3;
	    if (!(1 & (l_rc = smg$change_virtual_display (
			 &cntrl_info_block.dir_dx[l_w].filer_display.id,
			 &cntrl_info_block.dir_dx[l_w].filer_display.rows,
			 0,
			 0,
			 0,
			 0))))
	        $LOG(l_rc);
	}

	for (l_i = 0; l_i < l_num_of_scrolls; l_i++)
	{
	    if (!(1 & (l_rc = smg$insert_line (
			 &cntrl_info_block.dir_dx[l_w].dir_info_display.id,
			 &3,
			 $DESCR (" "),
			 &SMG$M_DOWN,
			 0,
			 0,
			 0,
			 0))))
	        $LOG(l_rc);

	    /**  Adjust filer_display  **/
	    cntrl_info_block.dir_dx[l_w].filer_display.view_rows++;
	    if (cntrl_info_block.dir_dx[l_w].filer_display.view_beg_y > 1 &&
		cntrl_info_block.dir_dx[l_w].filer_display.view_beg_y +
		    cntrl_info_block.dir_dx[l_w].filer_display.view_rows - 1 >
		    cntrl_info_block.dir_dx[l_w].non_dir_filelist->backward->beg_y)
		cntrl_info_block.dir_dx[l_w].filer_display.view_beg_y--;
	    if (!(1 & (l_rc = smg$change_viewport (
			 &cntrl_info_block.dir_dx[l_w].filer_display.id,
			 &cntrl_info_block.dir_dx[l_w].filer_display.view_beg_y,
			 0,
			 &cntrl_info_block.dir_dx[l_w].filer_display.view_rows,
			 0))))
	        $LOG(l_rc);
	}
	if (!(1 & (l_rc = smg$end_pasteboard_update (
		     &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);

	/**  Reposition the cursor in the current display  **/
	change_filer_viewport(l_w);
    }

    /*
    **	Free resoueces used by the window deleted
    **/

    free_filespec_list(
	&l_prev_dir_dx.cur_filter.name_filespec_list);
    free_filespec_list(
	&l_prev_dir_dx.cur_filter.exclude_filespec_list);

    free_filelist(
	&l_prev_dir_dx.non_dir_filelist);
    free_filelist(
	&l_prev_dir_dx.dir_filelist);
    free_filelist(
	&l_prev_dir_dx.exclude_filelist);
    if (!(1 & (l_rc = smg$delete_virtual_display (
		&l_prev_dir_dx.dir_info_display.id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$delete_virtual_display (
		&l_prev_dir_dx.filer_display.id))))
        $LOG(l_rc);

    return DX__NORMAL;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_one_win() expands the current window to fill the whole
**	pasteboard, closing every other window (refusing if this is
**	already the only window): it repastes the current window's
**	displays on top, then grows them one line at a time -- scrolling
**	upward to absorb the space used by windows above the current one,
**	then scrolling downward to absorb the space used by windows below
**	it -- adjusting dir_info_display/filer_display size and viewport
**	at each step, frees every other window's filter/filelist/display
**	resources, and finally collapses cntrl_info_block down to a single
**	window (windows=1, cur_win=0, dir_dx[0] = the surviving window's
**	state).
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
**	cntrl_info_block (windows, cur_win, dir_dx[]).
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.windows/cur_win/dir_dx[] (collapsed to one
**	window); every other window's resources are freed and its
**	displays deleted; the surviving window's displays are resized to
**	fill the pasteboard.
**
**  RETURNS VALUE:
**
**	DX__ERROR if this is already the only window (nothing changes);
**	DX__NORMAL otherwise.
**
**--
**/
int	filer_one_win()
{
    int l_i;
    int l_num_of_scrolls;
    unsigned long l_rc;

    if (cntrl_info_block.windows == 1)
    {
        signal_err("There is only one window", bell);
	return DX__ERROR;
    }

    /**  Make the current displays the topmost displays  **/
    if (!(1 & (l_rc = smg$repaste_virtual_display (
	     	  &DX_CURRENT_DIRECTORY.filer_display.id,
	     	  &cntrl_info_block.pasteboard_id,
	     	  &DX_CURRENT_DIRECTORY.filer_display.beg_y,
	     	  &DX_CURRENT_DIRECTORY.filer_display.beg_x,
	     	  0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$repaste_virtual_display (
	     	  &DX_CURRENT_DIRECTORY.dir_info_display.id,
	     	  &cntrl_info_block.pasteboard_id,
	     	  &DX_CURRENT_DIRECTORY.dir_info_display.beg_y,
	     	  &DX_CURRENT_DIRECTORY.dir_info_display.beg_x,
	     	  &DX_CURRENT_DIRECTORY.filer_display.id))))
        $LOG(l_rc);

    /**  Scroll upward  **/
    l_num_of_scrolls = 0;
    for (l_i = 0;  l_i < cntrl_info_block.cur_win;  l_i++)
    {
    	l_num_of_scrolls += cntrl_info_block.dir_dx[l_i].dir_info_display.rows;
    }
    for (l_i = 0; l_i < l_num_of_scrolls; l_i++)
    {
	if (!(1 & (l_rc = smg$begin_pasteboard_update (
		     &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);

	/**  Adjust dir_info_display  **/
	DX_CURRENT_DIRECTORY.dir_info_display.beg_y--;
	if (!(1 & (l_rc = smg$move_virtual_display (
		     &DX_CURRENT_DIRECTORY.dir_info_display.id,
		     &cntrl_info_block.pasteboard_id,
		     &DX_CURRENT_DIRECTORY.dir_info_display.beg_y,
		     &DX_CURRENT_DIRECTORY.dir_info_display.beg_x,
		     0))))
	    $LOG(l_rc);
	DX_CURRENT_DIRECTORY.dir_info_display.rows++;
	if (!(1 & (l_rc = smg$change_virtual_display (
		     &DX_CURRENT_DIRECTORY.dir_info_display.id,
		     &DX_CURRENT_DIRECTORY.dir_info_display.rows,
		     0,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$insert_line (
		     &DX_CURRENT_DIRECTORY.dir_info_display.id,
		     &3,
		     $DESCR (" "),
		     &SMG$M_DOWN,
		     0,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);

	/**  Adjust filer_display  **/
	DX_CURRENT_DIRECTORY.filer_display.beg_y--;
	if (!(1 & (l_rc = smg$move_virtual_display (
		     &DX_CURRENT_DIRECTORY.filer_display.id,
		     &cntrl_info_block.pasteboard_id,
		     &DX_CURRENT_DIRECTORY.filer_display.beg_y,
		     &DX_CURRENT_DIRECTORY.filer_display.beg_x,
		     0))))
	    $LOG(l_rc);
	if (DX_CURRENT_DIRECTORY.filer_display.view_beg_y > 1)
	    DX_CURRENT_DIRECTORY.filer_display.view_beg_y--;
	DX_CURRENT_DIRECTORY.filer_display.view_rows++;
	if (DX_CURRENT_DIRECTORY.filer_display.rows <
		DX_CURRENT_DIRECTORY.filer_display.view_rows)
	{	/**  Increase the size of filer_display  **/
	    DX_CURRENT_DIRECTORY.filer_display.rows =
		DX_CURRENT_DIRECTORY.filer_display.view_rows;
	    if (!(1 & (l_rc = smg$change_virtual_display (
		     	   &DX_CURRENT_DIRECTORY.filer_display.id,
		     	   &DX_CURRENT_DIRECTORY.filer_display.rows,
		     	   0,
		     	   0,
		     	   0,
		     	   0))))
	        $LOG(l_rc);
	}
	if (!(1 & (l_rc = smg$change_viewport (
		     &DX_CURRENT_DIRECTORY.filer_display.id,
		     &DX_CURRENT_DIRECTORY.filer_display.view_beg_y,
		     0,
		     &DX_CURRENT_DIRECTORY.filer_display.view_rows,
		     0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$end_pasteboard_update (
		     &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);
    }

    /**  Scroll downward  **/
    l_num_of_scrolls = 0;
    for (l_i = cntrl_info_block.cur_win+1;  l_i < cntrl_info_block.windows;  l_i++)
    {
    	l_num_of_scrolls += cntrl_info_block.dir_dx[l_i].dir_info_display.rows;
    }
    for (l_i = 0;  l_i < l_num_of_scrolls;  l_i++)
    {
	if (!(1 & (l_rc = smg$begin_pasteboard_update (
	      &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);

	/**  Adjust dir_info_display  **/
	DX_CURRENT_DIRECTORY.dir_info_display.rows++;
	if (!(1 & (l_rc = smg$change_virtual_display (
		     &DX_CURRENT_DIRECTORY.dir_info_display.id,
		     &DX_CURRENT_DIRECTORY.dir_info_display.rows,
		     0,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$insert_line (
		     &DX_CURRENT_DIRECTORY.dir_info_display.id,
		     &3,
		     $DESCR (" "),
		     &SMG$M_DOWN,
		     0,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);

	/**  Adjust filer_display  **/
	DX_CURRENT_DIRECTORY.filer_display.view_rows++;
	if (DX_CURRENT_DIRECTORY.filer_display.rows <
		DX_CURRENT_DIRECTORY.filer_display.view_rows)
	{	/**  Increase the size of filer_display  **/
	    DX_CURRENT_DIRECTORY.filer_display.rows =
		DX_CURRENT_DIRECTORY.filer_display.view_rows;
	    if (!(1 & (l_rc = smg$change_virtual_display (
		     	   &DX_CURRENT_DIRECTORY.filer_display.id,
		     	   &DX_CURRENT_DIRECTORY.filer_display.rows,
		     	   0,
		     	   0,
		     	   0,
		     	   0))))
	        $LOG(l_rc);
	}
	if (DX_CURRENT_DIRECTORY.filer_display.view_beg_y > 1 &&
	    DX_CURRENT_DIRECTORY.filer_display.view_beg_y +
		DX_CURRENT_DIRECTORY.filer_display.view_rows - 1 >
		DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y)
	    DX_CURRENT_DIRECTORY.filer_display.view_beg_y--;
	if (!(1 & (l_rc = smg$change_viewport (
		     &DX_CURRENT_DIRECTORY.filer_display.id,
		     &DX_CURRENT_DIRECTORY.filer_display.view_beg_y,
		     0,
		     &DX_CURRENT_DIRECTORY.filer_display.view_rows,
		     0))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$end_pasteboard_update (
	      &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);
    }

    /**	Reposition the cursor in the current display  **/
    change_filer_viewport(cntrl_info_block.cur_win);

    /*
    **	Free resoueces used by the other window
    **/

    for (l_i = 0;  l_i < cntrl_info_block.windows;  l_i++)
    {
    	if (l_i != cntrl_info_block.cur_win)
	{
	    free_filespec_list(
		&cntrl_info_block.dir_dx[l_i].cur_filter.name_filespec_list);
	    free_filespec_list(
		&cntrl_info_block.dir_dx[l_i].cur_filter.exclude_filespec_list);

	    free_filelist(
		&cntrl_info_block.dir_dx[l_i].non_dir_filelist);
	    free_filelist(
		&cntrl_info_block.dir_dx[l_i].dir_filelist);
	    free_filelist(
		&cntrl_info_block.dir_dx[l_i].exclude_filelist);
	    if (!(1 & (l_rc = smg$delete_virtual_display (
			&cntrl_info_block.dir_dx[l_i].dir_info_display.id))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$delete_virtual_display (
			&cntrl_info_block.dir_dx[l_i].filer_display.id))))
	        $LOG(l_rc);
	}
    }

    /**  Make the first display the current display  **/
    cntrl_info_block.dir_dx[0] = DX_CURRENT_DIRECTORY;
    cntrl_info_block.cur_win = 0;
    cntrl_info_block.windows = 1;

    return DX__NORMAL;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_next_win() switches to the next window (wrapping from the
**	last window back to window 0), refusing if there is only one
**	window: it un-highlights the current file in the old window,
**	advances cur_win, restores the new current window's default
**	directory via setddir(), and highlights its current file.
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
**	cntrl_info_block.windows/cur_win, DX_CURRENT_DIRECTORY.cur_dir
**	(of the new current window, after cur_win is advanced).
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.cur_win; the process default directory (via
**	setddir()).
**
**  RETURNS VALUE:
**
**	DX__ERROR if there is only one window (nothing changes); otherwise
**	falls off the end without an explicit return (pre-existing --
**	the caller does not use the return value on this path).
**
**--
**/
int	filer_next_win()
{
    char l_errmsg[MAXFILESPEC+1];

    if (cntrl_info_block.windows == 1)
    {
        signal_err("There is only one window", bell);
	return DX__ERROR;
    }
    unhighlight_filer_current_file();
    cntrl_info_block.cur_win++;
    if (cntrl_info_block.cur_win >= cntrl_info_block.windows)
    	cntrl_info_block.cur_win = 0;
    setddir(DX_CURRENT_DIRECTORY.cur_dir, l_errmsg);
    highlight_filer_current_file();
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_prev_win() switches to the previous window (wrapping from
**	window 0 back to the last window), refusing if there is only one
**	window: it un-highlights the current file in the old window,
**	retreats cur_win, restores the new current window's default
**	directory via setddir(), and highlights its current file.  Mirror
**	image of filer_next_win().
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
**	cntrl_info_block.windows/cur_win, DX_CURRENT_DIRECTORY.cur_dir
**	(of the new current window, after cur_win is retreated).
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.cur_win; the process default directory (via
**	setddir()).
**
**  RETURNS VALUE:
**
**	DX__ERROR if there is only one window (nothing changes); otherwise
**	falls off the end without an explicit return (pre-existing --
**	the caller does not use the return value on this path).
**
**--
**/
int	filer_prev_win()
{
    char l_errmsg[MAXFILESPEC+1];

    if (cntrl_info_block.windows == 1)
    {
        signal_err("There is only one window", bell);
	return DX__ERROR;
    }
    unhighlight_filer_current_file();
    cntrl_info_block.cur_win--;
    if ((short) cntrl_info_block.cur_win < 0)
    	cntrl_info_block.cur_win = cntrl_info_block.windows-1;
    setddir(DX_CURRENT_DIRECTORY.cur_dir, l_errmsg);
    highlight_filer_current_file();
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_exitset() is the " Window " menu's "eXit" command: it exits
**	via dxscr_pb_exit() (not resetting the default directory back to
**	the original one), so the pasteboard is deleted -- which erases the
**	screen -- and the terminal is taken out of SMG$'s no-echo mode
**	before the process ends.  No "scroll up one line" is done here, unlike
**	the KP0/CTRL-Z/CTRL-E exits (s_filer_scroll_off()): the erase follows
**	at once and would wipe it anyway.
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
**	cntrl_info_block.pasteboard_id (via dxscr_pb_exit()).
**
**  IMPLICIT OUTPUTS:
**
**	None (terminates the process via dxscr_pb_exit()).
**
**  RETURNS VALUE:
**
**	None (void function; does not return -- dxscr_pb_exit() ends the
**	process).
**
**--
**/
void	filer_exitset()
{
    /*
    **	Exit without resetting default directory back to the
    **	original directory.  Goes through dxscr_pb_exit() (not a plain
    **	exit()) so the terminal is restored out of SMG$'s no-echo mode
    **	-- see INQUIRE.C (formerly DXSCR.C).
    **/

    dxscr_pb_exit(SS$_NORMAL);
}



/* ---- (from FILERKPD.C -- GOLD-key keypad filer commands) ---- */

/**  FILER INACTIVE SCROLL REGION		**/
#define FILER_INACTIVE_ROWS			\
    (cntrl_info_block.dir_dx[cntrl_info_block.cur_win].filer_display.view_rows/4)
#define FILER_INACTIVE_FRONT_ROW		(1+FILER_INACTIVE_ROWS)
 



    static int		    filer_find_first$2(struct fil_dx_tag *, struct fil_dx_tag **);

/*
**	filer_find_first$2()'s file-membership scan cache -- see that
**	function's header.  Invalidated (invalidate_find_cache(), a
**	public entry point declared in FILERKPD.H) whenever the set of
**	files it scans can change: setddir() (any directory/window
**	change) and a new Name filter (filer_filter$1(), case 'N').
**/
struct s_find_list_tag
	{
	char fn[MAXFILESPEC+1];
	struct s_find_list_tag *next;
	};

static char			s_find_cache_pattern[MAXFILESPEC+1] = "";
static struct s_find_list_tag	*s_find_cache_head = NULL;
static int			s_find_cache_valid = 0;
 



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_find_first() prompts the user for a search pattern (via
**	get_userinput_and_execute()) to locate the first file matching a
**	wildcard, searching forward or backward from the current file
**	depending on DX_CURRENT_DIRECTORY.direction.  The actual search is
**	done by filer_find_first$1()/filer_find_first$2() once the user
**	has typed something.
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
**	DX_CURRENT_DIRECTORY.direction.
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_find_first$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_find_first()
{
    char l_errmsg[80];
 
    int filer_find_first$1(char *, char *, char *);
 
    strcpy(l_errmsg, "File not found");
    get_userinput_and_execute(
	filer_find_first$1,
	DX_CURRENT_DIRECTORY.direction == advance? "Forward Search" : "Reverse Search",
	"Find What? ",
	"",
	l_errmsg,
	"");
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_find_first$1() is the callback invoked with the user's typed
**	search pattern.  It records the pattern (so filer_find_next() can
**	reuse it), asks filer_find_first$2() for the nearest matching file
**	starting at the current file, and if one is found, moves
**	DX_CURRENT_FILE there and scrolls it into view.
**
**  INPUT PARAMETERS:
**
**	a_pattern  -  wildcard file specification typed by the user.
**	a_dummy    -  unused (required by the get_userinput_and_execute()
**		      callback signature).
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message if no file matches
**		     (via the DX__ERROR return, the caller signals the user).
**
**  IMPLICIT INPUTS:
**
**	None beyond what filer_find_first$2() reads.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.search_pattern; DX_CURRENT_FILE (moved to the
**	match, if any).
**
**  RETURNS VALUE:
**
**	DX__NORMAL if a match was found and the cursor moved to it;
**	DX__ERROR if no file matches the pattern.
**
**--
**/
int	filer_find_first$1(a_pattern, a_dummy, a_errmsg)
char *a_pattern;
char *a_dummy;
char *a_errmsg;
{
    struct fil_dx_tag *l_new_ptr;
 
    /**  Preserve search string so that it can be used by FIND_NEXT.  **/
    strcpy(DX_CURRENT_DIRECTORY.search_pattern, a_pattern);
 
    if (!filer_find_first$2(DX_CURRENT_FILE, &l_new_ptr))
	return DX__ERROR;
 
    unhighlight_filer_current_file();
    DX_CURRENT_FILE = l_new_ptr;
 
    change_filer_viewport(cntrl_info_block.cur_win);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_find_first$2() scans forward or backward (per
**	DX_CURRENT_DIRECTORY.direction) from a_start_ptr for the next file
**	in the current window whose full path name is in the cached
**	file-membership list built from DX_CURRENT_DIRECTORY.search_pattern.
**	That list (s_find_cache_head/s_find_cache_pattern/s_find_cache_valid)
**	is rebuilt via lib$find_file() only when the pattern has actually
**	changed since the last call, and is discarded by
**	invalidate_find_cache() whenever the set of files it describes can
**	change (directory/window switch, or a new Name filter).
**
**  INPUT PARAMETERS:
**
**	a_start_ptr  -  file-list entry to start scanning from (not itself
**			checked as a match).
**
**  OUTPUT PARAMETERS:
**
**	a_hit_ptr  -  set to the matching file-list entry on success;
**		      left pointing at whatever the scan stopped on
**		      otherwise (only meaningful when the function returns 1).
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY (search_pattern, direction, dir_filelist/
**	non_dir_filelist, cur_filter.name_filespec_list).
**
**  IMPLICIT OUTPUTS:
**
**	s_find_cache_head/s_find_cache_pattern/s_find_cache_valid, rebuilt
**	when the search pattern has changed.
**
**  RETURNS VALUE:
**
**	1 if a match was found (*a_hit_ptr is valid); 0 if the whole
**	non-directory file list was scanned with no match, or if the
**	membership-list rebuild ran out of virtual memory (in which case
**	*a_hit_ptr must not be dereferenced by the caller).
**
**--
**/
static int filer_find_first$2(a_start_ptr, a_hit_ptr)
struct fil_dx_tag *a_start_ptr;
struct fil_dx_tag **a_hit_ptr;
{
    char l_pattern[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];
    struct s_find_list_tag *l_r;

    strcpy(l_pattern, DX_CURRENT_DIRECTORY.search_pattern);

    /*
    **	Rebuild the file-membership list only when the search pattern
    **	actually changed since the last call -- this used to re-scan
    **	every equivalent name via lib$find_file() on every single
    **	FIND NEXT keystroke, even though filer_find_next() always
    **	passes the same, unchanged pattern back in here.
    */
    if (!s_find_cache_valid || strcmp(l_pattern, s_find_cache_pattern))
    {
	unsigned int l_context;
	$DESCRIPTOR(l_pattern_descrip, DX_CURRENT_DIRECTORY.search_pattern);
	char l_default_filespec[MAXFILESPEC+1];
	$DESCRIPTOR(l_default_filespec_descrip, l_default_filespec);
	unsigned long l_condcode;
	char *l_indx;
	char l_resultant_filespec[MAXFILESPEC+1];
	$DESCRIPTOR (l_resultant_filespec_descrip, l_resultant_filespec);
	struct filespec_list_tag *l_p;
	struct w_dids_tag *l_q;
	struct s_find_list_tag *l_new_entry;
	int l_allocate_size;
	unsigned long l_rc;

	invalidate_find_cache();

	LENGTH(l_pattern_descrip) = strlen(l_pattern);

	l_resultant_filespec[MAXFILESPEC] = EOS;	/* 28-Sep-2026 CLD: lib$find_file() writes no EOS */
	l_allocate_size = sizeof (struct s_find_list_tag);
	for (l_p = DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list;  l_p;  l_p = l_p->next)
	{
	    for (l_q = l_p->w_dids;  l_q;  l_q = l_q->next)
	    {
		strcpy (l_default_filespec, l_q->equivalent_name);
		strcat (l_default_filespec, "*.*;*");
		LENGTH (l_default_filespec_descrip) = strlen (l_default_filespec);
		l_context = 0;
		while ((l_condcode = lib$find_file (
				       &l_pattern_descrip,
				       &l_resultant_filespec_descrip,
				       &l_context,
				       &l_default_filespec_descrip,
				       0,
				       0,
				       0)) != RMS$_NMF)
		{
		    if (l_condcode == RMS$_NORMAL &&
			!strchr(l_resultant_filespec, '*'))
		    {
			if ((l_indx = strchr(l_resultant_filespec, ' ')))
			{
			    *l_indx = EOS;
			    if (!(1 & (l_rc = lib$get_vm (
					     &l_allocate_size,
					     &l_new_entry,
					     0))))
			    {
			        $LOG(l_rc);
				return 0;	/**  Can't allocate --
						**  treat as "not found" so
						**  the caller never touches
						**  an uninitialized *a_hit_ptr **/
			    }
			    strcpy(l_new_entry->fn, l_resultant_filespec);
			    l_new_entry->next = NULL;
			    if (s_find_cache_head == NULL)
				s_find_cache_head = l_new_entry;
			    else
			    {
				for (l_r = s_find_cache_head;  l_r->next != NULL;  l_r = l_r->next)
				{
				       ;
				}
				l_r->next = l_new_entry;
			    }
			}
		    }
		}
		lib$find_file_end(&l_context);
	    }
	}

	strcpy(s_find_cache_pattern, l_pattern);
	s_find_cache_valid = 1;
    }

    if (DX_CURRENT_DIRECTORY.direction == advance)
	*a_hit_ptr = a_start_ptr->forward;
    else
	*a_hit_ptr = a_start_ptr->backward;

    while (*a_hit_ptr != DX_CURRENT_DIRECTORY.non_dir_filelist)
    {
	 if (*a_hit_ptr == DX_CURRENT_DIRECTORY.dir_filelist)
	 {
	    *a_hit_ptr = DX_CURRENT_DIRECTORY.non_dir_filelist->forward;
	    if (*a_hit_ptr == DX_CURRENT_DIRECTORY.non_dir_filelist)
	    	 break;
	 }
	 find_full_path_name ((*a_hit_ptr)->fn, l_fn, cntrl_info_block.cur_win);
	 for (l_r = s_find_cache_head;  l_r != NULL;  l_r = l_r->next)
	 {
	     if (!strcmp (l_fn, l_r->fn))
		return 1;
	 }
	 if (DX_CURRENT_DIRECTORY.direction == advance)
	     *a_hit_ptr = (*a_hit_ptr)->forward;
	 else
	     *a_hit_ptr = (*a_hit_ptr)->backward;
    }

    return 0;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      INVALIDATE_FIND_CACHE() discards filer_find_first$2()'s cached
**	file-membership list.  Call whenever the set of files it would
**	scan can change: any directory/window change (setddir()) or a
**	new Name filter (filer_filter$1(), case 'N').
**
**--
**/
int	invalidate_find_cache()
{
    struct s_find_list_tag *l_s, *l_t;
    int l_allocate_size;
    unsigned long l_rc;

    l_allocate_size = sizeof (struct s_find_list_tag);
    l_s = s_find_cache_head;
    while (l_s != NULL)
    {
	l_t = l_s->next;
	if (!(1 & (l_rc = lib$free_vm (
			 &l_allocate_size,
			 &l_s,
			 0))))
	    $LOG(l_rc);
	l_s = l_t;
    }
    s_find_cache_head = NULL;
    s_find_cache_valid = 0;
    s_find_cache_pattern[0] = EOS;

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	change_filer_viewport() scrolls window a_win's filer-display
**	viewport (vertically) so that the current file stays visible,
**	keeping it out of a margin of "inactive" rows at the top and
**	bottom of the screen (a quarter of the viewport height each) so
**	the cursor doesn't sit right at the edge.  When the required
**	scroll is small (at most half a screen) it scrolls line-by-line
**	via repeated smg$scroll_viewport() calls; for a larger jump it
**	scrolls directly to the target position in one call.  If the
**	current file and the last file in the directory both already fit
**	in the visible area, it instead scrolls down (if needed) to reveal
**	as much of the tail of the list as fits without moving the current
**	file out of view.
**
**  INPUT PARAMETERS:
**
**	a_win  -  window number whose viewport to adjust.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.dir_dx[a_win] (cur_file, filer_display,
**	non_dir_filelist).
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y; scrolls
**	the actual SMG$ viewport to match.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/

int	change_filer_viewport(unsigned short int a_win)
{
    int l_new_viewport_row, l_new_viewport_row1;
    int l_number_of_scroll;
    int l_i;
    int l_lastentry_viewport_row;
    int l_filer_inactive_rows =
	(cntrl_info_block.dir_dx[a_win].filer_display.view_rows/4);
    int l_filer_inactive_front_row = 1+l_filer_inactive_rows;
    int l_filer_inactive_rear_row =
	cntrl_info_block.dir_dx[a_win].filer_display.view_rows-l_filer_inactive_rows;
    unsigned long l_rc;
 
    /**  Viewport for the last screen  **/
    l_lastentry_viewport_row =
	max(1,
	    cntrl_info_block.dir_dx[a_win].non_dir_filelist->backward->beg_y -
		cntrl_info_block.dir_dx[a_win].filer_display.view_rows + 1);
 
    if (cntrl_info_block.dir_dx[a_win].cur_file->beg_y < cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y +
	l_filer_inactive_front_row - 1)
    {	    /**  Target row before inactive front row  **/
	l_new_viewport_row1 =
	    max(1, cntrl_info_block.dir_dx[a_win].cur_file->beg_y-(l_filer_inactive_front_row-1));
	l_new_viewport_row =
	    min(l_new_viewport_row1, l_lastentry_viewport_row);
 
	l_number_of_scroll =
	    cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y-l_new_viewport_row;
	if (l_number_of_scroll <= cntrl_info_block.dir_dx[a_win].filer_display.view_rows/2)
	{
	    /*
	    **	The displacement between target row and current row
	    **	is leq half of the screen, use scroll by line.
	    **/
 
	    for (l_i = 1; l_i <= l_number_of_scroll; l_i++)
		if (!(1 & (l_rc = smg$scroll_viewport (
			  &cntrl_info_block.dir_dx[a_win].filer_display.id,
			  &SMG$M_DOWN,
			  &1))))
		    $LOG(l_rc);
	}
	else
	{	
	    /*	
	    **	The displacement is larger than half the screen,
	    **	use jump scroll instead.
	    **/
 
	    l_new_viewport_row1 =
		max(1, cntrl_info_block.dir_dx[a_win].cur_file->beg_y-(l_filer_inactive_rear_row-1));
	    l_new_viewport_row =
		min(l_new_viewport_row1, l_lastentry_viewport_row);
 
	    l_number_of_scroll =
		cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y-l_new_viewport_row;
	    if (!(1 & (l_rc = smg$scroll_viewport (
		      &cntrl_info_block.dir_dx[a_win].filer_display.id,
		      &SMG$M_DOWN,
		      &l_number_of_scroll))))
	        $LOG(l_rc);
	}
 
	cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y = l_new_viewport_row;
 
    } else if (cntrl_info_block.dir_dx[a_win].cur_file->beg_y > cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y +
	l_filer_inactive_rear_row - 1)
    {
	/**  Target row beyond inactive rear row  **/
	l_new_viewport_row1 =
	    max(1, cntrl_info_block.dir_dx[a_win].cur_file->beg_y-(l_filer_inactive_rear_row-1));
	l_new_viewport_row =
	    min(l_new_viewport_row1, l_lastentry_viewport_row);
 
	l_number_of_scroll =
	    l_new_viewport_row-cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y;
	if (l_number_of_scroll <= cntrl_info_block.dir_dx[a_win].filer_display.view_rows/2)
	{
	    /*
	    **	The displacement between target row and current row
	    **	is leq half of the screen, use scroll by line.
	    **/
 
	    for (l_i = 1; l_i <= abs(l_number_of_scroll); l_i++)
		if (l_number_of_scroll > 0)
		    if (!(1 & (l_rc = smg$scroll_viewport (
			      &cntrl_info_block.dir_dx[a_win].filer_display.id,
			      &SMG$M_UP,
			      &1))))
		        $LOG(l_rc);
		else
		    if (!(1 & (l_rc = smg$scroll_viewport (
			      &cntrl_info_block.dir_dx[a_win].filer_display.id,
			      &SMG$M_DOWN,
			      &1))))
		        $LOG(l_rc);
	}
	else
	{	
	    /*	
	    **	The displacement is larger than half the screen,
	    **	use jump scroll instead.
	    **/
 
	    l_new_viewport_row1 =
		max(1, cntrl_info_block.dir_dx[a_win].cur_file->beg_y-(l_filer_inactive_front_row-1));
	    l_new_viewport_row =
		min(l_new_viewport_row1, l_lastentry_viewport_row);
 
	    l_number_of_scroll =
		l_new_viewport_row-cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y;
	    if (l_number_of_scroll > 0)
		if (!(1 & (l_rc = smg$scroll_viewport (
			  &cntrl_info_block.dir_dx[a_win].filer_display.id,
			  &SMG$M_UP,
			  &l_number_of_scroll))))
		    $LOG(l_rc);
	    else
		if (!(1 & (l_rc = smg$scroll_viewport (
			  &cntrl_info_block.dir_dx[a_win].filer_display.id,
			  &SMG$M_DOWN,
			  &l_number_of_scroll))))
		    $LOG(l_rc);
	}
 
	cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y = l_new_viewport_row;
 
    }
    else if (cntrl_info_block.dir_dx[a_win].non_dir_filelist->forward->beg_y <=
		cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y +
		    cntrl_info_block.dir_dx[a_win].filer_display.view_rows - 1)
    {
	/*
	**  Target file is in active area, and
	**  last file in this directory is also in current screen.
	**/
 
	l_new_viewport_row = l_lastentry_viewport_row;
	l_number_of_scroll = min(
	    cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y-l_new_viewport_row,
	    cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y-1);
	if (l_number_of_scroll > 0)
	{	/**  One jump scroll, not N one-line scrolls (25-Sep-2026 RRL)  **/
	    if (!(1 & (l_rc = smg$scroll_viewport (
		      &cntrl_info_block.dir_dx[a_win].filer_display.id,
		      &SMG$M_DOWN,
		      &l_number_of_scroll))))
		$LOG(l_rc);
	    cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y -= l_number_of_scroll;
	}
    }
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_find_next() repeats the last filer_find_first() search
**	(reusing DX_CURRENT_DIRECTORY.search_pattern) starting from the
**	current file, moving the cursor to the next match or signalling an
**	error if none is found.
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
**	DX_CURRENT_DIRECTORY.search_pattern/direction.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_FILE (moved to the next match, via filer_find_first$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always (a failed search rings the bell/signals an
**	error rather than returning failure).
**
**--
**/
int	filer_find_next()
{
    char l_dummy1[80], l_dummy2[80];
    char l_errmsg[81];
 
    if (filer_find_first$1(
	    DX_CURRENT_DIRECTORY.search_pattern,
	    l_dummy1,
	    l_dummy2) == -1)
    {
	strcpy(l_errmsg,
	     DX_CURRENT_DIRECTORY.direction == advance?
		"Forward" : "Reverse");
	strcat(l_errmsg, " search: file not found");
	signal_err(l_errmsg, bell);
    }
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**  FILER_TOP() moves the cursor to point to the first record.
**
**--
**/
int	filer_top()
{
    int l_i;
 
    if (DX_CURRENT_FILE->beg_y == 1 && DX_CURRENT_FILE->beg_x == 1)
	signal_err("Already at top of records", bell);
    else
    {
	DX_CURRENT_FILE = DX_CURRENT_DIRECTORY.dir_filelist->forward;
	if (DX_CURRENT_FILE == DX_CURRENT_DIRECTORY.dir_filelist)
	    DX_CURRENT_FILE = DX_CURRENT_DIRECTORY.non_dir_filelist->forward;
	change_filer_viewport(cntrl_info_block.cur_win);
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**  FILER_BOTTOM() moves the cursor to point to the last record.
**
**--
**/
int	filer_bottom()
{
    if (DX_CURRENT_FILE->beg_y ==
	    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y &&
	DX_CURRENT_FILE->beg_x ==
	    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_x)
    {
        signal_err("Already at bottom of records", bell);
    }
    else
    {
	DX_CURRENT_FILE = DX_CURRENT_DIRECTORY.non_dir_filelist->backward;
        if (DX_CURRENT_FILE == DX_CURRENT_DIRECTORY.non_dir_filelist &&
	    DX_CURRENT_DIRECTORY.dir_filelist->backward !=
		DX_CURRENT_DIRECTORY.dir_filelist)
            DX_CURRENT_FILE = DX_CURRENT_DIRECTORY.dir_filelist->backward;
	change_filer_viewport(cntrl_info_block.cur_win);
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**  FILER_PAGE_DOWN() scrolls down one screenful of records.
**
**--
**/
int	filer_page_down()
{
    int l_i, l_number_of_scroll;
    int l_rows_in_page;
    struct fil_dx_tag *l_p, *l_q;
    unsigned long l_rc;
 
    if (DX_CURRENT_FILE->beg_y ==
	    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y &&
	DX_CURRENT_FILE->beg_x ==
	    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_x)
    {
        signal_err("Already at bottom of records", bell);
    }
    else
    {
	l_rows_in_page = DX_CURRENT_DIRECTORY.filer_display.view_rows-1;
	l_p = DX_CURRENT_FILE;
	while (!(l_p->beg_y ==
		    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y &&
		 l_p->beg_x ==
		    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_x))
        {
	    l_p = l_p->forward;
            if (l_p == DX_CURRENT_DIRECTORY.dir_filelist)
                l_p = DX_CURRENT_DIRECTORY.non_dir_filelist;
            else
            {
		l_q = (l_p->forward == DX_CURRENT_DIRECTORY.dir_filelist?
		    DX_CURRENT_DIRECTORY.non_dir_filelist->forward : l_p->forward);
                if ((l_p->beg_y == DX_CURRENT_FILE->beg_y + l_rows_in_page &&
			l_q->beg_x > DX_CURRENT_FILE->beg_x)	||
		    (l_q->beg_y == DX_CURRENT_FILE->beg_y + l_rows_in_page + 1 &&
			l_q->beg_x == 1))
                    break;
            }
        }
	DX_CURRENT_FILE = l_p;
 
	/*
	**  Scroll one full screen only if the current file
	**  is within inavtive area
	**/
 
	if (DX_CURRENT_FILE->beg_y <
		DX_CURRENT_DIRECTORY.filer_display.view_beg_y +
		DX_CURRENT_DIRECTORY.filer_display.view_rows +
		FILER_INACTIVE_FRONT_ROW - 2)
	{
	    l_number_of_scroll =
		DX_CURRENT_DIRECTORY.filer_display.view_rows - 1 -
		    (FILER_INACTIVE_FRONT_ROW-1);
	}
        else
        {
            l_number_of_scroll =
		DX_CURRENT_DIRECTORY.filer_display.view_rows - 1;
        }
	
	/*
	**  Scroll filer display, stopping as soon as bottom of record
	**  is reached.  25-Sep-2026 RRL: one jump scroll of N lines instead
	**  of N one-line smg$scroll_viewport() calls -- each call emitted its
	**  own terminal update, so a page took ~view_rows round trips.
	**/
 
	l_i = DX_CURRENT_DIRECTORY.filer_display.rows -
	    (DX_CURRENT_DIRECTORY.filer_display.view_beg_y +
		DX_CURRENT_DIRECTORY.filer_display.view_rows - 1);
	l_number_of_scroll = min(l_number_of_scroll, l_i);
	if (l_number_of_scroll > 0)
	{
	    if (!(1 & (l_rc = smg$scroll_viewport (
			 &DX_CURRENT_DIRECTORY.filer_display.id,
			 &SMG$M_UP,
			 &l_number_of_scroll))))
		$LOG(l_rc);
	    DX_CURRENT_DIRECTORY.filer_display.view_beg_y += l_number_of_scroll;
	}
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**  FILER_PAGE_UP() scrolls up one screenful of records.
**
**--
**/
int	filer_page_up()
{
    int l_number_of_scroll;
    int l_rows_in_page;
    struct fil_dx_tag *l_p;
    int l_filer1_inactive_rear_row =
	DX_CURRENT_DIRECTORY.filer_display.view_rows-FILER_INACTIVE_ROWS;
    unsigned long l_rc;
 
    if (DX_CURRENT_FILE->beg_y == 1 && DX_CURRENT_FILE->beg_x == 1)
	signal_err("Already at top of records", bell);
    else
    {
	l_rows_in_page = DX_CURRENT_DIRECTORY.filer_display.view_rows-1;
	l_p = DX_CURRENT_FILE;
        while (!(l_p->beg_y == 1 && l_p->beg_x == 1))
        {
	    l_p = l_p->backward;
	    if (l_p == DX_CURRENT_DIRECTORY.non_dir_filelist)
		l_p = DX_CURRENT_DIRECTORY.dir_filelist->backward;
	    else if (l_p->beg_y == DX_CURRENT_FILE->beg_y-l_rows_in_page && l_p->beg_x <= DX_CURRENT_FILE->beg_x)
                break;
        }
	DX_CURRENT_FILE = l_p;
 
	if (DX_CURRENT_FILE->beg_y > DX_CURRENT_DIRECTORY.filer_display.view_beg_y -
		(DX_CURRENT_DIRECTORY.filer_display.view_rows-1) + l_filer1_inactive_rear_row - 1)
	    l_number_of_scroll = DX_CURRENT_DIRECTORY.filer_display.view_rows - 1 - (FILER_INACTIVE_FRONT_ROW-1);
        else
            l_number_of_scroll = DX_CURRENT_DIRECTORY.filer_display.view_rows - 1;
	
	/*
	**  Scroll filer display, stopping as soon as top of record
	**  is reached.  25-Sep-2026 RRL: one jump scroll (see
	**  filer_page_down()).
	**/
 
	l_number_of_scroll = min(l_number_of_scroll,
	    DX_CURRENT_DIRECTORY.filer_display.view_beg_y - 1);
	if (l_number_of_scroll > 0)
	{
	    if (!(1 & (l_rc = smg$scroll_viewport (
			 &DX_CURRENT_DIRECTORY.filer_display.id,
			 &SMG$M_DOWN,
			 &l_number_of_scroll))))
		$LOG(l_rc);
	    DX_CURRENT_DIRECTORY.filer_display.view_beg_y -= l_number_of_scroll;
	}
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_right() moves DX_CURRENT_FILE one entry forward along the
**	display-order linked list (directory files, then non-directory
**	files), refusing with a bell if already on the very last entry;
**	crossing from the end of the directory list into the non-
**	directory list is handled automatically.  Scrolls the viewport to
**	keep the new current file visible.
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
**	DX_CURRENT_FILE, DX_CURRENT_DIRECTORY (dir_filelist/
**	non_dir_filelist), cntrl_info_block.cur_win.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_FILE (advanced); the current window's viewport (via
**	change_filer_viewport()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	filer_right()
{
    if (DX_CURRENT_FILE->beg_y ==
	    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y &&
	DX_CURRENT_FILE->beg_x ==
	    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_x)
    {
        signal_err("Already at bottom of records", bell);
    }
    else
    {
        DX_CURRENT_FILE = DX_CURRENT_FILE->forward;
        if (DX_CURRENT_FILE == DX_CURRENT_DIRECTORY.dir_filelist)
            DX_CURRENT_FILE = DX_CURRENT_DIRECTORY.non_dir_filelist->forward;
	change_filer_viewport(cntrl_info_block.cur_win);
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_left() moves DX_CURRENT_FILE one entry backward along the
**	display-order linked list, refusing with a bell if already on the
**	very first entry; crossing from the start of the non-directory
**	list back into the directory list is handled automatically.
**	Mirror image of filer_right().  Scrolls the viewport to keep the
**	new current file visible.
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
**	DX_CURRENT_FILE, DX_CURRENT_DIRECTORY (dir_filelist/
**	non_dir_filelist), cntrl_info_block.cur_win.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_FILE (retreated); the current window's viewport (via
**	change_filer_viewport()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	filer_left()
{
    if (DX_CURRENT_FILE->beg_y == 1 && DX_CURRENT_FILE->beg_x == 1)
        signal_err("Already at top of records", bell);
    else
    {
        DX_CURRENT_FILE = DX_CURRENT_FILE->backward;
        if (DX_CURRENT_FILE == DX_CURRENT_DIRECTORY.non_dir_filelist)
            DX_CURRENT_FILE = DX_CURRENT_DIRECTORY.dir_filelist->backward;
	change_filer_viewport(cntrl_info_block.cur_win);
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**  FILER_DOWN() moves the cursor down one record.
**
**--
**/
int	filer_down()
{
    struct fil_dx_tag *l_p, *l_q;
 
    if (DX_CURRENT_FILE->beg_y ==
	    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y &&
	DX_CURRENT_FILE->beg_x ==
	    DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_x)
    {
        signal_err("Already at bottom of records", bell);
    }
    else
    {
        for (l_p = l_q = DX_CURRENT_FILE;  l_p != DX_CURRENT_DIRECTORY.non_dir_filelist;  l_p = l_p->forward)
        {
            if (l_p == DX_CURRENT_DIRECTORY.dir_filelist)
                l_p = DX_CURRENT_DIRECTORY.non_dir_filelist;
            else
            {
		l_q = l_p;
		if (l_p->beg_x == DX_CURRENT_FILE->beg_x &&
		    l_p->beg_y >  DX_CURRENT_FILE->beg_y)
		    break;
	    }
        }
	DX_CURRENT_FILE = l_q;
	change_filer_viewport(cntrl_info_block.cur_win);
    }
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**  FILER_UP() moves the cursor up one line.
**
**--
**/
int	filer_up()
{
    struct fil_dx_tag *l_p, *l_q;
 
    if (DX_CURRENT_FILE->beg_y == 1 && DX_CURRENT_FILE->beg_x == 1)
	signal_err("Already at top of records", bell);
    else
    {
        for (l_p = l_q = DX_CURRENT_FILE;  l_p != DX_CURRENT_DIRECTORY.dir_filelist;  l_p = l_p->backward)
        {
            if (l_p == DX_CURRENT_DIRECTORY.non_dir_filelist)
                l_p = DX_CURRENT_DIRECTORY.dir_filelist;
            else
            {
		l_q = l_p;
		if (l_p->beg_x == DX_CURRENT_FILE->beg_x &&
		    l_p->beg_y <  DX_CURRENT_FILE->beg_y)
		    break;
	    }
        }
	DX_CURRENT_FILE = l_q;
	change_filer_viewport(cntrl_info_block.cur_win);
    }
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_mark() selects (marks) a single file entry for a multi-file
**	operation, unless it is already selected or is the "[-]"
**	parent-directory placeholder (which can never be selected).
**	Updates the selected-files/selected-blocks totals and the status
**	line, and optionally redraws the entry's mark character on screen.
**
**  INPUT PARAMETERS:
**
**	a_p       -  file-list entry to mark.
**	a_update  -  non-zero to redraw the entry's "*" mark on screen
**		     immediately; zero to update only the internal state
**		     (used when the caller is about to redraw the whole
**		     display anyway).
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	None beyond a_p itself.
**
**  IMPLICIT OUTPUTS:
**
**	a_p->state; DX_CURRENT_DIRECTORY.sel_files/sel_blocks; the status
**	line (via put_filer_stat()); the on-screen mark character if
**	a_update is set.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_mark(a_p, a_update)
struct fil_dx_tag *a_p;
int a_update;
{
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR(l_str_descrip, l_str);
    unsigned long l_rc;
 
    if (strcmp(a_p->fn, "[-]"))
    {
	if (a_p != DX_CURRENT_DIRECTORY.non_dir_filelist && a_p->state != ready)
	{
	    a_p->state = ready;
 
	    DX_CURRENT_DIRECTORY.sel_files++;
	    DX_CURRENT_DIRECTORY.sel_blocks += a_p->filesize;
 
	    put_filer_stat(cntrl_info_block.cur_win);
            if (a_update)
            {
		if (!(1 & (l_rc = smg$put_chars (
			    &DX_CURRENT_DIRECTORY.filer_display.id,
			    $DESCR ("*"),
			    &a_p->beg_y,
			    &a_p->beg_x,
			    0,
			    0,
			    0,
			    0))))
		    $LOG(l_rc);
            }
	}
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_unmark() is the inverse of filer_mark(): deselects a single
**	file entry, unless it is already deselected or is the "[-]"
**	parent-directory placeholder.  Updates the selected-files/
**	selected-blocks totals and the status line, and optionally erases
**	the entry's mark character on screen.
**
**  INPUT PARAMETERS:
**
**	a_p       -  file-list entry to unmark.
**	a_update  -  non-zero to redraw (blank out) the entry's mark on
**		     screen immediately; zero to update only the internal
**		     state.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	None beyond a_p itself.
**
**  IMPLICIT OUTPUTS:
**
**	a_p->state; DX_CURRENT_DIRECTORY.sel_files/sel_blocks; the status
**	line (via put_filer_stat()); the on-screen mark character if
**	a_update is set.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_unmark(a_p, a_update)
struct fil_dx_tag *a_p;
int a_update;
{
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR(l_str_descrip, l_str);
    unsigned long l_rc;
 
    if (a_p != DX_CURRENT_DIRECTORY.non_dir_filelist && a_p->state != waiting)
    {
	if (a_p->state == ready)
	{
	    DX_CURRENT_DIRECTORY.sel_files--;
	    DX_CURRENT_DIRECTORY.sel_blocks -= a_p->filesize;
	}
 
	a_p->state = waiting;
 
	put_filer_stat(cntrl_info_block.cur_win);
        if (a_update)
        {
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			$DESCR (" "),
			&a_p->beg_y,
			&a_p->beg_x,
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
        }
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_execute() marks a single file entry as "executed" (its
**	multi-file operation has already run against it) and redraws it
**	in reverse video -- mirroring the same short/long-form,
**	directory/non-directory display logic as write_to_filer(), but
**	with the SMG$M_REVERSE rendition added so the entry visibly
**	stands out as "done" while the rest of a multi-file operation
**	continues.  Also decrements the selected-files/selected-blocks
**	totals, since an executed entry is no longer counted as merely
**	"selected".
**
**  INPUT PARAMETERS:
**
**	a_p  -  file-list entry that has just been acted on.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY.form (short_form vs. long_form display).
**
**  IMPLICIT OUTPUTS:
**
**	a_p->state (set to executed); DX_CURRENT_DIRECTORY.sel_files/
**	sel_blocks; the status line; the on-screen entry and cursor
**	position.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_execute(a_p)
struct fil_dx_tag *a_p;
{
    int l_rest_col;	/**  1st column after the file name field  **/
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR(l_str_descrip, l_str);
    unsigned long l_rc;
 
    format_filer_entry(a_p, l_str, cntrl_info_block.cur_win);

    /*  14-Sep-2026 RRL: batch the redraw of this one entry (up to 3
    **  smg$put_chars calls in the long-form case) plus the final cursor
    **  reposition into a single flush.  **/
    if (!(1 & (l_rc = smg$begin_display_update(&DX_CURRENT_DIRECTORY.filer_display.id))))
        $LOG(l_rc);

    if (a_p->fn[ strlen(a_p->fn)-1 ] != ']')
    {	/**  Current file is not a directory file  **/
	LENGTH(l_str_descrip) = strlen(l_str);
	if (!(1 & (l_rc = smg$put_chars (
		    &DX_CURRENT_DIRECTORY.filer_display.id,
		    &l_str_descrip,
		    &a_p->beg_y,
		    &a_p->beg_x,
		    0,
		    0,
		    &SMG$M_REVERSE,
		    0))))
	    $LOG(l_rc);
    }
    else if (DX_CURRENT_DIRECTORY.form == short_form)
    {	/**  Directory file with short form  **/
	LENGTH(l_str_descrip) = strlen(l_str);
	if (!(1 & (l_rc = smg$put_chars (
		    &DX_CURRENT_DIRECTORY.filer_display.id,
		    &l_str_descrip,
		    &a_p->beg_y,
		    &a_p->beg_x,
		    0,
		    &SMG$M_BOLD,
		    &SMG$M_REVERSE,
		    0))))
	    $LOG(l_rc);
    }
    else
    {	/**  Directory file with long form  **/
	LENGTH(l_str_descrip) = 1;
	if (!(1 & (l_rc = smg$put_chars (
		    &DX_CURRENT_DIRECTORY.filer_display.id,
		    &l_str_descrip,
		    &a_p->beg_y,
		    &1,
		    0,
		    0,
		    &SMG$M_REVERSE,
		    0))))
	    $LOG(l_rc);
	memcpy (l_str, l_str+1, strlen(l_str));
	if (!strcmp(l_str, "[-]"))
	{
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			$DESCR ("[-]"),
			&a_p->beg_y,
			&2,
			0,
			&SMG$M_BOLD,
			&SMG$M_REVERSE,
			0))))
	        $LOG(l_rc);
	}
	else
	{
	    LENGTH(l_str_descrip) = FILER_NAME_WIDTH;
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&a_p->beg_y,
			&2,
			0,
			&SMG$M_BOLD,
			&SMG$M_REVERSE,
			0))))
	        $LOG(l_rc);
	    memcpy (l_str, l_str+FILER_NAME_WIDTH, strlen(l_str)-FILER_NAME_WIDTH+1);
	    LENGTH(l_str_descrip) = strlen(l_str);
	    l_rest_col = FILER_NAME_WIDTH+2;
	    if (!(1 & (l_rc = smg$put_chars (
			&DX_CURRENT_DIRECTORY.filer_display.id,
			&l_str_descrip,
			&a_p->beg_y,
			&l_rest_col,
			0,
			0,
			&SMG$M_REVERSE,
			0))))
	        $LOG(l_rc);
	}
    }
 
    a_p->state = executed;
 
    DX_CURRENT_DIRECTORY.sel_files--;
    DX_CURRENT_DIRECTORY.sel_blocks -= a_p->filesize;
 
    put_filer_stat(cntrl_info_block.cur_win);
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
	      &DX_CURRENT_DIRECTORY.filer_display.id,
	      &a_p->beg_y,
	      &a_p->beg_x))))
        $LOG(l_rc);

    if (!(1 & (l_rc = smg$end_display_update(&DX_CURRENT_DIRECTORY.filer_display.id))))
        $LOG(l_rc);

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_multiple() is the shared "apply to every marked file" driver
**	behind every filer_multiple_xxx() command: it walks the current
**	window's whole file list (directory files, then non-directory
**	files), and for every entry whose state is ready (marked), makes
**	it the current file (un/highlighting and scrolling the viewport),
**	reverse-videos it via filer_execute() as a visual "processing
**	this one now" indicator, then calls
**	(*a_func)(a_to, <full path of the file>, a_errmsg) --
**	stopping and reporting a_errmsg (bell if a_func returned -1,
**	silent otherwise) on the first non-DX__NORMAL result.  When every
**	marked file succeeds, it reports a summary count and verb chosen
**	by comparing a_func against the known filer_xxx$1() callback
**	pointers (analyzed/typed/printed/edited/copied/renamed/protection
**	set/searched/deleted/purged/... -- see the if/else ladder below).
**
**  INPUT PARAMETERS:
**
**	a_func    -  per-file worker callback, called as
**		     (*a_func)(a_to, <full path>, a_errmsg); one of the
**		     filer_xxx$1() routines.
**	a_to      -  operation-specific argument passed through unchanged
**		     to every call of a_func (e.g. a destination
**		     specification for copy/rename, or a confirmation
**		     string for delete/purge).
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in by a_func with an error message if it
**		     returns anything other than DX__NORMAL.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY (dir_filelist/non_dir_filelist, each entry's
**	state), cntrl_info_block.cur_win.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_FILE (advances through the marked entries); whatever
**	a_func itself changes for each file processed.
**
**  RETURNS VALUE:
**
**	DX__NORMAL if every marked file was processed successfully (or
**	there were none); otherwise the first non-DX__NORMAL status
**	a_func returned, which stopped the loop.
**
**--
**/
int	filer_multiple(a_func, a_to, a_errmsg)
int (*a_func)();
char *a_to;
char *a_errmsg;
{
    struct fil_dx_tag *l_marker, *l_p;
    int l_status = DX__NORMAL;
    int l_count;
    char l_fn[MAXVMSFULLNAME];
    char l_msg[81];
    unsigned long l_rc;
 
    l_p = DX_CURRENT_DIRECTORY.dir_filelist->forward;
    l_count = 0;
 
    while (l_p != DX_CURRENT_DIRECTORY.non_dir_filelist)
    {	
        if (l_p == DX_CURRENT_DIRECTORY.dir_filelist)
            l_p = DX_CURRENT_DIRECTORY.non_dir_filelist->forward;
	else
	{
	    if (l_p->state != ready)
		l_p = l_p->forward;
	    else
	    {    /**  Found a target file  **/
		unhighlight_filer_current_file();
		DX_CURRENT_FILE = l_p;
		change_filer_viewport(cntrl_info_block.cur_win);
		filer_execute(l_p);
 
		find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
		if ((l_status = (*a_func)(a_to, l_fn, a_errmsg)) != DX__NORMAL)
		{	/**  Opeartion failed or canceled by the user  **/
		    signal_err(a_errmsg, l_status == -1? bell : silence);
		    break;
		}
 
		l_p = DX_CURRENT_FILE;
		l_count++;	    /**  One more file processed  **/
	    }
	}
    }
 
    if (l_status == DX__NORMAL)
    {
	{
	    static $DESCRIPTOR (l_fao_ctrl, "!SL !AZ ");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;

	    l_out_desc.dsc$w_length  = sizeof(l_msg) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_msg;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
				       (unsigned long) l_count, (l_count > 1? "files" : "file")))))
	        $LOG(l_rc);
	    l_msg[l_len] = EOS;
	}
 
	if (a_func == filer_status$1)
	    strcat(l_msg, "analyzed");
	else if (a_func == filer_type$1)
	    strcat(l_msg, "typed");
	else if (a_func == filer_print$1)
	    strcat(l_msg, "printed ");
	else if (a_func == filer_edt$1)
	    strcat(l_msg, "edited");
	else if (a_func == filer_copy$1)
	    strcat(l_msg, "copied");
	else if (a_func == filer_rename$1)
	    strcat(l_msg, "renamed");
	else if (a_func == filer_protection$1)
	    strcat(l_msg, "protection set");
	else if (a_func == filer_find$1)
	    strcat(l_msg, "searched");
	else if (a_func == filer_delete$1)
	    strcat(l_msg, "deleted");
	else if (a_func == filer_purge$1)
	    strcat(l_msg, "purged");
	else if (a_func == filer_launch$1)
	    strcat(l_msg, "launched");
	else if (a_func == filer_diff$1)
	    strcat(l_msg, "compared");
	else if (a_func == filer_append$1)
	    strcat(l_msg, "decoded");
	else
	    strcpy(l_msg, "Internal error");    /**  Internal error  **/
 
	signal_err(l_msg, silence);
    }
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
	      &DX_CURRENT_DIRECTORY.filer_display.id,
	      &DX_CURRENT_FILE->beg_y,
	      &DX_CURRENT_FILE->beg_x))))
        $LOG(l_rc);
 
    return l_status;
}



/* ---- (from MENUNAVI.C -- menu navigation / GOLD-key menu display) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	navigator() runs one general-menu category (a_m[*a_menu_choice_nop])
**	after the user has selected it: it builds the string of valid
**	single-letter hotkeys from that category's item list, then reads
**	one keystroke with a 1-second timeout.  If a hotkey letter was
**	typed fast enough (before timeout) and it uniquely identifies an
**	item, the submenu is skipped and that item runs directly (after
**	briefly highlighting the category).  Otherwise (timeout, ambiguous
**	letter, or no letter) the submenu is displayed via
**	get_menu_choice(), with LEFT/RIGHT allowed to cycle to the
**	adjacent top-level category (wrapping around) before a submenu
**	choice is made.  KP0 cancels; KP1 shows that category's HELP
**	topic instead of running anything; any other terminator runs the
**	chosen item's op() callback with its item string as the argument.
**	The category is always un-highlighted before returning.
**
**  INPUT PARAMETERS:
**
**	a_m                 -  the general-menu table (always filer_menu[]
**			       in practice).
**	a_number_of_choices -  number of top-level categories in a_m (for
**			       LEFT/RIGHT wraparound).
**	a_hotkey_timeout    -  seconds to wait for a fast hotkey letter
**			       before showing the submenu (1 when the
**			       category was picked by its letter); 0 opens
**			       the submenu at once on its first item, with
**			       no keystroke read (the DO key, 25-Sep-2026).
**
**  OUTPUT PARAMETERS:
**
**	a_menu_choice_nop  -  in/out: index of the category to run on
**			      entry; updated if LEFT/RIGHT moves to a
**			      different category.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block (keyboard_id, commands_display.id).
**
**  IMPLICIT OUTPUTS:
**
**	The commands display (categories are highlighted/un-highlighted,
**	the PF-key line is swapped in/out); whatever the executed item's
**	op() callback itself changes.
**
**  RETURNS VALUE:
**
**	The SMG$ terminator code of whichever keystroke ended the
**	interaction (the hotkey typed, KP0, KP1, or the terminator that
**	picked a submenu item).
**
**--
**/
unsigned short int	    navigator(a_m, a_menu_choice_nop, a_number_of_choices,
				      a_hotkey_timeout)
struct menutype a_m[];
int *a_menu_choice_nop;
int a_number_of_choices;
int a_hotkey_timeout;
{
    char l_validitm[MAXCOMMANDSITEMNUM+1];
    unsigned short int l_word_terminator_code = 0;
    int l_submenu_choice_no;
    struct menutype *l_p;
    char **l_q;
    char *l_c1, *l_c2;
    unsigned int l_status;
    char *l_cp;
    int l_timeout;
    unsigned long l_rc;

    l_c1 = l_validitm;
    for (l_q = a_m[*a_menu_choice_nop].item; *l_q; l_q++)
	for (l_c2 = *l_q; *l_c2; l_c2++)
	    if (isupper (*l_c2) || *l_c2 == '-') {
		*l_c1++ = *l_c2;
		break;
	    }
    *l_c1 = EOS;

    /*
    **	Submenu will not be displayed if the user types the option letter
    **	fast enough (before time-out).
    **/

    if (a_hotkey_timeout > 0)
    {
	l_timeout = a_hotkey_timeout;
	l_status = smg$read_keystroke (
		    &cntrl_info_block.keyboard_id,
		    &l_word_terminator_code,
		    0,
		    &l_timeout,
		    0,
		    0,
		    0);
	l_word_terminator_code = DX_KEY_TOUPPER(l_word_terminator_code);
	l_cp = (l_word_terminator_code < 256) ?	    /**  not a key code  **/
	    strchr(l_validitm, l_word_terminator_code) : (char *) NULL;
    }
    else
    {	    /**  Open the submenu at once, as if the hotkey wait timed out  **/
	l_status = SS$_TIMEOUT;
	l_word_terminator_code = SMG$K_TRM_TIMEOUT;
	l_cp = (char *) NULL;
    }
    if (l_word_terminator_code == SMG$K_TRM_KP1)
    {	    /**  Display help message  **/
	highlight_general_cmd(
	    a_m[*a_menu_choice_nop],
	    &cntrl_info_block.commands_display.id);
    }
    else if (l_status != SS$_TIMEOUT &&
	l_word_terminator_code != '-' &&
	(l_cp && !strchr(l_cp+1, l_word_terminator_code)) &&
	l_word_terminator_code != SMG$K_TRM_CANCELLED)
    {	    /**  Option letter was typed before timeout  **/
	highlight_general_cmd(
	    a_m[*a_menu_choice_nop],
	    &cntrl_info_block.commands_display.id);
	l_submenu_choice_no = l_cp - l_validitm;
    }
    else
    {
	dxscr_tick_clock(cntrl_info_block.commands_display.id);
	if (l_cp && l_word_terminator_code != '-')
	{	/**  Found a valid option letter  **/
	    l_submenu_choice_no = l_cp - l_validitm;
	}
	else
	{
	    l_submenu_choice_no = 0;
	    while (!strcmp(
			*(a_m[*a_menu_choice_nop].item+l_submenu_choice_no),
			MENU_SEPARATOR))
	    {
	    	 l_submenu_choice_no++;
	    }
	}
	put_filer_pfs(FILER_PFS, cntrl_info_block.commands_display.id);
	do
	{
	    highlight_general_cmd(
		a_m[*a_menu_choice_nop],
		&cntrl_info_block.commands_display.id);

	    l_word_terminator_code =
		get_menu_choice(a_m[*a_menu_choice_nop], &l_submenu_choice_no);

	    if (l_word_terminator_code == SMG$K_TRM_LEFT)
	    {
		unhighlight_general_cmd(
		    a_m[*a_menu_choice_nop],
		    &cntrl_info_block.commands_display.id);
		if (--(*a_menu_choice_nop) < 0)
		    *a_menu_choice_nop = a_number_of_choices-1;
		l_submenu_choice_no = 0;
		while (!strcmp(
			    *(a_m[*a_menu_choice_nop].item+l_submenu_choice_no),
			    MENU_SEPARATOR))
		{
		     l_submenu_choice_no++;
		}
	    }
	    else if (l_word_terminator_code == SMG$K_TRM_RIGHT)
	    {
		unhighlight_general_cmd(
		    a_m[*a_menu_choice_nop],
		    &cntrl_info_block.commands_display.id);
		if (++(*a_menu_choice_nop) >= a_number_of_choices)
		    *a_menu_choice_nop = 0;
		l_submenu_choice_no = 0;
		while (!strcmp(
			    *(a_m[*a_menu_choice_nop].item+l_submenu_choice_no),
			    MENU_SEPARATOR))
		{
		     l_submenu_choice_no++;
		}
	    }
	} while (l_word_terminator_code == SMG$K_TRM_LEFT ||
		 l_word_terminator_code == SMG$K_TRM_RIGHT);
	put_filer_pfs(FILER_PFS, cntrl_info_block.commands_display.id);
    }

    if (l_word_terminator_code == SMG$K_TRM_KP0)
    {	    /**  Command cancelled  **/
	signal_err("Command canceled by user", silence);
    }
    else if (l_word_terminator_code != SMG$K_TRM_KP1)
    {	    /**  Execute the command  **/
        if (!(1 & (l_rc = smg$set_cursor_abs (
                     &cntrl_info_block.commands_display.id,
                     &cntrl_info_block.commands_display.rows,
                     &cntrl_info_block.commands_display.width))))
            $LOG(l_rc);
	(a_m[*a_menu_choice_nop].op[l_submenu_choice_no])(
	    a_m[*a_menu_choice_nop].item[l_submenu_choice_no]);
    }
    else
    {	    /**  Show a specific help message  **/
	switch (*a_menu_choice_nop)
	{
	    case 0:
		filer_help("DX WINDOW_MENU");
		break;
	    case 1:
	    	filer_help("DX ACTION_MENU");
		break;
	    case 2:
	    	filer_help("DX EXECUTE_MENU");
		break;
	    case 3:
	    	filer_help("DX SORT_MENU");
		break;
	    case 4:
	    	filer_help("DX FILTER_MENU");
		break;
	    case 5:
	    	filer_help("DX OPTION_MENU");
		break;
	    case 6:
	    	filer_help("DX CUSTOMIZE_MENU");
		break;
	    default:
	    	 ;
	}
    }

    unhighlight_general_cmd(
	a_m[*a_menu_choice_nop],
	&cntrl_info_block.commands_display.id);

    return l_word_terminator_code;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	get_menu_choice() pops up a_submenu's bordered item list as a new
**	virtual display (sized to the longest item plus margin, one row
**	per item), draws it via create_submenu(), lets the user navigate
**	and pick an item via select_from_submenu(), then deletes the
**	popup display.  If the pick was made with ENTER/CR, the returned
**	terminator code is replaced with the actual hotkey letter of the
**	chosen item (so callers can treat "picked via cursor+ENTER" and
**	"picked by typing the letter" identically).
**
**  INPUT PARAMETERS:
**
**	a_submenu  -  the submenu (one category's item/op arrays) to
**		      display and choose from.
**
**  OUTPUT PARAMETERS:
**
**	a_choice_nop  -  in/out: index of the item to start highlighted;
**			 updated to the index of the item actually chosen.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block (used indirectly by create_submenu()/
**	select_from_submenu()).
**
**  IMPLICIT OUTPUTS:
**
**	A temporary virtual display is created and destroyed.
**
**  RETURNS VALUE:
**
**	The SMG$ terminator code for the chosen item -- either the raw
**	terminator from select_from_submenu(), or (for ENTER/CR) that
**	item's own hotkey letter.
**
**--
*/
unsigned short int get_menu_choice(a_submenu, a_choice_nop)
struct menutype a_submenu;
int *a_choice_nop;
{
    char l_item[MAXCOMMANDSITEMLENGTH+1];
    $DESCRIPTOR (l_item_descriptor, l_item);

    int l_i, l_j;
    char *l_c1, *l_c2;
    char l_validitm[MAXCOMMANDSITEMNUM+1];
    char **l_p;
    unsigned short int l_word_terminator_code;
    struct display_tag l_submenu_items_display;
    unsigned long l_rc;

    l_submenu_items_display.rows = l_submenu_items_display.width = 0;
    l_c1 = l_validitm;
    for (l_p = a_submenu.item; *l_p; l_p++)
    {
	for (l_c2 = *l_p; *l_c2; l_c2++)
	    if (isupper (*l_c2) || *l_c2 == '-')
	    {
		*l_c1++ = *l_c2;
		break;
	    }
	l_submenu_items_display.rows++;
    }
    *l_c1 = EOS;

    for (l_i = 0, l_p = a_submenu.item; *l_p && l_i < MAXCOMMANDSITEMNUM; l_i++, l_p++)
	l_submenu_items_display.width =
		max((int)l_submenu_items_display.width, (int)strlen(*l_p));

    l_submenu_items_display.width += 4;

    if (!(1 & (l_rc = smg$create_virtual_display (
	    &l_submenu_items_display.rows,
	    &l_submenu_items_display.width,
	    &l_submenu_items_display.id,
	    &SMG$M_BORDER,
	    0,
	    0))))
        $LOG(l_rc);

    create_submenu(l_submenu_items_display, a_submenu.item);

    select_from_submenu(l_submenu_items_display, a_submenu, a_choice_nop,
			l_validitm, &l_word_terminator_code);

    if (l_word_terminator_code == SMG$K_TRM_ENTER ||
	l_word_terminator_code == SMG$K_TRM_CR)
	l_word_terminator_code = (unsigned short int) (*(l_validitm+*a_choice_nop));

    if (!(1 & (l_rc = smg$delete_virtual_display (
		&l_submenu_items_display.id))))
        $LOG(l_rc);

    return l_word_terminator_code;
}



/*
**++
**	tbd
**--
**/
int	create_submenu(a_menu_display, a_items)
struct display_tag a_menu_display;
char **a_items;
{
    char **l_p;

    char l_item[MAXCOMMANDSITEMLENGTH+1];
    $DESCRIPTOR (l_item_descriptor, l_item);

    int l_i, l_j;
    char *l_c;
    char l_ch[1];
    $DESCRIPTOR (l_ch_descriptor, l_ch);
    unsigned long l_rc;

    LENGTH(l_ch_descriptor) = 1;

    /*  28-Sep-2026 CLD: batch the whole menu paint (up to 2 smg$put_chars
    **  per item) in one begin/end_display_update -- create_submenu() only
    **  writes content (no paste/create/get_pasting_info in the loop).  **/
    if (!(1 & (l_rc = smg$begin_display_update(&a_menu_display.id))))
        $LOG(l_rc);

    for (l_p = a_items, l_i = 1;  *l_p;  l_p++, l_i++)
    {
	if (!strcmp(*l_p, MENU_SEPARATOR))
	{
	    *l_item = ' ';
	    memset (l_item+1, '-', a_menu_display.width-2);
	    *(l_item+a_menu_display.width-1) = ' ';
	}
	else
	{
	    l_item[0] = l_item[1] = ' '; strcpy(l_item+2, *l_p);
	    for (l_j = strlen(l_item);  l_j < a_menu_display.width;  l_j++)
	    {
		*(l_item+l_j) = ' ';
	    }
	}
	LENGTH(l_item_descriptor) = a_menu_display.width;
        if (!(1 & (l_rc = smg$put_chars (
                  &a_menu_display.id,
                  &l_item_descriptor,
                  &l_i,
                  &1,
                  0,
		  0,
                  0,
                  0))))
            $LOG(l_rc);
        for (l_c = *l_p, l_j = 3;  *l_c;  l_c++, l_j++)
        {
            if (isupper (*l_c))
            {
		l_ch[0] = *l_c;
                if (!(1 & (l_rc = smg$put_chars (
                          &a_menu_display.id,
                          &l_ch_descriptor,
                          0,
                          &l_j,
                          0,
                          &SMG$M_UNDERLINE,
                          &SMG$M_BOLD,
                          0))))
                    $LOG(l_rc);
		break;
            }
        }
    }

    if (!(1 & (l_rc = smg$end_display_update(&a_menu_display.id))))
        $LOG(l_rc);
    return DX__NORMAL;
}



/*
**++
**	tbd
**--
**/
int	select_from_submenu(a_menu_display, a_submenu, a_choice_nop,
			    a_validitm, a_word_terminator_codep)
struct display_tag a_menu_display;
struct menutype a_submenu;
int *a_choice_nop;
char a_validitm[];
unsigned short int *a_word_terminator_codep;
{
    char **l_p;

    char l_item[MAXCOMMANDSITEMLENGTH+1];
    $DESCRIPTOR (l_item_descriptor, l_item);

    int l_i, l_j;
    char *l_c1, *l_c2;
    char *l_c, *l_d;
    int l_start_row, l_start_col;
    char l_ch[1];
    $DESCRIPTOR (l_ch_descriptor, l_ch);
    int l_prev_choice;
    int l_timeout;
    unsigned int l_status;
    unsigned long l_rc;

    LENGTH (l_ch_descriptor) = 1;

    l_p = a_submenu.item+*a_choice_nop;

    l_item[0] = l_item[1] = ' '; strcpy(l_item+2, *l_p);
    for (l_i = strlen(l_item);  l_i < a_menu_display.width;  l_i++)
    {
	*(l_item+l_i) = ' ';
    }

    /*  28-Sep-2026 CLD: batch the initial-choice paint (2 smg$put_chars +
    **  smg$set_cursor_abs) -- runs before smg$paste_virtual_display below,
    **  content-only, safe to batch.  **/
    if (!(1 & (l_rc = smg$begin_display_update(&a_menu_display.id))))
        $LOG(l_rc);

    l_i = *a_choice_nop+1;
    LENGTH(l_item_descriptor) = a_menu_display.width;
    if (!(1 & (l_rc = smg$put_chars (
	      &a_menu_display.id,
	      &l_item_descriptor,
	      &l_i,
	      &1,
	      0,
	      0,
	      &SMG$M_REVERSE,
	      0))))
        $LOG(l_rc);
    for (l_c1 = *l_p, l_j = 3;  *l_c1;  l_c1++, l_j++)
    {
	if (isupper (*l_c1))
	{
	    l_ch[0] = *l_c1;
	    if (!(1 & (l_rc = smg$put_chars (
		      &a_menu_display.id,
		      &l_ch_descriptor,
		      0,
		      &l_j,
		      0,
		      &SMG$M_REVERSE,
		      &SMG$M_BOLD,
		      0))))
	        $LOG(l_rc);
	    break;
	}
    }

    if (!(1 & (l_rc = smg$set_cursor_abs (
              &a_menu_display.id,
              &l_i,
              &1))))
        $LOG(l_rc);

    if (!(1 & (l_rc = smg$end_display_update(&a_menu_display.id))))
        $LOG(l_rc);

    l_start_row = a_submenu.start_row+2;
    if (a_submenu.start_col + a_menu_display.width <= cntrl_info_block.pasteboard_width)
    	l_start_col = a_submenu.start_col + 1;
    else
    {
    	l_start_col = cntrl_info_block.pasteboard_width - a_menu_display.width;
	a_submenu.start_col = l_start_col - 1;
    }

    if (!(1 & (l_rc = smg$paste_virtual_display (
	    &a_menu_display.id,
	    &cntrl_info_block.pasteboard_id,
	    &l_start_row,
	    &l_start_col,
	    0))))
        $LOG(l_rc);

    l_timeout = cntrl_info_block.user_pref.update_in_second? 1 : 60;
    do {
	do {
	    l_status = smg$read_keystroke (
				&cntrl_info_block.keyboard_id,
				a_word_terminator_codep,
				0,
				&l_timeout,
				&a_menu_display.id,
				0,
				0);
	    dxscr_tick_clock(cntrl_info_block.commands_display.id);
	} while (l_status == SS$_TIMEOUT ||
		 *a_word_terminator_codep == SMG$K_TRM_CANCELLED);

	/*  26-Sep-2026 RRL: the keys that open the menu from the filer
	**  (DO, keypad "-", PF4, plain "-" -- see DX_process_loop()) close
	**  it again here, exactly like KP0; mapped before DX_KEY_TOUPPER() and
	**  before the hotkey matching below, where "-" would otherwise hit
	**  a separator entry of a_validitm and just ring the bell.  **/
	if (*a_word_terminator_codep == SMG$K_TRM_DO ||
	    *a_word_terminator_codep == SMG$K_TRM_MINUS ||
	    *a_word_terminator_codep == SMG$K_TRM_PF4 ||
	    *a_word_terminator_codep == '-')
	    *a_word_terminator_codep = SMG$K_TRM_KP0;

	*a_word_terminator_codep = DX_KEY_TOUPPER(*a_word_terminator_codep);

        if (*a_word_terminator_codep < 256 &&	    /**  not a key code  **/
	    (l_c = strchr (a_validitm, *a_word_terminator_codep)) &&
	    !strchr (l_c+1, *a_word_terminator_codep) &&
	    *a_word_terminator_codep != '-')
        {	/**  Exact one submenu item matchs the options letter  **/
            *a_choice_nop = l_c - a_validitm;
	    break;
        }

	if ((*a_word_terminator_codep < 256 &&	    /**  not a key code  **/
	     (l_c = strchr(a_validitm+*a_choice_nop, *a_word_terminator_codep)) &&
	     *a_word_terminator_codep != '-') ||
	    *a_word_terminator_codep == SMG$K_TRM_KP5 ||
	    *a_word_terminator_codep == SMG$K_TRM_KP6 ||
	    *a_word_terminator_codep == SMG$K_TRM_UP ||
            *a_word_terminator_codep == SMG$K_TRM_DOWN)
	{
	    l_prev_choice = *a_choice_nop;	    /**  Save old option number  **/

	    /**  Get new option number  **/

	    if (*a_word_terminator_codep == SMG$K_TRM_KP5)
	    {
		*a_choice_nop = 0;
	    }
	    else if (*a_word_terminator_codep == SMG$K_TRM_KP6)
	    {
		*a_choice_nop = a_menu_display.rows-1;
	    }
	    else if (*a_word_terminator_codep == SMG$K_TRM_UP)
	    {
		if (--(*a_choice_nop) < 0)
		    *a_choice_nop = a_menu_display.rows-1;
		while (!strcmp (
			*(a_submenu.item+*a_choice_nop), MENU_SEPARATOR))
		{
		    if (--(*a_choice_nop) < 0)
			*a_choice_nop = a_menu_display.rows-1;
		}
	    }
	    else if (*a_word_terminator_codep == SMG$K_TRM_DOWN)
	    {
		if (++(*a_choice_nop) > a_menu_display.rows-1)
		    *a_choice_nop = 0;
		while (!strcmp (
                            *(a_submenu.item+*a_choice_nop), MENU_SEPARATOR))
		{
		    if (++(*a_choice_nop) > a_menu_display.rows-1)
			*a_choice_nop = 0;
		}
	    }
            else if (l_c == a_validitm+*a_choice_nop)
	    {	    /**  Current cursor is the option letter  **/
                if ((l_d = strchr (l_c+1, *a_word_terminator_codep)))
                {   /*
		    **  One or more submenu items with the option
		    **	letter ahead
		    **/
		    *a_choice_nop = l_d - a_validitm;
                    if (!strchr (l_d+1, *a_word_terminator_codep))
                    {	/*
			**  Only one submenu items with the option
			**  letter ahead
			**/
                        break;
                    }
                }
                else
                {   /*
		    **	The cursor is the only current submenu item
		    **	with the option letter
		    **/
                    break;
                }
	    }
	    else
	    {	    /**  Current cusor is not the option letter  **/
		*a_choice_nop = l_c - a_validitm;
                if (!strchr (l_c+1, *a_word_terminator_codep))
                    break;
	    }

	    /*  28-Sep-2026 CLD: batch un-highlight+highlight (4 smg$put_chars
	    **  + smg$set_cursor_abs below) -- fires on every arrow-key/hotkey
	    **  move inside an already-pasted, visible submenu; content-only,
	    **  no paste/create in between.  **/
	    if (!(1 & (l_rc = smg$begin_display_update(&a_menu_display.id))))
	        $LOG(l_rc);

	    /**  Un-highlight previous option  **/
	    l_p = a_submenu.item + l_prev_choice;
	    l_item[0] = l_item[1] = ' '; strcpy(l_item+2, *l_p);
	    for (l_j = strlen(l_item);  l_j < a_menu_display.width;  l_j++)
	    {
		*(l_item+l_j) = ' ';
	    }
	    l_i = l_prev_choice+1;
	    LENGTH(l_item_descriptor) = a_menu_display.width;
	    if (!(1 & (l_rc = smg$put_chars (
		      &a_menu_display.id,
		      &l_item_descriptor,
		      &l_i,
		      &1,
		      0,
		      0,
		      0,
		      0))))
	        $LOG(l_rc);
	    for (l_c1 = *l_p, l_j = 3;  *l_c1;  l_c1++, l_j++)
	    {
		if (isupper (*l_c1))
		{
		    l_ch[0] = *l_c1;
		    if (!(1 & (l_rc = smg$put_chars (
			      &a_menu_display.id,
			      &l_ch_descriptor,
			      0,
			      &l_j,
			      0,
			      &SMG$M_UNDERLINE,
			      &SMG$M_BOLD,
			      0))))
		        $LOG(l_rc);
		    break;
		}
	    }

	    /**  Highlight new option  **/
	    l_p = a_submenu.item+*a_choice_nop;
	    l_item[0] = l_item[1] = ' '; strcpy(l_item+2, *l_p);
	    for (l_j = strlen(l_item);  l_j < a_menu_display.width;  l_j++)
	    {
		*(l_item+l_j) = ' ';
	    }
	    l_i = *a_choice_nop+1;
	    LENGTH(l_item_descriptor) = a_menu_display.width;
	    if (!(1 & (l_rc = smg$put_chars (
		      &a_menu_display.id,
		      &l_item_descriptor,
		      &l_i,
		      &1,
		      0,
		      0,
		      &SMG$M_REVERSE,
		      0))))
	        $LOG(l_rc);
	    for (l_c1 = *l_p, l_j = 3;  *l_c1;  l_c1++, l_j++)
	    {
		if (isupper (*l_c1))
		{
		    l_ch[0] = *l_c1;
		    if (!(1 & (l_rc = smg$put_chars (
			      &a_menu_display.id,
			      &l_ch_descriptor,
			      0,
			      &l_j,
			      0,
			      &SMG$M_REVERSE,
			      &SMG$M_BOLD,
			      0))))
		        $LOG(l_rc);
		    break;
		}
	    }

	    if (!(1 & (l_rc = smg$set_cursor_abs (
		      &a_menu_display.id,
		      &l_i,
		      &1))))
	        $LOG(l_rc);

	    if (!(1 & (l_rc = smg$end_display_update(&a_menu_display.id))))
	        $LOG(l_rc);
	}
	else if (*a_word_terminator_codep != SMG$K_TRM_LEFT &&
		 *a_word_terminator_codep != SMG$K_TRM_RIGHT &&
		 *a_word_terminator_codep != SMG$K_TRM_KP0 &&
		 *a_word_terminator_codep != SMG$K_TRM_KP1 &&
		 *a_word_terminator_codep != SMG$K_TRM_ENTER &&
		 *a_word_terminator_codep != SMG$K_TRM_CR)
	{
            if (!(1 & (l_rc = smg$ring_bell (
                         &a_menu_display.id,
                         &1))))
                $LOG(l_rc);
	}
    } while (*a_word_terminator_codep != SMG$K_TRM_KP0 &&
	     *a_word_terminator_codep != SMG$K_TRM_KP1 &&
	     *a_word_terminator_codep != SMG$K_TRM_CR &&
	     *a_word_terminator_codep != SMG$K_TRM_ENTER &&
	     *a_word_terminator_codep != SMG$K_TRM_LEFT &&
	     *a_word_terminator_codep != SMG$K_TRM_RIGHT);

    return DX__NORMAL;
}



/*
**	HIGHLIGHT_GENERAL_CMD highlights the FILER command
**      that was selected by the user
**/
int	highlight_general_cmd(a_m, a_cmds_display_idp)
struct menutype a_m;
unsigned int *a_cmds_display_idp;
{
    char l_c[1];
    $DESCRIPTOR (l_c_descriptor, l_c);
    int l_limit;
    int l_i;
    char *l_cp;
    unsigned long l_rendition_set = SMG$M_REVERSE+SMG$M_BOLD;
    unsigned long l_rc;

    LENGTH(l_c_descriptor) = 1;
    l_limit = a_m.start_col+strlen (a_m.itmstr);

    if (!(1 & (l_rc = smg$begin_display_update (
	      a_cmds_display_idp))))
        $LOG(l_rc);

    for (l_i = a_m.start_col, l_cp = a_m.itmstr;  l_i < l_limit;  l_i++, l_cp++)
    {
        if (! isupper((l_c[0] = *l_cp)))
        {
            if (!(1 & (l_rc = smg$put_chars (
                      a_cmds_display_idp,
                      &l_c_descriptor,
                      &(a_m.start_row),
                      &l_i,
                      0,
                      &SMG$M_REVERSE,
                      0,
                      0))))
                $LOG(l_rc);
        }
        else
        {
            if (!(1 & (l_rc = smg$put_chars (
                      a_cmds_display_idp,
                      &l_c_descriptor,
                      &(a_m.start_row),
                      &l_i,
                      0,
                      &l_rendition_set,
		      0,
                      0))))
                $LOG(l_rc);
        }
    }

    if (!(1 & (l_rc = smg$end_display_update (
	      a_cmds_display_idp))))
        $LOG(l_rc);
    return DX__NORMAL;
}


/*
**	UNHIGHLIGHT_GENERAL_CMD un-highlights the FILER command
**      after execution is completed
**/
int	unhighlight_general_cmd(a_m, a_cmds_display_idp)
struct menutype a_m;
unsigned int *a_cmds_display_idp;
{
    char l_c[1];
    $DESCRIPTOR (l_c_descriptor, l_c);
    int l_limit;
    int l_i;
    char *l_cp;
    unsigned long l_rc;

    LENGTH(l_c_descriptor) = 1;
    l_limit = a_m.start_col+strlen (a_m.itmstr);

    if (!(1 & (l_rc = smg$begin_display_update (
	      a_cmds_display_idp))))
        $LOG(l_rc);

    for (l_i = a_m.start_col, l_cp = a_m.itmstr;  l_i < l_limit;  l_i++, l_cp++)
    {
        if (! isupper((l_c[0] = *l_cp)))
        {
            if (!(1 & (l_rc = smg$put_chars (
                      a_cmds_display_idp,
                      &l_c_descriptor,
                      &(a_m.start_row),
                      &l_i,
                      0,
                      &SMG$M_NORMAL,
                      0,
                      0))))
                $LOG(l_rc);
        }
        else
        {
            if (!(1 & (l_rc = smg$put_chars (
                      a_cmds_display_idp,
                      &l_c_descriptor,
                      &(a_m.start_row),
                      &l_i,
                      0,
                      &SMG$M_UNDERLINE,
                      &SMG$M_BOLD,
                      0))))
                $LOG(l_rc);
        }
    }

    if (!(1 & (l_rc = smg$end_display_update (
	      a_cmds_display_idp))))
        $LOG(l_rc);
    return DX__NORMAL;
}
