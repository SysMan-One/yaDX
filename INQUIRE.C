/* Program Name            : INQUIRE.C                                  */
/*   Original Author       : C. K. Hung					*/
/*   Editor                : StarLet Squad and Ruslan R. Laishev (RRL)  */
/*   Date                  : 9-26-89					*/
/*   Program Description   : User-input prompting/editing (this file)  */
/*                           plus shared SMG$ screen-management helpers */
/*                           -- consolidated 13-Sep-2026 from INQUIRE.C */
/*                           + DXSCR.C (see that module's own history   */
/*                           below, retained for the record).           */
/*                         :                                            */
/* Revision History follows                                             */
/*	(9sep89)CKH	1. delete INQUIRE()
**			2. add GET_USERINPUT_AND_EXECUTE() to replace
**			    GENERAL_DIR_MANIPULATE_ROUTINE() in MAINMENU.C and
**			    GENERAL_FILER_MAINP_ROUTINE() in FILER.C.
**	(09oct89)CKH	1. add READ_STRING_AND_CLEAR_ERRMSG
**	(17nov89)CKH	1. delete CREATE_USERINPUT_DISPLAY().
**	(12Jan91)CKH	1. Add initial value to get_userinput_and_execute and
**			    multiple_get_userinput_and_exeute
**	13-Sep-2026 RRL	1. a_/l_/$LOG naming convention
**			2. Merged in DXSCR.C (shared SMG$ helpers, see below)
**	25-Sep-2026 RRL	ECO2: __NEW_STARLET prototypes, types fixed (0 CC diags)
**			    SIGALRM handlers -> void f(int); SMG$ ids -> unsigned int
**	26-Sep-2026 CLD	ECO3: ident sync with DX.C (no code change)
**	26-Sep-2026 CLD	ECO5: SIGALRM "Working..." handlers -> dxscr_working_start()
**	21-Oct-2026 CLD	ECO13: input line -- SMG key codes >= 256 no longer cut to a
**			    printable char (Find gave "7", F9 "!"); Home/End (E1/E4) = CTRL/H,
**			    CTRL/E; TAB (5 spaces) painted in one display update
*/

#define __MODULE__	"INQUIRE"
#define __IDENT__	"X-02-05ECO13"
#define __REV__		"02.05.13"
 
#define	__NEW_STARLET	1
#include	<starlet.h>
#include	<lib$routines.h>
#include	<str$routines.h>
#include	<signal.h>
#include	<trmdef.h>
#include	<iodef.h>	    /**  IO$_SETMODE / IO$M_CTRLYAST, for
			    **   dxscr_enable_ctrly_cleanup()  **/

#include "global.h"
#include "direct.h"
#include "dx.h"
#include "filer.h"
#include "filercpy.h"
#include "fileropt.h"
#include "filerque.h"
#include "filersta.h"
#include "inquire.h"
 
/*
**
**  MACRO DEFINITIONS
**
**/
 
#define FIELD_LENGTH	    255
#define MAX_RECALL_LINES    20
 
/*
**
**  GLOBAL DECLARATIONS
**
**/
 
static unsigned int userinput_display_id;
static char *recall_buf[MAX_RECALL_LINES] =
    {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };
enum mode_tag
    {
	replace, insert
    };
 
