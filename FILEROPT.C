/* Program Name            : FILEROPT.C                                 */
/*   Original Author       : C. K. Hung					*/
/*   Editor                : StarLet Squad and Ruslan R. Laishev (RRL)  */
/*   Date                  : 3-AUG-1990					*/
/*   Program Description   : Execution and settings commands --         */
/*                           consolidated 14-Sep-2026 from FILEROPT.C + */
/*                           FILEREXE.C + FILERLAU.C + FILERPRO.C +     */
/*                           FILERFND.C + FILERCUS.C.                   */
/*                         :                                            */
/* Revision History follows                                             */
/*  25-Sep-2026 RRL ECO2: __NEW_STARLET prototypes, types fixed (0 CC diags)  */
/*                        FIBDEF/FIBDEF1 typedefs; sys$qiow IOSB/P2-P6 casts  */
/*  26-Sep-2026 CLD ECO3: ident sync with DX.C (no code change)         */
/*  26-Sep-2026 CLD ECO5: SIGALRM "Working..." handlers -> dxscr_working_start()  */
/*  28-Sep-2026 CLD ECO6: Find viewer: Home/End go to the first/last page  */

#define __MODULE__	"FILEROPT"
#define __IDENT__	"X-02-05ECO6"
#define __REV__		"02.05.06"
 
 
#define	__NEW_STARLET	1
#include	<starlet.h>
#include	<lib$routines.h>
#include	<str$routines.h>
#include	<lbr$routines.h>
#include	<fibdef.h>
#include	<iodef.h>
#include	<signal.h>
#include	<stat.h>
#include	<atrdef.h>
#include	<libclidef.h>

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
**  INTERNAL FUNCTION PROTOTYPE
**/
 
static int	filer_toggle_form(void);
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_update() re-reads the current directory from disk and
**	redraws it, exactly as if the user had entered the Option -> Update
**	command.  Disables VMS broadcast-message trapping and arms the
**	"Working..." alarm (dxscr_working_start()) for the duration of the
**	re-scan, since a large directory can take a few seconds.

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
**	DX_CURRENT_DIRECTORY.cur_filter (the active name/exclude/date/
**	size filter, unchanged -- this is a re-read, not a new filter).

**
**  IMPLICIT OUTPUTS:
**
**	The current window's file lists and screen are rebuilt by
**	filer_filter$1("", "Update", ...).

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_update()
{
    char l_errmsg[MAXFILESPEC+1];
    unsigned long l_rc;
    void (*l_old_alarm)(int);
 
    /*
    **	Output 'working...' message if total processing time
    **	is longer than 3 seconds.  Needs to disable broadcast message
    **	first because of interference.
    **/
 
    if (!(1 & (l_rc = smg$disable_broadcast_trapping (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = dxscr_working_start(cntrl_info_block.commands_display.id, 2, 0, cntrl_info_block.commands_display.id, &l_old_alarm))))
	$LOG(l_rc);
 
    /*
    **	Re-read this directory
    **/
 
    filer_filter$1("", "Update", l_errmsg);
 
    /*
    **	Set the signal back to system default
    **/
 
    alarm(0);
    signal(SIGALRM, SIG_DFL);
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
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FILER_HELP() outputs the help messages for FILER
**
**--
**/
int	filer_help(a_topic)
char *a_topic;
{
    unsigned int l_internal_display_id;
    $DESCRIPTOR(l_key, a_topic);
    char l_libnam[80];
    $DESCRIPTOR(l_lib, l_libnam);
    FILE *l_fp;
    unsigned long l_rc;
 
    /* Check if logical name "DX$HELPLIB" point to a .HLB file	*/
    strcpy (l_libnam, DX$HELPLIB);
    if ((l_fp = fopen (l_libnam, "r")) != NULL){
	fclose(l_fp);
    } else {
	/* Check if in DXHELP.HLB is in HELPLIB (DX$HOME)	*/
	strcpy (l_libnam, HELPLIB);
	strcat (l_libnam, "DXHELP.HLB");
	if ((l_fp = fopen (l_libnam, "r")) != NULL){
		fclose (l_fp);
 
	/* Check if in SYS$HELP					*/
	} else if ((l_fp = fopen ("SYS$HELP:DXHELP.HLB", "r")) != NULL) {
		fclose (l_fp);
		strcpy (l_libnam, "SYS$HELP:DXHELP.HLB");
 
	/* Damned you ! You did not install the help file	*/
	} else {
		signal_err ("Help library not found", bell);
		return DX__ERROR;
	}
    }
 
    /**  .HLB exist.  Output the help messages  **/
    if (!(1 & (l_rc = smg$save_physical_screen (
	    &cntrl_info_block.pasteboard_id,
	    &l_internal_display_id,
	    0,
	    0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$set_physical_cursor (
	      &cntrl_info_block.pasteboard_id,
	      &1,
	      &1))))
        $LOG(l_rc);
 
    LENGTH(l_key) = strlen(a_topic);
    LENGTH(l_lib) = strlen(l_libnam);
    lbr$output_help(	    /**  STARLET prototypes disagree on the return type  **/
	(int (*)()) &lib$put_output,
	0,
	&l_key,
	&l_lib,
	0,
	&lib$get_input);
 
    if (!(1 & (l_rc = smg$restore_physical_screen (
	    &cntrl_info_block.pasteboard_id,
	    &l_internal_display_id))))
        $LOG(l_rc);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FILER_WRITE() calls PRINT_FILER_DISPLAY() to save
**	all the file characteristics in the current directory to a file
**
**--
**/
int	filer_write()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error writing to file");
    get_userinput_and_execute(
	print_filer_display,
	"Write File Characteristics",
	"File Name: ",
	"",
	l_errmsg,
	"dummy");
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      PRINT_FILER_DISPLAY() outputs the contents of FILER
**	virtual display to a hardcopy.
**
**--
**/
int	print_filer_display(a_filespec, a_dummy, a_errmsg)
char *a_filespec;
char *a_dummy;
char *a_errmsg;
{
    register l_k;
    int l_i;
    FILE *l_dxf;
    char l_display_line[DX_MAX_PB_WIDTH+3];	/**  One display line (up to
						**   DX_MAX_PB_WIDTH chars) +
						**   newline + EOS  **/
    $DESCRIPTOR(l_display_line_descriptor, l_display_line);
    char l_total_str[MAXFILESPEC+1], l_select_str[MAXFILESPEC+1];
    int l_spaces;
    char l_filler[MAXFILESPEC+1];
    char l_msg[MAXFILESPEC+1];
    struct fil_dx_tag l_temp_entry;
    unsigned long l_rc;
    struct dsc$descriptor_s l_out_desc;
    unsigned short int l_len;

    if ((l_dxf = fopen(a_filespec, "w")) == (FILE *)NULL) {
	static $DESCRIPTOR (l_fao_ctrl1, "Error opening !AZ as output");
	l_out_desc.dsc$w_length = MAXFILESPEC;
	l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = a_errmsg;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl1, &l_len, &l_out_desc, a_filespec))))
	    $LOG(l_rc);
	a_errmsg[l_len] = EOS;
	return DX__ERROR;
    }

    /**  Write directory name  **/
    {
	static $DESCRIPTOR (l_fao_ctrl2, "Directory: !AZ!/!/");
	char l_scratch[MAXFILESPEC+16];

	l_out_desc.dsc$w_length = sizeof(l_scratch) - 1;
	l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_scratch;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl2, &l_len, &l_out_desc, DX_CURRENT_DIRECTORY.cur_dir))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	fputs(l_scratch, l_dxf);
    }

    for (l_i = 1; l_i <= DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y; l_i++) {
	    LENGTH(l_display_line_descriptor) = cntrl_info_block.pasteboard_width;
	    if (!(1 & (l_rc = smg$read_from_display (
		    &DX_CURRENT_DIRECTORY.filer_display.id,
		    &l_display_line_descriptor,
		    0,
		    &l_i))))
	        $LOG(l_rc);
	    l_display_line[cntrl_info_block.pasteboard_width] = EOS;
	    l_display_line[cntrl_info_block.pasteboard_width+1] = EOS;
	    for (l_k = cntrl_info_block.pasteboard_width-1; l_k >= 0; l_k--)
		    if (isprint(l_display_line[l_k]) && !isspace(l_display_line[l_k])) {
			l_display_line[l_k+1] = '\n';
			break;
		    } else
			l_display_line[l_k] = EOS;
	    fputs(l_display_line, l_dxf);
    }
 
    /**  Write directory statistics  **/
    {
	static $DESCRIPTOR (l_fao_ctrl3, " (Total: !SLf/!SLb)");
	l_out_desc.dsc$w_length = sizeof(l_total_str) - 1;
	l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_total_str;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl3, &l_len, &l_out_desc,
		DX_CURRENT_DIRECTORY.tot_files, DX_CURRENT_DIRECTORY.tot_blocks))))
	    $LOG(l_rc);
	l_total_str[l_len] = EOS;
    }
    {
	static $DESCRIPTOR (l_fao_ctrl4, "(Selections: !SLf/!SLb) ");
	l_out_desc.dsc$w_length = sizeof(l_select_str) - 1;
	l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_select_str;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl4, &l_len, &l_out_desc,
		DX_CURRENT_DIRECTORY.sel_files, DX_CURRENT_DIRECTORY.sel_blocks))))
	    $LOG(l_rc);
	l_select_str[l_len] = EOS;
    }
    l_spaces = cntrl_info_block.pasteboard_width - (strlen(l_total_str)+strlen(l_select_str));

    if (l_spaces <= 0)

	{
	    fputs("\n\n", l_dxf);
	    fputs(l_total_str, l_dxf);
	    fputs(l_select_str, l_dxf);
	}

    else {

	    memset(l_filler, ' ', l_spaces);
	    l_filler[l_spaces] = EOS;

	    fputs("\n\n", l_dxf);
	    fputs(l_total_str, l_dxf);
	    fputs(l_filler, l_dxf);
	    fputs(l_select_str, l_dxf);

    }

    if (fclose(l_dxf) == EOF) {
	static $DESCRIPTOR (l_fao_ctrl5, "Error closing !AZ");
	l_out_desc.dsc$w_length = MAXFILESPEC;
	l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = a_errmsg;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl5, &l_len, &l_out_desc, a_filespec))))
	    $LOG(l_rc);
	a_errmsg[l_len] = EOS;
	return DX__ERROR;
    }

    /*****  Add this file to this display  *****/

    {
	static $DESCRIPTOR (l_fao_ctrl6, "!SL lines written to file !AZ");
	l_out_desc.dsc$w_length = sizeof(l_msg) - 1;
	l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_msg;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl6, &l_len, &l_out_desc,
		DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y+4, a_filespec))))
	    $LOG(l_rc);
	l_msg[l_len] = EOS;
    }
    signal_err(l_msg, silence);
 
    /*
    **	Update the filer list and display
    **/
 
    strcpy(a_errmsg, "Screen not updated");
    if (filestat(a_filespec, &l_temp_entry, a_errmsg) == DX__ERROR)
        return DX__ERROR;
 
    l_temp_entry.state = waiting;
    l_temp_entry.beg_y = l_temp_entry.beg_x = 0;
    l_temp_entry.forward = l_temp_entry.backward = NULL;
 
    /*
    **  Update cache queue.
    **	In case of running out of memory, output error message
    **	but update current display(s) anyway.
    **/
 
    if (update_filer_cache(l_temp_entry, add_to_filer_cache, a_errmsg) == DX__ERROR)
        signal_err(a_errmsg, bell);
 
    /**  Add it to the FILER  **/
    if (add_to_filer(l_temp_entry, a_errmsg, cntrl_info_block.cur_win) == DX__ERROR)
        return DX__ERROR;
 
    /**  Update other windows  **/
    for (l_i = 0;  l_i < cntrl_info_block.windows;  l_i++)
    {
        if (l_i != cntrl_info_block.cur_win)
        {
            if (add_to_filer(l_temp_entry, a_errmsg, l_i) == DX__ERROR)
                 return DX__ERROR;
        }
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_goto() is the "Goto..." command entry point: prompts the
**	user for a page number and dispatches to filer_goto$1() to move the
**	cursor there.

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_goto()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error setting cursor to new position");
    get_userinput_and_execute(
	filer_goto$1,
	"Goto Page",
	"Page Number: ",
	"",
	l_errmsg,
	"dummy");
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_goto$1() converts a_page_str to a page number, computes the
**	display row that page starts at, walks the directory- and non-
**	directory-file lists to find the first entry at or after that row,
**	and moves the highlighted "current file" there.

**
**  INPUT PARAMETERS:
**
**	a_page_str  -  page number typed by the user, as a decimal string.
**	a_dummy     -  unused (get_userinput_and_execute() calling convention).
**	a_errmsg    -  unused here (present for the same reason).

**
**  OUTPUT PARAMETERS:
**
**	None.

**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY.filer_display.view_rows (rows per page)
**	and .dir_filelist/.non_dir_filelist (the file lists walked to find
**	the first file whose row falls on the requested page).

**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_FILE is set to the first file on the requested page
**	(or the last file, if the page number is past the end of the list);
**	the viewport is scrolled there via change_filer_viewport().

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_goto$1(a_page_str, a_dummy, a_errmsg)
char *a_page_str;
char *a_dummy;
char *a_errmsg;
{
    register int l_i;
    struct fil_dx_tag *l_p;
    int l_disp;
    int l_page_no;
 
    l_page_no = atoi(a_page_str);
    l_disp = 1 + abs(DX_CURRENT_DIRECTORY.filer_display.view_rows*(l_page_no-1));
 
    unhighlight_filer_current_file();
 
    for (l_p = DX_CURRENT_DIRECTORY.dir_filelist->forward;
	l_p != DX_CURRENT_DIRECTORY.non_dir_filelist;  l_p = l_p->forward)
    {
        if (l_disp <= l_p->beg_y)
        {
            break;
        }
	else if (l_p->forward == DX_CURRENT_DIRECTORY.dir_filelist)
	{
	    if (DX_CURRENT_DIRECTORY.non_dir_filelist ==
		    DX_CURRENT_DIRECTORY.non_dir_filelist->forward &&
		l_p != DX_CURRENT_DIRECTORY.dir_filelist)
	    {
		break;
	    }
	    else
		l_p = DX_CURRENT_DIRECTORY.non_dir_filelist->forward;
	}
    }
 
    DX_CURRENT_FILE = (l_p == DX_CURRENT_DIRECTORY.non_dir_filelist?
	DX_CURRENT_DIRECTORY.non_dir_filelist->backward : l_p);
 
    change_filer_viewport(cntrl_info_block.cur_win);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_mark_all() selects (marks) every file in the current
**	window -- both the directory-file list and the non-directory-file
**	list -- by calling filer_mark() on each entry in turn.  Used by the
**	Option -> "Include all" command.

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
**	DX_CURRENT_DIRECTORY.dir_filelist/.non_dir_filelist.

**
**  IMPLICIT OUTPUTS:
**
**	Every file's selection state is set to "ready" (marked) via
**	filer_mark(); the screen is updated inside one
**	smg$begin_pasteboard_update()/smg$end_pasteboard_update() bracket so
**	the whole redraw happens as a single atomic screen update.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_mark_all()
{
    struct fil_dx_tag *l_p;
    struct fil_dx_tag *l_marker;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = smg$begin_pasteboard_update (
	      &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    l_marker = DX_CURRENT_DIRECTORY.dir_filelist;
    for (l_p = l_marker->forward; l_p != l_marker; l_p = l_p->forward)
    {
	filer_mark(l_p, 1);
    }
 
    l_marker = DX_CURRENT_DIRECTORY.non_dir_filelist;
    for (l_p = l_marker->forward; l_p != l_marker; l_p = l_p->forward)
    {
	filer_mark(l_p, 1);
    }
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
	      &DX_CURRENT_DIRECTORY.filer_display.id,
	      &DX_CURRENT_FILE->beg_y,
	      &DX_CURRENT_FILE->beg_x))))
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
**	filer_unmark_all() clears the selection (mark) on every file in
**	the current window, both lists, via filer_unmark().  Used by the
**	Option -> "Exclude all" command.

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
**	DX_CURRENT_DIRECTORY.dir_filelist/.non_dir_filelist.

**
**  IMPLICIT OUTPUTS:
**
**	Every file's selection state is cleared via filer_unmark();
**	screen updated as one atomic smg$ pasteboard update.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_unmark_all()
{
    struct fil_dx_tag *l_p;
    struct fil_dx_tag *l_marker;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = smg$begin_pasteboard_update (
	      &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    l_marker = DX_CURRENT_DIRECTORY.dir_filelist;
    for (l_p = l_marker->forward; l_p != l_marker; l_p = l_p->forward)
    {
	filer_unmark(l_p, 1);
    }
 
    l_marker = DX_CURRENT_DIRECTORY.non_dir_filelist;
    for (l_p = l_marker->forward; l_p != l_marker; l_p = l_p->forward)
    {
	filer_unmark(l_p, 1);
    }
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
	      &DX_CURRENT_DIRECTORY.filer_display.id,
	      &DX_CURRENT_FILE->beg_y,
	      &DX_CURRENT_FILE->beg_x))))
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
**	filer_remark_all() re-selects only the files whose state is
**	"executed" (i.e. files a previous Launch/Execute/Edit already acted
**	on) in both file lists.  Used by the Option -> "Re-include all"
**	command, to bring back just the previously-acted-on selection
**	rather than every file.

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
**	DX_CURRENT_DIRECTORY.dir_filelist/.non_dir_filelist and each
**	entry's .state field.