/*
**
**  INTERNAL FUNCTION PROTOTYPING
**
**/
    static int		    read_string_and_clear_errmsg(
				struct display_tag *,
				char *,
				char *,
				unsigned short *,
				unsigned short);
 
 
    static int		    put_chars_in_userinput(
				int *,
				int *,
				char *,
				char *,
				struct display_tag *,
				unsigned int,
				enum mode_tag,
				int *);
    static void		    s_dxscr_alarm_wait(int);
    static void		    s_dxscr_alarm_blink(int);
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      GET_USERINPUT_AND_EXECUTE() prompts the user for information
**	needed and call the actual function to perform desired operation.
**
**  MODIFICATION HISTORY:
**	(10oct89)CKH	1. Check field full
**
**--
**/
int	get_userinput_and_execute(
		int (*a_func)(),
		char *a_title_str,
		char *a_prompt_str,
		char *a_start_str,
		char *a_err_str,
		char *a_from)
{
    struct display_tag l_read_area_display;
    register int l_i;
    int l_status;
    long int l_title_start_row = 2;
    long int l_title_start_column;
    long int l_prompt_start_row = 5;
    long int l_prompt_start_column = 10;
    long int l_input_start_column;
    unsigned short l_terminator;
    int l_occlusion_state;
    char l_initial_str[MAXFILESPEC+1];
    char l_resultant_str[MAXFILESPEC+1];
    char l_upper_str[FIELD_LENGTH+1];
    char l_filler[FIELD_LENGTH+1];
    $DESCRIPTOR (l_filler_descrip, l_filler);
    $DESCRIPTOR (l_title_descrip, a_title_str);
    $DESCRIPTOR (l_prompt_str_descrip, a_prompt_str);
    $DESCRIPTOR (l_err_str_descrip, a_err_str);
    static char l_str[128];
    $DESCRIPTOR (l_str_descrip, l_str);
    int l_userinput_pasteboard_row;
    void (*l_cstat)(int);
    unsigned long l_rc;
 
    /*  14-Sep-2026 RRL: this section interleaves smg$create_virtual_
    **  display/smg$paste_virtual_display (structural pasteboard
    **  operations) with smg$put_chars (content writes) -- NOT wrapped in
    **  smg$begin/end_pasteboard_update: that pairing is only valid around
    **  content writes to already-pasted displays, and wrapping a paste
    **  broke signal_err() the same way (%DX-F-SFC in live testing) when
    **  tried there.  **/
    if (!(1 & (l_rc = smg$create_virtual_display (
	    &USERINPUT_BOTTOM,
	    &USERINPUT_RIGHTMOST,
	    &userinput_display_id,
	    0,
	    0,
	    0))))
        $LOG(l_rc);
    l_userinput_pasteboard_row =
	cntrl_info_block.pasteboard_rows-USERINPUT_BOTTOM+1;
    if (!(1 & (l_rc = smg$paste_virtual_display (
	    &userinput_display_id,
	    &cntrl_info_block.pasteboard_id,
	    &l_userinput_pasteboard_row,
	    &USERINPUT_PASTEBOARD_COLUMN,
	    0))))
        $LOG(l_rc);
 
    /*
    **	    WRITE FORM TITLE
    **/
 
    LENGTH(l_title_descrip) = strlen(a_title_str);
    l_title_start_column = max ( 1,
			 (USERINPUT_RIGHTMOST-strlen(a_title_str))/2);
 
    if (!(1 & (l_rc = smg$put_chars (
	      &userinput_display_id,
	      &l_title_descrip,
	      &l_title_start_row,
	      &l_title_start_column,
	      0,
	      &SMG$M_BOLD,
	      0,
	      0))))
        $LOG(l_rc);
 
    /*
    **	    WRITE PROMPT STRING
    **/
 
    LENGTH(l_prompt_str_descrip) = strlen(a_prompt_str);
    if (!(1 & (l_rc = smg$put_chars (
	      &userinput_display_id,
	      &l_prompt_str_descrip,
	      &l_prompt_start_row,
	      &l_prompt_start_column,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    /*
    **	CREATE USER READ AREA
    **/
 
    l_read_area_display.rows = 1;
    l_read_area_display.width = FIELD_LENGTH+1;
    l_read_area_display.beg_x = strlen(a_prompt_str)+l_prompt_start_column;
    l_read_area_display.beg_y = l_userinput_pasteboard_row+l_prompt_start_row-1;
 
    l_read_area_display.view_rows = 1;
    l_input_start_column = strlen(a_prompt_str)+l_prompt_start_column;
    l_read_area_display.view_width =
	USERINPUT_RIGHTMOST-l_input_start_column-l_prompt_start_column;
    l_read_area_display.view_beg_x = 1;
    l_read_area_display.view_beg_y = 1;
 
    if (!(1 & (l_rc = smg$create_virtual_display (
                 &l_read_area_display.rows,
                 &l_read_area_display.width,
                 &l_read_area_display.id,
                 0,
                 &SMG$M_UNDERLINE,
                 0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$create_viewport (
                 &l_read_area_display.id,
                 &l_read_area_display.view_beg_y,
                 &l_read_area_display.view_beg_x,
                 &l_read_area_display.view_rows,
                 &l_read_area_display.view_width))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
                 &l_read_area_display.id,
                 &cntrl_info_block.pasteboard_id,
                 &l_read_area_display.beg_y,
                 &l_read_area_display.beg_x,
                 0))))
        $LOG(l_rc);
 
    memset(l_filler, ' ', FIELD_LENGTH);
 
    LENGTH(l_filler_descrip) = FIELD_LENGTH;
    if (!(1 & (l_rc = smg$put_chars (
		&l_read_area_display.id,
		&l_filler_descrip,
		&1,
		&1,
		0,
		0,
		0,
		0))))
        $LOG(l_rc);
 
    strcpy(l_str, "Enter information and press RETURN,");
    LENGTH(l_str_descrip) = strlen(l_str);
    if (!(1 & (l_rc = smg$put_chars (
	      &userinput_display_id,
	      &l_str_descrip,
	      &7,
	      &20,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    strcpy(l_str, "or press keypad 0 to exit");
    LENGTH(l_str_descrip) = strlen(l_str);
    if (!(1 & (l_rc = smg$put_chars (
	      &userinput_display_id,
	      &l_str_descrip,
	      &8,
	      &20,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);

    l_initial_str[0] = EOS;
    strcpy(l_initial_str, a_start_str);
    strcpy(l_resultant_str, "");

    read_string_and_clear_errmsg(
		&l_read_area_display,
		l_initial_str,
		l_resultant_str,
		&l_terminator,
		(a_func != filer_launch$1));
 
    strcpy(l_initial_str, l_resultant_str);
 
    while (l_terminator != SMG$K_TRM_KP0)
    {
        for (l_i = 0;  l_i < strlen(l_resultant_str);  l_i++)
        {
            l_upper_str[l_i] = toupper (l_resultant_str[l_i]);
        }
	l_upper_str[l_i] = EOS;
 
	/*
	**	Output 'working...' message if total processing time
	**	is longer than 3 seconds.  Needs to disable broadcast message
	**	first because of interference.
	**/
 
#ifndef DEBUG
	if (!(1 & (l_rc = smg$disable_broadcast_trapping (
		     &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);
 
	if (!(1 & (l_rc = dxscr_working_start(userinput_display_id, USERINPUT_BOTTOM, 1, 0, &l_cstat))))
	    $LOG(l_rc);
#endif
 
	l_status = (*a_func)(l_upper_str, a_from, a_err_str);
 
	/*
	**	Set the signal back to system default
	**/
 
#ifndef DEBUG
	alarm(0);
	signal(SIGALRM, l_cstat);
#endif
	if (!(1 & (l_rc = smg$erase_chars (
		     &userinput_display_id,
		     &10,
		     &USERINPUT_BOTTOM,
		     &1))))
	    $LOG(l_rc);
	if (!(1 & (l_rc = smg$set_broadcast_trapping (
		     &cntrl_info_block.pasteboard_id,
		     broadcast_routine,
		     0))))
	    $LOG(l_rc);
 
	if (l_status != DX__ERROR)
	{	    /**  Success.  Exit loop  **/
	    break;
	}
	else
	{
	    LENGTH(l_err_str_descrip) = strlen(a_err_str);
	    if (!(1 & (l_rc = smg$put_chars (
		      &userinput_display_id,
		      &l_err_str_descrip,
		      &USERINPUT_BOTTOM,
		      &1,
		      &SMG$M_ERASE_TO_EOL,
		      &SMG$M_BOLD,
		      0,
		      0))))
	        $LOG(l_rc);
	}
 
	if (!(1 & (l_rc = smg$check_for_occlusion (
		  &userinput_display_id,
		  &cntrl_info_block.pasteboard_id,
		  &l_occlusion_state))))
	    $LOG(l_rc);
 
	if (l_occlusion_state)
	{
            if (!(1 & (l_rc = smg$begin_pasteboard_update (
                         &cntrl_info_block.pasteboard_id))))
                $LOG(l_rc);
	    if (!(1 & (l_rc = smg$repaste_virtual_display (
			&userinput_display_id,
			&cntrl_info_block.pasteboard_id,
			&l_userinput_pasteboard_row,
			&USERINPUT_PASTEBOARD_COLUMN,
			0))))
	        $LOG(l_rc);
            if (!(1 & (l_rc = smg$repaste_virtual_display (
                         &l_read_area_display.id,
			 &cntrl_info_block.pasteboard_id,
                         &l_read_area_display.beg_y,
                         &l_read_area_display.beg_x,
                         0))))
                $LOG(l_rc);
            if (!(1 & (l_rc = smg$end_pasteboard_update (
                         &cntrl_info_block.pasteboard_id))))
                $LOG(l_rc);
	}
 
	if (!(1 & (l_rc = smg$ring_bell (
		  &userinput_display_id,
		  &1))))
	    $LOG(l_rc);
 
	read_string_and_clear_errmsg(
	    &l_read_area_display,
	    l_initial_str,
	    l_resultant_str,
	    &l_terminator,
	    (a_func != filer_launch$1));
	strcpy(l_initial_str, l_resultant_str);
    }
 
    if (l_terminator == SMG$K_TRM_KP0)
    {
	if (a_func == dirtree_find_first$1)
	    strcpy(a_err_str, "FIND FIRST");
	else if (a_func == filer_append$1)
	    strcpy(a_err_str, "APPEND");
	else if (a_func == filer_copy$1)
	    strcpy(a_err_str, "COPY");
	else if (a_func == select_a_editor)
	    strcpy(a_err_str, "EDITOR SELECTION");
	else if (a_func == select_a_display_date)
	    strcpy(a_err_str, "SELECT DATE TYPE");
	else if (a_func == confirm_delete)
	    strcpy(a_err_str, "SET DELETE CONFIRMATION");
	else if (a_func == confirm_purge)
	    strcpy(a_err_str, "SET PURGE CONFIRMATION");
	else if (a_func == select_type_method)
	    strcpy(a_err_str, "SET TYPE BY PAGE");
	else if (a_func == auto_update)
	    strcpy(a_err_str, "AUTO UPDATE");
	else if (a_func == display_clock)
	    strcpy(a_err_str, "SET CLOCK");
	else if (a_func == update_in_second)
	    strcpy(a_err_str, "UPDATE CLOCK");
	else if (a_func == save_setting)
	    strcpy(a_err_str, "SAVE SETTINGS");
	else if (a_func == restore_setting)
	    strcpy(a_err_str, "RESTORE SETTINGS");
	else if (a_func == filer_delete$1)
	    strcpy(a_err_str, "DELETE");
	else if (a_func == filer_diff$1)
	    strcpy(a_err_str, "COMPARE");
	else if (a_func == vms_command)
	    strcpy(a_err_str, "EXECUTE VMS COMMAND");
	else if (a_func == filer_filter$1)
	    strcpy(a_err_str, "FILTER");
	else if (a_func == filer_find$1)
	    strcpy(a_err_str, "FIND");
	else if (a_func == filer_find_first$1)
	    strcpy(a_err_str, "FIND FIRST");
	else if (a_func == filer_launch$1)
	    strcpy(a_err_str, "LAUNCH");
	else if (a_func == print_filer_display)
	    strcpy(a_err_str, "PRINT DISPLAY");
	else if (a_func == filer_goto$1)
	    strcpy(a_err_str, "GOTO");
	else if (a_func == filer_mkdir$1)
	    strcpy(a_err_str, "CREATE DIRECTORY");
	else if (a_func == filer_protection$1)
	    strcpy(a_err_str, "SET PROTECTION");
	else if (a_func == filer_purge$1)
	    strcpy(a_err_str, "PURGE");
	else if (a_func == filer_rename$1)
	    strcpy(a_err_str, "RENAME");
	else if (a_func == filer_sort$1)
	    strcpy(a_err_str, "SORT");
	else
	    strcpy(a_err_str, "");
 
	strcat(a_err_str, " canceled by request");
	signal_err(a_err_str, silence);
    }
 
    if (!(1 & (l_rc = smg$begin_pasteboard_update (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$delete_virtual_display (
		&userinput_display_id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$delete_virtual_display (
                 &l_read_area_display.id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$end_pasteboard_update (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    return DX__DONTCARE;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      TBS
**
**--
**/
static int	read_string_and_clear_errmsg(
			struct display_tag *a_display,
			char *a_initial_str,
			char *a_resultant_str,
			unsigned short *a_terminatorp,
			unsigned short a_input_required)
{
    int l_cursor_pos = 1;
    int l_len = 0;
    int l_p;
    int l_scroll_num;
    char l_char_str[2];
    enum mode_tag l_mode = replace;
    int l_err = 0;
    short int l_recall_line;
    int l_allocate_size;
    int l_i;
    int l_sav_cursor, l_del_chars;
    int l_timeout;
    unsigned int l_status;
    unsigned int l_clock_display_id;
    unsigned long l_rc;
 
    l_recall_line = (!strcmp(a_resultant_str, "")? -1 : 0);
    if (!(1 & (l_rc = smg$erase_display (
                 &a_display->id,
                 0,
                 0,
                 0,
                 0))))
        $LOG(l_rc);
    a_display->view_beg_x = 1;
    if (!(1 & (l_rc = smg$change_viewport (
                 &a_display->id,
                 0,
                 &a_display->view_beg_x,
                 0,
                 0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$begin_display_update (
                 &a_display->id))))
        $LOG(l_rc);
    put_chars_in_userinput (&l_cursor_pos, &l_len, a_initial_str,
	a_resultant_str, a_display, userinput_display_id, l_mode, &l_err);
    if (!(1 & (l_rc = smg$end_display_update (
                 &a_display->id))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
                 &a_display->id,
                 0,
                 &l_cursor_pos))))
        $LOG(l_rc);
 
    l_clock_display_id = dxscr_clock_display_id();
 
    /*
    **	Read user input until user type RETURN to commit or
    **	keypad 0 to cancel
    **/
 
    l_timeout = cntrl_info_block.user_pref.update_in_second? 1 : 60;
    do {
	l_status = smg$read_keystroke (
			     &cntrl_info_block.keyboard_id,
			     a_terminatorp,
			     0,
			     &l_timeout,
			     &a_display->id,
			     0,
			     0);
	dxscr_tick_clock(l_clock_display_id);
    } while (l_status == SS$_TIMEOUT || *a_terminatorp == SMG$K_TRM_CANCELLED);
 
    /**  Clear error message if any  **/
    if (!(1 & (l_rc = smg$begin_pasteboard_update (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$erase_line (
		 &userinput_display_id,
		 &USERINPUT_BOTTOM,
		 &1))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$set_cursor_abs (
                 &a_display->id,
                 &1,
                 &l_cursor_pos))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$end_pasteboard_update (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    while (*a_terminatorp != SMG$K_TRM_KP0)
    {
        switch (*a_terminatorp)
        {
            case SMG$K_TRM_LEFT:
                if (l_cursor_pos == 1)
                {
                    if (!(1 & (l_rc = smg$ring_bell (
                                 &a_display->id,
                                 0))))
                        $LOG(l_rc);
                    if (!(1 & (l_rc = smg$put_chars (
                                 &userinput_display_id,
                                 $DESCR ("Already at the leftmost column"),
                                 &USERINPUT_BOTTOM,
                                 &1,
				 &SMG$M_ERASE_TO_EOL,
                                 &SMG$M_BOLD,
                                 0,
                                 0))))
                        $LOG(l_rc);
                    if (!(1 & (l_rc = smg$set_cursor_abs (
                                 &a_display->id,
                                 &1,
                                 &1))))
                        $LOG(l_rc);
		    l_err++;
                }
		else
		{
		    l_cursor_pos--;
 
		    /*
		    **	Scroll display right if cursor falls off left margin
		    **/
 
                    if (l_cursor_pos < a_display->view_beg_x)
                    {
                        l_scroll_num = a_display->view_width/2;
			a_display->view_beg_x = max (
			    1, a_display->view_beg_x-l_scroll_num);
                        if (!(1 & (l_rc = smg$change_viewport (
                                     &a_display->id,
                                     0,
                                     &a_display->view_beg_x,
                                     0,
                                     0))))
                            $LOG(l_rc);
                    }
		    if (!(1 & (l_rc = smg$set_cursor_abs (
				 &a_display->id,
				 0,
				 &l_cursor_pos))))
		        $LOG(l_rc);
		}
		break;
            case SMG$K_TRM_CTRLU:
		l_cursor_pos--;
                if (!(1 & (l_rc = smg$delete_chars (
                             &a_display->id,
                             &l_cursor_pos,
                             &1,
                             &1))))
                    $LOG(l_rc);
		l_len -= l_cursor_pos;
		strncpy(a_resultant_str, a_resultant_str+l_cursor_pos, l_len);
		l_cursor_pos = 1;
		a_display->view_beg_x = 1;
		if (!(1 & (l_rc = smg$change_viewport (
			     &a_display->id,
			     0,
			     &a_display->view_beg_x,
			     0,
			     0))))
		    $LOG(l_rc);
		if (!(1 & (l_rc = smg$set_cursor_abs (
			     &a_display->id,
			     0,
			     &l_cursor_pos))))
		    $LOG(l_rc);
		break;
            case SMG$K_TRM_CTRLJ:	/**  Delete a word from left  **/
		l_sav_cursor = l_cursor_pos;
                while (l_cursor_pos > 1 && a_resultant_str[l_cursor_pos-2] == ' ')
                {
		    l_cursor_pos--;
		    l_len--;
                }
                while (l_cursor_pos > 1 && a_resultant_str[l_cursor_pos-2] != ' ')
                {
		    l_cursor_pos--;
		    l_len--;
                }
		l_scroll_num = a_display->view_width/2;
                while (l_cursor_pos <= a_display->view_beg_x &&
		    a_display->view_beg_x > 1)
                {
		    a_display->view_beg_x = max (
			1, a_display->view_beg_x-l_scroll_num);
                }
		l_del_chars = l_sav_cursor-l_cursor_pos;
                if (!(1 & (l_rc = smg$change_viewport (
                             &a_display->id,
                             0,
                             &a_display->view_beg_x,
                             0,
                             0))))
                    $LOG(l_rc);
                if (l_del_chars > 0)
                {
		    if (!(1 & (l_rc = smg$delete_chars (
				 &a_display->id,
				 &l_del_chars,
				 &1,
				 &l_cursor_pos))))
		        $LOG(l_rc);
		    for (l_p = l_cursor_pos-1;  l_p < l_len;  l_p++)
		    {
			a_resultant_str[l_p] = a_resultant_str[l_p+l_del_chars];
		    }
		    a_resultant_str[l_p] = EOS;
                }
                if (!(1 & (l_rc = smg$set_cursor_abs (
                             &a_display->id,
                             0,
                             &l_cursor_pos))))
                    $LOG(l_rc);
		break;
            case SMG$K_TRM_DELETE:
                if (l_cursor_pos == 1)
                {
                    if (!(1 & (l_rc = smg$ring_bell (
                                 &a_display->id,
                                 0))))
                        $LOG(l_rc);
                    if (!(1 & (l_rc = smg$put_chars (
                                 &userinput_display_id,
                                 $DESCR ("Already at the leftmost column"),
                                 &USERINPUT_BOTTOM,
                                 &1,
				 &SMG$M_ERASE_TO_EOL,
                                 &SMG$M_BOLD,
                                 0,
                                 0))))
                        $LOG(l_rc);
                    if (!(1 & (l_rc = smg$set_cursor_abs (
                                 &a_display->id,
                                 &1,
                                 &1))))
                        $LOG(l_rc);
		    l_err++;
                }
		else
		{
		    l_cursor_pos--;
 
		    /*
		    **	Scroll display right if cursor falls off left margin
		    **/
 
                    if (l_cursor_pos <= a_display->view_beg_x)
                    {
                        l_scroll_num = a_display->view_width/2;
			a_display->view_beg_x = max (
			    1, a_display->view_beg_x-l_scroll_num);
                        if (!(1 & (l_rc = smg$change_viewport (
                                     &a_display->id,
                                     0,
                                     &a_display->view_beg_x,
                                     0,
                                     0))))
                            $LOG(l_rc);
                    }
 
		    /*
		    **	Shift down 1 character
		    **/
 
                    if (!(1 & (l_rc = smg$begin_display_update (
                                 &a_display->id))))
                        $LOG(l_rc);
		    if (!(1 & (l_rc = smg$set_cursor_abs (
				 &a_display->id,
				 0,
				 &l_cursor_pos))))
		        $LOG(l_rc);
		    if (!(1 & (l_rc = smg$delete_chars (
				 &a_display->id,
				 &1,
				 &1,
				 &l_cursor_pos))))
		        $LOG(l_rc);
                    if (!(1 & (l_rc = smg$end_display_update (
                                 &a_display->id))))
                        $LOG(l_rc);
		    l_len--;
		    for (l_p = l_cursor_pos;  l_p <= l_len;  l_p++)
		    {
			a_resultant_str[l_p-1] = a_resultant_str[l_p];
		    }
		    a_resultant_str[l_p] = EOS;
		}
		break;
            case SMG$K_TRM_RIGHT:
                if (l_cursor_pos < l_len+1)
                {
                    l_cursor_pos++;
		    if (l_cursor_pos >= a_display->view_beg_x+a_display->view_width)
		    {
			l_scroll_num = min(
			    a_display->view_width/2,
			    FIELD_LENGTH-(a_display->view_beg_x+a_display->view_width-1));
			a_display->view_beg_x += l_scroll_num;
			if (!(1 & (l_rc = smg$change_viewport (
				     &a_display->id,
				     0,
				     &a_display->view_beg_x,
				     0,
				     0))))
			    $LOG(l_rc);
		    }
		    if (!(1 & (l_rc = smg$set_cursor_abs (
				 &a_display->id,
				 &1,
				 &l_cursor_pos))))
		        $LOG(l_rc);
                }
		break;
            case SMG$K_TRM_UP:
		if (!(1 & (l_rc = smg$erase_display (
			     &a_display->id,
			     0,
			     0,
			     0,
			     0))))
		    $LOG(l_rc);
		if (l_recall_line < 0)
		{
		    /**  First UP in this field -- nothing to index into
		    **   recall_buf[] with yet, start at the most recent
		    **   entry instead of reading recall_buf[-1].  **/
		    l_recall_line = 0;
		}
		else if (recall_buf[l_recall_line] != NULL)
		{
		    ++l_recall_line;
		    l_recall_line %= MAX_RECALL_LINES;
		}

		if (recall_buf[l_recall_line] == NULL)
		{
		    strcpy(a_resultant_str, "");
		    l_len = strlen(a_resultant_str);
		    l_cursor_pos = 1;
		    a_display->view_beg_x = 1;
		    if (!(1 & (l_rc = smg$change_viewport (
				 &a_display->id,
				 0,
				 &a_display->view_beg_x,
				 0,
				 0))))
		        $LOG(l_rc);
		    if (!(1 & (l_rc = smg$set_cursor_abs (
				 &a_display->id,
				 &1,
				 &l_cursor_pos))))
		        $LOG(l_rc);
		}
		else
		{
		    strcpy(a_initial_str, recall_buf[l_recall_line]);
		    l_len = 0;
		    l_cursor_pos = 1;
		    a_display->view_beg_x = 1;
		    if (!(1 & (l_rc = smg$change_viewport (
				 &a_display->id,
				 0,
				 &a_display->view_beg_x,
				 0,
				 0))))
		        $LOG(l_rc);
		    if (!(1 & (l_rc = smg$begin_display_update (
				 &a_display->id))))
		        $LOG(l_rc);
		    put_chars_in_userinput (&l_cursor_pos, &l_len, a_initial_str,
			a_resultant_str, a_display, userinput_display_id, l_mode, &l_err);
		    if (!(1 & (l_rc = smg$end_display_update (
				 &a_display->id))))
		        $LOG(l_rc);
		    if (!(1 & (l_rc = smg$set_cursor_abs (
				 &a_display->id,
				 &1,
				 &l_cursor_pos))))
		        $LOG(l_rc);
		}
		break;
            case SMG$K_TRM_DOWN:
		if (!(1 & (l_rc = smg$erase_display (
			     &a_display->id,
			     0,
			     0,
			     0,
			     0))))
		    $LOG(l_rc);
                if (l_recall_line <= 0)
                {
		    strcpy(a_resultant_str, "");
		    l_len = 0;
		    l_cursor_pos = 1;
		    a_display->view_beg_x = 1;
		    if (!(1 & (l_rc = smg$change_viewport (
				 &a_display->id,
				 0,
				 &a_display->view_beg_x,
				 0,
				 0))))
		        $LOG(l_rc);
		    if (!(1 & (l_rc = smg$set_cursor_abs (
				 &a_display->id,
				 &1,
				 &l_cursor_pos))))
		        $LOG(l_rc);
                    if (l_recall_line == 0)
                        l_recall_line--;
                }
		else
                {
		    l_recall_line--;
		    strcpy(a_initial_str, recall_buf[l_recall_line]);
		    l_len = 0;
		    l_cursor_pos = 1;
		    a_display->view_beg_x = 1;
		    if (!(1 & (l_rc = smg$change_viewport (
				 &a_display->id,
				 0,
				 &a_display->view_beg_x,
				 0,
				 0))))
		        $LOG(l_rc);
		    if (!(1 & (l_rc = smg$begin_display_update (
				 &a_display->id))))
		        $LOG(l_rc);
		    put_chars_in_userinput (&l_cursor_pos, &l_len, a_initial_str,
			a_resultant_str, a_display, userinput_display_id, l_mode, &l_err);
		    if (!(1 & (l_rc = smg$end_display_update (
				 &a_display->id))))
		        $LOG(l_rc);
		    if (!(1 & (l_rc = smg$set_cursor_abs (
				 &a_display->id,
				 0,
				 &l_cursor_pos))))
		        $LOG(l_rc);
		}
                break;
            case SMG$K_TRM_E1:		/**  Home (Find)  **/
            case SMG$K_TRM_CTRLH:
                a_display->view_beg_x = 1;
                if (!(1 & (l_rc = smg$change_viewport (
                             &a_display->id,
                             &1,
                             &1,
                             0,
                             0))))
                    $LOG(l_rc);
		l_cursor_pos = 1;
                if (!(1 & (l_rc = smg$set_cursor_abs (
                             &a_display->id,
                             &1,
                             &1))))
                    $LOG(l_rc);
		break;
            case SMG$K_TRM_E4:		/**  End (Select)  **/
            case SMG$K_TRM_CTRLE:
                while (l_len+1 > a_display->view_beg_x+a_display->view_width-1)
                {
		    l_scroll_num = min(
			a_display->view_width/2,
			FIELD_LENGTH-(a_display->view_beg_x+a_display->view_width-1));
                    if (l_scroll_num <= 0)
                        break;
                    else
			a_display->view_beg_x += l_scroll_num;
                }
                if (!(1 & (l_rc = smg$change_viewport (
                             &a_display->id,
                             0,
                             &a_display->view_beg_x,
                             0,
                             0))))
                    $LOG(l_rc);
		l_cursor_pos = l_len+1;
                if (!(1 & (l_rc = smg$set_cursor_abs (
                             &a_display->id,
                             0,
                             &l_cursor_pos))))
                    $LOG(l_rc);
		break;
            case SMG$K_TRM_HT:
		/**  insert five spaces  **/
		if (!(1 & (l_rc = smg$begin_display_update (
			     &a_display->id))))
		    $LOG(l_rc);
		put_chars_in_userinput (
		    &l_cursor_pos, &l_len, "     ", a_resultant_str,
		    a_display, userinput_display_id, insert, &l_err);
		if (!(1 & (l_rc = smg$end_display_update (
			     &a_display->id))))
		    $LOG(l_rc);
		break;
            case SMG$K_TRM_CTRLA:
                l_mode = (l_mode == replace)? insert : replace;
		break;
            case SMG$K_TRM_CR:
            case SMG$K_TRM_ENTER:
                if (l_len > 0 || !a_input_required)
		{
		    a_resultant_str[l_len] = EOS;
                    if (!(1 & (l_rc = smg$set_cursor_abs (
                                 &userinput_display_id,
                                 &USERINPUT_BOTTOM,
                                 &1))))
                        $LOG(l_rc);
                    if (recall_buf[MAX_RECALL_LINES-1] != NULL)
                    {
			l_allocate_size = strlen(recall_buf[MAX_RECALL_LINES-1]);
			l_allocate_size++;
                        if (!(1 & (l_rc = lib$free_vm (
                                     &l_allocate_size,
                                     &recall_buf[MAX_RECALL_LINES-1],
                                     0))))
                            $LOG(l_rc);
                    }
                    for (l_i = MAX_RECALL_LINES-1;  l_i > 0;  l_i--)
                    {
                        recall_buf[l_i] = recall_buf[l_i-1];
                    }
		    l_allocate_size = l_len+1;
                    if (!(1 & (l_rc = lib$get_vm (
                                 &l_allocate_size,
                                 &recall_buf[0],
                                 0))))
                        return $LOG(l_rc);
		    memcpy(recall_buf[0], a_resultant_str, l_len+1);
		    return DX__NORMAL;
		}
		else
                {
                    if (!(1 & (l_rc = smg$ring_bell (
                                 &a_display->id,
                                 0))))
                        $LOG(l_rc);
                    if (!(1 & (l_rc = smg$put_chars (
                                 &userinput_display_id,
                                 $DESCR ("Input required"),
                                 &USERINPUT_BOTTOM,
                                 &1,
				 &SMG$M_ERASE_TO_EOL,
                                 &SMG$M_BOLD,
                                 0,
                                 0))))
                        $LOG(l_rc);
                    if (!(1 & (l_rc = smg$set_cursor_abs (
                                 &a_display->id,
                                 &1,
                                 &1))))
                        $LOG(l_rc);
		    l_err++;
                }
		break;
            default:
		/**  a key code >= 256 (F8, Help, Do, E2...) is not a character:  **/
		/**  (char) would cut it to a printable one (311 -> "7")          **/
		if (*a_terminatorp >= 256)
		    break;
		l_char_str[0] = (char) *a_terminatorp;
		l_char_str[1] = EOS;
		put_chars_in_userinput (
		    &l_cursor_pos, &l_len, l_char_str, a_resultant_str,
		    a_display, userinput_display_id, l_mode, &l_err);
        }
 
	do {
	    l_status = smg$read_keystroke (
				 &cntrl_info_block.keyboard_id,
				 a_terminatorp,
				 0,
				 &l_timeout,
				 &a_display->id,
				 0,
				 0);
	    dxscr_tick_clock(l_clock_display_id);
	} while (l_status == SS$_TIMEOUT || *a_terminatorp == SMG$K_TRM_CANCELLED);
 
        if (l_err)
        {
	    l_err = 0;
	    if (!(1 & (l_rc = smg$begin_pasteboard_update (
			 &cntrl_info_block.pasteboard_id))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$erase_line (
			 &userinput_display_id,
			 &USERINPUT_BOTTOM,
			 &1))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$set_cursor_abs (
			 &a_display->id,
			 &1,
			 &l_cursor_pos))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$end_pasteboard_update (
			 &cntrl_info_block.pasteboard_id))))
	        $LOG(l_rc);
        }
    }
    return DX__DONTCARE;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      TBS
**
**--
**/
static int	put_chars_in_userinput (a_cursor_pos, a_len, a_input_str,
			a_resultant_str, a_display, a_userinput_display_id, a_mode, a_err)
int *a_cursor_pos,
    *a_len;
char *a_input_str;
char *a_resultant_str;
struct display_tag *a_display;
unsigned int a_userinput_display_id;
enum mode_tag a_mode;
int *a_err;
{
    int l_scroll_num;
    char l_char_str[1];
    $DESCRIPTOR (l_char_str_descrip, l_char_str);
    int l_i, l_j;
    unsigned long l_rc;
 
    /*
    **	Echo the user input on to the screen.
    **	Stop when the field is full.
    **/
 
    LENGTH(l_char_str_descrip) = 1;
    for (l_i = 0;  l_i < strlen(a_input_str);  l_i++)
    {
	if (isprint (a_input_str[l_i]))
	{
	    l_char_str[0] =  a_input_str[l_i];
	    if (a_mode == replace && *a_cursor_pos <= *a_len)
	    {
		if (!(1 & (l_rc = smg$put_chars (
			     &a_display->id,
			     &l_char_str_descrip,
			     0,
			     0,
			     0,
			     0,
			     0,
			     0))))
		    $LOG(l_rc);
		 a_resultant_str[*a_cursor_pos-1] = a_input_str[l_i];
		(*a_cursor_pos)++;
	    }
	    else if (*a_len >= FIELD_LENGTH)
	    {
		if (!(1 & (l_rc = smg$ring_bell (
			     &a_display->id,
			     0))))
		    $LOG(l_rc);
		if (!(1 & (l_rc = smg$put_chars (
			     &a_userinput_display_id,
			     $DESCR ("Field full"),
			     &USERINPUT_BOTTOM,
			     &1,
			     &SMG$M_ERASE_TO_EOL,
			     &SMG$M_BOLD,
			     0,
			     0))))
		    $LOG(l_rc);
		if (!(1 & (l_rc = smg$set_cursor_abs (
			     &a_display->id,
			     &1,
			     a_cursor_pos))))
		    $LOG(l_rc);
 
		(*a_err)++;
		return DX__DONTCARE;
	    }
	    else
	    {
		if (!(1 & (l_rc = smg$insert_chars (
			     &a_display->id,
			     &l_char_str_descrip,
			     &1,
			     a_cursor_pos,
			     0,
			     0,
			     0))))
		    $LOG(l_rc);
		 /**  Shift right 1 character  **/
		 for (l_j = *a_len;  l_j >= *a_cursor_pos;  l_j--)
		 {
		     a_resultant_str[l_j] = a_resultant_str[l_j-1];
		 }
		 a_resultant_str[*a_cursor_pos-1] = a_input_str[l_i];
		 (*a_cursor_pos)++;
		 (*a_len)++;
	    }
	    if (*a_cursor_pos >= a_display->view_beg_x+a_display->view_width)
	    {
		l_scroll_num = min(
		    a_display->view_width/2,
		    FIELD_LENGTH-(a_display->view_beg_x+a_display->view_width-1));
		a_display->view_beg_x += l_scroll_num;
		if (!(1 & (l_rc = smg$change_viewport (
			     &a_display->id,
			     0,
			     &a_display->view_beg_x,
			     0,
			     0))))
		    $LOG(l_rc);
                if (!(1 & (l_rc = smg$set_cursor_abs (
                             &a_display->id,
                             &1,
                             a_cursor_pos))))
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
**      TBS
**
**--
**/
int	signal_err(a_err_str, a_ring)
char *a_err_str;
enum rings a_ring;
{
    unsigned int l_status_flags;
    char l_msg[256];
    $DESCRIPTOR (l_msg_descrip, l_msg);
    unsigned long l_rc;

    /*  14-Sep-2026 RRL: NOT wrapped in smg$begin/end_display_update --
    **  tried that, but it broke smg$get_pasting_info/smg$(re)paste_
    **  virtual_display below (%DX-F-SFC in live testing).  Those are
    **  structural pasteboard operations, not content writes to an
    **  already-pasted display -- begin/end_display_update batching is
    **  only valid around the latter (see dxscr_ljust()-adjacent
    **  functions elsewhere for correctly-scoped examples).  **/
    if (!(1 & (l_rc = smg$erase_display (
	      &cntrl_info_block.status_display.id,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);

    /*
    **  Repaste the display if it has not been pasted already (not
    **	a normal case.)  Otherwise, just paste the display.
    **/
 
    if (!(1 & (l_rc = smg$get_pasting_info (
	      &cntrl_info_block.status_display.id,
	      &cntrl_info_block.pasteboard_id,
	      &l_status_flags,
	      0,
	      0))))
        $LOG(l_rc);
 
    if (l_status_flags == SMG$M_DISPLAY_PASTED)
 
	if (!(1 & (l_rc = smg$repaste_virtual_display (
		  &cntrl_info_block.status_display.id,
		  &cntrl_info_block.pasteboard_id,
		  &STATUS_PBD_ROW,
		  &STATUS_PBD_COLUMN,
		  0))))
	    $LOG(l_rc);
 
    else
 
	if (!(1 & (l_rc = smg$paste_virtual_display (
		&cntrl_info_block.status_display.id,
		&cntrl_info_block.pasteboard_id,
		&STATUS_PBD_ROW,
		&STATUS_PBD_COLUMN,
		0))))
	    $LOG(l_rc);
 
    if (a_ring == bell)	    /**  Ring a bell if error occurred  **/
 
	if (!(1 & (l_rc = smg$ring_bell (
		  &cntrl_info_block.status_display.id,
		  &1))))
	    $LOG(l_rc);
 
    /*
    **  Write a error message at the specific location
    **/
 
    strcpy(l_msg, a_err_str);
    LENGTH(l_msg_descrip) = strlen(l_msg);
    if (!(1 & (l_rc = smg$put_chars (
	     &cntrl_info_block.status_display.id,
	     &l_msg_descrip,
	     &1,
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
**      MULTI_GET_USERINPUT_AND_EXEC() prompts the user for information
**	needed and call the actual function to perform desired operation.
**
**  MODIFICATION HISTORY:
**
**--
**/
int	multi_get_userinput_and_exec(
	    a_func,
	    a_title_str,
	    a_prompt_str,
	    a_start_str,
	    a_err_str)
int (*a_func)();
char *a_title_str;
char *a_prompt_str;
char *a_start_str;
char *a_err_str;
{
    struct display_tag l_read_area_display;
    register int l_i;
    long int l_title_start_row = 2;
    long int l_title_start_column;
    long int l_prompt_start_row = 5;
    long int l_prompt_start_column = 10;
    long int l_input_start_column;
    int l_lastrow;
    unsigned short l_terminator;
    char l_initial_str[FIELD_LENGTH+1];
    char l_resultant_str[FIELD_LENGTH+1];
    char l_upper_str[FIELD_LENGTH+1];
    char l_filler[FIELD_LENGTH+1];
    $DESCRIPTOR (l_filler_descrip, l_filler);
    $DESCRIPTOR (l_title_descrip, a_title_str);
    $DESCRIPTOR (l_prompt_str_descrip, a_prompt_str);
    $DESCRIPTOR (l_err_str_descrip, a_err_str);
    static char l_str[128];
    $DESCRIPTOR (l_str_descrip, l_str);
    int l_userinput_pasteboard_row =
	cntrl_info_block.pasteboard_rows-USERINPUT_BOTTOM+1;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = smg$create_virtual_display (
	    &USERINPUT_BOTTOM,
	    &USERINPUT_RIGHTMOST,
	    &userinput_display_id,
	    0,
	    0,
	    0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
	    &userinput_display_id,
	    &cntrl_info_block.pasteboard_id,
	    &l_userinput_pasteboard_row,
	    &USERINPUT_PASTEBOARD_COLUMN,
	    0))))
        $LOG(l_rc);
 
    LENGTH(l_title_descrip) = strlen(a_title_str);
    l_title_start_column = max ( 1,
			 (USERINPUT_RIGHTMOST-strlen(a_title_str))/2);
 
    if (!(1 & (l_rc = smg$put_chars (
	      &userinput_display_id,
	      &l_title_descrip,
	      &l_title_start_row,
	      &l_title_start_column,
	      0,
	      &SMG$M_BOLD,
	      0,
	      0))))
        $LOG(l_rc);
 
    LENGTH(l_prompt_str_descrip) = strlen(a_prompt_str);
    if (!(1 & (l_rc = smg$put_chars (
	      &userinput_display_id,
	      &l_prompt_str_descrip,
	      &l_prompt_start_row,
	      &l_prompt_start_column,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    /*
    **	CREATE USER READ AREA
    **/
 
    l_read_area_display.rows = 1;
    l_read_area_display.width = FIELD_LENGTH+1;
    l_read_area_display.beg_x = strlen(a_prompt_str)+l_prompt_start_column;
    l_read_area_display.beg_y = l_userinput_pasteboard_row+l_prompt_start_row-1;
 
    l_read_area_display.view_rows = 1;
    l_input_start_column = strlen(a_prompt_str)+l_prompt_start_column;
    l_read_area_display.view_width =
	USERINPUT_RIGHTMOST-l_input_start_column-l_prompt_start_column;
    l_read_area_display.view_beg_x = 1;
    l_read_area_display.view_beg_y = 1;
 
    if (!(1 & (l_rc = smg$create_virtual_display (
                 &l_read_area_display.rows,
                 &l_read_area_display.width,
                 &l_read_area_display.id,
                 0,
                 &SMG$M_UNDERLINE,
                 0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$create_viewport (
                 &l_read_area_display.id,
                 &l_read_area_display.view_beg_y,
                 &l_read_area_display.view_beg_x,
                 &l_read_area_display.view_rows,
                 &l_read_area_display.view_width))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
                 &l_read_area_display.id,
                 &cntrl_info_block.pasteboard_id,
                 &l_read_area_display.beg_y,
                 &l_read_area_display.beg_x,
                 0))))
        $LOG(l_rc);
 
    memset(l_filler, ' ', FIELD_LENGTH);
 
    LENGTH(l_filler_descrip) = FIELD_LENGTH;
    if (!(1 & (l_rc = smg$put_chars (
		&l_read_area_display.id,
		&l_filler_descrip,
		&1,
		&1,
		0,
		0,
		0,
		0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$erase_display (
                 &l_read_area_display.id,
                 0,
                 0,
                 0,
                 0))))
        $LOG(l_rc);
 
    strcpy(l_str, "Enter information and press RETURN, ");
    LENGTH(l_str_descrip) = strlen(l_str);
    if (!(1 & (l_rc = smg$put_chars (
	      &userinput_display_id,
	      &l_str_descrip,
	      &7,
	      &20,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
 
    strcpy(l_str, "or press keypad 0 to exit");
    LENGTH(l_str_descrip) = strlen(l_str);
    if (!(1 & (l_rc = smg$put_chars (
	      &userinput_display_id,
	      &l_str_descrip,
	      &8,
	      &20,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    strcpy(l_initial_str, a_start_str);
 
    read_string_and_clear_errmsg(
		&l_read_area_display,
		l_initial_str,
		l_resultant_str,
		&l_terminator,
		(a_func != filer_launch$1));
 
    if (!(1 & (l_rc = smg$begin_pasteboard_update (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$delete_virtual_display (
		&userinput_display_id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$delete_virtual_display (
                 &l_read_area_display.id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$end_pasteboard_update (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    if (l_terminator != SMG$K_TRM_KP0)
    {
	l_lastrow = l_userinput_pasteboard_row+USERINPUT_BOTTOM-1;
 
	if (!(1 & (l_rc = smg$set_physical_cursor (
		  &cntrl_info_block.pasteboard_id,
		  &l_lastrow,
		  &USERINPUT_PASTEBOARD_COLUMN))))
	    $LOG(l_rc);
 
        for (l_i = 0;  l_i < strlen(l_resultant_str);  l_i++)
        {
            l_upper_str[l_i] = toupper (l_resultant_str[l_i]);
        }
	l_upper_str[l_i] = EOS;
 
	return filer_multiple(a_func, l_upper_str, a_err_str);
    }
 
    if (a_func == filer_copy$1)
	strcpy(a_err_str, "MULTIPLE COPY");
    else if (a_func == filer_find$1)
	strcpy(a_err_str, "MULTIPLE FIND");
    else if (a_func == filer_rename$1)
	strcpy(a_err_str, "MULTIPLE RENAME");
    else if (a_func == filer_protection$1)
	strcpy(a_err_str, "MULTIPLE SET PROTECTION");
    else if (a_func == filer_delete$1)
	strcpy(a_err_str, "MULTIPLE DELETE");
    else if (a_func == filer_purge$1)
	strcpy(a_err_str, "MULTIPLE PURGE");
    else if (a_func == filer_launch$1)
	strcpy(a_err_str, "MULTIPLE LAUNCH");
    else if (a_func == filer_diff$1)
	strcpy(a_err_str, "MULTIPLE COMPARE");
    else
	strcpy(a_err_str, "");
 
    strcat(a_err_str, " canceled by request");
    signal_err(a_err_str, silence);
 
    return 0;
}
 
 

/*
**  State of the "Working..." indicator, set by dxscr_working_start() and
**  used by the two SIGALRM handlers below.  Only one indicator runs at a
**  time: every caller disarms it (alarm(0) + old handler back) before
**  the next one can start.
**/
static unsigned int	s_working_display_id;	/* Display where "Working..." blinks			*/
static int		s_working_row;		/* Row of it, column is always 1			*/
static unsigned int	s_working_eol;		/* 1: erase to end of line; 0: erase 10 chars		*/
static unsigned int	s_working_clock_id;	/* Clock display; 0 = dxscr_clock_display_id() per tick	*/
static unsigned int	s_working_flags;	/* smg$put_chars rendition flags			*/
static int		s_remain_alarm;		/* Wait ticks left before blinking starts		*/
static int		s_switch_on;		/* "Working..." is shown now				*/
static int		s_remain_timeout;	/* Blink ticks left to the next minute clock update	*/

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_working_start() starts the "Working..." indicator for a long
**	operation: it remembers where to draw, resets the tick counters,
**	installs the first-stage SIGALRM handler and arms a 1-second alarm().
**	For two seconds only the clock is kept going (per-second mode), then
**	"Working..." blinks once a second.  The caller stops it itself with
**	alarm(0) and signal(SIGALRM, old handler).
**
**	Replaces the three former look-alike handler pairs
**	filter_alarm_working/$1 (FILER.C), direct_alarm_working/$1 (DIRECT.C)
**	and inquire_alarm_working/$1 (INQUIRE.C).
**
**  INPUT PARAMETERS:
**
**	a_display_id	- SMG$ display to blink "Working..." in
**	a_row		- its row (column 1)
**	a_eol		- 1: draw with SMG$M_ERASE_TO_EOL and erase the whole
**			  line; 0: draw plain and erase 10 characters
**	a_clock_id	- display for the clock; 0 = take
**			  dxscr_clock_display_id() on every tick
**
**  OUTPUT PARAMETERS:
**
**	a_old_handler	- the SIGALRM handler that was installed before
**
**  IMPLICIT OUTPUTS:
**
**	s_working_* and the tick counters above; SIGALRM handler; alarm().
**
**  RETURNS VALUE:
**
**	SS$_NORMAL, or SS$_ABORT when signal() returns SIG_ERR (the
**	caller reports it with $LOG; nothing is armed then).
**
**--
*/
unsigned int	dxscr_working_start (
		unsigned int	a_display_id,
		int		a_row,
		unsigned int	a_eol,
		unsigned int	a_clock_id,
		void		(**a_old_handler)(int)
			)
{
void		(*l_old)(int);

	s_working_display_id = a_display_id;
	s_working_row = a_row;
	s_working_eol = a_eol;
	s_working_clock_id = a_clock_id;
	s_working_flags = a_eol ? SMG$M_ERASE_TO_EOL : 0;
	s_remain_alarm = 2;
	s_switch_on = 0;
	s_remain_timeout = 60;

	if ((l_old = signal(SIGALRM, s_dxscr_alarm_wait)) == SIG_ERR)
		return	SS$_ABORT;

	*a_old_handler = l_old;
	alarm(1);

	return	SS$_NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	s_dxscr_alarm_wait() is the first-stage SIGALRM handler: for the
**	first two ticks it only keeps the clock going (when per-second
**	update is on), then hands over to s_dxscr_alarm_blink().
**
**  INPUT PARAMETERS:
**
**	a_sig		- signal number (SIGALRM); unused
**
**  IMPLICIT INPUTS/OUTPUTS:
**
**	s_working_clock_id, s_remain_alarm; SIGALRM handler; alarm().
**
**  RETURNS VALUE:
**
**	None (signal handler).
**
**--
*/
static void	s_dxscr_alarm_wait (
		int		a_sig
			)
{
	if (cntrl_info_block.user_pref.display_clock && cntrl_info_block.user_pref.update_in_second)
		write_time(s_working_clock_id ? s_working_clock_id : dxscr_clock_display_id());

	if (--s_remain_alarm > 0)
		signal(SIGALRM, s_dxscr_alarm_wait);
	else
		{
		s_remain_alarm = 2;
		signal(SIGALRM, s_dxscr_alarm_blink);
		}

	alarm(1);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	s_dxscr_alarm_blink() is the second-stage SIGALRM handler: every
**	tick it shows or erases "Working..." (a visible blink) and keeps the
**	clock going -- every tick in per-second mode, else once a minute.
**
**  INPUT PARAMETERS:
**
**	a_sig		- signal number (SIGALRM); unused
**
**  IMPLICIT INPUTS/OUTPUTS:
**
**	s_working_*, s_switch_on, s_remain_timeout; SIGALRM handler; alarm().
**
**  RETURNS VALUE:
**
**	None (signal handler).  A failed SMG$ call is reported by $LOG.
**
**--
*/
static void	s_dxscr_alarm_blink (
		int		a_sig
			)
{
unsigned int	l_rc, l_clock_id;

	if (!s_switch_on)
		{
		s_switch_on = 1;
		if (!(1 & (l_rc = smg$put_chars(&s_working_display_id, $DESCR("Working..."), &s_working_row, &1, &s_working_flags, 0, 0, 0))))
			$LOG(l_rc);
		}
	else
		{
		s_switch_on = 0;
		if (s_working_eol)
			l_rc = smg$erase_line(&s_working_display_id, &s_working_row, &1);
		else
			l_rc = smg$erase_chars(&s_working_display_id, &10, &s_working_row, &1);

		if (!(1 & l_rc))
			$LOG(l_rc);
		}

	if (cntrl_info_block.user_pref.display_clock)
		{
		l_clock_id = s_working_clock_id ? s_working_clock_id : dxscr_clock_display_id();

		if (cntrl_info_block.user_pref.update_in_second)
			write_time(l_clock_id);
		else if (--s_remain_timeout == 0)
			{
			write_time(l_clock_id);
			s_remain_timeout = 60;
			}
		}

	signal(SIGALRM, s_dxscr_alarm_blink);
	alarm(1);
}



/* ---- (from DXSCR.C -- shared SMG$ screen-management helpers) ---- */

/* Program Name            : DXSCR.C                                     */
/*   Original Author       : StarLet Squad and Ruslan R. Laishev (RRL)   */
/*   Date                  : 13-Sep-2026                                 */
/*   Program Description   :                                            */
/*      Shared SMG$ screen-management helpers used by more than one     */
/*      DX module.  Extracted during the code-reorganization pass       */
/*      (see PLAN.TXT) to remove verbatim duplication.                  */
/*                                                                       */
/*      Currently holds the "clock ticker" helpers that used to be      */
/*      copy-pasted, byte for byte, into DIRECT.C, FILER.C, INQUIRE.C   */
/*      and MENUNAVI.C:                                                 */
/*        - the three-line ternary that picks which command-line        */
/*          display the clock belongs on (INQUIRE.C had it three        */
/*          times over);                                                */
/*        - the two-line "if DISPLAY CLOCK is on, write_time() to the   */
/*          current display" idiom that follows every                  */
/*          smg$read_keystroke() call so the clock keeps ticking while  */
/*          the user is thinking (13 call sites across four files).     */
/*                                                                       */
/*      Also holds three later additions, each replacing a duplicated   */
/*      pattern found elsewhere in the project:                        */
/*        - dxscr_dispatch_filemode(): the single/multiple-file mode    */
/*          dispatch body repeated in 13 filer_XXX() wrappers.          */
/*        - dxscr_yes_or_no()/dxscr_confirm_yes(): the "[YES/NO]"       */
/*          prompt classifier, replacing three different hand-rolled    */
/*          strcmp()/strncmp() ladders (FILERDEL.C, FILERPUR.C,         */
/*          FILERCUS.C).                                                */
/*        - dxscr_find_chan_for_did()/dxscr_count_win_dirs(): the       */
/*          "chan_found" directory-cache lookup, split into the two     */
/*          genuinely independent questions FILERCPY.C/FILERSTA.C/      */
/*          FILERPRO.C were each asking of it                          */
/*          (see the functions' own banners below for the reasoning).   */
/*                                                                       */
/*      NOT covered here (found during the same survey, left alone      */
/*      on purpose -- see PLAN.TXT):                                    */
/*        - direct_alarm_working/$1, filter_alarm_working/$1 and        */
/*          inquire_alarm_working/$1 (DIRECT.C/FILER.C/INQUIRE.C) are   */
/*          themselves three near-identical SIGALRM handler pairs       */
/*          (blink "Working..." + periodic clock tick).  Unifying them  */
/*          needs each pair's own static state (switch_on/remain_alarm/ */
/*          remain_timeout) kept separate, plus a decision on how to    */
/*          parameterize the differing blink-target/clock-target        */
/*          display ids (they coincide in DIRECT.C/FILER.C but differ   */
/*          in INQUIRE.C) -- a bigger, separate piece of work.          */
/*          [Done 26-Sep-2026 CLD ECO5: dxscr_working_start().]         */
/*        - paste/unpaste/change_virtual_display/change_viewport calls  */
/*          are NOT boilerplate here: each call site resizes/positions  */
/*          a different virtual display with its own row/column         */
/*          arithmetic, so there is nothing left to factor out beyond   */
/*          what already lives in write_to_filer()/write_time()/        */
/*          get_userinput_and_execute() (FILER.C, INQUIRE.C) -- those   */
/*          were already shared, single-definition routines before      */
/*          this pass, contrary to the original assumption that SMG$    */
/*          usage across the project was widely ad-hoc.                 */
/*                                                                       */
/* Revision History follows                                             */
/*    13-Sep-2026  RRL  Initial version (ECO0): clock-ticker helpers    */
/*    13-Sep-2026  RRL  ECO1: dxscr_dispatch_filemode()                 */
/*    13-Sep-2026  RRL  ECO2: dxscr_yes_or_no()/dxscr_confirm_yes()     */
/*    13-Sep-2026  RRL  ECO3: dxscr_find_chan_for_did()/                */
/*                            dxscr_count_win_dirs()                    */
/*    13-Sep-2026  RRL  ECO4: dxscr_find_chan_for_did()/                */
/*                            dxscr_count_win_dirs() gained an           */
/*                            a_filter (struct cur_filter_tag *)        */
/*                            parameter (were hardwired to the current  */
/*                            window's DX_CURRENT_DIRECTORY); a_chan    */
/*                            is now NULL-able; added                   */
/*                            dxscr_count_win_devices() -- together     */
/*                            these now also cover add_to_filer()       */
/*                            (FILERCPY.C) and its FILERQUE.C sibling   */
/*                            add_to_filer_cache(), neither of which    */
/*                            operate on the current window, and the    */
/*                            latter of which lives on a                */
/*                            filer_cache_tag, not a dir_dx_tag         */
/*    13-Sep-2026  RRL  ECO5: dxscr_dir_open_elsewhere() -- the         */
/*                            "is this fid open as a merged directory   */
/*                            in some OTHER window" check duplicated    */
/*                            in FILERDEL.C/FILERREN.C                  */
/*    13-Sep-2026  RRL  ECO6: dxscr_vms_date_string() -- the            */
/*                            "DD-MMM-YYYY" formatter duplicated (and,  */
/*                            since Y2K, broken) in FILER.C/FILERQUE.C  */
/*    13-Sep-2026  RRL  ECO7: dxscr_pb_exit()/dxscr_ctrly_ast()/        */
/*                            dxscr_enable_ctrly_cleanup() -- restore   */
/*                            terminal characteristics (smg$delete_     */
/*                            pasteboard) on every exit path, including */
/*                            CTRL/Y, instead of leaving the terminal   */
/*                            in SMG$'s no-echo mode                    */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_clock_display_id() picks the SMG$ virtual display the clock
**	should be written to: the DIRTREE command-line display if the
**	directory-tree window is currently pasted up on the pasteboard,
**	otherwise the main FILER command-line display.  This is the exact
**	ternary that used to be repeated verbatim three times in
**	INQUIRE.C (get_userinput() and the former inquire_alarm_working()
**	pair, now s_dxscr_alarm_wait()/s_dxscr_alarm_blink()).
**
**  INPUT PARAMETERS:
**
**	None.  Reads (extern) dirtree_cmds_display.id and the global
**	cntrl_info_block.commands_display.id.
**
**  OUTPUT PARAMETERS:
**
**	None -- see RETURNS VALUE.  A plain function-return value (rather
**	than an output parameter) is used here because this routine has no
**	failure mode to report; it mirrors the pre-existing local variable
**	it replaces.
**
**  RETURNS VALUE:
**
**	The SMG$ virtual display id (unsigned int) to pass to
**	write_time() / dxscr_tick_clock().
**
**--
*/
unsigned int	dxscr_clock_display_id()
{
	extern struct display_tag	dirtree_cmds_display;

	return dirtree_cmds_display.id != 0?
	    dirtree_cmds_display.id : cntrl_info_block.commands_display.id;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_tick_clock() writes the current time to the given SMG$
**	virtual display, but only when the user has DISPLAY CLOCK turned
**	on (cntrl_info_block.user_pref.display_clock).  This is the exact
**	two-line "if (...display_clock) write_time(id);" idiom that used
**	to be repeated verbatim in DIRECT.C, FILER.C, INQUIRE.C and
**	MENUNAVI.C right after every smg$read_keystroke() call.
**
**	Deliberately NOT used for the SIGALRM handlers
**	(s_dxscr_alarm_wait()/s_dxscr_alarm_blink()): those additionally gate on
**	user_pref.update_in_second and/or a per-handler countdown, which
**	is a different condition from the one this routine wraps.
**
**  INPUT PARAMETERS:
**
**	a_display_id  -  SMG$ virtual display id to write the time to.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL always (write_time() itself, FILER.C, has no failure
**	return to propagate).
**
**--
*/
int	dxscr_tick_clock(a_display_id)
unsigned int	a_display_id;
{
	if (cntrl_info_block.user_pref.display_clock)
		write_time(a_display_id);

	return DX__NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_dispatch_filemode() picks and calls one of two routines
**	depending on whether the current filer window is in single- or
**	multiple-file mode (cntrl_info_block.dir_dx[cur_win].filemode,
**	GLOBAL.H's enum filemodes).  This is the exact six-line
**	"if (...filemode == multiple) filer_multiple_X(); else
**	filer_single_X();" body that used to be repeated verbatim in
**	filer_copy(), filer_delete(), filer_rename(), filer_purge(),
**	filer_status(), filer_diff(), filer_append(), filer_launch(),
**	filer_find(), filer_print(),
**	filer_protection(), filer_edt() and filer_type() -- 13 call
**	sites across 13 different FILERxxx.C files.
**
**	Each of those 15 routines is still a real, separately-named
**	function (they are referenced by name from the command-keystroke
**	tables in FILERKPD.C and friends, so none of them can be deleted
**	outright); only their body collapses to a single call:
**
**		int	filer_copy()
**		{
**			return dxscr_dispatch_filemode(
**					filer_multiple_copy,
**					filer_single_copy);
**		}
**
**  INPUT PARAMETERS:
**
**	a_multi_func   -  routine to call when the current window is in
**			  multiple-file mode.
**	a_single_func  -  routine to call otherwise (single-file mode).
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL always -- this mirrors every one of the 15 call sites
**	being replaced, none of which propagated the callee's own return
**	status (each callee reports its own errors via signal_err()/
**	errmsg, not via its return value).
**
**--
*/
int	dxscr_dispatch_filemode(a_multi_func, a_single_func)
int	(*a_multi_func)();
int	(*a_single_func)();
{
	if (DX_CURRENT_DIRECTORY.filemode == multiple)
		(*a_multi_func)();
	else
		(*a_single_func)();

	return DX__NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_yes_or_no() classifies a typed answer to a "[YES/NO]"
**	prompt by its FIRST character alone, case-insensitively.  This is
**	the one primitive behind every such prompt in the project --
**	nothing else should hand-roll a fresh strcmp()/strncmp() ladder
**	against spelled-out "YES"/"NO" (that was the previous state of
**	FILERDEL.C/FILERPUR.C, enumerating "NO"/"N"/"YE"/"Y"/"YES" one
**	string at a time, and separately FILERCUS.C's confirm_delete()/
**	confirm_purge(), doing the same thing again via
**	strncmp(confirm, "YES", strlen(confirm)) prefix matching -- three
**	different spellings of the same one-letter question).
**
**  INPUT PARAMETERS:
**
**	a_answer  -  the user's typed answer.  Only a_answer[0] is
**		     examined; an empty string classifies as invalid.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	1   -  first character is 'Y' (either case).
**	0   -  first character is 'N' (either case).
**	-1  -  anything else (including an empty string) -- invalid.
**
**--
*/
int	dxscr_yes_or_no(a_answer)
char	*a_answer;
{
	int	l_c;

	l_c = toupper((unsigned char) a_answer[0]);

	if (l_c == 'Y')
		return 1;
	if (l_c == 'N')
		return 0;

	return -1;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_confirm_yes() checks the user's answer to a "[YES/NO]"
**	confirmation prompt that guards a one-shot destructive operation
**	(delete/purge): proceed on "yes", silently abandon on "no",
**	reject anything else.  Built on dxscr_yes_or_no() -- see there for
**	what this replaced in FILERDEL.C/FILERPUR.C.
**
**  INPUT PARAMETERS:
**
**	a_confirm  -  the user's typed answer; classified by
**		      dxscr_yes_or_no() (first letter, case-insensitive).
**	a_verb     -  the operation name to put in the cancellation
**		      message (e.g. "DELETE", "PURGE").
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with a human-readable reason whenever the
**		     return value is not DX__NORMAL.
**
**  RETURNS VALUE:
**
**	DX__NORMAL    -  confirmed; caller should proceed.
**	DX__DONTCARE  -  declined; caller should abandon the operation
**			 (already reported via a_errmsg, but this is not
**			 itself an error).
**	DX__ERROR     -  a_confirm was neither yes nor no.
**
**--
*/
int	dxscr_confirm_yes(a_confirm, a_verb, a_errmsg)
char	*a_confirm;
char	*a_verb;
char	*a_errmsg;
{
	switch (dxscr_yes_or_no(a_confirm))
	{
	case 1:
		return DX__NORMAL;

	case 0:
		strcpy(a_errmsg, a_verb);
		strcat(a_errmsg, " canceled by request");
		return DX__DONTCARE;

	default:
		strcpy(a_errmsg, "Invalid input data");
		return DX__ERROR;
	}
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_find_chan_for_did() finds the I/O channel already open for
**	the physical directory identified by a_did, by scanning
**	*a_filter's name_filespec_list/w_dids cache built by
**	find_direct_file()/find_non_direct_file() (FINDFILE.C).  A filer
**	window can show files merged from several physical directories at
**	once (via logical-name equivalence), each kept open on its own
**	channel in that cache -- this is how any routine that needs to
**	QIO against "the file currently under the cursor" gets back the
**	right channel for whichever physical directory that file actually
**	lives in.
**
**	This replaces the exact nested-loop-plus-break search that used
**	to be repeated verbatim in FILERCPY.C, FILERSTA.C and FILERPRO.C,
**	and is also the first half of what FILERQUE.C/FINDFILE.C need to
**	do in one longer, non-early-exit loop (see dxscr_count_win_dirs()
**	for the other half: those also need to know how many distinct
**	physical directories this window merges, which is a genuinely
**	different question from "what channel does THIS did have" and
**	cannot be answered without scanning the whole list regardless of
**	whether the target did was found early -- so it is kept as its
**	own routine rather than folded in here as extra output parameters
**	nobody else needs).  add_to_filer() (FILERCPY.C) is a third user
**	that does not need a_chan at all -- only a_dir_name -- which is
**	why a_chan is NULL-able too.
**
**	A did uniquely identifies one physical directory, so if it occurs
**	under more than one $(w_dids) entry (possible when two filter
**	patterns both resolve into the same merged directory), every such
**	entry necessarily carries the same chan/equivalent_name -- an
**	early return on the first match is therefore exact, not an
**	approximation, and identical to what the old non-early-exit
**	variant eventually left in chan/dir_name after scanning the rest.
**
**  INPUT PARAMETERS:
**
**	a_did  -  the did[3] (directory-ID triple) to look for, e.g.
**		  DX_CURRENT_FILE->did.
**	a_filter  -  pointer to the cur_filter_tag whose cache to search,
**		     e.g. &DX_CURRENT_DIRECTORY.cur_filter for the current
**		     window, &cntrl_info_block.dir_dx[win].cur_filter for
**		     an arbitrary one, or &cachep->cur_filter for a
**		     filer_cache_tag entry (add_to_filer_cache(),
**		     FILERQUE.C).
**
**  OUTPUT PARAMETERS:
**
**	a_chan      -  optional (pass NULL to skip): set to the matching
**		       entry's I/O channel.
**	a_dir_name  -  optional (pass NULL to skip): set to the matching
**		       entry's VMS equivalent directory name.  Buffer must
**		       be at least MAXFILESPEC+1 bytes.
**
**  RETURNS VALUE:
**
**	1  -  found; a_chan/a_dir_name (whichever were given) are valid.
**	0  -  a_did is not open under any entry of *a_filter's cache.
**
**--
*/
int	dxscr_find_chan_for_did(a_did, a_chan, a_dir_name, a_filter)
unsigned short int	*a_did;
unsigned short int	*a_chan;
char			*a_dir_name;
struct cur_filter_tag	*a_filter;
{
	struct filespec_list_tag	*l_s;
	struct w_dids_tag		*l_t;

	for (l_s = a_filter->name_filespec_list;
	     l_s != (struct filespec_list_tag *) NULL;  l_s = l_s->next)
	{
		for (l_t = l_s->w_dids;  l_t != (struct w_dids_tag *) NULL;  l_t = l_t->next)
		{
			if (l_t->did[0] == a_did[0] &&
			    l_t->did[1] == a_did[1] &&
			    l_t->did[2] == a_did[2])
			{
				if (a_chan != (unsigned short int *) NULL)
					*a_chan = l_t->chan;
				if (a_dir_name != (char *) NULL)
					strcpy(a_dir_name, l_t->equivalent_name);
				return 1;
			}
		}
	}
	return 0;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_count_win_dirs() counts how many distinct physical
**	directories (by VMS equivalent name) *a_filter's file list
**	merges together -- i.e. how many different real directories map,
**	via logical-name equivalence, onto the single directory the user
**	is looking at.  add_to_filer() (FILERCPY.C) needs this to decide
**	whether a file being added to a (possibly different) window's
**	display has to have its display name prefixed with its physical
**	directory to stay unambiguous.
**
**	This is an unconditional full scan of the same cache
**	dxscr_find_chan_for_did() searches -- unlike that lookup, it
**	cannot stop early, since the count is over the whole list, not
**	over entries matching one particular did.
**
**	NOTE: this reproduces the original inline code's dedup logic
**	exactly (a name counts as "new" only when it differs from the
**	IMMEDIATELY PRECEDING entry scanned, not from every name seen so
**	far) -- faithful to the behavior being replaced, but technically
**	capable of overcounting if the same directory's entries are ever
**	non-adjacent in the list (e.g. A, B, A would count as 3 distinct
**	names instead of 2).  Flagged here rather than silently changed;
**	worth a deliberate look if it ever turns out to matter in
**	practice.  (add_to_filer()'s own inline version had this same
**	behavior already.)
**
**  INPUT PARAMETERS:
**
**	a_filter  -  pointer to the cur_filter_tag whose cache to scan (see
**		     dxscr_find_chan_for_did() above for the three shapes
**		     this takes across its callers).
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	Count of distinct equivalent directory names encountered.
**
**--
*/
int	dxscr_count_win_dirs(a_filter)
struct cur_filter_tag	*a_filter;
{
	struct filespec_list_tag	*l_s;
	struct w_dids_tag		*l_t;
	char				l_equ_name[MAXFILESPEC+1];
	int				l_total_dids;

	l_total_dids = 0;
	l_equ_name[0] = EOS;

	for (l_s = a_filter->name_filespec_list;
	     l_s != (struct filespec_list_tag *) NULL;  l_s = l_s->next)
	{
		for (l_t = l_s->w_dids;  l_t != (struct w_dids_tag *) NULL;  l_t = l_t->next)
		{
			if (strcmp(l_equ_name, l_t->equivalent_name))
			{
				l_total_dids++;
				strcpy(l_equ_name, l_t->equivalent_name);
			}
		}
	}
	return l_total_dids;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_count_win_devices() counts how many distinct VMS devices
**	(the part of the equivalent name up to, but not including, the
**	first "[") *a_filter's merged directories span.  Used only by
**	add_to_filer() (FILERCPY.C) to decide, when more than one
**	physical directory is merged (dxscr_count_win_dirs() > 1),
**	whether disambiguating the display filename needs the full
**	"DEVICE:[DIR]" or just "[DIR]" is enough because every merged
**	directory lives on the same device.
**
**	Same full-scan shape and same adjacent-only dedup caveat as
**	dxscr_count_win_dirs() -- see there.  (add_to_filer()'s own inline
**	version had this same behavior already.)
**
**  INPUT PARAMETERS:
**
**	a_filter  -  pointer to the cur_filter_tag whose cache to scan.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	Count of distinct device names encountered.
**
**--
*/
int	dxscr_count_win_devices(a_filter)
struct cur_filter_tag	*a_filter;
{
	struct filespec_list_tag	*l_s;
	struct w_dids_tag		*l_t;
	char				l_dev_name[MAXFILESPEC+1];
	int				l_total_devices;
	unsigned short int		l_len;

	l_total_devices = 0;
	l_dev_name[0] = EOS;

	for (l_s = a_filter->name_filespec_list;
	     l_s != (struct filespec_list_tag *) NULL;  l_s = l_s->next)
	{
		for (l_t = l_s->w_dids;  l_t != (struct w_dids_tag *) NULL;  l_t = l_t->next)
		{
			l_len = strcspn(l_t->equivalent_name, "[");
			if (strncmp(l_t->equivalent_name, l_dev_name, l_len))
			{
				l_total_devices++;
				strncpy(l_dev_name, l_t->equivalent_name, l_len);
				l_dev_name[l_len] = EOS;
			}
		}
	}
	return l_total_devices;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_dir_open_elsewhere() checks whether a_fid is currently open
**	as a merged physical directory (did) in some window OTHER than
**	a_this_win.  Used before deleting/renaming a directory file, to
**	refuse the operation if another window is currently displaying
**	that same physical directory -- replaces the exact
**	"for every other window, scan its cur_filter.name_filespec_list/
**	w_dids for a did match" search that used to be repeated verbatim
**	(save for the error message's verb) in filer_delete$2() (FILERDEL.C)
**	and filer_rename$2() (FILERREN.C).
**
**	Unlike dxscr_find_chan_for_did(), this deliberately does NOT stop
**	at the first matching window found beyond confirming existence --
**	there is nothing to report back except yes/no, so the first match
**	is enough.
**
**  INPUT PARAMETERS:
**
**	a_fid       -  the fid[3] of the (directory) file being deleted or
**		       renamed, compared against every other window's did.
**	a_this_win  -  the window the operation is being performed in
**		       (cntrl_info_block.cur_win); every OTHER window is
**		       searched.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	1  -  a_fid is open as a merged directory in some other window.
**	0  -  it is not -- the operation may proceed.
**
**--
*/
int	dxscr_dir_open_elsewhere(a_fid, a_this_win)
unsigned short int	*a_fid;
unsigned short int	a_this_win;
{
	int				l_i;
	struct filespec_list_tag	*l_p;
	struct w_dids_tag		*l_q;

	for (l_i = 0;  l_i < cntrl_info_block.windows;  l_i++)
	{
		if (l_i == a_this_win)
			continue;

		for (l_p = cntrl_info_block.dir_dx[l_i].cur_filter.name_filespec_list;
		     l_p != (struct filespec_list_tag *) NULL;  l_p = l_p->next)
		{
			for (l_q = l_p->w_dids;  l_q != (struct w_dids_tag *) NULL;  l_q = l_q->next)
			{
				if (l_q->did[0] == a_fid[0] &&
				    l_q->did[1] == a_fid[1] &&
				    l_q->did[2] == a_fid[2])
					return 1;
			}
		}
	}
	return 0;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_vms_date_string() formats a SYS$NUMTIM-style timbuf array as
**	a standard VMS absolute date "DD-MMM-YYYY" (e.g. "13-SEP-2026").
**	Replaces the "%2.2d/%02.2d/%02.2d", timbuf[0]-1900, ... idiom that
**	was duplicated in FILER.C (print_filer_display(), the "older than
**	a week" case) and three times over in FILERQUE.C
**	(display_filer_cache()) -- that idiom was correct only while the
**	year was still under 2000 (timbuf[0]-1900 gave a plain two-digit
**	number); today it silently prints three digits instead of two
**	(e.g. "126" for 2026), which is what actually flagged this.
**
**	14-Sep-2026 RRL: fixed a live ACCVIO here -- a_timbuf was declared
**	"int a_timbuf[7]" (32-bit longwords), but every caller passed a
**	SYS$NUMTIM output buffer, which is genuinely "unsigned short int
**	[7]" (16-bit words) per the OpenVMS spec.  Reading that 14-byte
**	buffer as 7 longwords misaligned every field past a_timbuf[0]:
**	a_timbuf[1] (month) actually straddled the real timbuf[2]/[3]
**	words, so a lookup keyed on that garbage month index indexed a
**	month-name table out of bounds and dereferenced a bogus pointer.
**	Rather than just fix the type, dropped the whole
**	SYS$NUMTIM-array-plus-month-name-table design and the C RTL
**	sprintf() call: SYS$FAO()'s "!%D" directive formats a 64-bit VMS
**	absolute time value directly (no expansion, no month-name lookup
**	needed at all), so the caller now passes the address of the
**	original DATE_TIME quadword instead of an expanded timbuf array.
**
**  INPUT PARAMETERS:
**
**	a_out    -  caller-supplied buffer, at least 12 bytes
**		    ("DD-MMM-YYYY" plus the terminating EOS).
**	a_time   -  address of a 64-bit VMS absolute time value
**		    (a DATE_TIME quadword, e.g. a_p->cdat or the second
**		    argument previously passed to SYS$NUMTIM).
**
**  OUTPUT PARAMETERS:
**
**	a_out  -  filled in with the formatted date.
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
**	a_out itself, so the call can be used inline as a formatting
**	argument.
**
**--
*/
char	*dxscr_vms_date_string(a_out, a_time)
char		*a_out;
DATE_TIME	*a_time;
{
	static $DESCRIPTOR (l_fao_ctrl, "!11%D");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int	l_len;
	unsigned long		l_rc;

	l_out_desc.dsc$w_length = 11;
	l_out_desc.dsc$b_dtype  = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class  = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = a_out;

	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_time))))
	    $LOG(l_rc);
	a_out[l_len] = EOS;
	return a_out;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_ljust() left-justifies a_src into a_dst, truncating or
**	space-padding it to exactly a_width characters, then EOS-
**	terminates.  Replaces the "%-N.Ns" sprintf idiom used throughout
**	the display-formatting code -- SYS$FAO() (see dxscr_vms_date_
**	string() above; project direction is no C RTL printf-family
**	calls) has no directive that truncates/pads a string to a fixed
**	width the way printf's width.precision does, so callers now
**	build the fixed-width field with this helper first and hand the
**	already-sized result to SYS$FAO() as a plain "!AZ" string.
**
**  INPUT PARAMETERS:
**
**	a_src    -  source string (NUL-terminated).
**	a_width  -  exact output width, in characters.
**
**  OUTPUT PARAMETERS:
**
**	a_dst  -  filled with a_src, truncated or space-padded to exactly
**		  a_width characters, plus a terminating EOS.  Caller-
**		  supplied buffer must be at least a_width+1 bytes.
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
**	a_dst itself, so the call can be used inline as a formatting
**	argument.
**
**--
*/
char	*dxscr_ljust(a_dst, a_src, a_width)
char	*a_dst;
char	*a_src;
int	a_width;
{
	int	l_len;

	l_len = strlen(a_src);
	if (l_len > a_width)
	    l_len = a_width;
	memcpy(a_dst, a_src, l_len);
	if (l_len < a_width)
	    memset(a_dst + l_len, ' ', a_width - l_len);
	a_dst[a_width] = EOS;
	return a_dst;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_pb_exit() deletes the SMG$ pasteboard (which restores the
**	terminal to its normal, pre-DX characteristics -- echo, line mode,
**	etc.) and then exits.  Every exit() call in the program that can
**	run after smg$create_pasteboard() has already succeeded must go
**	through here instead of calling exit() directly -- four call sites
**	in DX_initialized() (DX.C: the post-creation NANSITERM/NONVT100CRT/
**	PGLEN/PGWID checks) and filer_exitset()'s normal "EXIT" command
**	(FILERWIN.C) used to call exit() straight, leaving the terminal in
**	SMG$ mode (no echo) for whatever ran next -- exactly the class of
**	problem reported for CTRL/C and CTRL/Y (see dxscr_ctrly_ast()).
**
**  INPUT PARAMETERS:
**
**	a_status  -  the exit status to pass to exit().
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	Does not return -- exit() never returns.
**
**--
*/
void	dxscr_pb_exit(a_status)
unsigned long int	a_status;
{
	smg$delete_pasteboard(&cntrl_info_block.pasteboard_id);
	exit(a_status);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_ctrly_ast() is the CTRL/Y AST routine established by
**	dxscr_enable_ctrly_cleanup() (DX_initialized(), DX.C, right after
**	the pasteboard is created).  Without it, CTRL/Y suspends the image
**	back to DCL exactly as the terminal driver left it -- still in the
**	no-echo/no-line-editing mode SMG$ put it in for full-screen display
**	-- so the DCL prompt that follows looks broken until the user
**	either resumes DX (which repaints and restores SMG$ mode) or
**	manually resets the terminal.  CTRL/C is not separately handled:
**	DX never enables a CTRL/C AST of its own, so CTRL/C only ever
**	interrupts a pending SMG$ keystroke read (SS$_CONTROLC returned to
**	smg$read_keystroke()) without touching terminal characteristics --
**	nothing to restore there.
**
**  INPUT PARAMETERS:
**
**	None (AST routine, called asynchronously by the terminal driver).
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	Does not return -- dxscr_pb_exit() calls exit().
**
**--
*/
void	dxscr_ctrly_ast()
{
	dxscr_pb_exit(SS$_NORMAL);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dxscr_enable_ctrly_cleanup() arranges for CTRL/Y to restore the
**	terminal (via dxscr_ctrly_ast()) instead of leaving it in SMG$'s
**	no-echo full-screen mode.  Assigns a channel to SYS$COMMAND (the
**	first I/O channel this project has ever needed to assign directly
**	-- everything else goes through SMG$) and enables a CTRL/Y AST on
**	it with IO$_SETMODE/IO$M_CTRLYAST.  Call once, after
**	smg$create_pasteboard() succeeds.
**
**  INPUT PARAMETERS:
**
**	None.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL  -  always; a failure to enable the AST is not fatal to
**		       DX itself (worst case, CTRL/Y behaves as it did
**		       before this change) so it is reported via check_OK's
**		       normal SFC path rather than aborting startup.
**
**--
*/
int	dxscr_enable_ctrly_cleanup(void)
{
	static unsigned short	l_chan;
	$DESCRIPTOR(l_command_descrip, "SYS$COMMAND");
	int			l_rc;

	if (!(1 & (l_rc = sys$assign(&l_command_descrip, &l_chan, 0, 0))))
		return $LOG(l_rc);

	if (!(1 & (l_rc = sys$qiow(0, l_chan, IO$_SETMODE | IO$M_CTRLYAST,
				   0, 0, 0, dxscr_ctrly_ast, 0, 0, 0, 0, 0))))
		return $LOG(l_rc);

	return DX__NORMAL;
}