**
**  IMPLICIT OUTPUTS:
**
**	Files with state==executed are marked via filer_mark(); screen
**	updated as one atomic smg$ pasteboard update.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_remark_all()
{
    struct fil_dx_tag *l_p;
    struct fil_dx_tag *l_marker;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = smg$begin_pasteboard_update (
	      &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    l_marker = DX_CURRENT_DIRECTORY.dir_filelist;
    for (l_p = l_marker->forward; l_p != l_marker; l_p = l_p->forward)
	if (l_p->state == executed)
	    filer_mark(l_p, 1);
 
    l_marker = DX_CURRENT_DIRECTORY.non_dir_filelist;
    for (l_p = l_marker->forward; l_p != l_marker; l_p = l_p->forward)
	if (l_p->state == executed)
	    filer_mark(l_p, 1);
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
	      &DX_CURRENT_DIRECTORY.filer_display.id,
	      &DX_CURRENT_FILE->beg_y,
	      &DX_CURRENT_FILE->beg_x))))
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
**	filer_short_form() switches the current window to the compact
**	("short form") file listing -- file name only, no date/size/
**	protection columns -- and repaints via filer_toggle_form().

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.form is set to short_form.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_short_form()
{
    DX_CURRENT_DIRECTORY.form = short_form;
    filer_toggle_form();
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_long_form() switches the current window to the detailed
**	("long form") file listing -- one file per line with date/size/
**	protection -- and repaints via filer_toggle_form().

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.form is set to long_form.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_long_form()
{
    DX_CURRENT_DIRECTORY.form = long_form;
    filer_toggle_form();
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_toggle_form() is the shared redraw logic behind
**	filer_short_form()/filer_long_form(): it unpastes and erases the
**	current FILER virtual display, recomputes every file's screen
**	position for the newly-selected form via compute_filer_begin_xy(),
**	resizes the virtual display if needed, rewrites every file entry via
**	write_to_filer(), restores the viewport/highlight, and repastes the
**	display.

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
**	DX_CURRENT_DIRECTORY.form (already updated by the caller) and
**	both file lists.

**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.filer_display (size/contents) and the
**	screen are rebuilt to match the new form.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
static int	filer_toggle_form()
{
    int l_row, l_col;
    int l_sav_row;
    int l_prev_disp_rows;
    unsigned long l_rc;
 
    l_sav_row = DX_CURRENT_FILE->beg_y -
	DX_CURRENT_DIRECTORY.filer_display.view_beg_y;
    l_prev_disp_rows = DX_CURRENT_DIRECTORY.filer_display.rows;
 
    if (!(1 & (l_rc = smg$unpaste_virtual_display (
	     	  &DX_CURRENT_DIRECTORY.filer_display.id,
	     	  &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$erase_display (
	     	  &DX_CURRENT_DIRECTORY.filer_display.id,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
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
 
    DX_CURRENT_DIRECTORY.filer_display.rows =
	max (DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y,
	     DX_CURRENT_DIRECTORY.filer_display.view_rows);
    DX_CURRENT_DIRECTORY.filer_display.width =
	cntrl_info_block.pasteboard_width;
    if (l_prev_disp_rows < DX_CURRENT_DIRECTORY.filer_display.rows)
    {
	if (!(1 & (l_rc = smg$change_virtual_display (
		     &DX_CURRENT_DIRECTORY.filer_display.id,
		     &DX_CURRENT_DIRECTORY.filer_display.rows,
		     0,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);
    }
 
    /*
    **  Write all the files to screen
    **/
 
    write_to_filer(
	cntrl_info_block.dir_dx[cntrl_info_block.cur_win].dir_filelist);
    write_to_filer(
	cntrl_info_block.dir_dx[cntrl_info_block.cur_win].non_dir_filelist);
 
    if (l_prev_disp_rows > DX_CURRENT_DIRECTORY.filer_display.rows)
    {
	if (!(1 & (l_rc = smg$change_virtual_display (
		     &DX_CURRENT_DIRECTORY.filer_display.id,
		     &DX_CURRENT_DIRECTORY.filer_display.rows,
		     0,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);
    }
 
    change_filer_viewport(cntrl_info_block.cur_win);
 
    highlight_filer_current_file();
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
	     	  &DX_CURRENT_DIRECTORY.filer_display.id,
	     	  &cntrl_info_block.pasteboard_id,
	     	  &DX_CURRENT_DIRECTORY.filer_display.beg_y,
	     	  &DX_CURRENT_DIRECTORY.filer_display.beg_x,
	     	  0))))
        $LOG(l_rc);
    return DX__NORMAL;
}
 
 

/*
**++
**
**  GET_QUOTA uses ACP services to examine a user's permanent
**  disk quota.  Read access to [0,0]QUOTA.SYS;1 is required
**  to examine all non-user entries.  Current version of DX
**  does not call this function, it is put here for possible
**  future use.
**
**--
**/
int	get_quota(unsigned long a_uic)
{
    int l_iosb[2];
 
    static FIBDEF1 l_fib;
    $DESCRIPTOR (l_fib_descriptor, (char *)& l_fib);
 
    struct dqfs
	{
	    long int dqf$l_flags;
	    long int dqf$w_uic;
	    long int dqf$l_usage;
	    long int dqf$l_permquota;
	    long int dqf$l_overdraft;
	    long filler[3];
	} l_from_dqf, l_to_dqf;
 
    $DESCRIPTOR (l_from_dqf_descriptor, (char *)&l_from_dqf);
    $DESCRIPTOR (l_to_dqf_descriptor, (char *)&l_to_dqf);
 
    int l_len;
    unsigned long l_rc;
 
    l_from_dqf.dqf$w_uic = a_uic;
 
    LENGTH(l_from_dqf_descriptor) = sizeof (struct dqfs);
    LENGTH(l_to_dqf_descriptor) = sizeof (struct dqfs);
 
    LENGTH(l_fib_descriptor) = sizeof (FIBDEF1);
    l_fib.fib$w_cntrlfunc = FIB$C_EXA_QUOTA;
 
    if (!(1 & (l_rc = sys$qiow (
		0,
		DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list->w_dids->chan,
		IO$_ACPCONTROL,
		(struct _iosb *) l_iosb,
		0,
		0,
		&l_fib_descriptor,
		(__int64) &l_from_dqf_descriptor,
		(__int64) &l_len,
		(__int64) &l_to_dqf_descriptor,
		0,
		0))))
        $LOG(l_rc);
 
    return l_to_dqf.dqf$l_permquota;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_about() is the "About DX" command: displays the DXVERSION
**	banner string (name, version, and original-author credit) on the
**	status line.

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	    filer_about()
{
    signal_err(DXVERSION, silence);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_mkdir() is the "Mkdir..." command entry point: prompts the
**	user for a new directory name/spec and dispatches to filer_mkdir$1()
**	to actually create it.

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	    filer_mkdir()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error creating a directory");
    get_userinput_and_execute(
	filer_mkdir$1,
	"Create Directory",
	"Directory Name: ",
	"",
	l_errmsg,
	"dummy");
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_mkdir$1() creates a_newdir on disk via lib$create_dir(),
**	then brings the new directory file into the filer cache and every
**	open window's display the same way any newly-created file is added
**	(filestat()/update_filer_cache()/add_to_direct_cache()/add_to_filer()).

**
**  INPUT PARAMETERS:
**
**	a_newdir  -  the new directory specification typed by the user.
**	a_dummy   -  unused (get_userinput_and_execute() calling convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with a human-readable message if directory
**		     creation or a later step fails.

**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_FILE (used indirectly via filestat()'s directory
**	lookup) and cntrl_info_block (windows/cur_win, for updating every
**	open window's display).

**
**  IMPLICIT OUTPUTS:
**
**	Creates the directory on disk via lib$create_dir(); updates the
**	filer cache (update_filer_cache()), the directory-tree cache
**	(add_to_direct_cache()), and every open window's FILER display
**	(add_to_filer()) so the new directory shows up immediately.

**
**  RETURNS VALUE:
**
**	DX__NORMAL on success; DX__ERROR if lib$create_dir() or filestat()
**	fails; -1 if the new directory spec has an unexpected shape (no '.'
**	or '[' to locate the parent directory).

**
**--
*/
int	    filer_mkdir$1(a_newdir, a_dummy, a_errmsg)
char *a_newdir,
     *a_dummy,
     *a_errmsg;
{
    $DESCRIPTOR (l_newdir_descrip, a_newdir);
    unsigned short int l_len;
    char l_filespec[MAXFILESPEC+1];
    char *l_indx;
    char l_name[MAXFILESPEC+1];
    struct fil_dx_tag l_temp_entry;
    struct NAM l_nam;
    struct FAB l_fab;
    struct XABPRO l_xabpro;
    struct XABDAT l_xabdat;
    int l_i;
    char l_resultant_name[NAM$C_MAXRSS];
    unsigned int l_status;
 
    LENGTH(l_newdir_descrip) = strlen(a_newdir);
 
    if (!((l_status = lib$create_dir (
			&l_newdir_descrip,
			0,
			0,
			0,
			0,
			0)) & 1))
	return DX__ERROR;
 
    /*
    **	Convert directory spec to file spec
    **/
 
    strcpy(l_filespec, a_newdir);
    if ((l_indx = strrchr(l_filespec, '.')) == NULL)
	if ((l_indx = strchr(l_filespec, '[')) == NULL)
	    return -1;
 
    /*
    **	Get parent directory name
    **/
    l_indx++;
    strcpy(l_name, l_indx);
    if ((l_indx = strchr(l_name, ']')) == NULL)
	return -1;
    *l_indx = EOS;
 
    if (strcmp(l_name, "000000")) {
 
	/*
	**  Not a system master directory
	**/
	l_indx = strchr(l_filespec, ']');
	*l_indx++ = '.';
	*l_indx++ = '-';
	*l_indx++ = ']';
	*l_indx = EOS;
    }
 
    strcat(l_filespec, l_name);
    strcat(l_filespec, ".DIR;1");
 
    /*
    **	Get directory file attributes
    **/
 
    if (filestat(l_filespec, &l_temp_entry, a_errmsg) == DX__ERROR)
    {
	strcpy(a_errmsg, "Error getting directory attributes");
        return DX__ERROR;
    }
 
    l_temp_entry.state = waiting;
    l_temp_entry.beg_y = l_temp_entry.beg_x = 0;
    l_temp_entry.forward = l_temp_entry.backward = NULL;
 
    /*
    **  Update cache queue.
    **	In case of running out of memory, output error message
    **	but update current display(s) anyway.
    **/
 
    if (update_filer_cache(l_temp_entry, add_to_filer_cache, a_errmsg) == DX__ERROR)
        signal_err(a_errmsg, bell);
 
    /*
    **	Update direct queue
    **/
 
    /**  Init RMS$ data structures  **/
 
    l_fab = cc$rms_fab;
    l_nam = cc$rms_nam;
    l_xabpro = cc$rms_xabpro;
    l_xabdat = cc$rms_xabdat;
 
    /*
    **	use RMS service to get the full file spec and
    **	characteristics.  Default directory is cwd.
    **
    **/
 
    l_fab.fab$l_fna = l_filespec;
    l_fab.fab$b_fns = strlen(l_fab.fab$l_fna);
    l_fab.fab$l_nam = &l_nam;
    l_fab.fab$l_xab = &l_xabpro;
    l_xabpro.xab$l_nxt = &l_xabdat;
 
    l_nam.nam$l_rsa = l_resultant_name;
    l_nam.nam$b_rss = sizeof l_resultant_name;
 
    if ((l_status = sys$open (
			&l_fab,
			0,
			0)) & 1)
    {
	l_resultant_name[l_nam.nam$b_rsl] = EOS;
	l_indx = strchr(l_resultant_name, ']');
	*l_indx = '.';
	l_indx = strrchr(l_resultant_name, '.');
	*l_indx = ']';
	*++l_indx = EOS;	    /**  Convert filespec to dirspec  **/
 
	l_status = sys$close(
		    &l_fab,
		    0,
		    0);
 
	if (add_to_direct_cache(l_resultant_name, a_errmsg) == DX__ERROR)
	    signal_err(a_errmsg, bell);
    }
 
    /**  Add it to the FILER  **/
    if (add_to_filer(l_temp_entry, a_errmsg, cntrl_info_block.cur_win) == DX__ERROR)
        return DX__ERROR;
 
    /**  Update other windows  **/
    for (l_i = 0;  l_i < cntrl_info_block.windows;  l_i++)
    {
        if (l_i != cntrl_info_block.cur_win)
        {
            if (add_to_filer(l_temp_entry, a_errmsg, l_i) == DX__ERROR)
                 return DX__ERROR;
        }
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_show_buffer() is the "show Buffer" command: lets the user
**	pick one of the saved directory-cache slots via
**	select_from_filer_cache(), then restores that slot as the current
**	directory view via filer_filter$1(..., "Restore", ...).

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	    filer_show_buffer()
{
    char l_errmsg[256];
    char l_numstr[80];
    int l_n;
    unsigned long l_rc;
 
    if ((l_n = select_from_filer_cache()) != -1)
    {
    	strcpy(l_errmsg, "Error restoring from filer buffer");
	{
	    static $DESCRIPTOR (l_fao_ctrl, "!SL");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;

	    l_out_desc.dsc$w_length = sizeof(l_numstr) - 1;
	    l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_numstr;
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_n))))
		$LOG(l_rc);
	    l_numstr[l_len] = EOS;
	}
        if (!(1 & (l_rc = smg$set_cursor_abs (
                     &cntrl_info_block.commands_display.id,
                     &cntrl_info_block.commands_display.rows,
                     &cntrl_info_block.commands_display.width))))
            $LOG(l_rc);
	if (filer_filter$1(l_numstr, "Restore", l_errmsg) == DX__ERROR)
	    signal_err(l_errmsg, bell);
    }
 
    return DX__NORMAL;
}



/* ---- (from FILEREXE.C -- execute VMS command / gateway) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_vms_gateway() is the "VMS Gateway" command: spawns an
**	interactive DCL subprocess via vms_gateway(), and if that succeeds
**	and auto-update is enabled, re-reads the current directory afterward
**	(the subprocess may have changed files) and unpastes any leftover
**	status display.

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
**	cntrl_info_block.user_pref.auto_update.

**
**  IMPLICIT OUTPUTS:
**
**	May trigger a full directory re-read (see filer_update()).

**
**  RETURNS VALUE:
**
**	Whatever vms_gateway() returned (DX__NORMAL or DX__ERROR).

**
**--
*/
int	filer_vms_gateway()
{
    int l_status;
    unsigned long l_rc;

    l_status = vms_gateway();
    if (l_status == DX__NORMAL && cntrl_info_block.user_pref.auto_update)
    {
	signal_err("Updating directory...", silence);
	filer_update();
	if (!(1 & (l_rc = smg$unpaste_virtual_display (
		  &cntrl_info_block.status_display.id,
		  &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);
    }

    return l_status;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      VMS_GATEWAY() spawns a child process and execute at DCL level.
**
**--
**/
int	vms_gateway()
{
    char l_errmsg[MAXFILESPEC+1];
    unsigned int l_status;
    int l_subpid;	/**  Sub-process  **/
    unsigned int l_display_id;
    unsigned long l_rc;

    /*
    **	Set default directory so that the subprocess will have
    **	correct SYS$DISK definition
    **/

    setddir(DX_CURRENT_DIRECTORY.cur_dir, l_errmsg);

    /*
    **	Spawn an new process
    **/

    if (!(1 & (l_rc = smg$save_physical_screen (
		&cntrl_info_block.pasteboard_id,
		&l_display_id,
		0,
		0))))
        $LOG(l_rc);

    if (!(1 & (l_rc = smg$set_physical_cursor (
	      &cntrl_info_block.pasteboard_id,
	      &cntrl_info_block.pasteboard_rows,
	      &1))))
        $LOG(l_rc);

    if (!(1 & (l_rc = smg$disable_broadcast_trapping (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);

    fputs("TYPE LOGOUT TO RETURN TO DX", stdout);

    l_status = lib$spawn (
		    0,
		    0,
		    0,
		    0,
		    0,
		    &l_subpid,
		    0,
		    0,
		    0,
		    0,
		    $DESCR ("1$ "),
		    0);

    /*
    **	Restore old states
    **/

    if (!(1 & (l_rc = smg$set_broadcast_trapping (
                 &cntrl_info_block.pasteboard_id,
                 broadcast_routine,
                 0))))
        $LOG(l_rc);

    setddir(DX_CURRENT_DIRECTORY.cur_dir, l_errmsg);

    if (!(1 & (l_rc = smg$restore_physical_screen (
	      &cntrl_info_block.pasteboard_id,
	      &l_display_id))))
        $LOG(l_rc);

    if (l_status & 1)
	return DX__NORMAL;
    else
    {
	signal_err("Error spawning a child process", bell);
	return DX__ERROR;
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_vms_command() is the "Command" command: prompts the user
**	for one DCL command line and dispatches it to vms_command() for
**	execution, then -- if auto-update is enabled -- re-reads the current
**	directory afterward, since the command may have changed files.

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
**	cntrl_info_block.user_pref.auto_update.

**
**  IMPLICIT OUTPUTS:
**
**	May trigger a full directory re-read (see filer_update()).

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_vms_command()
{
    char l_errmsg[255];
    unsigned long l_rc;

    strcpy(l_errmsg, "Error executing command");
    get_userinput_and_execute(
	vms_command,
	"DCL Command",
	"Command: ",
	"",
	l_errmsg,
	"");
    if (cntrl_info_block.user_pref.auto_update)
    {
	signal_err("Updating directory...", silence);
	filer_update();
	if (!(1 & (l_rc = smg$unpaste_virtual_display (
		  &cntrl_info_block.status_display.id,
		  &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      VMS_COMMAND() spawns a child process and execute a command.
**
**--
**/
int	vms_command(a_result_string, a_dummy, a_errmsg)
char *a_result_string;
char *a_dummy;
char *a_errmsg;
{
    /**  Subprocess display id and AST arguments  **/
    static unsigned int l_display_id = 0;

    /**  Current working directory  **/
    static char l_cwd[MAXFILESPEC+1] = "";

    /**  COMMAND STRING  **/
    char l_command_string[255];
    $DESCRIPTOR (l_command_string_descrip, l_command_string);

    int l_cur_row;
    unsigned int l_status;
    unsigned int l_prompt_display_id;
    unsigned short int l_word_terminator_code;
    int l_p;
    unsigned long l_rc;

    /*
    **	Set the signal back to system default so that it won't
    **	interfere with I/O used by the subprocess.
    **/

    alarm(0);
    signal(SIGALRM, SIG_DFL);

    if (l_display_id == 0)
    {	    /**  This is the first time called, create a subprocess  **/
        if (!(1 & (l_rc = smg$create_virtual_display (
                     &cntrl_info_block.pasteboard_rows,
                     &cntrl_info_block.pasteboard_width,
                     &l_display_id,
                     0,
                     0,
                     0))))
            $LOG(l_rc);
        if (!(1 & (l_rc = smg$create_subprocess (
                     &l_display_id,
                     0,
                     0))))
            $LOG(l_rc);
    }

    /*
    **	Set default directory so that the subprocess will have
    **	the correct SYS$DISK definition.
    **/

    if (strcmp (l_cwd, DX_CURRENT_DIRECTORY.cur_dir))
    {
        if (strcmp (l_cwd, ""))
        {
            l_cur_row = smg$cursor_row (
                          &l_display_id);
	    {
		static $DESCRIPTOR (l_fao_ctrl, "$ SET DEFAULT !AZ");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_len;

		l_out_desc.dsc$w_length = sizeof(l_command_string) - 1;
		l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_command_string;
		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			DX_CURRENT_DIRECTORY.cur_dir))))
		    $LOG(l_rc);
		l_command_string[l_len] = EOS;
	    }
	    LENGTH(l_command_string_descrip) = strlen(l_command_string);
	    if (!(1 & (l_rc = smg$execute_command (
			 &l_display_id,
			 &l_command_string_descrip,
			 0,
			 &l_status))))
	        $LOG(l_rc);
            if (!(l_status &1))
            {
                {
		    static $DESCRIPTOR (l_fao_ctrl, "Error changing default directory to !AZ");
		    struct dsc$descriptor_s l_out_desc;
		    unsigned short int l_len;

		    l_out_desc.dsc$w_length = MAXFILESPEC;
		    l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
		    l_out_desc.dsc$b_class = DSC$K_CLASS_S;
		    l_out_desc.dsc$a_pointer = a_errmsg;
		    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			    DX_CURRENT_DIRECTORY.cur_dir))))
			$LOG(l_rc);
		    a_errmsg[l_len] = EOS;
		}
		return DX__NORMAL;
            }
            if (!(1 & (l_rc = smg$erase_display (
                         &l_display_id,
                         &l_cur_row,
                         &1,
                         0,
                         0))))
                $LOG(l_rc);
            if (!(1 & (l_rc = smg$set_cursor_abs (
                         &l_display_id,
                         &l_cur_row,
                         &1))))
                $LOG(l_rc);
        }
        strcpy (l_cwd, DX_CURRENT_DIRECTORY.cur_dir);
    }

    /*
    **	Paste the subprocess display
    **/

    if (!(1 & (l_rc = smg$paste_virtual_display (
                 &l_display_id,
                 &cntrl_info_block.pasteboard_id,
                 &1,
                 &1,
                 0))))
        $LOG(l_rc);

    /*
    **	Execute the DCL command
    **/

    {
	static $DESCRIPTOR (l_fao_ctrl, "$ !AZ");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	l_out_desc.dsc$w_length = sizeof(l_command_string) - 1;
	l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_command_string;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_result_string))))
	    $LOG(l_rc);
	l_command_string[l_len] = EOS;
    }
    LENGTH(l_command_string_descrip) = strlen(l_command_string);
    if (!(1 & (l_rc = smg$execute_command (
		 &l_display_id,
		 &l_command_string_descrip,
		 0,
		 &l_status))))
        $LOG(l_rc);

    l_cur_row = smg$cursor_row (
	     &l_display_id);

    /*
    **	Scroll until two blank lines are at the bottom of the
    **	screen and wait for user response.
    **/

    for (l_p = l_cur_row; l_p > cntrl_info_block.pasteboard_rows-2; l_p--)
    {
	if (!(1 & (l_rc = smg$scroll_display_area (
		     &l_display_id,
		     0,
		     0,
		     0,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);
	l_cur_row--;
	if (!(1 & (l_rc = smg$set_cursor_abs (
		     &l_display_id,
		     &l_cur_row,
		     0))))
	    $LOG(l_rc);
    }

    /*
    **  Create a display to get user's response.  No I/O operation
    **  is allowed in subprocess's display.
    **/

    if (!(1 & (l_rc = smg$create_virtual_display (
		 &1,
		 &cntrl_info_block.pasteboard_width,
		 &l_prompt_display_id,
		 0,
		 0,
		 0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$paste_virtual_display (
		 &l_prompt_display_id,
		 &cntrl_info_block.pasteboard_id,
		 &cntrl_info_block.pasteboard_rows,
		 &1,
		 0))))
        $LOG(l_rc);
    smg$read_keystroke (
		 &cntrl_info_block.keyboard_id,
		 &l_word_terminator_code,
		 $DESCR ("Press any key to continue ... "),
		 0,
		 &l_prompt_display_id,
		 0,
		 0);
    if (!(1 & (l_rc = smg$delete_virtual_display (
		 &l_prompt_display_id))))
        $LOG(l_rc);

    /*
    **	Restore to old states
    **/

    if (!(1 & (l_rc = smg$unpaste_virtual_display (
		 &l_display_id,
		 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);

    return (l_status & 1) ? DX__NORMAL : DX__ERROR;
}



/* ---- (from FILERLAU.C -- launch file) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_launch() is the "Run..." command entry point: dispatches
**	to filer_multiple_launch() or filer_single_launch() depending on
**	whether the current window is in single- or multiple-file mode.

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
**	The current window's file-selection mode (read by
**	dxscr_dispatch_filemode()).

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_launch()
{
    return dxscr_dispatch_filemode(filer_multiple_launch, filer_single_launch);
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_multiple_launch() prompts once for an optional argument
**	string, then runs filer_launch$1() (via
**	multi_get_userinput_and_exec()) for every marked file in the current
**	window.  Re-reads the directory afterward if auto-update is on.

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
**	cntrl_info_block.user_pref.auto_update.

**
**  IMPLICIT OUTPUTS:
**
**	May trigger a full directory re-read.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_multiple_launch()
{
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error launching file");
    multi_get_userinput_and_exec(
	filer_launch$1,
	"Multiple Launch",
	"Specify optional argument(s): ",
	"",
	l_errmsg);
    if (cntrl_info_block.user_pref.auto_update)
	filer_update();
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_single_launch() prompts for an optional argument string and
**	runs filer_launch$1() on the file currently under the cursor (unless
**	it is the "[-]" parent-directory placeholder, which cannot be
**	launched).  Re-reads the directory afterward if auto-update is on.

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
**	DX_CURRENT_FILE; cntrl_info_block.user_pref.auto_update.

**
**  IMPLICIT OUTPUTS:
**
**	May trigger a full directory re-read.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_single_launch()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
	signal_err("Execute a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error launching file");
	get_userinput_and_execute(
	    filer_launch$1,
	    "Launch File",
	    "Specify optional argument(s): ",
	    "",
	    l_errmsg,
	    l_fn);
	if (cntrl_info_block.user_pref.auto_update)
	    filer_update();
    }
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_launch$1() runs a_from as a command: if it is a .COM file it
**	is executed with an '@' prefix (a DCL procedure call); otherwise a
**	temporary DCL symbol is defined pointing at it (so it runs as an
**	image) and that symbol name is spawned instead.  Either way the
**	command runs in a spawned subprocess with a_arguments appended, and
**	the user is prompted to press a key before the screen is restored.

**
**  INPUT PARAMETERS:
**
**	a_arguments  -  optional command-line argument string typed by the
**			user, passed through to the spawned command/procedure.
**	a_from       -  full file specification of the file to run.

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with a message if lib$set_symbol() fails.

**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY.cur_dir (used by setddir() so the spawned
**	subprocess starts in the right default directory).

**
**  IMPLICIT OUTPUTS:
**
**	Saves and restores the physical screen around the spawned
**	subprocess; defines and deletes the temporary DXM__LAUNCH_SYMBOL DCL
**	symbol used to invoke non-.COM files.

**
**  RETURNS VALUE:
**
**	DX__NORMAL if the spawn succeeded; DX__ERROR if a screen-save/
**	restore call failed; DX__DONTCARE if lib$set_symbol() failed (the
**	physical screen is restored first in that case, so the caller need
**	not do anything further).

**
**--
*/
int	filer_launch$1(a_arguments, a_from, a_errmsg)
char *a_arguments;
char *a_from;
char *a_errmsg;
{
    char l_command_string[256];
    $DESCRIPTOR (l_command_string_descrip, l_command_string);
    unsigned int l_status;
    unsigned long l_rc;	/**  SMG$ screen save/restore/cursor status --
			**   kept separate from l_status so restoring the
			**   screen at the end never clobbers the real
			**   lib$set_symbol/lib$spawn result this function
			**   returns.  **/
    char l_launch_sym[] = "DXM__LAUNCH_SYMBOL";
    $DESCRIPTOR (l_launch_symbol_descrip, l_launch_sym);
    char l_value_string[256];
    $DESCRIPTOR (l_value_string_descrip, l_value_string);
    unsigned int l_display_id;
    char l_dirbuf[MAXFILESPEC+1];
    unsigned short l_word_terminator_code;
    int l_subpid;	/**  Sub-process  **/
    char l_str[128];
    $DESCRIPTOR (l_str_descrip, l_str);

    /*
    **	Set the signal back to system default so that it won't
    **	interfere with I/O used by the subprocess.
    **/

    alarm(0);
    signal(SIGALRM, SIG_DFL);

    if (!(1 & (l_rc = smg$save_physical_screen(&cntrl_info_block.pasteboard_id, &l_display_id, 0, 0))))
	return $LOG(DX__ERROR);

    if (!(1 & (l_rc = smg$set_physical_cursor(&cntrl_info_block.pasteboard_id, &cntrl_info_block.pasteboard_rows, &1))))
	return $LOG(DX__ERROR);

    /*
    **	    Format a command-line string.  Prefix a '@' if file type is
    **	    .COM; othererwise, define a temporary symbol to be execute by
    **	    SYS$SPAWN routine.
    **/

    if (strstr(a_from, ".COM;") != NULL)
    {
	l_command_string[0] = '@';
	strcpy(l_command_string + 1, a_from);
	strcat(l_command_string, " ");
	strcat(l_command_string, a_arguments);
	fputs("1$ ", stdout);
	fputs(l_command_string, stdout);
	fputs("\n", stdout);
    }
    else
    {
	l_value_string[0] = '$';
	strcpy(l_value_string + 1, a_from);
	LENGTH(l_value_string_descrip) = strlen(l_value_string);
	l_status = lib$set_symbol (
		    &l_launch_symbol_descrip,
		    &l_value_string_descrip,
		    0);
	if (!(l_status & 1))
	{
	    strcpy(a_errmsg, "Error setting local symbol to execute a DCL command");
	    /**  Undo smg$save_physical_screen() above -- this early
	    **   return used to skip smg$restore_physical_screen(),
	    **   leaving the physical screen in its saved/blanked state.
	    **/
	    if (!(1 & (l_rc = smg$restore_physical_screen(&cntrl_info_block.pasteboard_id, &l_display_id))))
		return $LOG(DX__ERROR);
	    return DX__DONTCARE;
	}

	strcpy(l_command_string, l_launch_sym);
	strcat(l_command_string, " ");
	strcat(l_command_string, a_arguments);
	fputs("1$ ", stdout);
	fputs(strchr(a_from, ']')+1, stdout);
	fputs(" ", stdout);
	fputs(a_arguments, stdout);
	fputs("\n", stdout);
    }

    /*
    **	Change the definition of SYS$DISK and terminate the mailbox
    **/

    setddir(DX_CURRENT_DIRECTORY.cur_dir, a_errmsg);

    LENGTH(l_command_string_descrip) = strlen(l_command_string);
    l_status = lib$spawn (
		    &l_command_string_descrip,
		    0,
		    0,
		    0,
		    0,
		    &l_subpid,
		    0,
		    0,
		    0,
		    0,
		    0,
		    0);

    /*
    **	Restore to old states
    **/

    setddir(DX_CURRENT_DIRECTORY.cur_dir, a_errmsg);

    lib$delete_symbol (	    /**  DELETE SYMBOL  **/
        &l_launch_symbol_descrip,
        0);

    if ((l_status & 1))
    {
	/**  Executing a DCL command was successful  **/
	sleep(2);
        smg$read_keystroke (
            &cntrl_info_block.keyboard_id,
            &l_word_terminator_code,
            $DESCR ("\n\n\nPress any key to continue ... "),
            0,
            0,
            0,
            0);
    }

    if (!(1 & (l_rc = smg$restore_physical_screen(&cntrl_info_block.pasteboard_id, &l_display_id))))
	return $LOG(DX__ERROR);

    return ((l_status & 1)? DX__NORMAL : DX__ERROR);
}



/* ---- (from FILERPRO.C -- file protection) ---- */

    static int		    filer_protection$2(char *, unsigned short, char *, unsigned short);
    static int		    chprot$1(char *, unsigned short *, char *);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_protection() is the "Set protection..." command entry
**	point: dispatches to filer_multiple_protection() or
**	filer_single_protection() depending on the current window's file-
**	selection mode.

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
**	The current window's file-selection mode.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_protection()
{
    return dxscr_dispatch_filemode(filer_multiple_protection, filer_single_protection);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_multiple_protection() builds the current file's protection
**	string (S:xxxx,O:xxxx,G:xxxx,W:xxxx, from DX_CURRENT_FILE->fpro via
**	getpro()) as the initial prompt value, then runs filer_protection$1()
**	(via multi_get_userinput_and_exec()) on every marked file.

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
**	DX_CURRENT_FILE->fpro (used only to seed the prompt's default
**	value -- each marked file keeps its own protection unless the user's
**	spec changes it).

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_multiple_protection()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_spro[5],   /**  PROTECTION CODES  **/
	 l_opro[5],
	 l_gpro[5],
	 l_wpro[5];
    char l_original_prot[28];

    getpro(DX_CURRENT_FILE->fpro, l_spro, l_opro, l_gpro, l_wpro);
    strcpy(l_original_prot, "S");
    if (strlen(l_spro) > 0)
	strcat(l_original_prot, ":");
	strcat(l_original_prot, l_spro);
    strcat(l_original_prot, ",");
    strcat(l_original_prot, "O");
    if (strlen(l_opro) > 0)
	strcat(l_original_prot, ":");
	strcat(l_original_prot, l_opro);
    strcat(l_original_prot, ",");
    strcat(l_original_prot, "G");
    if (strlen(l_gpro) > 0)
	strcat(l_original_prot, ":");
	strcat(l_original_prot, l_gpro);
    strcat(l_original_prot, ",");
    strcat(l_original_prot, "W");
    if (strlen(l_wpro) > 0)
	strcat(l_original_prot, ":");
	strcat(l_original_prot, l_wpro);

    strcpy(l_errmsg, "Error setting file protection");
    multi_get_userinput_and_exec(
	filer_protection$1,
	"Multiple Set Protection",
	"Enter File Protection Spec: ",
	l_original_prot,
	l_errmsg);
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FILER_SINGLE_PROTECTION() calls FILER_PROTECTION$1 to change
**	the file protection
**
**--
**/
int filer_single_protection()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];
    char l_spro[5],   /**  PROTECTION CODES  **/
	 l_opro[5],
	 l_gpro[5],
	 l_wpro[5];
    char l_original_prot[28];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Set protection of a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	getpro(DX_CURRENT_FILE->fpro, l_spro, l_opro, l_gpro, l_wpro);
	strcpy(l_original_prot, "S");
	if (strlen(l_spro) > 0)
	    strcat(l_original_prot, ":");
	    strcat(l_original_prot, l_spro);
	strcat(l_original_prot, ",");
	strcat(l_original_prot, "O");
	if (strlen(l_opro) > 0)
	    strcat(l_original_prot, ":");
	    strcat(l_original_prot, l_opro);
	strcat(l_original_prot, ",");
	strcat(l_original_prot, "G");
	if (strlen(l_gpro) > 0)
	    strcat(l_original_prot, ":");
	    strcat(l_original_prot, l_gpro);
	strcat(l_original_prot, ",");
	strcat(l_original_prot, "W");
	if (strlen(l_wpro) > 0)
	    strcat(l_original_prot, ":");
	    strcat(l_original_prot, l_wpro);

	strcpy(l_errmsg, "Error setting file protections");
	get_userinput_and_execute(
	    filer_protection$1,
	    "Set File Protection",
	    "Enter File Protection Spec: ",
	    l_original_prot,
	    l_errmsg,
	    l_fn);
    }
   return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FILER_PROTECTION$1() calls FILER_PROTECTION$2 to
**	change filer protection
**
**--
**/
int	filer_protection$1(a_protstr, a_from, a_errmsg)
char *a_protstr;
char *a_from;
char *a_errmsg;
{
    unsigned short l_protcode;
    int l_i;
    struct fil_dx_tag l_temp_entry;

    if (chprot(a_from, a_protstr, a_errmsg, &l_protcode) != DX__NORMAL)
	return DX__ERROR;

    if (filer_protection$2(
	    a_from, l_protcode, a_errmsg, cntrl_info_block.cur_win) == DX__ERROR)
        return DX__ERROR;

    /*
    **  Update filer cache
    **/

    if (filestat(a_from, &l_temp_entry, a_errmsg) == DX__ERROR)
        return DX__ERROR;
    if (update_filer_cache(
            l_temp_entry, change_to_filer_cache, a_errmsg) == DX__ERROR)
        return DX__ERROR;

    /**  Update other windows  **/
    for (l_i = 0;  l_i < cntrl_info_block.windows;  l_i++)
    {
        if (l_i != cntrl_info_block.cur_win)
        {
            if (filer_protection$2(a_from, l_protcode, a_errmsg, l_i) == DX__ERROR)
                 return DX__ERROR;
        }
    }

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FILER_PROTECTION$2() changes file protection and
**	update FILER display
**
**--
**/
static int filer_protection$2(
		char *a_from,
		unsigned short a_protcode,
		char *a_errmsg,
		unsigned short a_win)
{
    int l_rest_col;	/**  1st column after the file name field  **/
    char l_full_path_name[MAXFILESPEC+1];
    struct fil_dx_tag *l_p;
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR(l_str_descrip, l_str);
    unsigned long l_rc;

    /*
    **	    Update FILER
    **/
 
    if (cntrl_info_block.cur_win == a_win)
        l_p = DX_CURRENT_FILE;
    else
    {
	for (l_p = cntrl_info_block.dir_dx[a_win].dir_filelist->forward;
	     l_p != cntrl_info_block.dir_dx[a_win].non_dir_filelist; l_p = l_p->forward)
	{
	    if (l_p == cntrl_info_block.dir_dx[a_win].dir_filelist)
		l_p = cntrl_info_block.dir_dx[a_win].non_dir_filelist;
	    else
	    {
		find_full_path_name(l_p->fn, l_full_path_name, a_win);
		if (!strcmp(l_full_path_name, a_from))
		    break;
	    }
	}
	if (l_p == cntrl_info_block.dir_dx[a_win].non_dir_filelist)
	    return DX__NORMAL;
    }
 
    l_p->fpro = a_protcode;
    format_filer_entry(l_p, l_str, a_win);

    /*  14-Sep-2026 RRL: batch this entry's redraw (up to 3 smg$put_chars
    **  calls in the long-form case) plus the final cursor reposition into
    **  one flush.  **/
    if (!(1 & (l_rc = smg$begin_display_update(&cntrl_info_block.dir_dx[a_win].filer_display.id))))
        $LOG(l_rc);

    if (cntrl_info_block.dir_dx[a_win].form == short_form)
    {
	LENGTH(l_str_descrip) = strlen(l_str);
        if (l_p->fn[ strlen(l_p->fn)-1 ] != ']')
	{
	    if (!(1 & (l_rc = smg$put_chars (
			&cntrl_info_block.dir_dx[a_win].filer_display.id,
			&l_str_descrip,
			&l_p->beg_y,
			&l_p->beg_x,
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
	}
	else
	{
	    if (!(1 & (l_rc = smg$put_chars (
			&cntrl_info_block.dir_dx[a_win].filer_display.id,
			&l_str_descrip,
			&l_p->beg_y,
			&l_p->beg_x,
			0,
			&SMG$M_BOLD,
			0,
			0))))
	        $LOG(l_rc);
	}
    }
    else
    {
        if (l_p->fn[ strlen(l_p->fn)-1 ] != ']')
	{
	    LENGTH(l_str_descrip) = strlen(l_str);
	    if (!(1 & (l_rc = smg$put_chars (
			&cntrl_info_block.dir_dx[a_win].filer_display.id,
			&l_str_descrip,
			&l_p->beg_y,
			&l_p->beg_x,
			&SMG$M_ERASE_TO_EOL,
			0,
			0,
			0))))
	        $LOG(l_rc);
	}
	else
	{
	    LENGTH(l_str_descrip) = 1;
	    if (!(1 & (l_rc = smg$put_chars (
			&cntrl_info_block.dir_dx[a_win].filer_display.id,
			&l_str_descrip,
			&l_p->beg_y,
			&1,
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
	    memcpy (l_str, l_str+1, strlen(l_str));
	    LENGTH(l_str_descrip) = FILER_NAME_WIDTH;
	    if (!(1 & (l_rc = smg$put_chars (
			&cntrl_info_block.dir_dx[a_win].filer_display.id,
			&l_str_descrip,
			&l_p->beg_y,
			&2,
			0,
			&SMG$M_BOLD,
			0,
			0))))
	        $LOG(l_rc);
	    memcpy (l_str, l_str+FILER_NAME_WIDTH, strlen(l_str)-FILER_NAME_WIDTH+1);
	    LENGTH(l_str_descrip) = strlen(l_str);
	    l_rest_col = FILER_NAME_WIDTH+2;
	    if (!(1 & (l_rc = smg$put_chars (
			&cntrl_info_block.dir_dx[a_win].filer_display.id,
			&l_str_descrip,
			&l_p->beg_y,
			&l_rest_col,
			&SMG$M_ERASE_TO_EOL,
			0,
			0,
			0))))
	        $LOG(l_rc);
        }
    }
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
	      &cntrl_info_block.dir_dx[a_win].filer_display.id,
	      &l_p->beg_y,
	      &l_p->beg_x))))
        $LOG(l_rc);

    if (!(1 & (l_rc = smg$end_display_update(&cntrl_info_block.dir_dx[a_win].filer_display.id))))
        $LOG(l_rc);

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      CHPROT() uses ACP services to change a file's protection.
**	Use of RMS service will fail if the user tries to change
**	a file's protection when he does not have WRITE or DELETE privilege.
**
**--
**/
int chprot(a_from, a_protstr, a_errmsg, a_protcode)
char *a_from;
char *a_protstr;
char *a_errmsg;
unsigned short *a_protcode;
{
 
    /**  VARIABLES used by ACP  **/
 
    unsigned short int l_fpro;	    /**  File Protection  **/
    struct {			    /**  Access Control Block  **/
	unsigned short w_size;
	unsigned short w_type;
	void *l_addr;
    } l_acb[] = {
	{  2, ATR$C_FPRO, &l_fpro },
	{ 0, 0, (char *)NULL },
    };
    int l_iosb[2];
    static FIBDEF l_fib;
    $DESCRIPTOR(l_fib_descriptor, (char *)&l_fib);
    char l_short_filename[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descriptor, l_short_filename);
    unsigned short l_chan;	    /**  I/O channel  **/

    /**  LOCAL variables  **/

    char *l_cp;
    int l_status;
    int l_i;
    unsigned long l_rc;

    /*
    **  Initialization for ACP
    **/

    LENGTH(l_fib_descriptor) = sizeof (FIBDEF);

    for (l_i = 0; l_i <= 2; l_i++)
	__FIB(l_fib,fib$r_did_overlay,fib$w_did[l_i]) = DX_CURRENT_FILE->did[l_i];
 
    if ((l_cp = strchr(a_from, ']')) == 0)
	strcpy(l_short_filename, a_from);
    else
	strcpy(l_short_filename, l_cp+1);
 
    /*
    **	Find the I/O channel for this file
    **/
    if (!dxscr_find_chan_for_did(DX_CURRENT_FILE->did, &l_chan, (char *)NULL, &DX_CURRENT_DIRECTORY.cur_filter))
    {
	strcpy(a_errmsg, "Internal error - I/O channel for file not found");
	return DX__ERROR;
    }

    LENGTH(l_filespec_descriptor) = strlen(l_short_filename);
 
    if (!(1 & (l_rc = sys$qiow (0,
		 l_chan,
		 IO$_ACCESS,
		 (struct _iosb *) l_iosb,
		 0,
		 0,
		 &l_fib_descriptor,
		 (__int64) &l_filespec_descriptor,
		 0,
		 0,
		 (__int64) l_acb,
		 0))))
        $LOG(l_rc);
 
    if (!(l_iosb[0] & 1))
    {	    /**  No READ access  **/
	strcpy(a_errmsg, "Insufficient privilege for read operation");
	return DX__ERROR;
    }
    else
    {
	*a_protcode = l_fpro;
 
	if ((l_status = chprot$1(a_protstr, a_protcode, a_errmsg)) == DX__NORMAL)
	    l_fpro = *a_protcode;
 
	if (!(1 & (l_rc = sys$qiow (0,
			 l_chan,
			 IO$_MODIFY,
			 (struct _iosb *) l_iosb,
			 0,
			 0,
			 &l_fib_descriptor,
			 (__int64) &l_filespec_descriptor,
			 0,
			 0,
			 (__int64) l_acb,
			 0))))
	    $LOG(l_rc);
 
	if (!(l_iosb[0] & 1))
	{	/**  No MODIFY access  **/
	    strcpy(a_errmsg, "Error setting file protections - insufficient privileges");
	    return DX__ERROR;
	}
    }
 
    return l_status;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	chprot$1() walks a_protstr's comma-separated list of per-class
**	protection substrings (S/O/G/W), calling getprotbits() on each to get
**	its clear/set bitmasks, and applies them to *a_protcode one class at
**	a time.

**
**  INPUT PARAMETERS:
**
**	a_protstr   -  comma-separated protection spec, e.g.
**		       "S:RWED,O:RWED,G:RE,W:" (classes not mentioned are
**		       left unchanged).

**
**  OUTPUT PARAMETERS:
**
**	a_protcode  -  in/out: the file's current protection word on
**		       entry; updated in place, one class at a time, as each
**		       comma-separated substring is parsed.

**
**  IMPLICIT INPUTS:
**
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	a_errmsg  -  filled in with a syntax-error message if any
**		     substring is malformed.

**
**  RETURNS VALUE:
**
**	DX__NORMAL if every substring parsed correctly; -1 on the first
**	syntax error (a_errmsg is set, *a_protcode may be partially updated).

**
**--
*/
static int	chprot$1(a_protstr, a_protcode, a_errmsg)
char *a_protstr;
unsigned short int *a_protcode;
char *a_errmsg;
{
    char *l_cp1, *l_cp2;
    unsigned short int l_clear_bits, l_set_bits;
    char l_protstr1[80];

    strcpy(l_protstr1, a_protstr);
    l_cp1 = l_protstr1;
    l_cp2 = strchr(l_cp1, ',');
    while (l_cp2 != NULL) {

	*l_cp2 = EOS;
	if (getprotbits(l_cp1, &l_clear_bits, &l_set_bits) == -1) {
	    strcpy(a_errmsg, "File protections syntax error, re-specify");
	    return -1;
	}
	*a_protcode &= ~l_clear_bits;
	*a_protcode |= l_set_bits;
	l_cp1 = l_cp2+1;
	l_cp2 = strchr(l_cp1, ',');

    }

    if (getprotbits(l_cp1, &l_clear_bits, &l_set_bits) == -1) {
	strcpy(a_errmsg, "File protections syntax error, re-specify");
	return -1;
    }
    *a_protcode &= ~l_clear_bits;
    *a_protcode |= l_set_bits;

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	getprotbits() decodes one VMS file-protection class letter
**	(S/O/G/W) and its optional ":RWED"-style access-letter suffix into
**	the clear/set bitmasks the caller (chprot$1()) needs to update a
**	protection word one nibble at a time.  Each access letter present
**	clears the corresponding "deny" bit; letters not present leave that
**	bit set (denied).

**
**  INPUT PARAMETERS:
**
**	a_protsubstr  -  one protection class substring, e.g. "S:RWED" or
**			 just "S" (meaning no access) or "S:" (also no access).

**
**  OUTPUT PARAMETERS:
**
**	a_clear_bitsp  -  set to the 4-bit mask (shifted into the correct
**			   nibble for S/O/G/W) to AND OUT of the protection word.
**	a_set_bitsp    -  set to the 4-bit mask (same nibble) to OR IN.

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
**	DX__NORMAL on success; -1 if the leading class letter isn't one of
**	S/O/G/W, or an access-letter suffix contains something other than
**	r/R, w/W, e/E, d/D.

**
**--
*/
int	getprotbits(a_protsubstr, a_clear_bitsp, a_set_bitsp)
char *a_protsubstr;
unsigned short int *a_clear_bitsp;
unsigned short int *a_set_bitsp;
{
    char *l_cp;
    int l_shifts;

    l_cp = a_protsubstr;
    switch (*l_cp) {

	case 'S':
	    *a_clear_bitsp = 0xF;
	    l_shifts = 0;
	    break;
	case 'O':
	    *a_clear_bitsp = 0xF0;
	    l_shifts = 4;
	    break;
	case 'G':
	    *a_clear_bitsp = 0xF00;
	    l_shifts = 8;
	    break;
	case 'W':
	    *a_clear_bitsp = 0xF000;
	    l_shifts = 12;
	    break;
	default:
	    return -1;
    }

    l_cp++;
    *a_set_bitsp = 0xF;	/**  No access at all  **/
    if (*l_cp == ':')
	for (l_cp++; *l_cp; l_cp++)
	    switch (*l_cp) {
 
		case 'r':
		case 'R':
		    *a_set_bitsp &= 0xE;
		    break;
		case 'w':
		case 'W':
		    *a_set_bitsp &= 0xD;
		    break;
		case 'e':
		case 'E':
		    *a_set_bitsp &= 0xB;
		    break;
		case 'd':
		case 'D':
		    *a_set_bitsp &= 0x7;
		    break;
		default:
		    return -1;
 
	    }
    else if (*l_cp != EOS)
	return -1;
 
    *a_set_bitsp = (*a_set_bitsp << l_shifts);
    return DX__NORMAL;
}



/* ---- (from FILERFND.C -- find file) ---- */

#define MAXLINES 65535



static int	filer_find_by_page(struct FAB,
				   unsigned int,
				   unsigned int,
				   unsigned short int *,
				   char *,
				   char *);
static int	filer_find_no_page(struct FAB,
				   unsigned int,
				   unsigned int,
				   unsigned short int *,
				   char *,
				   char *);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_find() is the "Find..." (search inside a file) command
**	entry point: dispatches to filer_multiple_find() or
**	filer_single_find() depending on the current window's file-selection
**	mode.

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
**	The current window's file-selection mode.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_find()
{
    return dxscr_dispatch_filemode(filer_multiple_find, filer_single_find);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_multiple_find() prompts once for a search string, then runs
**	filer_find$1() (via multi_get_userinput_and_exec()) against every
**	marked file in the current window.

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_multiple_find()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error searching string");
    multi_get_userinput_and_exec(
	filer_find$1,
	"Multiple Find",
	"Find what? ",
	"",
	l_errmsg);
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_single_find() prompts for a search string and runs
**	filer_find$1() against the file currently under the cursor (unless
**	it is the "[-]" parent-directory placeholder, which cannot be
**	searched).

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
**	DX_CURRENT_FILE.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	filer_single_find()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];
 
    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Search a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error searching string");
	get_userinput_and_execute(
	    filer_find$1,
	    "Search String",
	    "Find what? ",
	    "",
	    l_errmsg,
	    l_fn);
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_find$1() opens a_from and scans it for a_pattern using the
**	same paged/non-paged virtual-display machinery filer_type$2() uses
**	for plain viewing, but with filer_find_by_page() or
**	filer_find_no_page() as the per-page callback (chosen by the user's
**	Type-by-page preference) so that only matching lines are shown,
**	highlighted, instead of the whole file.

**
**  INPUT PARAMETERS:
**
**	a_pattern  -  text to search for inside the file (case-
**		      insensitive -- both the pattern and the file content
**		      are upcased before comparing).
**	a_from     -  full file specification of the file to search.

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with a message if the file can't be opened.

**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.user_pref.type_by_page (selects which of the
**	two callbacks is used).

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	Whatever filer_type$2() returns (DX__NORMAL or DX__ERROR).

**
**--
*/
int	filer_find$1(a_pattern, a_from, a_errmsg)
char *a_pattern;
char *a_from;
char *a_errmsg;
{
    return filer_type$2 (
		a_from,
		a_errmsg,
		a_pattern,
		(cntrl_info_block.user_pref.type_by_page?
                    filer_find_by_page : filer_find_no_page));
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_find_by_page() is the "find, one screenful at a time"
**	engine: it reads a_from_fab line by line, upcases each line, and
**	searches it for a_pattern, keeping only the lines that match (with
**	the matched substrings marked SMG$M_BOLD) in a scrolling display
**	that fills one screen before pausing for the user to page with the
**	arrow keys, CR (next page), or KP0/CTRL-Y/CTRL-Z (cancel).  Reaching
**	end-of-file appends a one-line "N lines, M matches" summary instead
**	of another line of file content.

**
**  INPUT PARAMETERS:
**
**	a_from_fab                 -  FAB of the already-parsed file to
**				     search.
**	a_filer_find1_display_id   -  virtual display id for the scrolling
**				     matched-lines area.
**	a_filer_find2_display_id   -  virtual display id for the one-line
**				     status/prompt bar below it.
**	a_word_terminator_code     -  in/out: last keystroke read; used both
**				     to receive the terminating key from the
**				     caller's earlier read and to report the
**				     final one back.
**	a_pattern                  -  search string (upcased in place by this
**				     function before use).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an RMS error message if a read fails.

**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block (pasteboard id/rows, for positioning and
**	broadcast trapping).

**
**  IMPLICIT OUTPUTS:
**
**	Builds an in-memory array (l_lines[]/l_attrs[], up to MAXLINES
**	entries) of every matched line's text and highlight attributes as it
**	scans, so the user can page back up through earlier matches with the
**	UP arrow without re-reading the file; frees that memory before
**	returning (free_vm: label).  Pastes/erases the two virtual displays
**	and reads keystrokes from the keyboard.

**
**  RETURNS VALUE:
**
**	DX__NORMAL if the scan completed normally (RMS$_NORMAL or
**	RMS$_EOF); DX__ERROR if a read failed for any other reason.

**
**--
*/
static int	filer_find_by_page(a_from_fab,
				   a_filer_find1_display_id,
				   a_filer_find2_display_id,
				   a_word_terminator_code,
				   a_errmsg,
				   a_pattern)
struct FAB a_from_fab;
unsigned int a_filer_find1_display_id,
              a_filer_find2_display_id;
unsigned short int *a_word_terminator_code;
char *a_errmsg;
char *a_pattern;
{
    char l_linebuf[BUFSIZ+1];
    $DESCRIPTOR (l_linebuf_descrip, l_linebuf);
 
    char l_rendition_set[BUFSIZ+1];
    $DESCRIPTOR (l_rendition_set_descrip, l_rendition_set);
 
    struct RAB l_from_rab;
 
    int l_filer_find1_bottom = cntrl_info_block.pasteboard_rows-2;
    int l_lastline, l_linepos;
    char l_errmsg1[MAXFILESPEC+1];
    $DESCRIPTOR (l_errmsg1_descrip, l_errmsg1);
    unsigned short int l_len;
    unsigned int l_retcode;
    char *l_base_address;
    char *l_lines[MAXLINES],
	 *l_attrs[MAXLINES];
    int l_allocate_size;
    int l_count;
 
    int l_total_found;
    char l_upcase_string[BUFSIZ+1];
    $DESCRIPTOR (l_upcase_string_descrip, l_upcase_string);
    char *l_start, *l_cp;
    int l_eof_flag = 0;
    int l_file_lines;
    int l_filer_find2_pasteboard_row = cntrl_info_block.pasteboard_rows-1;
    unsigned long l_rc;
    int l_to_edge = 0;		/* Home/End: repeat the page step to the edge */
 
    /*
    **  Trap broadcast messages
    **/
 
    if (!(1 & (l_rc = smg$set_broadcast_trapping (
                 &cntrl_info_block.pasteboard_id,
                 broadcast_routine,
                 0))))
        $LOG(l_rc);
 
    /*
    **	Initialize Source File RAB
    **/
 
    l_from_rab = cc$rms_rab;
    l_from_rab.rab$l_fab = &a_from_fab;
    l_from_rab.rab$b_rac = RAB$C_SEQ;
    l_from_rab.rab$l_ubf = l_linebuf;
    l_from_rab.rab$w_usz = BUFSIZ;
 
    if (!((l_retcode = sys$connect (
			&l_from_rab,
			0,
			0)) & 1))
    {
	lib$sys_getmsg (
	    &l_retcode,
	    &l_len,
	    &l_errmsg1_descrip,
	    0,
	    0);
	l_errmsg1[l_len] = EOS;
	strcpy(a_errmsg, l_errmsg1);
	return DX__ERROR;
    }
 
    l_len = strlen(a_pattern);

    /*
    **	Upcase the search pattern -- the file content it is matched
    **	against below is upcased too (see upcase_string), so a
    **	lower-case pattern would otherwise never match.
    **/
    {
    char *l_cp1;

    for (l_cp1 = a_pattern;  *l_cp1;  l_cp1++)
	*l_cp1 = toupper((int) *l_cp1);
    }

    /*
    **	Set the signal back to system default so that it won't
    **	interfere with smg$read_keystroke() routine.
    **	Reset display attributes to avoid SMG$PUT_LINE_MULTI bugs.
    **/
 
    alarm(0);
    signal(SIGALRM, SIG_DFL);
 
    if (!(1 & (l_rc = smg$change_virtual_display (
	     	  &a_filer_find1_display_id,
	     	  0,
	     	  0,
	     	  &0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
    l_file_lines = l_total_found = l_lastline = 0;
 
    /*
    **	Display the first page and wait for user response
    **/
 
    l_retcode = sys$get (
		  &l_from_rab,
		  0,
		  0);
 
    l_count = 1;
    while (l_count <= l_filer_find1_bottom)
    {
	if (l_retcode == RMS$_EOF && l_total_found > 0)
	{
	    l_eof_flag++;
	    LENGTH(l_rendition_set_descrip) = 0;
	    if (!(1 & (l_rc = smg$put_line_multi (
			&a_filer_find1_display_id,
			$DESCR (""),
			&l_rendition_set_descrip,
			0,
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = lib$get_vm (
			    &1,
			    &l_base_address,
			    0))))
	        return $LOG(l_rc);
	    l_base_address[0] = EOS;
	    if (l_lastline < MAXLINES-1)
	    {
	    l_lines[l_lastline] = l_base_address;
	    if (!(1 & (l_rc = lib$get_vm (
			    &1,
			    &l_base_address,
			    0))))
	        return $LOG(l_rc);
	    l_base_address[0] = SMG$M_NORMAL;
	    l_attrs[l_lastline] = l_base_address;
	    l_lastline++;
	    }
 
	    {
		static $DESCRIPTOR (l_fao_ctrl, "!AZ - !SL line!AZ, !SL match!AZ");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_len;
		unsigned long l_rc2;

		l_out_desc.dsc$w_length = sizeof(l_linebuf) - 1;
		l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_linebuf;
		if (!(1 & (l_rc2 = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			a_from_fab.fab$l_fna, l_file_lines, (l_file_lines > 1? "s" : ""),
			l_total_found, (l_total_found > 1? "s" : "")))))
		    $LOG(l_rc2);
		l_linebuf[l_len] = EOS;
	    }
	    LENGTH(l_linebuf_descrip) = strlen(l_linebuf);
	    memset(l_rendition_set, SMG$M_NORMAL, strlen(l_linebuf));
	    LENGTH(l_rendition_set_descrip) = strlen(l_linebuf);
	    if (!(1 & (l_rc = smg$put_line_multi (
			&a_filer_find1_display_id,
			&l_linebuf_descrip,
			&l_rendition_set_descrip,
			0,
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
	    l_allocate_size = strlen(l_linebuf)+1;
	    if (!(1 & (l_rc = lib$get_vm (
			    &l_allocate_size,
			    &l_base_address,
			    0))))
	        return $LOG(l_rc);
	    memcpy(l_base_address, l_linebuf, l_allocate_size-1);
	    l_base_address[l_allocate_size-1] = EOS;
	    if (l_lastline < MAXLINES-1)
	    {
	    l_lines[l_lastline] = l_base_address;
	    if (!(1 & (l_rc = lib$get_vm (
			    &l_allocate_size,
			    &l_base_address,
			    0))))
	        return $LOG(l_rc);
	    memset(l_base_address, SMG$M_NORMAL, l_allocate_size-1);
	    l_base_address[l_allocate_size-1] = EOS;
	    l_attrs[l_lastline] = l_base_address;
	    l_lastline++;
	    }
	    break;
	}
	else if (l_retcode != RMS$_NORMAL)
	{
	    lib$sys_getmsg (
		&l_retcode,
		&l_len,
		&l_errmsg1_descrip,
		0,
		0);
	    l_errmsg1[l_len] = EOS;
	    strcpy(a_errmsg, l_errmsg1);
	    if (l_lastline > 0)
		smg$end_display_update (&a_filer_find1_display_id);
	    goto free_vm;
	}
	
	l_file_lines++;
	l_linebuf[l_from_rab.rab$w_rsz] = EOS;
	LENGTH(l_linebuf_descrip) = l_from_rab.rab$w_rsz;
	memset(l_rendition_set, SMG$M_NORMAL, (size_t) l_from_rab.rab$w_rsz);
	LENGTH(l_rendition_set_descrip) = l_from_rab.rab$w_rsz;
        str$upcase (
            &l_upcase_string_descrip,
            &l_linebuf_descrip);
	l_upcase_string[l_from_rab.rab$w_rsz] = EOS;
 
	for (l_start = l_upcase_string;  l_cp = strstr(l_start, a_pattern);  l_start = l_cp+1)
	{
	    l_total_found++;
	    memset(l_rendition_set+(l_cp-l_upcase_string), SMG$M_BOLD, (size_t) l_len);
	}
	if (l_start != l_upcase_string)
	{
	    if (l_lastline == 0)
	    {
		if (!(1 & (l_rc = smg$paste_virtual_display (
			  &a_filer_find1_display_id,
			  &cntrl_info_block.pasteboard_id,
			  &FILER_TYPE1_PBD_ROW,
			  &FILER_TYPE1_PBD_COLUMN,
			  0))))
		    $LOG(l_rc);
 
		if (!(1 & (l_rc = smg$paste_virtual_display (
			  &a_filer_find2_display_id,
			  &cntrl_info_block.pasteboard_id,
			  &l_filer_find2_pasteboard_row,
			  &FILER_TYPE2_PBD_COLUMN,
			  0))))
		    $LOG(l_rc);

		/*  25-Sep-2026 RRL: batch the rest of the first page (one
		**  smg$put_line_multi() per matching line) into one physical-
		**  terminal flush, like the UP/DOWN page moves below.  Started
		**  only here, AFTER both displays are pasted (a paste must never
		**  sit inside a begin/end_display_update pair); ended after the
		**  loop -- or before "goto free_vm" -- whenever l_lastline > 0,
		**  i.e. exactly when this block ran.  **/
		if (!(1 & (l_rc = smg$begin_display_update (&a_filer_find1_display_id))))
		    $LOG(l_rc);
	    }
	    smg$put_line_multi (
		&a_filer_find1_display_id,
		&l_linebuf_descrip,
		&l_rendition_set_descrip,
		0,
		0,
		0,
		0,
		0);
	    l_allocate_size = (int) l_from_rab.rab$w_rsz+1;
	    if (!(1 & (l_rc = lib$get_vm (
			    &l_allocate_size,
			    &l_base_address,
			    0))))
	        return $LOG(l_rc);
	    memcpy(l_base_address, l_linebuf, l_from_rab.rab$w_rsz);
	    l_base_address[l_allocate_size-1] = EOS;
	    if (l_lastline < MAXLINES-1)
	    {
	    l_lines[l_lastline] = l_base_address;
	    if (!(1 & (l_rc = lib$get_vm (
			    &l_allocate_size,
			    &l_base_address,
			    0))))
	        return $LOG(l_rc);
	    memcpy(l_base_address, l_rendition_set, l_from_rab.rab$w_rsz);
	    l_base_address[l_allocate_size-1] = EOS;
	    l_attrs[l_lastline] = l_base_address;
	    l_lastline++;
	    }
	    l_count++;
	}
	l_retcode = sys$get (
		      &l_from_rab,
		      0,
		      0);
    }
 
    if (l_lastline > 0)
	if (!(1 & (l_rc = smg$end_display_update (&a_filer_find1_display_id))))
	    $LOG(l_rc);

    /*
    **	Return to caller immediately if none found
    **/
 
    if (l_total_found == 0)
    	return DX__NORMAL;
 
    smg$put_chars (
	&a_filer_find2_display_id,
	$DESCR (
    "Press ARROW KEYS for prev or next -or-  keypad 0 and CTRL/Y to cancel"),
	&FILER_TYPE2_BOTTOM,
	&1,
	0,
	&SMG$M_REVERSE,
	0,
	0);
    smg$read_keystroke (
	&cntrl_info_block.keyboard_id,
	a_word_terminator_code,
	0,
	0,
	&a_filer_find2_display_id,
	0,
	0);
    if (!(1 & (l_rc = smg$erase_line (
		  &a_filer_find2_display_id,
		  &FILER_TYPE2_BOTTOM,
		  &1))))
        $LOG(l_rc);
    l_linepos = 1;
 
    /*
    **	Keep typing until exit by the user
    **/
 
    while (*a_word_terminator_code != SMG$K_TRM_CTRLY &&
	   *a_word_terminator_code != SMG$K_TRM_CTRLC &&
	   *a_word_terminator_code != SMG$K_TRM_KP0 &&
	   *a_word_terminator_code != SMG$K_TRM_CTRLZ)
    {
	switch (*a_word_terminator_code)
	{
	    /*
	    **  26-Sep-2026 CLD: Home (E1) / End (E4) repeat the UP / DOWN page
	    **  step until the first line / the end of the file (as in Type).
	    **/
	    case SMG$K_TRM_E1:
		 l_to_edge = 1;
	    case SMG$K_TRM_UP :
		 if (l_linepos == 1)
		 {
		     if (!(1 & (l_rc = smg$ring_bell (
			          &a_filer_find1_display_id,
			          0))))
		         $LOG(l_rc);
		 }
		 else
		 {
		     /*  25-Sep-2026 RRL: batch each UP/DOWN page move (one
		     **  smg$scroll_display_area()+smg$put_chars_multi() pair per
		     **  line, rows-1 lines per keystroke) into one physical-
		     **  terminal flush -- same fix as type_by_page() (FILERSTA.C);
		     **  content writes to the already-pasted find1 display only.  **/
		     if (!(1 & (l_rc = smg$begin_display_update (&a_filer_find1_display_id))))
		         $LOG(l_rc);
		     do
		     for (l_count = l_filer_find1_bottom;
			  l_count > 1 && l_linepos > 1;  l_count--)
		     {
			l_linepos--;
			strcpy(l_linebuf, l_lines[l_linepos-1]);
			LENGTH(l_linebuf_descrip) = strlen(l_linebuf);
			memcpy(l_rendition_set, l_attrs[l_linepos-1], strlen(l_linebuf));
			LENGTH(l_rendition_set_descrip) = strlen(l_linebuf);
			smg$scroll_display_area (
			    &a_filer_find1_display_id,
			    0,
			    0,
			    0,
			    0,
			    &SMG$M_DOWN,
			    0);
			smg$put_chars_multi (
			    &a_filer_find1_display_id,
			    &l_linebuf_descrip,
			    &1,
			    &1,
			    &SMG$M_ERASE_TO_EOL,
			    &l_rendition_set_descrip,
			    0,
			    0);
		     }
		     while (l_to_edge && l_linepos > 1);
		     if (!(1 & (l_rc = smg$end_display_update (&a_filer_find1_display_id))))
		         $LOG(l_rc);
		 }
		 l_to_edge = 0;
		 break;
	    case SMG$K_TRM_E4:
		 l_to_edge = 1;
	    case SMG$K_TRM_DOWN:
	    case SMG$K_TRM_CR :	
		if (l_linepos+l_filer_find1_bottom-1 >= l_lastline &&
		     l_retcode == RMS$_EOF)
		{
		    if (!(1 & (l_rc = smg$ring_bell (
				    &a_filer_find1_display_id,
				    0))))
		        $LOG(l_rc);
		}
		else
		{
		    if (!(1 & (l_rc = smg$begin_display_update (&a_filer_find1_display_id))))
		        $LOG(l_rc);
		    do
		    {
		    l_count = l_filer_find1_bottom;
		    while (l_count > 1)
		    {
			if (l_linepos+l_filer_find1_bottom-1 < l_lastline)
			{
			    l_linepos++;
			    strcpy(
				l_linebuf,
				l_lines[l_linepos+l_filer_find1_bottom-2]);
			    LENGTH(l_linebuf_descrip) = strlen(l_linebuf);
			    memcpy(
				l_rendition_set,
				l_attrs[l_linepos+l_filer_find1_bottom-2],
				strlen(l_linebuf));
			    LENGTH(l_rendition_set_descrip) = strlen(l_linebuf);
			    smg$scroll_display_area (
				&a_filer_find1_display_id,
				0,
				0,
				0,
				0,
				&SMG$M_UP,
				0);
			    smg$put_chars_multi (
				&a_filer_find1_display_id,
				&l_linebuf_descrip,
				&l_filer_find1_bottom,
				&1,
				&SMG$M_ERASE_TO_EOL,
				&l_rendition_set_descrip,
				0,
				0);
			    l_count--;
			}
			else if (l_retcode == RMS$_EOF)
			{
			    if (!l_eof_flag)
			    {
				l_eof_flag++;
				LENGTH(l_rendition_set_descrip) = 0;
				smg$scroll_display_area (
				    &a_filer_find1_display_id,
				    0,
				    0,
				    0,
				    0,
				    &SMG$M_UP,
				    0);
				smg$put_chars_multi (
				    &a_filer_find1_display_id,
				    $DESCR (""),
				    &l_filer_find1_bottom,
				    &1,
				    &SMG$M_ERASE_TO_EOL,
				    &l_rendition_set_descrip,
				    0,
				    0);
				if (!(1 & (l_rc = lib$get_vm (
						&1,
						&l_base_address,
						0))))
				    return $LOG(l_rc);
				l_base_address[0] = EOS;
					if (l_lastline < MAXLINES-1)
					{
				l_lines[l_lastline] = l_base_address;
				if (!(1 & (l_rc = lib$get_vm (
						&1,
						&l_base_address,
						0))))
				    return $LOG(l_rc);
				l_base_address[0] = SMG$M_NORMAL;
				l_attrs[l_lastline] = l_base_address;
				l_linepos++;
				l_lastline++;
					}
 
				{
				    static $DESCRIPTOR (l_fao_ctrl, "!AZ - !SL line!AZ, !SL match!AZ");
				    struct dsc$descriptor_s l_out_desc;
				    unsigned short int l_len;
				    unsigned long l_rc2;

				    l_out_desc.dsc$w_length = sizeof(l_linebuf) - 1;
				    l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
				    l_out_desc.dsc$b_class = DSC$K_CLASS_S;
				    l_out_desc.dsc$a_pointer = l_linebuf;
				    if (!(1 & (l_rc2 = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
					    a_from_fab.fab$l_fna, l_file_lines, (l_file_lines > 1? "s" : ""),
					    l_total_found, (l_total_found > 1? "s" : "")))))
					$LOG(l_rc2);
				    l_linebuf[l_len] = EOS;
				}
				LENGTH(l_linebuf_descrip) = strlen(l_linebuf);
				memset(l_rendition_set, SMG$M_NORMAL, strlen(l_linebuf));
				LENGTH(l_rendition_set_descrip) = strlen(l_linebuf);
				smg$scroll_display_area (
				    &a_filer_find1_display_id,
				    0,
				    0,
				    0,
				    0,
				    &SMG$M_UP,
				    0);
				smg$put_chars_multi (
				    &a_filer_find1_display_id,
				    &l_linebuf_descrip,
				    &l_filer_find1_bottom,
				    &1,
				    &SMG$M_ERASE_TO_EOL,
				    &l_rendition_set_descrip,
				    0,
				    0);
				l_allocate_size = strlen(l_linebuf)+1;
				if (!(1 & (l_rc = lib$get_vm (
						&l_allocate_size,
						&l_base_address,
						0))))
				    return $LOG(l_rc);
				memcpy(l_base_address, l_linebuf, l_allocate_size-1);
				l_base_address[l_allocate_size-1] = EOS;
					if (l_lastline < MAXLINES-1)
					{
				l_lines[l_lastline] = l_base_address;
				if (!(1 & (l_rc = lib$get_vm (
						&l_allocate_size,
						&l_base_address,
						0))))
				    return $LOG(l_rc);
				memset(l_base_address, SMG$M_NORMAL, l_allocate_size-1);
				l_base_address[l_allocate_size-1] = EOS;
				l_attrs[l_lastline] = l_base_address;
				l_linepos++;
				l_lastline++;
					}
			    }
			    break;
			}
			else if (l_retcode != RMS$_NORMAL)
			{
			     lib$sys_getmsg (
				   &l_retcode,
				   &l_len,
				   &l_errmsg1_descrip,
				   0,
				   0);
			     l_errmsg1[l_len] = EOS;
			     strcpy(a_errmsg, l_errmsg1);
			     smg$end_display_update (&a_filer_find1_display_id);
			     goto free_vm;
			}
			else
			{
			    l_file_lines++;
			    l_linebuf[l_from_rab.rab$w_rsz] = EOS;
			    LENGTH(l_linebuf_descrip) = l_from_rab.rab$w_rsz;
			    memset(l_rendition_set, SMG$M_NORMAL, (size_t) l_from_rab.rab$w_rsz);
			    LENGTH(l_rendition_set_descrip) = l_from_rab.rab$w_rsz;
			    str$upcase (
				&l_upcase_string_descrip,
				&l_linebuf_descrip);
			    l_upcase_string[l_from_rab.rab$w_rsz] = EOS;
 
			    for (l_start = l_upcase_string;  l_cp = strstr(l_start, a_pattern);  l_start = l_cp+1)
			    {
				l_total_found++;
				memset(l_rendition_set+(l_cp-l_upcase_string), SMG$M_BOLD, (size_t) l_len);
			    }
			    if (l_start != l_upcase_string)
			    {
				smg$scroll_display_area (
				    &a_filer_find1_display_id,
				    0,
				    0,
				    0,
				    0,
				    &SMG$M_UP,
				    0);
				smg$put_chars_multi (
				    &a_filer_find1_display_id,
				    &l_linebuf_descrip,
				    &l_filer_find1_bottom,
				    &1,
				    &SMG$M_ERASE_TO_EOL,
				    &l_rendition_set_descrip,
				    0,
				    0);
				l_allocate_size = (int) l_from_rab.rab$w_rsz+1;
				if (!(1 & (l_rc = lib$get_vm (
						&l_allocate_size,
						&l_base_address,
						0))))
				    return $LOG(l_rc);
				memcpy(l_base_address, l_linebuf, l_from_rab.rab$w_rsz);
				l_base_address[l_allocate_size-1] = EOS;
					if (l_lastline < MAXLINES-1)
					    {
				l_lines[l_lastline] = l_base_address;
				if (!(1 & (l_rc = lib$get_vm (
						&l_allocate_size,
						&l_base_address,
						0))))
				    return $LOG(l_rc);
				memcpy(l_base_address, l_rendition_set, l_from_rab.rab$w_rsz);
				l_base_address[l_allocate_size-1] = EOS;
				l_attrs[l_lastline] = l_base_address;
				l_count--;
				l_lastline++;
				l_linepos++;
					    }
			    }
 
			    l_retcode = sys$get (
					  &l_from_rab,
					  0,
					  0);
			}
		    }
		    }
		    while (l_to_edge && !(l_linepos+l_filer_find1_bottom-1 >= l_lastline && l_retcode == RMS$_EOF));
		    if (!(1 & (l_rc = smg$end_display_update (&a_filer_find1_display_id))))
		        $LOG(l_rc);
		}
		l_to_edge = 0;
		break;
	    case SMG$K_TRM_CTRLW:
		if (!(1 & (l_rc = smg$repaint_screen (
			    &cntrl_info_block.pasteboard_id))))
		    $LOG(l_rc);
		break;
	    default:
		if (*a_word_terminator_code != SMG$K_TRM_CANCELLED)
		{	/**  Returned from broadcast message trapping  **/
		    if (!(1 & (l_rc = smg$ring_bell (
				  &a_filer_find1_display_id,
				  0))))
		        $LOG(l_rc);
		}
		break;
    }
    if (!(1 & (l_rc = smg$put_chars (
		   &a_filer_find2_display_id,
		   $DESCR (
    "Press ARROW KEYS for prev or next -or- keypad 0 and CTRL/Y to cancel"),
		   &FILER_TYPE2_BOTTOM,
		   &1,
		   0,
		   &SMG$M_REVERSE,
		   0,
		   0))))
        $LOG(l_rc);
    smg$read_keystroke (
	 &cntrl_info_block.keyboard_id,
	 a_word_terminator_code,
	 0,
	 0,
	 &a_filer_find2_display_id,
	 0,
	 0);
    if (!(1 & (l_rc = smg$erase_line (
		   &a_filer_find2_display_id,
		   &FILER_TYPE2_BOTTOM,
		   &1))))
        $LOG(l_rc);
    }
 
free_vm:
    for (l_count = 0;  l_count < l_lastline;  l_count++)
    {
	l_allocate_size = strlen(l_lines[l_count])+1;
    	if (!(1 & (l_rc = lib$free_vm (
		     &l_allocate_size,
		     &l_lines[l_count],
		     0))))
    	    $LOG(l_rc);
    	if (!(1 & (l_rc = lib$free_vm (
		     &l_allocate_size,
		     &l_attrs[l_count],
		     0))))
    	    $LOG(l_rc);
    }
 
    return (l_retcode == RMS$_NORMAL || l_retcode == RMS$_EOF?
		DX__NORMAL : DX__ERROR);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_find_no_page() is the "find, whole file at once" variant of
**	filer_find_by_page(): it reads every line of a_from_fab, upcases and
**	searches each one for a_pattern (also upcased first), and writes
**	every matching line straight into the scrolling display via
**	smg$put_line_multi() without ever pausing for a page break.  Once
**	end-of-file is reached it appends a "N lines, M matches" summary line
**	and waits once for the user to press a key before returning.

**
**  INPUT PARAMETERS:
**
**	a_from_fab                 -  FAB of the already-parsed file to
**				     search.
**	a_filer_find1_display_id   -  virtual display id for the (unpaged,
**				     scrollable) matched-lines area.
**	a_filer_find2_display_id   -  virtual display id for the one-line
**				     status/prompt bar below it.

**
**  OUTPUT PARAMETERS:
**
**	a_word_terminator_code  -  set to the key the user pressed at the
**				   final "press RETURN" prompt.
**	a_errmsg                -  filled in with an RMS error message if a
**				   read fails.

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
**	DX__NORMAL once the whole file has been scanned (including the
**	case where nothing matched, in which case it returns immediately
**	after the scan without waiting for a keystroke); DX__ERROR if a read
**	failed for any reason other than end-of-file.

**
**--
*/
static int	filer_find_no_page(a_from_fab,
				   a_filer_find1_display_id,
				   a_filer_find2_display_id,
				   a_word_terminator_code,
				   a_errmsg,
				   a_pattern)
struct FAB a_from_fab;
unsigned int a_filer_find1_display_id,
              a_filer_find2_display_id;
unsigned short int *a_word_terminator_code;
char *a_errmsg;
char *a_pattern;
{
    char l_linebuf[BUFSIZ+1];
    $DESCRIPTOR (l_linebuf_descrip, l_linebuf);
 
    char l_rendition_set[BUFSIZ+1];
    $DESCRIPTOR (l_rendition_set_descrip, l_rendition_set);
 
    struct RAB l_from_rab;
 
    char l_errmsg1[MAXFILESPEC+1];
    $DESCRIPTOR (l_errmsg1_descrip, l_errmsg1);
    unsigned short int l_len;
    unsigned int l_retcode;
 
    int l_total_found;
    int l_lines;
    char l_upcase_string[BUFSIZ+1];
    $DESCRIPTOR (l_upcase_string_descrip, l_upcase_string);
    char *l_start, *l_cp;
    int l_filer_find2_pasteboard_row = cntrl_info_block.pasteboard_rows-1;
    unsigned long l_rc;
 
    /*
    **	Initialize Source File RAB
    **/
 
    l_from_rab = cc$rms_rab;
    l_from_rab.rab$l_fab = &a_from_fab;
    l_from_rab.rab$b_rac = RAB$C_SEQ;
    l_from_rab.rab$l_ubf = l_linebuf;
    l_from_rab.rab$w_usz = BUFSIZ;
 
    if (!((l_retcode = sys$connect (
			&l_from_rab,
			0,
			0)) & 1))
    {
	lib$sys_getmsg (
	    &l_retcode,
	    &l_len,
	    &l_errmsg1_descrip,
	    0,
	    0);
	l_errmsg1[l_len] = EOS;
	strcpy(a_errmsg, l_errmsg1);
	return DX__ERROR;
    }
 
    l_len = strlen(a_pattern);

    /*
    **	Upcase the search pattern -- the file content it is matched
    **	against below is upcased too (see upcase_string), so a
    **	lower-case pattern would otherwise never match.
    **/
    {
    char *l_cp1;

    for (l_cp1 = a_pattern;  *l_cp1;  l_cp1++)
	*l_cp1 = toupper((int) *l_cp1);
    }

    /*
    **	Set the signal back to system default so that it won't
    **	interfere with smg$read_keystroke() routine.
    **	Reset display attributes to avoid SMG$PUT_LINE_MULTI bugs.
    **/
 
    alarm(0);
    signal(SIGALRM, SIG_DFL);
 
    if (!(1 & (l_rc = smg$change_virtual_display (
	     	  &a_filer_find1_display_id,
	     	  0,
	     	  0,
	     	  &0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
	      &a_filer_find1_display_id,
	      &cntrl_info_block.pasteboard_id,
	      &FILER_TYPE1_PBD_ROW,
	      &FILER_TYPE1_PBD_COLUMN,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
	      &a_filer_find2_display_id,
	      &cntrl_info_block.pasteboard_id,
	      &l_filer_find2_pasteboard_row,
	      &FILER_TYPE2_PBD_COLUMN,
	      0))))
        $LOG(l_rc);
 
    l_total_found = l_lines = 0;
    while ((l_retcode = sys$get(
			&l_from_rab,
			0,
			0)) != RMS$_EOF)
    {
	if (l_retcode != RMS$_NORMAL)
	{
	    lib$sys_getmsg (
		&l_retcode,
		&l_len,
		&l_errmsg1_descrip,
		0,
		0);
	    l_errmsg1[l_len] = EOS;
	    strcpy(a_errmsg, l_errmsg1);
	    return DX__ERROR;
	}
 
	l_lines++;
	l_linebuf[l_from_rab.rab$w_rsz] = EOS;
	LENGTH(l_linebuf_descrip) = l_from_rab.rab$w_rsz;
	memset(l_rendition_set, SMG$M_NORMAL, (size_t) l_from_rab.rab$w_rsz);
	l_rendition_set[l_from_rab.rab$w_rsz] = EOS;
	LENGTH(l_rendition_set_descrip) = l_from_rab.rab$w_rsz;
        str$upcase (
            &l_upcase_string_descrip,
            &l_linebuf_descrip);
	l_upcase_string[l_from_rab.rab$w_rsz] = EOS;
 
	for (l_start = l_upcase_string;  l_cp = strstr(l_start, a_pattern);  l_start = l_cp+1)
	{
	    l_total_found++;
	    memset(l_rendition_set+(l_cp-l_upcase_string), SMG$M_BOLD, (size_t) l_len);
	}
	if (l_start != l_upcase_string)
	{
	    smg$put_line_multi (
		&a_filer_find1_display_id,
		&l_linebuf_descrip,
		&l_rendition_set_descrip,
		0,
		0,
		0,
		0,
		0);
	}
    }
 
    /*
    **	Return to caller immediately if none found
    **/
 
    if (l_total_found == 0)
    	return DX__NORMAL;
 
    smg$put_line (
	&a_filer_find1_display_id,
	$DESCR (""),
	0,
	0,
	0,
	0,
	0,
	0);
 
    {
	static $DESCRIPTOR (l_fao_ctrl, "!AZ - !SL line!AZ, !SL match!AZ");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;
	unsigned long l_rc2;

	l_out_desc.dsc$w_length = sizeof(l_linebuf) - 1;
	l_out_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_linebuf;
	if (!(1 & (l_rc2 = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
		a_from_fab.fab$l_fna, l_lines, (l_lines > 1? "s" : ""),
		l_total_found, (l_total_found > 1? "s" : "")))))
	    $LOG(l_rc2);
	l_linebuf[l_len] = EOS;
    }
    POINTER(l_linebuf_descrip) = l_linebuf;
    LENGTH(l_linebuf_descrip) = strlen(l_linebuf);
    smg$put_line (
	&a_filer_find1_display_id,
	&l_linebuf_descrip,
	0,
	0,
	0,
	0,
	0,
	0);
 
    smg$put_chars (
	&a_filer_find2_display_id,
	$DESCR ("Press RETURN to continue -or- keypad 0 and CTRL/Y to cancel"),
	&FILER_TYPE2_BOTTOM,
	&1,
	0,
	&SMG$M_REVERSE,
	0,
	0);
    smg$read_keystroke (
	&cntrl_info_block.keyboard_id,
	a_word_terminator_code,
	0,
	0,
	&a_filer_find2_display_id,
	0,
	0);
    smg$erase_line (
	&a_filer_find2_display_id,
	&FILER_TYPE2_BOTTOM,
	&1);
 
    return DX__NORMAL;
}



/* ---- (from FILERCUS.C -- customize/settings menu) ---- */

static int	select_a_display_date$1(
		    short int dattype, enum sortbys sortdat, char *);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_editor() is the Customize -> "Editor..." command: prompts
**	for which editor (EDT, TPU, or LSE) the Edit command should invoke,
**	pre-filled with the current choice, and dispatches to
**	select_a_editor() to apply the answer.

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
**	cntrl_info_block.user_pref.editor.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_editor()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_current_editor[4];
 
    switch (cntrl_info_block.user_pref.editor) {
 
        case DX_EDITOR_EDT:
            strcpy(l_current_editor, "EDT");
	    break;
        case DX_EDITOR_TPU:
            strcpy(l_current_editor, "TPU");
	    break;
        default:
            strcpy(l_current_editor, "TPU");
	    break;
 
    }
 
    strcpy(l_errmsg, "Invalid input value");
    get_userinput_and_execute(
	select_a_editor,
	"Select an Editor",
	"Choose an editor: [EDT, TPU, or LSE] ",
	l_current_editor,
	l_errmsg,
	"dummy");
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	select_a_editor() applies the user's editor choice from
**	custom_editor()'s prompt to the persistent user-preference setting.

**
**  INPUT PARAMETERS:
**
**	a_editor  -  "EDT", "TPU", or "LSE" (any unambiguous prefix,
**		     matched with strncmp() against the typed length).
**	a_dummy   -  unused (get_userinput_and_execute() calling convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with "Invalid input data" if a_editor
**		     doesn't match one of the three choices.

**
**  IMPLICIT INPUTS:
**
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.user_pref.editor is set to DX_EDITOR_EDT,
**	DX_EDITOR_TPU, or DX_EDITOR_LSE accordingly.

**
**  RETURNS VALUE:
**
**	DX__NORMAL on a recognized choice; DX__ERROR otherwise.

**
**--
*/
int	select_a_editor(a_editor, a_dummy, a_errmsg)
char *a_editor;
char *a_dummy;
char *a_errmsg;
{
    int l_len;
 
    l_len = strlen(a_editor);
    if (!strncmp(a_editor, "EDT", l_len))		/**   User likes EDT better  **/
 
	cntrl_info_block.user_pref.editor = DX_EDITOR_EDT;
 
    else if (!strncmp(a_editor, "TPU", l_len))	/**  Use TPU as the editor   **/
 
	cntrl_info_block.user_pref.editor = DX_EDITOR_TPU;
 
    else if (!strncmp(a_editor, "LSE", l_len))	/**  Use LSE as the editor   **/
 
	cntrl_info_block.user_pref.editor = DX_EDITOR_LSE;
 
    else
    {
	strcpy(a_errmsg, "Invalid input data");
	return DX__ERROR;
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_display_date() is the Customize -> "Display with date..."
**	command: prompts which date field (CREATED/MODIFIED/EXPIRED/BACKUP)
**	the filer display and sort-by-date commands should use, pre-filled
**	with the current choice, and dispatches to select_a_display_date()
**	to apply the answer.

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
**	DX_CURRENT_DIRECTORY.cur_filter.date.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL; DX__ERROR if cur_filter.date somehow holds none of
**	the four recognized values (in which case get_userinput_and_execute()
**	is never even reached).

**
**--
*/
int	custom_display_date()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_init_value[MAXFILESPEC+1];
 
    switch (DX_CURRENT_DIRECTORY.cur_filter.date)
    {
        case DX_CREDATE:
            strcpy(l_init_value, "CREATED");
	    break;
        case DX_REVDATE:
            strcpy(l_init_value, "MODIFIED");
	    break;
        case DX_EXPDATE:
            strcpy(l_init_value, "EXPIRED");
	    break;
        case DX_BAKDATE:
            strcpy(l_init_value, "BACKUP");
	    break;
        default:
            return DX__ERROR;
    }
 
    strcpy(l_errmsg, "Invalid input value");
    get_userinput_and_execute(
	select_a_display_date,
	"Select a display date type",
	"Choose one: [CREATED, MODIFIED, EXPIRED, or BACKUP] ",
	l_init_value,
	l_errmsg,
	"dummy");
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	select_a_display_date() maps the user's typed date-type name to
**	the matching (DX_xxxDATE, sortxxxdate) pair and applies it via
**	select_a_display_date$1().

**
**  INPUT PARAMETERS:
**
**	a_dat    -  "CREATED", "MODIFIED", "EXPIRED", or "BACKUP" (any
**		    unambiguous prefix).
**	a_dummy  -  unused (get_userinput_and_execute() calling convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  not actually filled in on the invalid-input path here
**		     (the caller's default message stands); present only for
**		     the calling convention.

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
**	DX__NORMAL on a recognized choice; DX__ERROR otherwise.

**
**--
*/
int	select_a_display_date(a_dat, a_dummy, a_errmsg)
char *a_dat;
char *a_dummy;
char *a_errmsg;
{
    int l_len;
 
    l_len = strlen(a_dat);
 
    if (!strncmp(a_dat, "CREATED", l_len))		/**  Display creation date   **/
    {
	select_a_display_date$1(DX_CREDATE, sortcredate, a_errmsg);
    }
    else if (!strncmp(a_dat, "MODIFIED", l_len))	/**  Display revision date   **/
    {
	select_a_display_date$1(DX_REVDATE, sortrevdate, a_errmsg);
    }
    else if (!strncmp(a_dat, "EXPIRED", l_len))	/**  Display expiration date **/
    {
	select_a_display_date$1(DX_EXPDATE, sortexpdate, a_errmsg);
    }
    else if (!strncmp(a_dat, "BACKUP", l_len))	/**  Display backup date     **/
    {
	select_a_display_date$1(DX_BAKDATE, sortbakdate, a_errmsg);
    }
    else					/**  Invalid input value     **/
    {
	return DX__ERROR;
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	select_a_display_date$1() does nothing if the requested date type
**	is already the active one; otherwise it switches
**	DX_CURRENT_DIRECTORY.cur_filter.date and picks the cheapest way to
**	make the screen consistent with that change: re-filter if a date
**	range filter is active, re-sort if the window was sorted by date,
**	or just repaint if neither applies.

**
**  INPUT PARAMETERS:
**
**	a_dattype  -  the new date field to display/filter by (one of
**		      DX_CREDATE/DX_REVDATE/DX_EXPDATE/DX_BAKDATE).
**	a_sortdat  -  the matching sort-by-date criterion (sortcredate/
**		      sortrevdate/sortexpdate/sortbakdate), used only if the
**		      window is currently sorted by some other date field.

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in if the re-filter/re-sort this triggers
**		     fails.

**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY.cur_filter (date/since/before) and
**	.sortby.

**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.cur_filter.date is updated; if a Since/
**	Before date filter is active the directory is re-filtered
**	(filer_filter$1(..., "Update", ...)); else if the window was sorted
**	by the previous date field the sort criterion is switched to match
**	(filer_sort$1()); otherwise the display is simply repainted in place
**	so the (now different) date column values show up.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
static int	select_a_display_date$1(
			short int a_dattype,
			enum sortbys a_sortdat,
			char *a_errmsg)
{
    static unsigned short l_timlen;
 
    char l_since[50];
    $DESCRIPTOR (l_since_descriptor, l_since);
    char l_before[50];
    $DESCRIPTOR (l_before_descriptor, l_before);
    enum sortbys l_old_sortby;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = sys$asctim (
		&l_timlen,
		&l_since_descriptor,
		(struct _generic_64 *) &DX_CURRENT_DIRECTORY.cur_filter.since,
		0))))
        return $LOG(l_rc);
    l_since[ l_timlen ] = EOS;
    if (!(1 & (l_rc = sys$asctim (
		&l_timlen,
		&l_before_descriptor,
		(struct _generic_64 *) &DX_CURRENT_DIRECTORY.cur_filter.before,
		0))))
        return $LOG(l_rc);
    l_before[ l_timlen ] = EOS;
 
    if (DX_CURRENT_DIRECTORY.cur_filter.date != a_dattype)
    {
	DX_CURRENT_DIRECTORY.cur_filter.date = a_dattype;
	l_old_sortby = DX_CURRENT_DIRECTORY.sortby;
	if (DX_CURRENT_DIRECTORY.sortby == sortcredate ||
	    DX_CURRENT_DIRECTORY.sortby == sortrevdate ||
	    DX_CURRENT_DIRECTORY.sortby == sortexpdate ||
	    DX_CURRENT_DIRECTORY.sortby == sortbakdate)
	{	/**  Current directory sort by date  **/
	    DX_CURRENT_DIRECTORY.sortby = a_sortdat;
	}
 
	if (strncmp(l_since, "17-NOV-1858", 11) &&
	    strncmp(l_before, "31-DEC-2857", 11))
	{	/**  SINCE and BOFORE not null  **/
	    filer_filter$1("", "Update", a_errmsg);
	}
	else if (l_old_sortby != a_sortdat &&
		 l_old_sortby != sortname &&
		 l_old_sortby != sorttype &&
		 l_old_sortby != sortsize &&
		 l_old_sortby != sortmark)
	{	/**  Was sort by other date type  **/
	    filer_sort$1(
		DX_CURRENT_DIRECTORY.sortorder == ascending?
		    "ASCENDING" : "DESCENDING",
		"Date",
		a_errmsg);
	}
	else
	{	/**  Not sort by date  **/
	    if (!(1 & (l_rc = smg$begin_display_update (
			 &DX_CURRENT_DIRECTORY.filer_display.id))))
	        $LOG(l_rc);
 
	    if (!(1 & (l_rc = smg$erase_display (
			 &DX_CURRENT_DIRECTORY.filer_display.id,
			 0,
			 0,
			 0,
			 0))))
	        $LOG(l_rc);
 
	    write_to_filer(DX_CURRENT_DIRECTORY.dir_filelist);
	    write_to_filer(DX_CURRENT_DIRECTORY.non_dir_filelist);
 
	    change_filer_viewport(cntrl_info_block.cur_win);
 
	    highlight_filer_current_file();
 
	    if (!(1 & (l_rc = smg$end_display_update (
			 &DX_CURRENT_DIRECTORY.filer_display.id))))
	        $LOG(l_rc);
	}
    }
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_delete() is the Customize -> "Delete confirm..." command:
**	prompts YES/NO for whether individual file deletes should ask for
**	confirmation, pre-filled with the current setting, and dispatches to
**	confirm_delete() to apply the answer.

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
**	cntrl_info_block.user_pref.delete_confirm.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_delete()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Invalid input value");
    get_userinput_and_execute(
	confirm_delete,
	"File Delete Confirmation",
	"Confirm on each file DELETION? [YES/NO] ",
	(cntrl_info_block.user_pref.delete_confirm? "YES" : "NO"),
	l_errmsg,
	"dummy");
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	confirm_delete() applies the user's YES/NO answer from
**	custom_delete()'s prompt to the persistent delete-confirmation
**	setting, via dxscr_yes_or_no().

**
**  INPUT PARAMETERS:
**
**	a_confirm  -  "YES"/"NO" (or any prefix dxscr_yes_or_no() accepts).
**	a_dummy    -  unused (get_userinput_and_execute() calling convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with "Invalid input data" if the answer
**		     isn't recognized.

**
**  IMPLICIT INPUTS:
**
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.user_pref.delete_confirm is set accordingly.

**
**  RETURNS VALUE:
**
**	DX__NORMAL on a recognized YES/NO answer; DX__ERROR otherwise.

**
**--
*/
int	confirm_delete(a_confirm, a_dummy, a_errmsg)
char *a_confirm;
char *a_dummy;
char *a_errmsg;
{
    int l_yn;

    l_yn = dxscr_yes_or_no(a_confirm);
    if (l_yn < 0)
    {
	strcpy(a_errmsg, "Invalid input data");
	return DX__ERROR;
    }
    cntrl_info_block.user_pref.delete_confirm = l_yn;

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_purge() is the Customize -> "Purge confirm..." command:
**	prompts YES/NO for whether individual file purges should ask for
**	confirmation, pre-filled with the current setting, and dispatches to
**	confirm_purge() to apply the answer.

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
**	cntrl_info_block.user_pref.purge_confirm.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_purge()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Invalid input value");
    get_userinput_and_execute(
	confirm_purge,
	"File Purge Confirmation",
	"Confirm on each file PURGE? [YES/NO] ",
	(cntrl_info_block.user_pref.purge_confirm? "YES" : "NO"),
	l_errmsg,
	"dummy");
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	confirm_purge() applies the user's YES/NO answer from
**	custom_purge()'s prompt to the persistent purge-confirmation setting,
**	via dxscr_yes_or_no().

**
**  INPUT PARAMETERS:
**
**	a_confirm  -  "YES"/"NO" (or any prefix dxscr_yes_or_no() accepts).
**	a_dummy    -  unused (get_userinput_and_execute() calling convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with "Invalid input data" if the answer
**		     isn't recognized.

**
**  IMPLICIT INPUTS:
**
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.user_pref.purge_confirm is set accordingly.

**
**  RETURNS VALUE:
**
**	DX__NORMAL on a recognized YES/NO answer; DX__ERROR otherwise.

**
**--
*/
int	confirm_purge(a_confirm, a_dummy, a_errmsg)
char *a_confirm;
char *a_dummy;
char *a_errmsg;
{
    int l_yn;

    l_yn = dxscr_yes_or_no(a_confirm);
    if (l_yn < 0)
    {
	strcpy(a_errmsg, "Invalid input data");
	return DX__ERROR;
    }
    cntrl_info_block.user_pref.purge_confirm = l_yn;

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_type() is the Customize -> "Type file..." ("by page")
**	command: prompts YES/NO for whether the Type command should pause
**	one screenful at a time rather than scrolling straight through, pre-
**	filled with the current setting, and dispatches to
**	select_type_method() to apply the answer.

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
**	cntrl_info_block.user_pref.type_by_page.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_type()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Invalid input value");
    get_userinput_and_execute(
	select_type_method,
	"Type by Page",
	"Display one screen at a time? [YES/NO] ",
	(cntrl_info_block.user_pref.type_by_page? "YES" : "NO"),
	l_errmsg,
	"dummy");
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	select_type_method() applies the user's YES/NO answer from
**	custom_type()'s prompt to the persistent type-by-page setting, via
**	dxscr_yes_or_no().

**
**  INPUT PARAMETERS:
**
**	a_confirm  -  "YES"/"NO" (or any prefix dxscr_yes_or_no() accepts).
**	a_dummy    -  unused (get_userinput_and_execute() calling convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with "Invalid input data" if the answer
**		     isn't recognized.

**
**  IMPLICIT INPUTS:
**
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.user_pref.type_by_page is set accordingly.

**
**  RETURNS VALUE:
**
**	DX__NORMAL on a recognized YES/NO answer; DX__ERROR otherwise.

**
**--
*/
int	select_type_method(a_confirm, a_dummy, a_errmsg)
char *a_confirm;
char *a_dummy;
char *a_errmsg;
{
    int l_yn;

    l_yn = dxscr_yes_or_no(a_confirm);
    if (l_yn < 0)
    {
	strcpy(a_errmsg, "Invalid input data");
	return DX__ERROR;
    }
    cntrl_info_block.user_pref.type_by_page = l_yn;

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_auto_update() is the Customize -> "Auto update..." command:
**	prompts YES/NO for whether the directory should automatically re-
**	read itself after Edit/Launch/Execute, pre-filled with the current
**	setting, and dispatches to auto_update() to apply the answer.

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
**	cntrl_info_block.user_pref.auto_update.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_auto_update()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error setting AUTO UPDATE option");
    get_userinput_and_execute(
	auto_update,
	"Automatically Update",
	"Update after each EDIT, LAUNCH, or EXECUTE? [Y/N] ",
	(cntrl_info_block.user_pref.auto_update? "YES" : "NO"),
	l_errmsg,
	"dummy");
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	auto_update() applies the user's YES/NO answer from
**	custom_auto_update()'s prompt to the persistent auto-update setting,
**	via dxscr_yes_or_no().

**
**  INPUT PARAMETERS:
**
**	a_confirm  -  "YES"/"NO" (or any prefix dxscr_yes_or_no() accepts).
**	a_dummy    -  unused (get_userinput_and_execute() calling convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with "Invalid input data" if the answer
**		     isn't recognized.

**
**  IMPLICIT INPUTS:
**
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.user_pref.auto_update is set accordingly.

**
**  RETURNS VALUE:
**
**	DX__NORMAL on a recognized YES/NO answer; DX__ERROR otherwise.

**
**--
*/
int	auto_update(a_confirm, a_dummy, a_errmsg)
char *a_confirm;
char *a_dummy;
char *a_errmsg;
{
    int l_yn;

    l_yn = dxscr_yes_or_no(a_confirm);
    if (l_yn < 0)
    {
	strcpy(a_errmsg, "Invalid input data");
	return DX__ERROR;
    }
    cntrl_info_block.user_pref.auto_update = l_yn;

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_display_clock() is the Customize -> "Show clock..."
**	command: prompts YES/NO for whether the on-screen clock should be
**	displayed at all this session, pre-filled with the current setting,
**	and dispatches to display_clock() to apply the answer.

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
**	cntrl_info_block.user_pref.display_clock.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_display_clock()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error setting DISPLAY CLOCK option");
    get_userinput_and_execute(
	display_clock,
	"Display Clock",
	"Display clock in this session? [Y/N] ",
	(cntrl_info_block.user_pref.display_clock? "YES" : "NO"),
	l_errmsg,
	"dummy");
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	display_clock() applies the user's YES/NO answer from
**	custom_display_clock()'s prompt to the persistent clock-visibility
**	setting, via dxscr_yes_or_no().

**
**  INPUT PARAMETERS:
**
**	a_confirm  -  "YES"/"NO" (or any prefix dxscr_yes_or_no() accepts).
**	a_dummy    -  unused (get_userinput_and_execute() calling convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with "Invalid input data" if the answer
**		     isn't recognized.

**
**  IMPLICIT INPUTS:
**
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.user_pref.display_clock is set accordingly; if
**	turned off, clear_time() erases the clock from the screen
**	immediately rather than waiting for the next tick.

**
**  RETURNS VALUE:
**
**	DX__NORMAL on a recognized YES/NO answer; DX__ERROR otherwise.

**
**--
*/
int	display_clock(a_confirm, a_dummy, a_errmsg)
char *a_confirm;
char *a_dummy;
char *a_errmsg;
{
    int l_yn;

    l_yn = dxscr_yes_or_no(a_confirm);
    if (l_yn < 0)
    {
	strcpy(a_errmsg, "Invalid input data");
	return DX__ERROR;
    }
    cntrl_info_block.user_pref.display_clock = l_yn;
    if (!l_yn)
	clear_time();

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_update_in_second() is the Customize -> "Flash clock
**	colon..." command: prompts ON/OFF for whether the clock should
**	update (and its colon flash) every second versus once a minute, pre-
**	filled with the current setting, and dispatches to
**	update_in_second() to apply the answer.

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
**	cntrl_info_block.user_pref.update_in_second.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_update_in_second()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error setting FLASH CLOCK COLON option");
    get_userinput_and_execute(
	update_in_second,
	"Flash Clock Colon",
	"Flash clock colon? [ON/OFF] ",
	(cntrl_info_block.user_pref.update_in_second? "ON" : "OFF"),
	l_errmsg,
	"dummy");
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	update_in_second() applies the user's ON/OFF answer from
**	custom_update_in_second()'s prompt to the persistent clock-update-
**	rate setting.  Unlike the other custom_xxx() pairs, this one parses
**	ON/OFF itself (not via dxscr_yes_or_no(), which expects YES/NO) and
**	requires at least two characters so "O" alone is rejected instead of
**	silently picking OFF.

**
**  INPUT PARAMETERS:
**
**	a_confirm  -  "ON" or "OFF" (must be unambiguous -- a bare "O" is
**		      rejected as ambiguous between the two).
**	a_dummy    -  unused (get_userinput_and_execute() calling convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an "ambiguous"/"invalid" message if
**		     a_confirm doesn't unambiguously match ON or OFF.

**
**  IMPLICIT INPUTS:
**
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.user_pref.display_clock is forced on either
**	way; .update_in_second is set to 1 (ON) or 0 (OFF).  Turning it OFF
**	also forces one immediate write_time() so the clock doesn't sit
**	stale until its next once-a-minute tick.

**
**  RETURNS VALUE:
**
**	DX__NORMAL on a recognized, unambiguous answer; DX__ERROR
**	otherwise.

**
**--
*/
int	update_in_second(a_confirm, a_dummy, a_errmsg)
char *a_confirm;
char *a_dummy;
char *a_errmsg;
{
    int l_len;
 
    l_len = strlen(a_confirm);
 
    if (a_confirm[0] == 'O' && l_len == 1)
    {
	strcpy(a_errmsg, "Ambugious response - supply more characters");
	return DX__ERROR;
    }
    else if (!strncmp(a_confirm, "ON", l_len))	/** Update the clock every second    **/
    {
        cntrl_info_block.user_pref.display_clock = 1;
	cntrl_info_block.user_pref.update_in_second = 1;
    }
    else if (!strncmp(a_confirm, "OFF", l_len))/** Update the clock every 60 seconds**/
    {
        cntrl_info_block.user_pref.display_clock = 1;
	cntrl_info_block.user_pref.update_in_second = 0;
	write_time(cntrl_info_block.commands_display.id);
    }
    else
    {
	strcpy(a_errmsg, "Invalid input data");
	return DX__ERROR;
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_save_setting() is the Customize -> "Save settings..."
**	command: prompts for a file name (defaulting to DEFAULT_STARTUP) and
**	dispatches to save_setting() to write the current preferences there.

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_save_setting()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error save setting to a file");
    get_userinput_and_execute(
	save_setting,
	"Save Setting",
	"Save to: ",
	DEFAULT_STARTUP,
	l_errmsg,
	"dummy");
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	save_setting() writes every user preference (display form,
**	editor, display-date type, type-by-page, delete/purge confirm, auto-
**	update, clock visibility and update rate) to a_filename as plain
**	text, one "DX.name: value" line each -- the same format
**	restore_setting() reads back.

**
**  INPUT PARAMETERS:
**
**	a_filename  -  file to create/overwrite with the settings text.
**	a_dummy     -  unused (get_userinput_and_execute() calling
**		       convention).

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with a message if the file can't be created
**		     or closed.

**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.user_pref (every field) and
**	DX_CURRENT_DIRECTORY.form/.cur_filter.date.

**
**  IMPLICIT OUTPUTS:
**
**	Writes one "DX.<setting>: <value>" line per preference to
**	a_filename in plain text; then adds that new file to the filer cache
**	and every open window's display (same update_filer_cache()/
**	add_to_filer() sequence used after creating any file).

**
**  RETURNS VALUE:
**
**	DX__NORMAL on success; DX__ERROR if the file can't be created/
**	closed, if the current date-filter type is unrecognized, or if
**	adding the new file to a window's display fails.

**
**--
*/
int	save_setting(a_filename, a_dummy, a_errmsg)
char *a_filename;
char *a_dummy;
char *a_errmsg;
{
    FILE *l_f;
    struct fil_dx_tag l_temp_entry;
    int l_i;
 
    if ((l_f = fopen (a_filename, "w")) == NULL)
    {
        strcpy(a_errmsg, "Error creating an output file");
	return DX__ERROR;
    }
 
    fputs("DX.short_form: ", l_f);
    if (DX_CURRENT_DIRECTORY.form == short_form)
        fputs("YES\n", l_f);
    else
        fputs("NO\n", l_f);
 
    fputs("DX.editor: ", l_f);
    switch (cntrl_info_block.user_pref.editor)
    {
        case DX_EDITOR_EDT:
            fputs("EDT\n", l_f);
	    break;
        case DX_EDITOR_TPU:
            fputs("TPU\n", l_f);
	    break;
        case DX_EDITOR_LSE:
            fputs("LSE\n", l_f);
	    break;
    }
    fputs("DX.display_date: ", l_f);
    switch (DX_CURRENT_DIRECTORY.cur_filter.date)
    {
        case DX_CREDATE:
            fputs("CREATED\n", l_f);
	    break;
        case DX_REVDATE:
            fputs("MODIFIED\n", l_f);
	    break;
        case DX_EXPDATE:
            fputs("EXPIRED\n", l_f);
	    break;
        case DX_BAKDATE:
            fputs("BACKUP\n", l_f);
	    break;
        default:
            return DX__ERROR;
    }
        fputs("DX.type_by_page: ", l_f);
    if (cntrl_info_block.user_pref.type_by_page)
        fputs("YES\n", l_f);
    else
        fputs("NO\n", l_f);
    fputs("DX.delete_confirm: ", l_f);
    if (cntrl_info_block.user_pref.delete_confirm)
        fputs("YES\n", l_f);
    else
        fputs("NO\n", l_f);
    fputs("DX.purge_confirm: ", l_f);
    if (cntrl_info_block.user_pref.purge_confirm)
        fputs("YES\n", l_f);
    else
        fputs("NO\n", l_f);
    fputs("DX.auto_update: ", l_f);
    if (cntrl_info_block.user_pref.auto_update)
        fputs("YES\n", l_f);
    else
        fputs("NO\n", l_f);
    fputs("DX.display_clock: ", l_f);
    if (cntrl_info_block.user_pref.display_clock)
        fputs("YES\n", l_f);
    else
        fputs("NO\n", l_f);
    fputs("DX.flash_clock_colon: ", l_f);
    if (cntrl_info_block.user_pref.update_in_second)
        fputs("ON\n", l_f);
    else
        fputs("OFF\n", l_f);
 
    fclose (l_f);
 
    /*
    **	Update the filer list and display
    **/
 
    strcpy(a_errmsg, "Screen not updated");
    if (filestat(a_filename, &l_temp_entry, a_errmsg) == DX__ERROR)
        return DX__ERROR;
 
    l_temp_entry.state = waiting;
    l_temp_entry.beg_y = l_temp_entry.beg_x = 0;
    l_temp_entry.forward = l_temp_entry.backward = NULL;
 
    /*
    **  Update cache queue.
    **	In case of running out of memory, output error message
    **	but update current display(s) anyway.
    **/
 
    if (update_filer_cache(l_temp_entry, add_to_filer_cache, a_errmsg) == DX__ERROR)
        signal_err(a_errmsg, bell);
 
    /**  Add it to the FILER  **/
    if (add_to_filer(l_temp_entry, a_errmsg, cntrl_info_block.cur_win) == DX__ERROR)
        return DX__ERROR;
 
    /**  Update other windows  **/
    for (l_i = 0;  l_i < cntrl_info_block.windows;  l_i++)
    {
        if (l_i != cntrl_info_block.cur_win)
        {
            if (add_to_filer(l_temp_entry, a_errmsg, l_i) == DX__ERROR)
                 return DX__ERROR;
        }
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_restore_setting() is the Customize -> "Restore settings..."
**	command: prompts for a file name (defaulting to DEFAULT_STARTUP) and
**	dispatches to restore_setting() to load preferences from it.

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_restore_setting()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error restore setting from a file");
    get_userinput_and_execute(
	restore_setting,
	"Restore Setting",
	"Restore from: ",
	DEFAULT_STARTUP,
	l_errmsg,
	"dummy");
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	restore_setting() reads a_filename line by line and applies every
**	recognized "DX.name: value" setting it finds (unrecognized lines are
**	silently ignored), reproducing the exact set save_setting() can
**	write: short/long form, editor, display-date type, type-by-page,
**	delete/purge confirm, auto-update, clock visibility and update rate.

**
**  INPUT PARAMETERS:
**
**	a_filename    -  settings file to read (the format save_setting()
**			 writes).
**	a_init_flag   -  the literal string "init" when called during DX
**			 startup, or anything else (e.g. "") when called
**			 interactively from the Customize menu; suppresses the
**			 screen-redraw side effects below when equal to "init",
**			 since there is no filer display up yet at startup.

**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in if the file can't be opened.

**
**  IMPLICIT INPUTS:
**
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	Applies each recognized "DX.name: value" line to the matching
**	cntrl_info_block.user_pref field (or DX_CURRENT_DIRECTORY.form/
**	.cur_filter.date); when not called at startup, changing the display
**	form or date type also triggers the matching screen update
**	(filer_short_form()/filer_long_form(), or an inline repaint for the
**	date-type change).

**
**  RETURNS VALUE:
**
**	DX__NORMAL once the whole file has been processed; DX__ERROR if
**	the file can't be opened.

**
**--
*/
int	restore_setting(a_filename, a_init_flag, a_errmsg)
char *a_filename;
char *a_init_flag;
char *a_errmsg;
{
    FILE *l_f;
    char l_line[256];
    int l_len;
    int l_i;
    unsigned long l_rc;
 
    if ((l_f = fopen (a_filename, "r")) == NULL)
    {
        strcpy(a_errmsg, "Error opening an input file");
	return DX__ERROR;
    }
 
    l_i = 0;
    while (fgets (l_line, 255, l_f) != NULL)
    {
	l_len = strlen(l_line);
        if (l_len == 0)
            break;
	l_i++;
	l_len--;
	l_line[l_len] = EOS;
 
        if (!strncmp (l_line, "DX.short_form: ", 15))
        {
            if (!strncmp (l_line+15, "YES", 3))
	    {	    /**  Use short form  **/
		cntrl_info_block.user_pref.use_short_form = 1;
                if (DX_CURRENT_DIRECTORY.form != short_form)
                {
		    DX_CURRENT_DIRECTORY.form = short_form;
		    if (strcmp(a_init_flag, "init"))
			filer_short_form();
                }
	    }
	    else if (!strncmp (l_line+15, "NO", 2))
	    {	    /**  Use long form  **/
		cntrl_info_block.user_pref.use_short_form = 0;
		if (DX_CURRENT_DIRECTORY.form != long_form)
		{
		    DX_CURRENT_DIRECTORY.form = long_form;
		    if (strcmp(a_init_flag, "init"))
			filer_long_form();
		}
	    }
        }
        else if (!strncmp (l_line, "DX.editor: ", 11))
        {
            if (!strncmp (l_line+11, "EDT", 3))
            {
                cntrl_info_block.user_pref.editor = DX_EDITOR_EDT;
            }
            else if (!strncmp (l_line+11, "TPU", 3))
            {
                cntrl_info_block.user_pref.editor = DX_EDITOR_TPU;
            }
            else if (!strncmp (l_line+11, "LSE", 3))
                cntrl_info_block.user_pref.editor = DX_EDITOR_LSE;
        }
        else if (!strncmp (l_line, "DX.display_date: ", 17))
        {
	    if (!strncmp (l_line+17, "CREATED", 7) ||
		!strncmp(l_line+17, "MODIFIED", 8) ||
		!strncmp(l_line+17, "EXPIRED", 7) ||
		!strncmp(l_line+17, "BACKUP", 6))
	    {
		if (!strncmp (l_line+17, "CREATED", 7))
		{
		    DX_CURRENT_DIRECTORY.cur_filter.date = DX_CREDATE;
		}
		else if (!strncmp(l_line+17, "MODIFIED", 8))
		{
		    DX_CURRENT_DIRECTORY.cur_filter.date = DX_REVDATE;
		}
		else if (!strncmp(l_line+17, "EXPIRED", 7))
		{
		    DX_CURRENT_DIRECTORY.cur_filter.date = DX_EXPDATE;
		}
		else
		    DX_CURRENT_DIRECTORY.cur_filter.date = DX_BAKDATE;
 
                if (strcmp(a_init_flag, "init"))
                {	/**  Update the screen  **/
		    if (!(1 & (l_rc = smg$begin_display_update (
				 &DX_CURRENT_DIRECTORY.filer_display.id))))
		        $LOG(l_rc);
		    if (!(1 & (l_rc = smg$erase_display (
				 &DX_CURRENT_DIRECTORY.filer_display.id,
				 0,
				 0,
				 0,
				 0))))
		        $LOG(l_rc);
		    write_to_filer(DX_CURRENT_DIRECTORY.dir_filelist);
		    write_to_filer(DX_CURRENT_DIRECTORY.non_dir_filelist);
 
		    change_filer_viewport(cntrl_info_block.cur_win);
 
		    highlight_filer_current_file();
 
		    if (!(1 & (l_rc = smg$end_display_update (
				 &DX_CURRENT_DIRECTORY.filer_display.id))))
		        $LOG(l_rc);
		}
	    }
        }
        else if (!strncmp (l_line, "DX.type_by_page: ", 17))
        {
            if (!strncmp (l_line+17, "YES", 3))
 
		cntrl_info_block.user_pref.type_by_page = 1;
 
	    else if (!strncmp (l_line+17, "NO", 2))
 
		cntrl_info_block.user_pref.type_by_page = 0;
        }
        else if (!strncmp (l_line, "DX.delete_confirm: ", 19))
        {
            if (!strncmp (l_line+19, "YES", 3))
 
		cntrl_info_block.user_pref.delete_confirm = 1;
 
	    else if (!strncmp (l_line+19, "NO", 2))
 
		cntrl_info_block.user_pref.delete_confirm = 0;
        }
        else if (!strncmp (l_line, "DX.purge_confirm: ", 18))
        {
            if (!strncmp (l_line+18, "YES", 3))
 
		cntrl_info_block.user_pref.purge_confirm = 1;
 
	    else if (!strncmp (l_line+18, "NO", 2))
 
		cntrl_info_block.user_pref.purge_confirm = 0;
        }
        else if (!strncmp (l_line, "DX.auto_update: ", 16))
        {
            if (!strncmp (l_line+16, "YES", 3))
 
		cntrl_info_block.user_pref.auto_update = 1;
 
	    else if (!strncmp (l_line+16, "NO", 2))
 
		cntrl_info_block.user_pref.auto_update = 0;
        }
        else if (!strncmp (l_line, "DX.display_clock: ", 18))
        {
            if (!strncmp (l_line+18, "YES", 3))
 
		cntrl_info_block.user_pref.display_clock = 1;
 
	    else if (!strncmp (l_line+18, "NO", 2))
		cntrl_info_block.user_pref.display_clock = 0;
        }
        else if (!strncmp (l_line, "DX.flash_clock_colon: ", 22))
        {
            if (!strncmp (l_line+22, "ON", 2))
	    {
		cntrl_info_block.user_pref.update_in_second = 1;
	    }
	    else if (!strncmp (l_line+22, "OFF", 3))
		cntrl_info_block.user_pref.update_in_second = 0;
        }
    }
 
    fclose (l_f);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	custom_use_default() is the Customize -> "Use defaults" command:
**	resets every preference to its built-in default by calling
**	use_default() directly (no prompt -- this one is immediate).

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
**	None.

**
**  IMPLICIT OUTPUTS:
**
**	None.

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	custom_use_default()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error set to default");
 
    return use_default("", "", l_errmsg);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	use_default() resets every user preference to its hardcoded
**	default value in one assignment, overwriting whatever the user had
**	previously configured or restored from a settings file.

**
**  INPUT PARAMETERS:
**
**	a_nouse   -  unused (present only so this function matches the
**		     int(*)(char*,char*,char*) callback shape
**		     get_userinput_and_execute()/custom_use_default() use).
**	a_dummy   -  unused, same reason.

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
**	cntrl_info_block.user_pref is overwritten wholesale with the
**	built-in default struct literal: long form, EDT editor, creation-
**	date display, type-by-page on, no delete/purge confirmation, no
**	auto-update, clock on, clock updating once a minute (not every
**	second).

**
**  RETURNS VALUE:
**
**	DX__NORMAL.

**
**--
*/
int	use_default(a_nouse, a_dummy, a_errmsg)
char *a_nouse;
char *a_dummy;
char *a_errmsg;
{
    struct user_pref_tag l_default_settings =
    {				/**  Default setting and user preferences    **/
	0,			/**  Display with long form;		     **/
	DX_EDITOR_EDT,		/**  EDT for editor;			     **/
	DX_CREDATE,		/**  use creation date;			     **/
	1,			/**  pause type after each page;	     **/
	0,			/**  no delete confirm for individual file;  **/
	0,			/**  no purge confirm for individual file;   **/
	0,			/**  no re-read current directory after	     **/
				/**    edit, execute, and launch;	     **/
	1,			/**  display clock.			     **/
	0			/**  display clock every 60 seconds.	     **/
    };
 
    cntrl_info_block.user_pref = l_default_settings;
 
    return DX__NORMAL;
}
 
