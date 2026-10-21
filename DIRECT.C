/* Program Name            : DIRECT.C                                   */
/*   Original Author       : C. K. Hung                                 */
/*   Editor                : StarLet Squad and Ruslan R. Laishev (RRL)  */
/*   Date                  : 7-12-89                                    */
/*   Program Description   : Directory tree navigator -- consolidated   */
/*                           13-Sep-2026 from DIRECT.C + DIRECTKPD.C    */
/*                           (GOLD-key navigation) + DIRECTQUE.C        */
/*                           (directory tree LRU cache).                */
/* References                                                           */
/*   Files open for Input  :                                            */
/*   Files open for Output :                                            */
/*   Modules Referenced    :                                            */
/* Revision History follows                                             */
/*  25-Sep-2026 RRL ECO2: __NEW_STARLET prototypes, types fixed (0 CC diags)  */
/*                        SIGALRM handlers -> void f(int); struct node_tag casts  */
/*                        last-screen viewport: one jump scroll         */
/*  26-Sep-2026 CLD ECO3: ident sync with DX.C (no code change)         */
/*  26-Sep-2026 CLD ECO5: SIGALRM "Working..." handlers -> dxscr_working_start()  */
/*  28-Sep-2026 CLD ECO6: Home/End (E1/E4) = KP5/KP6 in the tree          */
/*  28-Sep-2026 CLD ECO7: tree lost all subdirs: EOS after lib$find_file  */

#define __MODULE__	"DIRECT"
#define __IDENT__	"X-02-05ECO7"
#define __REV__		"02.05.07"
 
 
#define	__NEW_STARLET	1
#include	<starlet.h>
#include	<lib$routines.h>
#include	<str$routines.h>
#include	<signal.h>
#include	<libdef.h>
#include	<strdef.h>

#include "global.h"
#include "direct.h"
#include "dx.h"
#include "filer.h"
#include "filerque.h"
#include "fileropt.h"
#include "inquire.h"


/* ****  MACRO DEFINITIONS (from DIRECTKPD.C) *** */

/**  DIRTREE INACTIVE SCROLL REGION		**/
#define DIRTREE_INACTIVE_ROWS			(dirtree_display.view_rows/4)
#define DIRTREE_INACTIVE_FRONT_ROW		(1+DIRTREE_INACTIVE_ROWS)


/* **  GLOBAL DECLARATION ** */
struct node_tag		*root,	     /**  Root directory		     **/
			*cwdnode;    /**  Current working directory	     **/
 
enum directions dirtree_search_direction;
char dirtree_search_pattern[MAXFILESPEC+1];
 
 
/* ****  VIRTUAL DISPLAY IDs *** */
struct display_tag 	dirtree_cmds_display;
struct display_tag 	dirtree_display;
 
 
/* ****  INTERNAL FUNCTION PROTOTYPING *** */
 
static int		traverse_tree$1(struct node_tag *, struct node_tag *,
				int, int, enum directions);
 
 
 
 
/* **  FUNCTIONAL DESCRIPTION: * */
 
int	    filer_direct()
{
    char l_initial_root[MAXFILESPEC+1];		/**  Initial root directory  **/
    int l_n;
    unsigned int l_status_flags;
    int l_status;
    char l_errmsg[MAXFILESPEC+1];
    void (*l_cstat)(int);
    unsigned long l_rc;
 
    /*
    **	Output 'working...' message if total processing time
    **	is longer than 3 seconds.  Needs to disable broadcast message
    **	first to avoid interference.
    **/
#ifndef DEBUG
    if (!(1 & (l_rc = smg$disable_broadcast_trapping (
                 &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = dxscr_working_start(cntrl_info_block.commands_display.id, 2, 0, cntrl_info_block.commands_display.id, &l_cstat))))
	$LOG(l_rc);
#endif
    /*
    **	Start building the directory tree.
    **	Find the root directory.
    **/
 
    find_root(DX_CURRENT_DIRECTORY.cur_dir, l_initial_root);
 
    /*
    **	Assign new value related to the directory tree.
    **	Do not free up the memory used by the tree.  it might
    **	be used the cache queue.
    **/
 
    if ((l_n = search_direct_cache(l_initial_root)) == -1)
    {	    /**  This is a new directory tree **/
	dirtree_search_direction = advance;
	strcpy(dirtree_search_pattern, "[]");
	root = (struct node_tag *)NULL;
	l_status = build_dirtree(&root, l_initial_root);
 
	if (l_status == DX__ERROR)
	{
#ifndef DEBUG
	    /**  28-Sep-2026 CLD: stop "Working..." here too, as below  **/
	    alarm(0);
	    signal(SIGALRM, l_cstat);
	    if (!(1 & (l_rc = smg$set_broadcast_trapping(&cntrl_info_block.pasteboard_id, broadcast_routine, 0))))
		$LOG(l_rc);
#endif
	    signal_err("Insufficient virtual memory", bell);
	    free_dirtree(&root, (struct node_tag *)NULL);
	    return DX__ERROR;
	}
    }
    else
    {	    /**  This directory tree was saved in the cache queue  **/
	restore_from_direct_cache(
	    l_n, &dirtree_search_direction, dirtree_search_pattern, &root);
    }
 
    traverse_tree(root, (struct node_tag *)NULL, 1, 1, dummy);
 
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
    /*
    **	Create a directory commands display
    **/
 
    dirtree_cmds_display.rows = cntrl_info_block.pasteboard_rows;
    dirtree_cmds_display.width = cntrl_info_block.pasteboard_width;
    dirtree_cmds_display.beg_y = DIRTREE_CMDS_PBD_ROW;
    dirtree_cmds_display.beg_x = DIRTREE_CMDS_PBD_COLUMN;

    /*  14-Sep-2026 RRL: this whole section builds two displays via
    **  smg$create_virtual_display/smg$paste_virtual_display (structural
    **  pasteboard operations, not content writes) plus calls signal_err()
    **  (which itself repastes a display) -- NOT wrapped in smg$begin/
    **  end_pasteboard_update: that pairing is only valid around content
    **  writes to already-pasted displays, and wrapping a paste/repaste
    **  broke signal_err() (%DX-F-SFC in live testing) when tried there.
    **/
    if (!(1 & (l_rc = smg$create_virtual_display (
		&cntrl_info_block.pasteboard_rows,
		&cntrl_info_block.pasteboard_width,
		&dirtree_cmds_display.id,
		&SMG$M_TRUNC_ICON,
		0,
		0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$paste_virtual_display (
                 &dirtree_cmds_display.id,
                 &cntrl_info_block.pasteboard_id,
                 &DIRTREE_CMDS_PBD_ROW,
                 &DIRTREE_CMDS_PBD_COLUMN,
                 0))))
        $LOG(l_rc);
 
    /*
    **	Locate current directory in the tree.  If not found,
    **	change initial directory to the root directory.
    **/
 
    if ((cwdnode = search_node(
	root, strchr(DX_CURRENT_DIRECTORY.cur_dir, '[')+1)) == NULL)
	cwdnode = root;
 
    /**  Change default directory  **/
    setddir(cwdnode->full_path_name, l_errmsg);
 
    dirtree_display.view_rows =
	cntrl_info_block.pasteboard_rows - DIRTREE_CMDS_RSV_LNS;
    dirtree_display.view_width = cntrl_info_block.pasteboard_width;
 
    /**  Resize dirtree virtual display's size  **/
 
    dirtree_display.rows += dirtree_display.view_rows-1;
    dirtree_display.width += dirtree_display.view_width-MAXDIRSPEC;
    dirtree_display.beg_y = DIRTREE_PBD_ROW;
    dirtree_display.beg_x = DIRTREE_PBD_COLUMN;
 
    signal_err("Press RETURN to change directory, or keypad-0 to return", silence);
    put_dirtree_title(cwdnode);
    dxscr_tick_clock(dirtree_cmds_display.id);
    put_filer_pfs(DIRTREE_PFS, dirtree_cmds_display.id);
 
    if (!(1 & (l_rc = smg$create_virtual_display (
		&dirtree_display.rows,
		&dirtree_display.width,
		&dirtree_display.id,
		&SMG$M_TRUNC_ICON,
		0,
		0))))
        $LOG(l_rc);
 
    paint_dirtree_display(root);
 
    dirtree_display.view_beg_y = dirtree_display.view_beg_x = 1;
    if (!(1 & (l_rc = smg$create_viewport (
		&dirtree_display.id,
		&dirtree_display.view_beg_y,
		&dirtree_display.view_beg_x,
		&dirtree_display.view_rows,
		&dirtree_display.view_width))))
        $LOG(l_rc);
 
    /**  Position viewport using current working directory  **/
    change_dirtree_viewport(
	    cwdnode->row,
	    cwdnode->column,
	    &dirtree_display.view_beg_y,
	    &dirtree_display.view_beg_x);
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
	    &dirtree_display.id,
	    &cntrl_info_block.pasteboard_id,
	    &dirtree_display.beg_y,
	    &dirtree_display.beg_x,
	    0))))
        $LOG(l_rc);

    dirtree_search_direction = advance;
    strcpy(dirtree_search_pattern, "[]");

    direct_process_loop();
 
    /**  Clear error line if any  **/
    if (!(1 & (l_rc = smg$get_pasting_info (
	      &cntrl_info_block.status_display.id,
	      &cntrl_info_block.pasteboard_id,
	      &l_status_flags,
	      0,
	      0))))
        $LOG(l_rc);
    if (l_status_flags == SMG$M_DISPLAY_PASTED)
    {
	if (!(1 & (l_rc = smg$unpaste_virtual_display (
		  &cntrl_info_block.status_display.id,
		  &cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);
    }
 
    if (!(1 & (l_rc = smg$begin_pasteboard_update (
	     	  &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$erase_display (
	     	  &dirtree_cmds_display.id,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$erase_display (
	     	  &dirtree_display.id,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$end_pasteboard_update (
	     	  &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$delete_virtual_display (
	      &dirtree_cmds_display.id))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$delete_virtual_display (
	      &dirtree_display.id))))
        $LOG(l_rc);
 
    /*
    **  Reset display ids.
    **	Will be checked by read_string_and_clear_errmsg().
    **/
 
    dirtree_cmds_display.id = dirtree_display.id = 0;
 
    /*
    **	Save to the directory cache queue.  If it was previously
    **	saved in the queue, just replace the slot with
    **	root.  Otherwise, find an empty slot to place root.
    **/
 
    if ((l_n = search_direct_cache(l_initial_root)) == -1)
    {
	/*
	**  Current directory info not found in the
	**  directory cache.  Find the LRU slot in the cache.
	**/
 
	l_n = find_LRU_in_direct_cache();
    }
 
    /*
    **  Save this directory information to the direct cache queue.
    **/
 
    save_to_direct_cache(
	l_n,
	l_initial_root,
	dirtree_search_direction,
	dirtree_search_pattern,
	root);
 
    return DX__NORMAL;
}
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	direct_process_loop() is the GOLD-key command loop for the directory-
**	tree browser (entered from filer_direct()).  On each iteration it
**	repaints the current node's highlight, reads one keystroke (ticking
**	the clock while waiting), then dispatches on keypad/arrow keys to
**	move the current-node cursor (up/down/left/right/top/bottom/page-up/
**	page-down), toggle search direction, run FIND FIRST/FIND NEXT, jump
**	to the initial or bottom-most node, or (on CR) change the FILER's
**	default directory to the selected one via filer_filter$1() -- which
**	is the one path that returns out of the loop instead of continuing
**	it.  Runs until keypad-0 is pressed.
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
**	cwdnode, root, dirtree_search_direction, dirtree_display,
**	dirtree_cmds_display, DX_CURRENT_DIRECTORY.cur_dir,
**	cntrl_info_block (keyboard id, user_pref.update_in_second).
**
**  IMPLICIT OUTPUTS:
**
**	cwdnode (moved as the user navigates); dirtree_search_direction
**	(toggled by keypad-2); the DIRTREE display (repainted/scrolled).
**
**  RETURNS VALUE:
**
**	DX__NORMAL when the user exits via keypad-0; otherwise, on CR,
**	whatever filer_filter$1() returned (a directory change occurred and
**	the caller should treat this as leaving the directory-tree browser).
**
**--
**/
int	    direct_process_loop()
{
    char l_errmsg[256];				/**  Error msg buffer	     **/
    int l_status;
    char l_keyname[16];
    $DESCRIPTOR (l_keyname_descrip, l_keyname);
    char l_short_path_name[MAXFILESPEC+1];
    $DESCRIPTOR (l_short_path_name_descrip, l_short_path_name);
    unsigned int l_status_flags;		/**  SMG$READ_KEYSTROKE status	     **/
    unsigned short l_keystroke;
    int l_find_down_flag;
    struct node_tag *l_lastnode,
		    *l_leftnode;
    int l_timeout = cntrl_info_block.user_pref.update_in_second? 1 : 60;
    void (*l_cstat)(int);
    unsigned long l_rc;
 
    /*
    **	Keep processing until exits by user.
    **	Exit is done by pressing kp0 or exit command.
    **/
 
    do
    {
	/**  Update changes made from last command  **/
	setddir(cwdnode->full_path_name, l_errmsg);
	change_dirtree_viewport(
	    cwdnode->row,
	    cwdnode->column,
	    &dirtree_display.view_beg_y,
	    &dirtree_display.view_beg_x);
	put_dirtree_title(cwdnode);
 
	strncpy(l_short_path_name, cwdnode->short_path_name, 8);
	l_short_path_name[8] = EOS;
	LENGTH(l_short_path_name_descrip) = strlen(l_short_path_name);
	if (!(1 & (l_rc = smg$put_chars (
		    &dirtree_display.id,
		    &l_short_path_name_descrip,
		    &cwdnode->row,
		    &cwdnode->column,
		    0,
		    &SMG$M_REVERSE,
		    0,
		    0))))
	    $LOG(l_rc);
 
	/*
	**  Read next command.
	**  Clear error message line if any
	**/
 
	do {
	    l_status_flags = smg$read_keystroke (
				&cntrl_info_block.keyboard_id,
				&l_keystroke,
				0,
				&l_timeout,
				0,
				0,
				0);
	    dxscr_tick_clock(dirtree_cmds_display.id);
	} while (l_status_flags == SS$_TIMEOUT ||
		 l_keystroke == SMG$K_TRM_CANCELLED);
 
	if (!(1 & (l_rc = smg$get_pasting_info (
		      &cntrl_info_block.status_display.id,
		      &cntrl_info_block.pasteboard_id,
		      &l_status_flags,
		      0,
		      0))))
	    $LOG(l_rc);
	if (l_status_flags == SMG$M_DISPLAY_PASTED)
	{
	    if (!(1 & (l_rc = smg$unpaste_virtual_display (
		      &cntrl_info_block.status_display.id,
		      &cntrl_info_block.pasteboard_id))))
	        $LOG(l_rc);
        }
	
	/*
	**  Do job according to key pressed
	**/
 
	switch (l_keystroke)
	{
 
	/**  Exit without changing directory  **/
	case SMG$K_TRM_KP0:
	    setddir(DX_CURRENT_DIRECTORY.cur_dir, l_errmsg);
	    break;
 
	case SMG$K_TRM_KP1:	    /**  Help command  **/
	    direct_helpdir();
	    break;
 
	case SMG$K_TRM_KP2:	    /**  Forward/backward switch  **/
 
	    if (dirtree_search_direction == advance)
		dirtree_search_direction = backup;
	    else
		dirtree_search_direction = advance;
 
	    put_dirtree_title(cwdnode);
	    break;
	case SMG$K_TRM_KP3:	    /**  Find first command  **/
 
	    dirtree_find_first();
	    break;
	case SMG$K_TRM_KP4:	    /**  Find next command  **/
 
	    if (dirtree_find_next() == DX__ERROR)
		signal_err("Directory not found", bell);
	    break;
	/*
	**  26-Sep-2026 CLD: Home/End (VT E1/E4) -- same as KP5/KP6.
	**/
	case SMG$K_TRM_KP5:	    /**  Goto top  **/
	case SMG$K_TRM_E1:	    /**  Home  **/
 
	    if (!(1 & (l_rc = smg$put_chars (
		    &dirtree_display.id,
		    &l_short_path_name_descrip,
		    &cwdnode->row,
		    &cwdnode->column,
		    0,
		    &SMG$M_NORMAL,
		    0,
		    0))))
	        $LOG(l_rc);
	    cwdnode = root;
	    break;
	case SMG$K_TRM_KP6:	    /**  Goto dirtree_display.rows  **/
	case SMG$K_TRM_E4:	    /**  End  **/
 
	    if (!(1 & (l_rc = smg$put_chars (
		    &dirtree_display.id,
		    &l_short_path_name_descrip,
		    &cwdnode->row,
		    &cwdnode->column,
		    0,
		    &SMG$M_NORMAL,
		    0,
		    0))))
	        $LOG(l_rc);
	    l_find_down_flag = 0;
	    find_down(root, (struct node_tag *)NULL, &cwdnode, &l_find_down_flag);
	    break;
	case SMG$K_TRM_KP7:	    /**  Goto initial  **/
 
	    direct_initial();
	    break;
	case SMG$K_TRM_KP8:	    /**  Page up/down  **/
 
	    if (dirtree_search_direction == advance)
	    {
		l_find_down_flag = 0;
		find_down(root, (struct node_tag *)NULL, &l_lastnode, &l_find_down_flag);
		if (cwdnode == l_lastnode)
		    signal_err("Already at bottom of directory tree", bell);
		else
		    cwdnode = dirtree_page_down(cwdnode, l_lastnode);
	    }
	    else if (cwdnode == root)
	    {
		signal_err("Already at top of directory tree", bell);
	    }
	    else
		cwdnode = dirtree_page_up(cwdnode);
	    break;
	case SMG$K_TRM_UP:	    /**  Go up one directory  **/
 
	    if (cwdnode == root)
		signal_err("Already at top of directory tree", bell);
	    else
	    {
		if (!(1 & (l_rc = smg$put_chars (
			&dirtree_display.id,
			&l_short_path_name_descrip,
			&cwdnode->row,
			&cwdnode->column,
			0,
			&SMG$M_NORMAL,
			0,
			0))))
		    $LOG(l_rc);
		cwdnode = (cwdnode->up != NULL)? cwdnode->up : cwdnode->left;
	    }
	    break;
	case SMG$K_TRM_DOWN:	/**  Go down one directory  **/
 
	    if (cwdnode->down != NULL || cwdnode->right != NULL)
	    {
		if (!(1 & (l_rc = smg$put_chars (
			&dirtree_display.id,
			&l_short_path_name_descrip,
			&cwdnode->row,
			&cwdnode->column,
			0,
			&SMG$M_NORMAL,
			0,
			0))))
		    $LOG(l_rc);
		cwdnode = (cwdnode->down != NULL)? cwdnode->down : cwdnode->right;
	    }
	    else
	    {
		for (l_leftnode = cwdnode->left; l_leftnode != (struct node_tag *)NULL; l_leftnode = l_leftnode->left)
		    if (l_leftnode->down != (struct node_tag *)NULL)
			if (l_leftnode->left == l_leftnode->down->left)
			    break;
		if (l_leftnode == (struct node_tag *)NULL)
		    signal_err("Already at bottom of directory tree", bell);
		else {
		    if (!(1 & (l_rc = smg$put_chars (
			    &dirtree_display.id,
			    &l_short_path_name_descrip,
			    &cwdnode->row,
			    &cwdnode->column,
			    0,
			    &SMG$M_NORMAL,
			    0,
			    0))))
		        $LOG(l_rc);
		    cwdnode = l_leftnode->down;
		}
	    }
	    break;
	case SMG$K_TRM_RIGHT:	/**  Go right one directory  **/
 
	    if (cwdnode->right != NULL)
	    {
		if (!(1 & (l_rc = smg$put_chars (
			&dirtree_display.id,
			&l_short_path_name_descrip,
			&cwdnode->row,
			&cwdnode->column,
			0,
			&SMG$M_NORMAL,
			0,
			0))))
		    $LOG(l_rc);
		cwdnode = cwdnode->right;
	    }
	    else
	    {
		for (l_leftnode = cwdnode; l_leftnode != (struct node_tag *)NULL; l_leftnode = l_leftnode->left)
		    if (l_leftnode->down != (struct node_tag *)NULL)
			if (l_leftnode->left == l_leftnode->down->left)
			    break;
		if (l_leftnode == (struct node_tag *)NULL)
		    signal_err("Already at bottom of directory tree", bell);
		else
		{
		    if (!(1 & (l_rc = smg$put_chars (
			    &dirtree_display.id,
			    &l_short_path_name_descrip,
			    &cwdnode->row,
			    &cwdnode->column,
			    0,
			    &SMG$M_NORMAL,
			    0,
			    0))))
		        $LOG(l_rc);
		    cwdnode = l_leftnode->down;
		}
	    }
	    break;
	case SMG$K_TRM_LEFT:	/**  Go left one directory  **/
 
	    if (cwdnode == root)
		signal_err("Already at top of directory tree", bell);
	    else
	    {
		if (!(1 & (l_rc = smg$put_chars (
			&dirtree_display.id,
			&l_short_path_name_descrip,
			&cwdnode->row,
			&cwdnode->column,
			0,
			&SMG$M_NORMAL,
			0,
			0))))
		    $LOG(l_rc);
		cwdnode = (cwdnode->left != NULL)? cwdnode->left : cwdnode->up;
	    }
	    break;
	case SMG$K_TRM_CR:	/**  Change directory  **/
	
	    /*
	    **	Output 'working...' message if total processing time
	    **	is longer than 1 second.  Needs to disable broadcast message
	    **	first because of interference.
	    **/
 
	    if (!(1 & (l_rc = smg$disable_broadcast_trapping (
			 &cntrl_info_block.pasteboard_id))))
	        $LOG(l_rc);
 
	    if (!(1 & (l_rc = dxscr_working_start(dirtree_cmds_display.id, 2, 0, dirtree_cmds_display.id, &l_cstat))))
		$LOG(l_rc);
 
	    l_status = filer_filter$1(
		    cwdnode->full_path_name, "Name", l_errmsg);
 
	    /*
	    **	Set the signal back to system default
	    **/
 
	    alarm(0);
	    signal(SIGALRM, l_cstat);
	    if (!(1 & (l_rc = smg$erase_chars (
			 &dirtree_cmds_display.id,
			 &10,
			 &2,
			 &1))))
	        $LOG(l_rc);
	    if (!(1 & (l_rc = smg$set_broadcast_trapping (
			 &cntrl_info_block.pasteboard_id,
			 broadcast_routine,
			 0))))
	        $LOG(l_rc);
 
	    if (l_status == DX__ERROR)
		signal_err(l_errmsg, bell);
	    else
		return l_status;
	    break;
 
	case SMG$K_TRM_CTRLW:    /**  Refresh screen  **/
 
	    if (!(1 & (l_rc = smg$repaint_screen (
			 &cntrl_info_block.pasteboard_id))))
	        $LOG(l_rc);
	    break;
	default:		/**  NO DEFINITION  **/
 
	    memset (l_keyname, ' ', 15);
	    smg$keycode_to_name (
	       &l_keystroke,
	       &l_keyname_descrip);
 
	    l_keyname[strcspn (l_keyname, " ")] = EOS;
	    {
		static $DESCRIPTOR (l_fao_ctrl, "Key '!AZ' currently has no definition");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_out_len;

		l_out_desc.dsc$w_length  = sizeof(l_errmsg) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_errmsg;

		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_out_len, &l_out_desc, l_keyname))))
		    $LOG(l_rc);
		l_errmsg[l_out_len] = EOS;
	    }
	    signal_err(l_errmsg, bell);
	}	/**  switch (l_keystroke)  **/
    } while ( l_keystroke != SMG$K_TRM_KP0);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	find_root() derives the root directory specification for the
**	directory tree from a starting directory: it truncates a_initdir at
**	the first '.' or ']' (keeping only the top-level directory name) and
**	appends the closing bracket, then special-cases the master file
**	directory ("[0,0]"/"[000,000]") to the conventional "[000000]" form.
**
**  INPUT PARAMETERS:
**
**	a_initdir  -  a full VMS directory specification (e.g.
**		      "DEV:[USER.SUB.DIR]") whose top-level root is wanted.
**
**  OUTPUT PARAMETERS:
**
**	a_rootdir  -  filled in with the root directory specification
**		      (e.g. "DEV:[USER]" or "DEV:[000000]").
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
**	DX__NORMAL always.
**
**--
**/
int	find_root(a_initdir, a_rootdir)
char *a_initdir;
char *a_rootdir;
{
    int l_len;
    char *l_cp;
 
    l_len = strcspn(a_initdir, ".]");
    strncpy(a_rootdir, a_initdir, l_len);
    a_rootdir[l_len] = ']';
    a_rootdir[l_len+1] = EOS;
 
    l_cp = strchr(a_rootdir, '[');
    if (!strcmp(l_cp, "[0,0]") || !strcmp(l_cp, "[000,000]"))
    {	    /**  Master system directory  **/
	*l_cp = EOS;
	strcat(a_rootdir, "[000000]");
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      CREATE_NODE() allocates virtual memory to a node.
**
**--
**/
struct node_tag *create_node(a_filespec, a_row, a_col)
char *a_filespec;
int a_row;
int a_col;
{
    struct node_tag *l_newnode;
    char *l_start, *l_end;
    int l_allocate_size = sizeof (struct node_tag);
    unsigned long l_rc;
 
    /*
    **	    ALLOCATE STOREAGE FOR A NEW NODE
    **/
 
    if (!(1 & (l_rc = lib$get_vm (
		 &l_allocate_size,
		 &l_newnode,
		 0))))
    {
        $LOG(l_rc);
	return (struct node_tag *)NULL;
    }
 
    /*
    **	Initialize the newly-created node
    **/
 
    strcpy(l_newnode->full_path_name, a_filespec);
    if ((l_start = strrchr(a_filespec, '.')) == NULL)
	l_start = strchr(a_filespec, '[');
    l_start++;
    strcpy(l_newnode->short_path_name, l_start);
    l_end = strchr(l_newnode->short_path_name, ']');
    *l_end = EOS;
    l_newnode->row = a_row;			    /**  DIR COORDINATES **/
    l_newnode->column = a_col;
    l_newnode->left = l_newnode->up = l_newnode->right =
	l_newnode->down = (struct node_tag *)NULL;
 
    /*
    **	Adjust the size of the dirtree display
    **/
 
    dirtree_display.width = max(dirtree_display.width, a_col+MAXDIRSPEC-1);
    dirtree_display.rows = max(dirtree_display.rows, a_row);
 
    return l_newnode;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      BUILD_DIRTREE() expands "DEV:[USERNAME]*.DIR;1" wildcard and
**	build a tree to be used for painting the DIRTREE virtual display.
**
**--
**/
int	build_dirtree(a_parentnode, a_pathname)
struct node_tag **a_parentnode;
char *a_pathname;
{
    static _align (QUADWORD) int l_logicalQ[2];
    static unsigned int l_context;
 
    struct logical_queue_entry_tag *l_e;
    unsigned long l_condcode;
    char *l_indx;
    int l_number_of_bytes;
 
    char l_filespec[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descrip, l_filespec);
    unsigned long l_rc;
 
    char l_resultant_filespec[MAXFILESPEC+1];
    $DESCRIPTOR (l_resultant_filespec_descrip, l_resultant_filespec);
 
    /*
    **  28-Sep-2026 CLD: lib$find_file() blank-pads the result to the
    **  descriptor length (MAXFILESPEC) and never writes an EOS; without this
    **  terminator strchr()/strrchr() ran past the buffer into the next
    **  stack variables (e.g. the "...]*.DIR;1" search spec) and saw a '*'.
    **/
    l_resultant_filespec[MAXFILESPEC] = EOS;

    l_logicalQ[0] = l_logicalQ[1] = 0;
    trnlnm(a_pathname, l_logicalQ);
 
    while (_REMQHI (l_logicalQ, &l_e) != 3) {
	if (add_node(a_parentnode, l_e->equivalent_name, (struct node_tag *)NULL)
	    == (struct node_tag *)NULL)
	    return DX__ERROR;
	/**  Get wildcard file spec  **/
	strncpy(l_filespec, l_e->equivalent_name, strlen(l_e->equivalent_name)-1);
	l_filespec[ strlen(l_e->equivalent_name)-1 ] = EOS;
	l_number_of_bytes = strlen(l_e->equivalent_name)+1;
        if (!(1 & (l_rc = lib$free_vm (
                     &l_number_of_bytes,
                     &l_e->equivalent_name,
                     0))))
            $LOG(l_rc);
	strcat(l_filespec, "...]*.DIR;1");
	LENGTH(l_filespec_descrip) = strlen(l_filespec);
 
	/**  Call system services to expand the wildcard  */
	while ((l_condcode = lib$find_file (
				&l_filespec_descrip,
				&l_resultant_filespec_descrip,
				&l_context,
				0,
				0,
				0,
				0)) != RMS$_NMF)
	{
	    if (l_condcode == RMS$_NORMAL &&
		!strchr(l_resultant_filespec, '*') &&
		strchr(l_resultant_filespec, ' '))
	    {	/** Convert "DEV:[*.*...]*.DIR;1" into "DEV:[*.*...]"  **/
		for (l_indx = l_resultant_filespec; l_indx; l_indx++)
		    if (*l_indx == ']')
		    {
			*l_indx = '.';
			break;
		    }
		for (l_indx++; l_indx; l_indx++)
		    if (*l_indx == '.')
		    {
			*l_indx = ']';
			break;
		    }
		l_indx++;
		*l_indx = EOS;
 
		if (strstr (l_filespec, "[000000."))
		{
		    if (!strstr (l_resultant_filespec, "[000000."))
		    {
			char l_s[MAXFILESPEC+1];
 
			l_indx = strchr(l_resultant_filespec, '[')+1;
			strcpy(l_s, l_indx);
			*l_indx = EOS;
			strcat(l_resultant_filespec, "000000.");
			strcat(l_resultant_filespec, l_s);
		    }
		}
 
		if (add_node(
			a_parentnode,
			l_resultant_filespec,
			(struct node_tag *)NULL) == (struct node_tag *)NULL)
		{
		    lib$find_file_end(&l_context);
		    return DX__ERROR;
		}
	    }
	}
	lib$find_file_end(&l_context);
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      add_node() inserts a node into the tree.
**
**--
**/
struct node_tag *add_node(a_p, a_filespec, a_q)
struct node_tag **a_p;
char *a_filespec;
struct node_tag *a_q;	/*
			**  Parent node if moved right or
			**  sibling node if moved down
			**/
{
    struct node_tag *l_newnode;
    char *l_indx;
    char *l_cp;
    int l_len;
 
    if (*a_p == (struct node_tag *)NULL)
    {
	l_newnode = create_node(a_filespec, 0, 0);
	if (l_newnode == (struct node_tag *)NULL)
	    return (struct node_tag *)NULL;
 
	/**  Do double linking  **/
	if (a_q != (struct node_tag *)NULL)
	{
	    if (a_filespec[ strlen(a_q->full_path_name)-1 ] == '.')
	    {	/**  New node is the oldest son  **/
		if ((l_newnode->up = find_prev_node(a_q)) != (struct node_tag *)NULL)
		    l_newnode->up->down = l_newnode;
		l_newnode->left = a_q;
	    }
	    else
	    {	/**  New node is a sibling  **/
		l_newnode->up = a_q;
		l_newnode->left = a_q->left;
	    }
	}
	*a_p = l_newnode;
	return *a_p;
    }
    else if (a_filespec[ strlen((*a_p)->full_path_name)-1 ] == '.' &&
	     !strncmp(
		a_filespec,
		(*a_p)->full_path_name,
		strlen((*a_p)->full_path_name)-1))
    {	    /**  Search right  **/
	return add_node(&((*a_p)->right), a_filespec, *a_p);
    }
    else if (strncmp(
		a_filespec,
		(*a_p)->full_path_name,
		strlen((*a_p)->full_path_name)-1) > 0)
    {	    /**  Search down  **/
	return add_node(&((*a_p)->down), a_filespec, *a_p);
    }
    else
    {
	if ((l_cp = strrchr((*a_p)->full_path_name, '.')) == NULL)
	    l_cp = strrchr((*a_p)->full_path_name, ']');
	l_len = l_cp-(*a_p)->full_path_name;
	if (strchr(a_filespec+l_len+1, '.') != NULL)
	{	/*
		**  The parent directory did not exist prior to the
		**  creation of this directory
		**/
	    return (struct node_tag *)NULL;
	}
	else
	{
	    l_newnode = create_node(a_filespec, 0, 0);
	    if (l_newnode == (struct node_tag *)NULL)
		return (struct node_tag *)NULL;
 
	    /**  Do double linking  **/
	    if (a_q != (struct node_tag *)NULL)
	    {
		if (a_filespec[ strlen(a_q->full_path_name)-1 ] == '.')
		{	/**  New node is the oldest son  **/
		    if ((l_newnode->up = (*a_p)->up) != (struct node_tag *)NULL)
			l_newnode->up->down = l_newnode;
		    l_newnode->left = a_q;
		    l_newnode->down = *a_p;
		    (*a_p)->up = l_newnode;
		}
		else
		{	/**  New node is not the oldest son  **/
		    l_newnode->up = a_q;
		    l_newnode->left = (*a_p)->left;
		    l_newnode->down = *a_p;
		    (*a_p)->up = l_newnode;
		}
	    }
	    *a_p = l_newnode;
	    return *a_p;
	}
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FIND_PREV_NODE() finds previous node with different parent directory.
**
**--
**/
struct node_tag *find_prev_node(a_q)
struct node_tag *a_q;
{
    struct node_tag *l_s;
 
    for (a_q = a_q->up; a_q != (struct node_tag *)NULL; a_q = a_q->up)
    {
	if (a_q->right != (struct node_tag *)NULL)
	{
	    for (l_s = a_q->right; l_s->down != (struct node_tag *)NULL; l_s = l_s->down)
		if (l_s->down->left != a_q)
		    return l_s;
	    return l_s;
	}
 
    }
    return (struct node_tag *)NULL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FIND_NEXT_NODE() finds next node with different parent directory.
**
**--
**/
struct node_tag *find_next_node(a_q)
struct node_tag *a_q;
{
    struct node_tag *l_s;
 
    for (a_q = a_q->down; a_q != (struct node_tag *)NULL; a_q = a_q->down)
    {
	if (a_q->right != (struct node_tag *)NULL)
	{
	    for (l_s = a_q->right; l_s->up != (struct node_tag *)NULL; l_s = l_s->up)
		if (l_s->up->left != a_q)
		    return l_s;
	    return l_s;
	}
    }
    return (struct node_tag *)NULL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      TRAVERSE_TREE() calculates the row and column value when
**	visiting each node.  The dimensions for DIRTREE are derived
**	when the function finished.
**
**--
**/
int	traverse_tree(a_childnode, a_parentnode, a_row, a_col, a_direction)
struct node_tag *a_childnode, *a_parentnode;
int a_row, a_col;
enum directions a_direction;
{
    /**  Reset DIRTREE virtual display's dimension  **/
    dirtree_display.rows = dirtree_display.width = 0;
 
    traverse_tree$1(a_childnode, a_parentnode, a_row, a_col, a_direction);
 
    /**  Resize DIRTREE virtual display's size  **/
    dirtree_display.rows += dirtree_display.view_rows-1;
    dirtree_display.width += dirtree_display.view_width-MAXDIRSPEC;
    return DX__NORMAL;
}
 
static int	traverse_tree$1(a_childnode, a_parentnode, a_row, a_col, a_direction)
struct node_tag *a_childnode, *a_parentnode;
int a_row, a_col;
enum directions a_direction;
{
    int l_lineno = 0;
 
    if (a_childnode == (struct node_tag *)NULL)
    {
	return (a_direction == right)? LINESPACE : 0;
    }
    else if (a_childnode->left == a_parentnode)
    {
	if (dirtree_display.width < a_col+MAXDIRSPEC-1)
	    dirtree_display.width = a_col+MAXDIRSPEC-1;
	if (dirtree_display.rows < a_row)
	    dirtree_display.rows = a_row;
	a_childnode->row = a_row;
	a_childnode->column = a_col;
	l_lineno += traverse_tree$1(a_childnode->right, a_childnode, a_row, a_col+DISTANCE_COUNT, right);
	l_lineno += traverse_tree$1(a_childnode->down, a_parentnode, a_row+l_lineno, a_col, down);
    }
    return l_lineno;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      SEARCH_NODE() searchs a node starting from the root directory.
**      Return error if not found.
**
**--
**/
struct node_tag *search_node(a_p, a_filespec)
struct node_tag *a_p;
char *a_filespec;
{
    if (a_p == (struct node_tag *)NULL)
    {
	return (struct node_tag *)NULL;
    }
    else if (!strncmp(a_p->short_path_name,
		      a_filespec,
		      strlen(a_p->short_path_name)) &&
	     !strncmp(a_p->short_path_name,
		      a_filespec,
		      strcspn(a_filespec, ".]")))
	/**  A MATCHED DIRECTORY IS FOUND  **/
    {
	if (*(a_filespec+strlen(a_p->short_path_name)) == ']')	
	    return a_p;
	else
	    return search_node(a_p->right, strchr(a_filespec, '.')+1);
    }
    else if (a_p->down != (struct node_tag *)NULL)
    {
        if (a_p->down->left == a_p->left)
	    return search_node(a_p->down, a_filespec);
    }
 
    return (struct node_tag *)NULL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      PAINT_DIRTREE_DISPLAY() takes the tree head as input and
**	writes the directory tree structure to the display.
**
**--
**/
void	paint_dirtree_display(a_p)
struct node_tag *a_p;
{
    char l_short_path_name[MAXDIRSPEC+1];
    $DESCRIPTOR(l_short_path_name_descrip, l_short_path_name);
 
    int l_start_row, l_end_row, l_start_column, l_end_column;
    unsigned long l_char_up = SMG$M_LEFT + SMG$M_RIGHT + SMG$M_DOWN;
    unsigned long l_char_left = SMG$M_UP + SMG$M_DOWN + SMG$M_RIGHT;
    unsigned int l_left_corner = SMG$M_UP + SMG$M_RIGHT;
    unsigned int l_draw_char;
    unsigned long l_rc;
 
    if (a_p != (struct node_tag *)NULL)
    {
	/*  14-Sep-2026 RRL: batch this node's (and, via the recursive calls
	**  below, the whole subtree's) smg$put_chars/smg$draw_line/
	**  smg$draw_char calls into one flush -- SMG$ nests begin/end pairs
	**  by reference count, so the physical terminal update only
	**  happens once, when the OUTERMOST call unwinds, instead of once
	**  per tree node.  **/
	if (!(1 & (l_rc = smg$begin_display_update(&dirtree_display.id))))
	    $LOG(l_rc);

	strncpy(l_short_path_name, a_p->short_path_name, 8);
	l_short_path_name[8] = EOS;
	LENGTH(l_short_path_name_descrip) = strlen(l_short_path_name);
	if (!(1 & (l_rc = smg$put_chars (
		    &dirtree_display.id,
		    &l_short_path_name_descrip,
		    &(a_p->row),
		    &(a_p->column),
		    0,
		    0,
		    0,
		    0))))
	    $LOG(l_rc);
 
	if (a_p->left != (struct node_tag *)NULL)
	{
	    if (a_p->up == NULL)
	    {
		l_start_column =
		    a_p->left->column+min(
			strlen(a_p->left->short_path_name), MAXDIRSPEC);
		l_end_column = a_p->column-1;
		if (!(1 & (l_rc = smg$draw_line (
		    &dirtree_display.id,
		    &(a_p->row),
		    &l_start_column,
		    &(a_p->row),
		    &l_end_column,
		    0,
		    0))))
		    $LOG(l_rc);
	    }
	    else if (a_p->left != a_p->up->left)
	    {
		l_start_column =
		    a_p->left->column+min(
			strlen(a_p->left->short_path_name), MAXDIRSPEC);
		l_end_column = a_p->column-1;
		if (!(1 & (l_rc = smg$draw_line (
		    &dirtree_display.id,
		    &(a_p->row),
		    &l_start_column,
		    &(a_p->row),
		    &l_end_column,
		    0,
		    0))))
		    $LOG(l_rc);
	    }
	    else
	    {
		if (a_p->up->up == (struct node_tag *)NULL)
		    l_draw_char = l_char_up;
		else if (a_p->up->up->left != a_p->up->left)
		    l_draw_char = l_char_up;
		else
		    l_draw_char = l_char_left;
		l_start_column = a_p->up->column-LINE_NODE_DISTANCE;
		if (!(1 & (l_rc = smg$draw_char (
			&dirtree_display.id,
			&l_draw_char,
			&(a_p->up->row),
			&l_start_column,
			0,
			0))))
		    $LOG(l_rc);
		l_start_row = a_p->up->row+1;
		l_end_row = a_p->row;
		l_start_column = a_p->column-LINE_NODE_DISTANCE;
		if (!(1 & (l_rc = smg$draw_line (
		    &dirtree_display.id,
		    &l_start_row,
		    &l_start_column,
		    &l_end_row,
		    &l_start_column,
		    0,
		    0))))
		    $LOG(l_rc);
		if (!(1 & (l_rc = smg$draw_char (
			&dirtree_display.id,
			&l_left_corner,
			&(a_p->row),
			&l_start_column,
			0,
			0))))
		    $LOG(l_rc);
		l_start_column = a_p->column-LINE_NODE_DISTANCE;
		l_end_column = a_p->column-1;
		if (!(1 & (l_rc = smg$draw_line (
		    &dirtree_display.id,
		    &(a_p->row),
		    &l_start_column,
		    &(a_p->row),
		    &l_end_column,
		    0,
		    0))))
		    $LOG(l_rc);
	    }
	}
	paint_dirtree_display(a_p->right);
	if (a_p->down != (struct node_tag *)NULL)
	    if (a_p->left == a_p->down->left)
		paint_dirtree_display(a_p->down);

	if (!(1 & (l_rc = smg$end_display_update(&dirtree_display.id))))
	    $LOG(l_rc);
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FREE_DIRTREE() makes the virtual memory previously allocated to
**	DIRTREE available again.
**
**--
**/
int	free_dirtree(a_p, a_q)
struct node_tag **a_p;
struct node_tag *a_q;
{
    int l_node_entry_size = sizeof (struct node_tag);
    unsigned long l_rc;
 
    if (*a_p != (struct node_tag *)NULL)
    {
        if ((*a_p)->left == a_q)
        {
            free_dirtree(&((*a_p)->down), a_q);
            free_dirtree(&((*a_p)->right), *a_p);

            if (!(1 & (l_rc = lib$free_vm (
                         &l_node_entry_size,
                         a_p,
                         0))))
                $LOG(l_rc);
	    *a_p = (struct node_tag *)NULL;
        }
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	put_dirtree_title() writes the directory-tree command display's
**	title line: the full path name of the given node (truncated with a
**	trailing "~" if too long) plus the current search direction
**	("Forward"/"Backward"), and, below it, the current page number
**	(computed from the node's row and the viewport height) framed by a
**	horizontal rule.
**
**  INPUT PARAMETERS:
**
**	a_n  -  the node (normally cwdnode) whose path and row are shown.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	dirtree_search_direction, dirtree_display.view_rows,
**	cntrl_info_block.pasteboard_rows.
**
**  IMPLICIT OUTPUTS:
**
**	dirtree_cmds_display.id (title/page-number lines are redrawn).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	put_dirtree_title(a_n)
struct node_tag *a_n;
{
    char l_dir[10];
    int l_current_page;
    char l_cwd_line[MAXFILESPEC+1];
    $DESCRIPTOR(l_cwd_line_descrip, l_cwd_line);
    char l_page_str[81];
    $DESCRIPTOR(l_page_str_descrip, l_page_str);
    int l_line_row;
    int l_len;
    unsigned long l_rc;
 
    if (dirtree_search_direction == advance)
        strcpy(l_dir, "Forward");
    else
        strcpy(l_dir, "Backward");
    {
	char	l_path_field[MAXFILESPEC+1];
	char	l_dir_field[9];
	static $DESCRIPTOR (l_fao_ctrl_short, " !AZ~ | !AZ ");
	static $DESCRIPTOR (l_fao_ctrl_long, " !AZ | !AZ ");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_out_len;

	l_out_desc.dsc$w_length  = sizeof(l_cwd_line) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_cwd_line;

	dxscr_ljust(l_dir_field, l_dir, 8);
	if (strlen(a_n->full_path_name) > DIRTREE_TITLE_PATH_WIDTH)
	{
	    dxscr_ljust(l_path_field, a_n->full_path_name, DIRTREE_TITLE_PATH_WIDTH-1);
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl_short, &l_out_len, &l_out_desc,
				       l_path_field, l_dir_field))))
		$LOG(l_rc);
	}
	else
	{
	    dxscr_ljust(l_path_field, a_n->full_path_name, DIRTREE_TITLE_PATH_WIDTH);
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl_long, &l_out_len, &l_out_desc,
				       l_path_field, l_dir_field))))
		$LOG(l_rc);
	}
	l_cwd_line[l_out_len] = EOS;
    }
    /*  14-Sep-2026 RRL: batch the title line + page-number line + rule
    **  (3 smg$ calls below) into one flush.  **/
    if (!(1 & (l_rc = smg$begin_display_update(&dirtree_cmds_display.id))))
        $LOG(l_rc);

    LENGTH(l_cwd_line_descrip) = strlen(l_cwd_line);
    if (!(1 & (l_rc = smg$put_chars (
	      &dirtree_cmds_display.id,
	      &l_cwd_line_descrip,
	      &DIRTREE_DIRECT_ROW,
	      &1,
	      &SMG$M_ERASE_LINE,
	      &SMG$M_REVERSE,
	      0,
	      0))))
        $LOG(l_rc);

    l_current_page = 1 + (a_n->row-1)/dirtree_display.view_rows;
    {
	static $DESCRIPTOR (l_fao_ctrl_page, "!SL");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_out_len;

	l_out_desc.dsc$w_length  = sizeof(l_page_str) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_page_str;

	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl_page, &l_out_len, &l_out_desc, l_current_page))))
	    $LOG(l_rc);
	l_page_str[l_out_len] = EOS;
    }
    LENGTH(l_page_str_descrip) = strlen(l_page_str);
    l_len = strlen(l_page_str)+2;
    l_line_row = cntrl_info_block.pasteboard_rows-1;
    if (!(1 & (l_rc = smg$draw_line (
		    &dirtree_cmds_display.id,
		    &l_line_row,
		    &1,
		    &l_line_row,
		    &1,
		    0,
		    0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$put_chars (
		    &dirtree_cmds_display.id,
		    &l_page_str_descrip,
		    &l_line_row,
		    &2,
		    0,
		    &SMG$M_BOLD,
		    0,
		    0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$draw_line (
		    &dirtree_cmds_display.id,
		    &l_line_row,
		    &l_len,
		    &l_line_row,
		    &dirtree_cmds_display.width,
		    0,
		    0))))
        $LOG(l_rc);

    if (!(1 & (l_rc = smg$end_display_update(&dirtree_cmds_display.id))))
        $LOG(l_rc);

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      DIRECT_HELPDIR() calls HELPDIR() to output help message
**	by using VMS HELP utilities.
**
**--
**/
int	direct_helpdir()
{
    filer_help("DX OPTION_MENU MOVING_AROUND_DIRTREE");
    return DX__NORMAL;
}
 
 

 
 




/* ---- (from DIRECTKPD.C -- GOLD-key directory-tree navigation) ---- */

static int	dirtree_find_next$1(char *, struct node_tag **);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dirtree_find_first() is the "Find Directory" (keypad-3) command
**	entry point.  It prompts the user for a directory-name pattern (the
**	prompt text depends on the current search direction) via
**	get_userinput_and_execute(), handing off to dirtree_find_first$1()
**	as the callback once the user has typed something.
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
**	dirtree_search_direction (selects the prompt wording).
**
**  IMPLICIT OUTPUTS:
**
**	None directly (dirtree_find_first$1() updates dirtree_search_pattern
**	and cwdnode once the user responds).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	dirtree_find_first()
{
    char l_errmsg[256];
 
    strcpy(l_errmsg, "Invalid directory specification or directory not found");
    get_userinput_and_execute(
	dirtree_find_first$1,
	(dirtree_search_direction == advance? "Find Directory Forward" : "Find Directory Backward"),
	"Find What: ",
	"",
	l_errmsg,
	"");
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dirtree_find_first$1() is the callback invoked once the user has
**	typed a directory-name pattern into the "Find Directory" prompt (see
**	dirtree_find_first()).  It resolves a_pattern to a full, expanded
**	directory specification via sys$open()/sys$close() on an RMS FAB/NAM
**	pair (ignoring the expected RMS$_WLD wildcard-lookup status), saves
**	the expanded pattern for subsequent FIND NEXT commands, and then
**	performs the first search via dirtree_find_next().
**
**  INPUT PARAMETERS:
**
**	a_pattern  -  directory-name pattern typed by the user.
**	a_dummy    -  unused; present only to match the get_userinput_and_
**		      execute() callback signature.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  not used by this function directly, but passed through
**		     to callers/signal_err() indirectly via dirtree_find_next().
**
**  IMPLICIT INPUTS:
**
**	None beyond a_pattern.
**
**  IMPLICIT OUTPUTS:
**
**	dirtree_search_pattern (the expanded pattern, for later FIND NEXT
**	calls).
**
**  RETURNS VALUE:
**
**	Whatever dirtree_find_next() returns (1 if a match was highlighted,
**	-1 if the pattern could not be parsed into a directory name).
**
**--
**/
int	dirtree_find_first$1(a_pattern, a_dummy, a_errmsg)
char *a_pattern;
char *a_dummy;
char *a_errmsg;
{
    struct FAB l_fab;
    struct NAM l_nam;
    char l_expanded_pattern[NAM$C_MAXRSS];
    char *l_cp;
    unsigned int l_status;
 
    /*
    **	    INITIALIZE FAB AND NAM DATA STRUCTURES
    **/
 
    l_fab = cc$rms_fab;
    l_nam = cc$rms_nam;
 
    l_fab.fab$l_fna = a_pattern;
    l_fab.fab$b_fns = strlen(a_pattern);
    l_fab.fab$l_nam = &l_nam;
    l_fab.fab$l_dna = "*.*;*";
    l_fab.fab$b_dns = strlen(l_fab.fab$l_dna);
 
    l_nam.nam$l_esa = l_expanded_pattern;
    l_nam.nam$b_ess = NAM$C_MAXRSS;
 
    /*
    **	    GET DIRECTORY SPEC USE 'OPEN' SYSTEM SERVICE.
    **	    IGNORE THE ERROR STATUS (RMS$WLD).
    **/
 
    if (sys$open(&l_fab, 0, 0) & 1)
	sys$close(&l_fab, 0, 0);
 
    l_expanded_pattern[ l_nam.nam$b_esl ] = EOS;
    if ((l_cp = strchr(l_expanded_pattern, ']')) != 0)
	*++l_cp = EOS;
 
    /**  Preserve search string so that it can be used by FIND_NEXT.  **/
    strcpy(dirtree_search_pattern, l_expanded_pattern);
 
    return dirtree_find_next();
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dirtree_find_next() re-runs the last saved search pattern
**	(dirtree_search_pattern) to find the next matching directory.  It
**	rewrites the saved "DEVNAME:[...DIRNAME]" pattern into the wildcard
**	form "DEVNAME:[.-]DIRNAME.DIR;1" (searching the parent's contents by
**	name, except at the master file directory, which is searched as-is),
**	delegates the actual tree scan to dirtree_find_next$1(), and on a
**	hit un-highlights the current node and moves cwdnode to the match.
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
**	dirtree_search_pattern (set by dirtree_find_first$1()), cwdnode,
**	dirtree_display.id.
**
**  IMPLICIT OUTPUTS:
**
**	cwdnode (moved to the matching node on success).
**
**  RETURNS VALUE:
**
**	-1 if dirtree_search_pattern could not be parsed into a directory
**	name, or if dirtree_find_next$1() found no match; 1 on success.
**
**--
**/
int	dirtree_find_next()
{
    struct node_tag *l_n;
    int l_found;
    char *l_indx;
    char l_patternfilespec[MAXFILESPEC+1];
    char l_name[MAXFILESPEC+1];
    char l_short_path_name[MAXDIRSPEC+1];
    $DESCRIPTOR(l_short_path_name_descrip, l_short_path_name);
    unsigned long l_rc;

    /*
    **	    Convert from DEVNAME:[...DIRNAME] to DEVNAME:[.-]DIRNAME.DIR;1
    **/
    strcpy(l_patternfilespec, dirtree_search_pattern);
    if ((l_indx = strrchr(l_patternfilespec, '.')) == NULL)
	if ((l_indx = strchr(l_patternfilespec, '[')) == NULL)
	    return -1;
 
    /*
    **	    Get DIRNAME
    **/
    l_indx++;
    strcpy(l_name, l_indx);
    if ((l_indx = strchr(l_name, ']')) == NULL)
	return -1;
    *l_indx = EOS;
 
    if (strcmp(l_name, "000000")) {
 
	/*
	**	NOT A SYSTEM MASTER DIRECTORY
	**/
	l_indx = strchr(l_patternfilespec, ']');
	*l_indx++ = '.';
	*l_indx++ = '-';
	*l_indx++ = ']';
	*l_indx = EOS;
    }
 
    strcat(l_patternfilespec, l_name);
    strcat(l_patternfilespec, ".DIR;1");
 
    l_found = dirtree_find_next$1(l_patternfilespec, &l_n);
 
    if (!l_found)
	return -1;
    else {
 
	strncpy(l_short_path_name, cwdnode->short_path_name, 8);
	l_short_path_name[8] = EOS;
	LENGTH(l_short_path_name_descrip) = strlen(l_short_path_name);
	if (!(1 & (l_rc = smg$put_chars (
		    &dirtree_display.id,
		    &l_short_path_name_descrip,
		    &(cwdnode->row),
		    &(cwdnode->column),
		    0,
		    &SMG$M_NORMAL,
		    0,
		    0))))
	    $LOG(l_rc);
	
	cwdnode = l_n;
 
    }
 
    return 1;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dirtree_find_next$1() is the actual tree-search worker behind
**	dirtree_find_next(): it runs a_pattern through lib$find_file() to
**	enumerate every matching directory file, converts each ACP-returned
**	name into the tree's node-name form, looks it up via search_node(),
**	and keeps the closest one in the current search direction (nearest
**	node with a smaller row/column when searching backward, or larger
**	when searching forward) relative to cwdnode.
**
**  INPUT PARAMETERS:
**
**	a_pattern  -  wildcard directory-file specification to search for
**		      (e.g. "DEV:[.-]NAME.DIR;1").
**
**  OUTPUT PARAMETERS:
**
**	a_nodep  -  set to the closest matching node found, or left as NULL
**		    if lib$find_file() enumerated a name search_node() could
**		    not locate in the tree, or if nothing matched at all.
**
**  IMPLICIT INPUTS:
**
**	root, cwdnode, dirtree_search_direction.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	0 if no match was found (or a matching file could not be resolved to
**	a tree node); 1 if *a_nodep was set to a match.
**
**--
**/
static int	dirtree_find_next$1(a_pattern, a_nodep)
char *a_pattern;
struct node_tag **a_nodep;
{
    static unsigned int l_context;
    $DESCRIPTOR(l_pattern_descrip, a_pattern);
    char l_candidate[MAXFILESPEC+1];
    char l_candid_buf[MAXFILESPEC+1];
    $DESCRIPTOR(l_candidate_descrip, l_candidate);
    unsigned int l_status;
    struct node_tag *l_n;
    char *l_p;
 
    *a_nodep = (struct node_tag *)NULL;
    LENGTH(l_pattern_descrip) = strlen(a_pattern);
    l_candidate[MAXFILESPEC] = EOS;		/* 28-Sep-2026 CLD: lib$find_file() writes no EOS */
 
    while ((l_status = lib$find_file (
	       &l_pattern_descrip,
	       &l_candidate_descrip,
	       &l_context,
	       0,
	       0,
	       0,
	       0)) != RMS$_NMF)
    {
	if (l_status == RMS$_NORMAL)
	{
	    if ((l_p = strchr(l_candidate, ']')) != (char *)NULL)
	    {
		/*
		**  Convert DEVNAME:[DIRNAME...]FILENAME.DIR;1
		**  to DEVNAME:[DIRNAME...FILENAME]
		**/
		*l_p = '.';
		l_p = strrchr(l_candidate, '.');
		*l_p = ']';
		*++l_p = EOS;
 
		l_n = search_node(root, strchr(l_candidate, '[')+1);
 
		if (l_n == (struct node_tag *)NULL)
		{
		    lib$find_file_end (&l_context);
		    return 0;
		}
 
		if (dirtree_search_direction == backup)
		{
		    if (l_n->row < cwdnode->row ||
			(l_n->row == cwdnode->row && l_n->column < cwdnode->column))
		    {
			if (*a_nodep == (struct node_tag *)NULL)
			{
			    *a_nodep = l_n;
			}
			else if ((*a_nodep)->row < l_n->row ||
				 ((*a_nodep)->row == l_n->row &&
				  (*a_nodep)->column < l_n->column))
			    *a_nodep = l_n;
		    }
		}
		else if (l_n->row > cwdnode->row ||
		   (l_n->row == cwdnode->row && l_n->column > cwdnode->column))
	        {
		    if (*a_nodep == (struct node_tag *)NULL)
		    {
			*a_nodep = l_n;
		    }
		    else if ((*a_nodep)->row > l_n->row ||
			     ((*a_nodep)->row == l_n->row &&
			      (*a_nodep)->column > l_n->column))
			*a_nodep = l_n;
		}
	    }
	}
    }
 
    lib$find_file_end (&l_context);
 
    if (*a_nodep == (struct node_tag *)NULL)
	return 0;
    else
	return 1;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	scroll_dirtree_rows() moves the DIRTREE viewport a_delta rows in
**	ONE smg$scroll_viewport() call: a_delta > 0 moves the viewport
**	down the tree (contents scroll up), a_delta < 0 moves it up, 0 does
**	nothing.  Added 25-Sep-2026 RRL to replace the one-line-at-a-time
**	loops in change_dirtree_viewport() (each line was a separate
**	terminal update, so half-screen moves visibly crawled), and to get
**	rid of their "if (n > 0) if (...) $LOG(); else ..." construct,
**	whose else bound to the INNER if -- a successful SMG$M_UP scroll
**	was immediately undone by an SMG$M_DOWN one.
**
**  INPUT PARAMETERS:
**
**	a_delta  -  new viewport row minus current viewport row.
**
**  RETURNS VALUE:
**
**	None.
**
**--
**/
static void	scroll_dirtree_rows(int a_delta)
{
    int l_count;
    unsigned long l_rc;

    if (a_delta > 0)
    {
	l_count = a_delta;
	if (!(1 & (l_rc = smg$scroll_viewport (
		  &dirtree_display.id,
		  &SMG$M_UP,
		  &l_count))))
	    $LOG(l_rc);
    }
    else if (a_delta < 0)
    {
	l_count = -a_delta;
	if (!(1 & (l_rc = smg$scroll_viewport (
		  &dirtree_display.id,
		  &SMG$M_DOWN,
		  &l_count))))
	    $LOG(l_rc);
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	change_dirtree_viewport() scrolls the DIRTREE virtual display's
**	viewport, both vertically and horizontally, so that the target node
**	at (a_dir_row, a_dir_col) is visible.  For the row axis, it decides
**	between keeping the target just inside the edge it crossed (small
**	moves) or placing it at the opposite margin (moves of more than
**	half a screen); either way the viewport moves in ONE
**	scroll_dirtree_rows() call (25-Sep-2026 RRL -- was one
**	smg$scroll_viewport() per line for small moves), and
**	takes into account an "inactive" margin (DIRTREE_INACTIVE_ROWS/
**	DIRTREE_INACTIVE_FRONT_ROW) so the target node doesn't end up right
**	at the very edge of the screen.  The column axis simply pages left
**	or right by MAXDIRSPEC until the target column is inside the visible
**	width.
**
**  INPUT PARAMETERS:
**
**	a_dir_row  -  row of the target node in the DIRTREE virtual display.
**	a_dir_col  -  column of the target node in the DIRTREE virtual display.
**
**  OUTPUT PARAMETERS:
**
**	a_dirtree_viewport_rowp  -  current viewport row; updated in place
**				    to the new viewport row after scrolling.
**	a_dirtree_viewport_colp  -  current viewport column; updated in
**				    place to the new viewport column.
**
**  IMPLICIT INPUTS:
**
**	dirtree_display (view_rows/view_width, the id of the virtual
**	display being scrolled); root (via find_down(), to locate the last
**	node in the tree for clamping the row scroll target).
**
**  IMPLICIT OUTPUTS:
**
**	None (all screen changes go through dirtree_display.id, which is
**	not itself modified here).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	change_dirtree_viewport(a_dir_row, a_dir_col, a_dirtree_viewport_rowp, a_dirtree_viewport_colp)
int a_dir_row, a_dir_col;
int *a_dirtree_viewport_rowp, *a_dirtree_viewport_colp;
{
    int l_filler_rows, l_number_of_scroll;
    int l_find_down_flag;
    struct node_tag *l_lastnode;
    int l_new_viewport_row, l_new_viewport_row1, l_lastnode_viewport_row;
    int l_dirtree_last_inactive_row =
	1+((dirtree_display.view_rows-1)/LINESPACE)*LINESPACE;
    int l_dirtree_inactive_rear_row =
	l_dirtree_last_inactive_row-DIRTREE_INACTIVE_ROWS;
    unsigned long l_rc;

    /*
    **	Find the row number of the last node
    **/
 
    l_find_down_flag = 0;
    find_down(root, (struct node_tag *)NULL, &l_lastnode, &l_find_down_flag);
 
    /*
    **	Compute new viewport row position
    **/
 
    l_filler_rows = dirtree_display.view_rows-l_dirtree_last_inactive_row;
    l_lastnode_viewport_row =
	max(1, (int) l_lastnode->row+l_filler_rows-dirtree_display.view_rows+1);
 
    if (a_dir_row < *a_dirtree_viewport_rowp+DIRTREE_INACTIVE_FRONT_ROW-1)
    {	    /**  TARGET ROW BEFORE INACTIVE FRONT ROW  **/
	l_new_viewport_row1 =
	    max(1, (int) (a_dir_row-(DIRTREE_INACTIVE_FRONT_ROW-1)));
	l_new_viewport_row =
	    min(l_new_viewport_row1, l_lastnode_viewport_row);
 
	l_number_of_scroll = *a_dirtree_viewport_rowp-l_new_viewport_row;
	if (l_number_of_scroll <= dirtree_display.view_rows/2)
	{
	    /*
	    **	The displacement between target row and current row
	    **	is leq half of the screen: keep the target at the front
	    **	margin (one scroll, not one per line).
	    **/
 
	    scroll_dirtree_rows(l_new_viewport_row-*a_dirtree_viewport_rowp);
	    *a_dirtree_viewport_rowp = l_new_viewport_row;
	}
	else
	{	
	    /*	
	    **	The displacement is larger than half the screen,
	    **	use jump scroll instead.
	    **/
 
	    l_new_viewport_row1 =
		max(1, (int) (a_dir_row-(l_dirtree_inactive_rear_row-1)));
	    l_new_viewport_row =
		min(l_new_viewport_row1, l_lastnode_viewport_row);
 
	    scroll_dirtree_rows(l_new_viewport_row-*a_dirtree_viewport_rowp);
	    *a_dirtree_viewport_rowp = l_new_viewport_row;
	}
    }
    else if (a_dir_row >
	*a_dirtree_viewport_rowp+l_dirtree_inactive_rear_row-1)
    {	    /**  Target row beyond inactive rear row  **/
	l_new_viewport_row1 =
	    max(1, (int) (a_dir_row-(l_dirtree_inactive_rear_row-1)));
	l_new_viewport_row =
	    min(l_new_viewport_row1, l_lastnode_viewport_row);
 
	l_number_of_scroll = l_new_viewport_row-*a_dirtree_viewport_rowp;
	if (abs(l_number_of_scroll) <= dirtree_display.view_rows/2)
	{
	    /*
	    **	The displacement between target row and current row
	    **	is leq half of the screen: keep the target at the rear
	    **	margin (one scroll, not one per line).
	    **/
 
	    scroll_dirtree_rows(l_number_of_scroll);
	    *a_dirtree_viewport_rowp = l_new_viewport_row;
	}
	else
	{	
	    /*	
	    **	The displacement is larger than half the screen,
	    **	use jump scroll instead.
	    **/
 
	    l_new_viewport_row1 =
		max(1, (int) (a_dir_row-(DIRTREE_INACTIVE_FRONT_ROW-1)));
	    l_new_viewport_row =
		min(l_new_viewport_row1, l_lastnode_viewport_row);
 
	    scroll_dirtree_rows(l_new_viewport_row-*a_dirtree_viewport_rowp);
	    *a_dirtree_viewport_rowp = l_new_viewport_row;
	}
    }
    else if (l_lastnode->row <=
		*a_dirtree_viewport_rowp+l_dirtree_last_inactive_row-1)
    {
	/*
	**  Target node is in active area, and the
	**  last node in this directory tree is also in current screen.
	**/
 
	l_new_viewport_row = l_lastnode_viewport_row;
	l_number_of_scroll = min(*a_dirtree_viewport_rowp-l_new_viewport_row,
				 *a_dirtree_viewport_rowp-1);
	if (l_number_of_scroll > 0)
	{	/**  One jump scroll, not N one-line scrolls (25-Sep-2026 RRL)  **/
	    if (!(1 & (l_rc = smg$scroll_viewport (
		      &dirtree_display.id,
		      &SMG$M_DOWN,
		      &l_number_of_scroll))))
	        $LOG(l_rc);
	    *a_dirtree_viewport_rowp -= l_number_of_scroll;
	}
    }
	
    /*
    **	Compute new viewport column position
    **/
 
    while (a_dir_col+MAXDIRSPEC >
	*a_dirtree_viewport_colp+dirtree_display.view_width-1)
    {
	if (!(1 & (l_rc = smg$scroll_viewport (
		&dirtree_display.id,
		&SMG$M_LEFT,
		&MAXDIRSPEC))))
	    $LOG(l_rc);
	 *a_dirtree_viewport_colp += MAXDIRSPEC;
    }
 
    while (a_dir_col < *a_dirtree_viewport_colp)
    {
	if (!(1 & (l_rc = smg$scroll_viewport (
		    &dirtree_display.id,
		    &SMG$M_RIGHT,
		    &MAXDIRSPEC))))
	    $LOG(l_rc);
	*a_dirtree_viewport_colp -= MAXDIRSPEC;
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FIND_DOWN(), FIND_RIGHT(), and FIND_LEFT() functions handle GOLD-key
**	fast move.
**
**--
**/
void	    find_down(a_curnode, a_parnode, a_q, a_flagp)
struct node_tag *a_curnode;
struct node_tag *a_parnode;
struct node_tag **a_q;
int *a_flagp;
{
    if (a_curnode != (struct node_tag *)NULL)
    {
	if (a_curnode->left == a_parnode)
	{
	    find_down(a_curnode->down, a_parnode, a_q, a_flagp);
	    find_down(a_curnode->right, a_curnode, a_q, a_flagp);
	
	    if (! *a_flagp)
	    {
		*a_flagp = 1;
		*a_q = a_curnode;
	    }
	}
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	find_right() walks the "right" (sibling) chain from a_p to its last
**	sibling -- the rightmost node at that tree level.
**
**  INPUT PARAMETERS:
**
**	a_p  -  node to start from.
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
**	The rightmost sibling of a_p (a_p itself if it has no right sibling).
**
**--
**/
struct node_tag *	find_right(a_p)
struct node_tag *a_p;
{
    if (a_p->right == (struct node_tag *)NULL)
	return a_p;
    else
	return find_right(a_p->right);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dirtree_page_down() implements the keypad-8/PAGE-DOWN command when
**	searching forward: it un-highlights a_n, picks a new current node up
**	to one screenful of rows further down the tree (or a_lastnode itself
**	if that would go past it), then scrolls the DIRTREE viewport down by
**	just enough lines to bring the new node into the active display
**	area, stopping early if the last node is reached first.
**
**  INPUT PARAMETERS:
**
**	a_n         -  the current node before paging.
**	a_lastnode  -  the last (bottom-most) node in the tree, as found by
**		       find_down(); used to clamp how far this can page.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	root (via search_node_page()), cwdnode, dirtree_display (view_rows,
**	view_beg_y, id).
**
**  IMPLICIT OUTPUTS:
**
**	dirtree_display.view_beg_y (advanced as the viewport scrolls); the
**	DIRTREE display (scrolled and the old node's highlight cleared).
**
**  RETURNS VALUE:
**
**	The new current node (a_lastnode, or the node found by
**	search_node_page() at the new target row/column).
**
**--
**/
struct node_tag *dirtree_page_down(a_n, a_lastnode)
struct node_tag *a_n;
struct node_tag *a_lastnode;
{
    char l_short_path_name[MAXDIRSPEC+1];	/**  Buffer to node name     **/
    $DESCRIPTOR(l_short_path_name_descrip, l_short_path_name);
    int l_max_scroll, l_number_of_scroll;
    int l_new_row, l_adjustment, l_i;
    int l_dirtree_last_inactive_row = 1+((dirtree_display.view_rows-1)/LINESPACE)*LINESPACE;
    unsigned long l_rc;

    strncpy(l_short_path_name, a_n->short_path_name, 8);
    l_short_path_name[8] = EOS;
    LENGTH(l_short_path_name_descrip) = strlen(l_short_path_name);
    if (!(1 & (l_rc = smg$put_chars (
	    &dirtree_display.id,
	    &l_short_path_name_descrip,
	    &(a_n->row),
	    &(a_n->column),
	    0,
	    &SMG$M_NORMAL,
	    0,
	    0))))
        $LOG(l_rc);
 
    l_max_scroll = LINESPACE*((dirtree_display.view_rows-1)/LINESPACE);
    l_new_row = min((int)a_lastnode->row, (int)(cwdnode->row+l_max_scroll));
 
    if (a_n->row == a_lastnode->row)    /**  CHANGE TO LAST NODE IF LAST ROW     **/
	a_n = a_lastnode;
    else			    /**  FIND NEW NODE			     **/
	a_n = search_node_page(root, l_new_row, a_n->column);
 
    /*
    **	    SCROLL LESS THAN ONE FULL SCREEN IF NEW DIRECTORY IS NOT
    **	    WITHIN THE INACTIVE AREA
    **/
 
    l_adjustment = dirtree_display.view_rows-l_dirtree_last_inactive_row+1;
    if (a_n->row <
	    dirtree_display.view_beg_y+dirtree_display.view_rows-l_adjustment+
	    DIRTREE_INACTIVE_FRONT_ROW-1)
	l_number_of_scroll = dirtree_display.view_rows-l_adjustment-
			    (DIRTREE_INACTIVE_FRONT_ROW-1);
    else
	l_number_of_scroll = dirtree_display.view_rows-l_adjustment;
	
    /*
    **	    SCROLL FILER DISPLAY.  STOP AS SOON AS BOTTOM OF RECORD
    **	    IS REACHED.
    **/
 
    /*  25-Sep-2026 RRL: one jump scroll of however many lines the old
    **  one-line-per-iteration loop would have done (each of those was a
    **  separate terminal update, so paging visibly crawled).  **/
    l_i = a_lastnode->row-
	(dirtree_display.view_beg_y+dirtree_display.view_rows-l_adjustment);
    l_number_of_scroll = min(l_number_of_scroll, l_i);
    if (l_number_of_scroll > 0)
    {
	if (!(1 & (l_rc = smg$scroll_viewport (
		      &dirtree_display.id,
		      &SMG$M_UP,
		      &l_number_of_scroll))))
	    $LOG(l_rc);
	dirtree_display.view_beg_y += l_number_of_scroll;
    }
 
    return a_n;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	search_node_page() finds the tree node closest to a given (row,
**	column) position, used by dirtree_page_up()/dirtree_page_down() to
**	locate a target node after scrolling a page's worth of rows.  It
**	descends the tree (via ->down) until it reaches or passes a_row,
**	then scans right (via ->right) along that row until it reaches or
**	passes a_col.
**
**  INPUT PARAMETERS:
**
**	a_p    -  node to start the search from (recursion accumulates here).
**	a_row  -  target row.
**	a_col  -  target column.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	None (pure tree walk over its arguments).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	The closest node at or past (a_row, a_col) found along the search
**	path; NULL if a_p is NULL.
**
**--
**/
struct node_tag *search_node_page (a_p, a_row, a_col)
struct node_tag *a_p;
int a_row;
int a_col;
{
    if (a_p == (struct node_tag *)NULL)
 
	return (struct node_tag *)NULL;
 
    else if (a_p->row < a_row)
	/**  No node has been found with the given row  **/
 
	if (a_p->down == (struct node_tag *)NULL)
 
	    return search_node_page(a_p->right, a_row, a_col);
 
	else if (a_p->down->row > a_row)
 
	    return search_node_page(a_p->right, a_row, a_col);
 
	else
 
	    return search_node_page(a_p->down, a_row, a_col);
 
    else if (a_p->column >= a_col || a_p->right == (struct node_tag *)NULL)
 
	return a_p;  /**  Target node found  **/
 
    else  /**  Search the nodes in the same row  **/
 
	return search_node_page(a_p->right, a_row, a_col);
 
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	dirtree_page_up() implements the keypad-8/PAGE-UP command when
**	searching backward: it un-highlights a_n, jumps straight to the root
**	if a_n is already in the top row, otherwise picks a new node up to
**	one screenful of rows higher via search_node_page(), then scrolls
**	the DIRTREE viewport up by just enough lines to bring the new node
**	into the active display area, stopping early once the top of the
**	tree is reached.
**
**  INPUT PARAMETERS:
**
**	a_n  -  the current node before paging.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	root, dirtree_display (view_rows, view_beg_y, id).
**
**  IMPLICIT OUTPUTS:
**
**	dirtree_display.view_beg_y (decremented as the viewport scrolls);
**	the DIRTREE display (scrolled and the old node's highlight cleared).
**
**  RETURNS VALUE:
**
**	The new current node (root, or the node found by
**	search_node_page() at the new target row/column).
**
**--
**/
struct node_tag *dirtree_page_up (a_n)
struct node_tag *a_n;
{
    char l_short_path_name[MAXDIRSPEC+1];	/**  Buffer to node name     **/
    $DESCRIPTOR(l_short_path_name_descrip, l_short_path_name);
    int l_max_scroll, l_number_of_scroll;
    int l_adjustment, l_new_row;
    int l_dirtree_last_inactive_row = 1+((dirtree_display.view_rows-1)/LINESPACE)*LINESPACE;
    int l_dirtree_inactive_rear_row = l_dirtree_last_inactive_row-DIRTREE_INACTIVE_ROWS;
    unsigned long l_rc;

    strncpy(l_short_path_name, a_n->short_path_name, 8);
    l_short_path_name[8] = EOS;
    LENGTH(l_short_path_name_descrip) = strlen(l_short_path_name);
    if (!(1 & (l_rc = smg$put_chars (
	    &dirtree_display.id,
	    &l_short_path_name_descrip,
	    &(a_n->row),
	    &(a_n->column),
	    0,
	    &SMG$M_NORMAL,
	    0,
	    0))))
        $LOG(l_rc);
 
    if (a_n->row == 1)	    /**  ALREADY IN THE TOP ROW			     **/
 
	a_n = root;
 
    else {		    /**  SCROLL UP ONE SCREEN			     **/
 
	l_max_scroll = LINESPACE*((dirtree_display.view_rows-1)/LINESPACE);
	l_new_row = max(1, (int)(a_n->row-l_max_scroll));
	a_n = search_node_page(root, l_new_row, a_n->column);
 
    }
 
    /**  CALCULATE NUMBER OF ROWS TO BE SCROLLED  **/
 
    l_adjustment = dirtree_display.view_rows-l_dirtree_last_inactive_row+1;
    if (a_n->row > dirtree_display.view_beg_y-(dirtree_display.view_rows-l_adjustment)+l_dirtree_inactive_rear_row-1)
	l_number_of_scroll = dirtree_display.view_rows-l_adjustment-(DIRTREE_INACTIVE_FRONT_ROW-1);
    else
	l_number_of_scroll = dirtree_display.view_rows-l_adjustment;
	
    /*
    **	    SCROLL FILER DISPLAY.  STOP AS SOON AS TOP OF RECORD
    **	    IS REACHED.
    **/
 
    /*  25-Sep-2026 RRL: one jump scroll, clamped at the top of the tree
    **  (was one smg$scroll_viewport() per line).  **/
    l_number_of_scroll = min(l_number_of_scroll, dirtree_display.view_beg_y-1);
    if (l_number_of_scroll > 0)
    {
	if (!(1 & (l_rc = smg$scroll_viewport (
		      &dirtree_display.id,
		      &SMG$M_DOWN,
		      &l_number_of_scroll))))
	    $LOG(l_rc);
	dirtree_display.view_beg_y -= l_number_of_scroll;
    }
 
    return a_n;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	direct_initial() implements the keypad-7 "Goto initial" command: it
**	un-highlights the current node, then re-locates and jumps cwdnode
**	back to the node matching the directory DX was started in (or the
**	FILER's directory at the time DIRECT was entered).
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
**	root, cwdnode, dirtree_display.id, DX_CURRENT_DIRECTORY.cur_dir.
**
**  IMPLICIT OUTPUTS:
**
**	cwdnode (moved to the initial-directory node).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	direct_initial()
{
    char l_initial_root[MAXFILESPEC+1];
    char l_short_path_name[MAXDIRSPEC+1];
    $DESCRIPTOR(l_short_path_name_descrip, l_short_path_name);
    unsigned long l_rc;

    /*
    **  Compare initial root directory name with current root
    **  directory name
    **/
 
    find_root(DX_CURRENT_DIRECTORY.cur_dir, l_initial_root);
 
    /*
    **	Un-highlight current directory
    **/
 
    strncpy(l_short_path_name, cwdnode->short_path_name, 8);
    l_short_path_name[8] = EOS;
    LENGTH(l_short_path_name_descrip) = strlen(l_short_path_name);
    if (!(1 & (l_rc = smg$put_chars (
	    &dirtree_display.id,
	    &l_short_path_name_descrip,
	    &(cwdnode->row) ,
	    &(cwdnode->column) ,
	    0,
	    &SMG$M_NORMAL,
	    0,
	    0 ))))
        $LOG(l_rc);
 
    cwdnode = search_node(root, strchr(DX_CURRENT_DIRECTORY.cur_dir, '[')+1);
 
    return DX__NORMAL;
}



/* ---- (from DIRECTQUE.C -- directory-tree LRU cache) ---- */

static struct node_tag *	search_queue_node(struct node_tag *, char *);
static int			free_root_equ(struct root_equ_names_tag **);
static int			insert_root_equ_name(
				    struct root_equ_names_tag **, char *);
static int			cmp_root_equ(
				    struct root_equ_names_tag *, struct root_equ_names_tag *);
static int			s_equ_name_in_list(char *, struct root_equ_names_tag *);
 
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	initialize_direct_cache() resets every slot of the directory-tree
**	LRU cache (direct_cache[]) to its empty state: a sentinel
**	"17-NOV-1858 00:00:00.00" timestamp (so find_LRU_in_direct_cache()
**	picks these slots first), no root-equivalence names, forward
**	search direction, the root-level "[]" pattern, and a NULL tree.
**	Called once at startup, before any directory has been cached.
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
**	direct_cache[] -- every slot reset as described above.
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
*/
int	    initialize_direct_cache()
{
    int l_i;

    for (l_i = 0;  l_i < MAX_DIRECT_BUFFER;  l_i++)
    {
	convert_date_string (
	    "17-NOV-1858 00:00:00.00", &direct_cache[l_i].time_stamp);
	direct_cache[l_i].root_equ_names = (struct root_equ_names_tag *)NULL;
	direct_cache[l_i].direction = advance;
	strcpy(direct_cache[l_i].pattern, "[]");
	direct_cache[l_i].root = (struct node_tag *)NULL;
    }
    return DX__NORMAL;
}
 
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      find_LRU_in_direct_cache() find the Least-Recently-Used slot
**	in the directory cache.
**
**--
**/
int	find_LRU_in_direct_cache()
{
    DATE_TIME l_earliest_time,
	      l_cmp;
    int l_m,
        l_n;

    l_m = 0;
    memcpy (&l_earliest_time, &direct_cache[0].time_stamp, 8);
    for (l_n = 1;  l_n < MAX_DIRECT_BUFFER;  l_n++)
    {
        if (lib$sub_times(
                &direct_cache[l_n].time_stamp,
                &l_earliest_time,
                &l_cmp) == LIB$_NEGTIM)
	{
	    memcpy(
		&l_earliest_time, &direct_cache[l_n].time_stamp, 8);
	    l_m = l_n;
	}
    }

    /*
    **  Reset the default values and free up memory used by this node
    **/

    direct_cache[l_m].direction = advance;
    strcpy(direct_cache[l_m].pattern, "[]");
    free_root_equ (&(direct_cache[l_m].root_equ_names));
    direct_cache[l_m].root_equ_names = (struct root_equ_names_tag *)NULL;
    free_dirtree (&(direct_cache[l_m].root), (struct node_tag *)NULL);
    direct_cache[l_m].root = (struct node_tag *)NULL;

    return l_m;
}
 
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	search_direct_cache() looks for a previously-cached directory tree
**	matching the root logical name a_n.  It translates a_n to its set of
**	equivalence names via trnlnm() (draining the queue into a temporary
**	root_equ_names_tag list), then compares that list against each
**	cache slot's saved root_equ_names via cmp_root_equ() until a match
**	is found.
**
**  INPUT PARAMETERS:
**
**	a_n  -  root logical name to look up (e.g. the initial root
**		directory of the tree being entered).
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	direct_cache[] (searched for a matching root_equ_names entry).
**
**  IMPLICIT OUTPUTS:
**
**	None (the temporary equivalence-name list built here is freed
**	before returning).
**
**  RETURNS VALUE:
**
**	Index of the matching direct_cache[] slot, or -1 if not found.
**
**--
**/
int	search_direct_cache(a_n)
char *a_n;
{
    static _align (QUADWORD) int l_logicalQ[2];

    struct logical_queue_entry_tag *l_E;
    int l_number_of_bytes;
    unsigned long l_rc;
    char *l_cp;
    int l_i;
    struct root_equ_names_tag *l_l;

    /*
    **	Translate the root logical name.
    **/

    l_l = (struct root_equ_names_tag *)NULL;
    l_logicalQ[0] = l_logicalQ[1] = 0;
    trnlnm(a_n, l_logicalQ);
    while (_REMQHI (l_logicalQ, &l_E) != 3)
    {
	l_number_of_bytes = strlen(l_E->equivalent_name)+1;
	if ((l_cp = strstr (l_E->equivalent_name, ".000000]")) != NULL)
	{
	    *l_cp++ = ']';
	    *l_cp = EOS;
	}
	insert_root_equ_name(&l_l, l_E->equivalent_name);
	if (!(1 & (l_rc = lib$free_vm(&l_number_of_bytes, &l_E->equivalent_name, 0))))
	    $LOG(l_rc);		/**  Should never happen; leaks this one
				**   buffer if it does, but the queue must
				**   still be drained, so keep going.  **/
    }

    for (l_i = 0;  l_i < MAX_DIRECT_BUFFER;  l_i++)
    {
	if (cmp_root_equ(l_l, direct_cache[l_i].root_equ_names))
	{
	    free_root_equ(&l_l);
	    return l_i;
	}
    }

    free_root_equ(&l_l);
    return -1;	    /**  Not found  **/
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	save_to_direct_cache() records a directory tree that is about to be
**	left (DIRECT exited) into cache slot a_i: it stamps the slot with
**	the current time (for the LRU replacement policy), translates the
**	root logical name a_r into its equivalence-name list (used later by
**	search_direct_cache() to recognize this same tree again), and saves
**	the search direction, search pattern, and tree root pointer as-is.
**
**  INPUT PARAMETERS:
**
**	a_i  -  cache slot index to save into (from search_direct_cache()
**		or find_LRU_in_direct_cache()).
**	a_r  -  root logical name of the tree (translated to equivalence
**		names for later lookup).
**	a_d  -  current search direction (dirtree_search_direction).
**	a_p  -  current search pattern (dirtree_search_pattern).
**	a_n  -  root node of the tree (NOT freed here -- ownership passes
**		to the cache).
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	None beyond its parameters.
**
**  IMPLICIT OUTPUTS:
**
**	direct_cache[a_i] (time_stamp, root_equ_names, direction, pattern,
**	root all set).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	    save_to_direct_cache(a_i, a_r, a_d, a_p, a_n)
int a_i;
char *a_r;
enum directions a_d;
char *a_p;
struct node_tag *a_n;
{
    static _align (QUADWORD) int l_logicalQ[2];

    struct logical_queue_entry_tag *l_E;
    int l_number_of_bytes;
    unsigned long l_rc;
    char *l_cp;

    if (!(1 & (l_rc = sys$gettim((struct _generic_64 *) &direct_cache[a_i].time_stamp))))
	$LOG(l_rc);	/**  Should never happen; the cache entry just keeps
			**   whatever time_stamp it had before.  **/

    /*
    **	Translate the root logical name.
    **	Should only expand to a single name.
    **/

    direct_cache[a_i].root_equ_names = (struct root_equ_names_tag *)NULL;
    l_logicalQ[0] = l_logicalQ[1] = 0;
    trnlnm(a_r, l_logicalQ);
    while (_REMQHI (l_logicalQ, &l_E) != 3)
    {
	l_number_of_bytes = strlen(l_E->equivalent_name)+1;
	if ((l_cp = strstr (l_E->equivalent_name, ".000000]")) != NULL)
	{
	    *l_cp++ = ']';
	    *l_cp = EOS;
	}
	insert_root_equ_name(&(direct_cache[a_i].root_equ_names), l_E->equivalent_name);
	if (!(1 & (l_rc = lib$free_vm(&l_number_of_bytes, &l_E->equivalent_name, 0))))
	    $LOG(l_rc);		/**  Leaks this one buffer if it happens,
				**   but the queue must still be drained.  **/
    }

    direct_cache[a_i].direction = a_d;
    strcpy(direct_cache[a_i].pattern, a_p);
    direct_cache[a_i].root = a_n;
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	restore_from_direct_cache() reads back a previously-cached directory
**	tree's search direction, search pattern, and root node pointer from
**	cache slot a_i, so filer_direct() can resume browsing an already-
**	built tree instead of rescanning the directory from scratch.
**
**  INPUT PARAMETERS:
**
**	a_i  -  cache slot index to read from (from search_direct_cache()).
**
**  OUTPUT PARAMETERS:
**
**	a_d  -  set to the cached search direction.
**	a_p  -  set to the cached search pattern.
**	a_n  -  set to the cached tree root pointer.
**
**  IMPLICIT INPUTS:
**
**	direct_cache[a_i].
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
int	    restore_from_direct_cache(a_i, a_d, a_p, a_n)
int a_i;
enum directions *a_d;
char *a_p;
struct node_tag **a_n;
{
    *a_d = direct_cache[a_i].direction;
    strcpy(a_p, direct_cache[a_i].pattern);
    *a_n = direct_cache[a_i].root;
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	add_to_direct_cache() updates every cached directory tree that is an
**	ancestor of a newly-created directory a_e, inserting it into that
**	cached tree (via add_node()) and recomputing the tree's row/column
**	layout (via traverse_tree()) so the cached tree stays consistent
**	with the actual file system.  A cache slot's tree is a candidate if
**	a_e's translated equivalence name has one of the slot's own root
**	equivalence names as a prefix.
**
**  INPUT PARAMETERS:
**
**	a_e  -  logical name of the newly-created directory (translated via
**		trnlnm() to get its single equivalence name).
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  not filled in by this function (present only for
**		     signature symmetry with other cache-update callers);
**		     left untouched.
**
**  IMPLICIT INPUTS:
**
**	direct_cache[] (each slot's root_equ_names and root are consulted/
**	updated).
**
**  IMPLICIT OUTPUTS:
**
**	direct_cache[].root (a new node is inserted into any matching
**	slot's tree, and its row/column layout is recomputed).
**
**  RETURNS VALUE:
**
**	DX__ERROR if add_node() failed (out of memory) for some matching
**	slot; DX__NORMAL otherwise.
**
**--
**/
int	    add_to_direct_cache(a_e, a_errmsg)
char *a_e;
char *a_errmsg;
{
    static _align (QUADWORD) int l_logicalQ[2];

    struct logical_queue_entry_tag *l_E;
    int l_number_of_bytes;
    unsigned long l_rc;
    char *l_cp;
    int l_i;
    char l_f[MAXFILESPEC+1];
    char l_s1[MAXFILESPEC+1];
    struct root_equ_names_tag *l_p;

    /*
    **	Translate the logical name.
    **	Should only expand to a single name.
    **/

    l_logicalQ[0] = l_logicalQ[1] = 0;
    trnlnm(a_e, l_logicalQ);
    while (_REMQHI (l_logicalQ, &l_E) != 3)
    {
	l_number_of_bytes = strlen(l_E->equivalent_name)+1;
	if ((l_cp = strstr (l_E->equivalent_name, ".000000]")) != NULL)
	{
	    *l_cp++ = ']';
	    *l_cp = EOS;
	}
	strcpy(l_f, l_E->equivalent_name);
	if (!(1 & (l_rc = lib$free_vm(&l_number_of_bytes, &l_E->equivalent_name, 0))))
	    $LOG(l_rc);		/**  Leaks this one buffer if it happens,
				**   but the queue must still be drained.  **/
    }

    /*
    **	Search queue for insertion.
    **/

    for (l_i = 0;  l_i < MAX_DIRECT_BUFFER;  l_i++)
    {	    /**  Compare device and root name  **/
        for (l_p = direct_cache[l_i].root_equ_names;
	     l_p != (struct root_equ_names_tag *)NULL;  l_p = l_p->next)
        {
	    strcpy(l_s1, l_p->equ_name);
	    l_s1[ strlen(l_s1)-1 ] = EOS;	    /**   Delete ']'  **/

            if (strstr (l_f, l_s1))
            {
                break;			    /**   Root is an ancestor  **/
            }
        }

	if (l_p != (struct root_equ_names_tag *)NULL)
	{
	    /*
	    **  Insert the newly created directory into the tree
	    **/
	    if (add_node(
		&direct_cache[l_i].root,
		l_f,
		(struct node_tag *)NULL) == (struct node_tag *)NULL)
		return DX__ERROR;


	    /*
	    **	    Rebuild the directory tree
	    **/

	    traverse_tree(
		direct_cache[l_i].root, (struct node_tag *)NULL, 1, 1, dummy);
	}
    }

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	delete_from_direct_cache() removes a deleted directory a_e from
**	every cached directory tree that is its ancestor: it locates the
**	corresponding node via search_queue_node(), re-links its up/down/
**	right neighbors around the gap, and frees the node's memory.  A
**	cache slot's tree is a candidate under the same ancestor test used
**	by add_to_direct_cache().
**
**  INPUT PARAMETERS:
**
**	a_e  -  logical name of the deleted directory (translated via
**		trnlnm() to get its single equivalence name).
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  not filled in by this function (present only for
**		     signature symmetry with other cache-update callers);
**		     left untouched.
**
**  IMPLICIT INPUTS:
**
**	direct_cache[] (each slot's root_equ_names and root are consulted).
**
**  IMPLICIT OUTPUTS:
**
**	direct_cache[].root (the matching node, if found, is unlinked and
**	its memory freed).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	    delete_from_direct_cache(a_e, a_errmsg)
char *a_e;
char *a_errmsg;
{
    static _align (QUADWORD) int l_logicalQ[2];

    struct logical_queue_entry_tag *l_E;
    int l_number_of_bytes;
    unsigned long l_rc;
    char *l_cp;
    int l_i;
    char l_f[MAXFILESPEC+1];
    struct node_tag *l_current,
		    *l_downnode,
		    *l_upnode,
		    *l_leftnode;
    char l_s1[MAXFILESPEC+1];
    int l_allocate_size = sizeof (struct node_tag);
    struct root_equ_names_tag *l_p;

    /*
    **	Translate the logical name.
    **	Should only expand to a single name.
    **/

    l_logicalQ[0] = l_logicalQ[1] = 0;
    trnlnm(a_e, l_logicalQ);
    while (_REMQHI (l_logicalQ, &l_E) != 3)
    {
	l_number_of_bytes = strlen(l_E->equivalent_name)+1;
	if ((l_cp = strstr (l_E->equivalent_name, ".000000]")) != NULL)
	{
	    *l_cp++ = ']';
	    *l_cp = EOS;
	}
	strcpy(l_f, l_E->equivalent_name);
	if (!(1 & (l_rc = lib$free_vm(&l_number_of_bytes, &l_E->equivalent_name, 0))))
	    $LOG(l_rc);		/**  Leaks this one buffer if it happens,
				**   but the queue must still be drained.  **/
    }

    /*
    **	Search queue for deletion.
    **/

    for (l_i = 0;  l_i < MAX_DIRECT_BUFFER;  l_i++)
    {
        for (l_p = direct_cache[l_i].root_equ_names;
	     l_p != (struct root_equ_names_tag *)NULL;  l_p = l_p->next)
        {
	    strcpy(l_s1, l_p->equ_name);
	    l_s1[ strlen(l_s1)-1 ] = EOS;	    /**   Delete ']'  **/

            if (strstr (l_f, l_s1))
            {
                break;			    /**   Root is an ancestor  **/
            }
        }

	if (l_p != (struct root_equ_names_tag *)NULL)
	{
	    if ((l_current = search_queue_node(direct_cache[l_i].root, l_f)) != NULL)
	    {
		/**  Re-link UP node  **/
		if ((l_upnode = l_current->up) != (struct node_tag *)NULL)
		    l_upnode->down = l_current->down;

		/**  Re-link DOWN node  **/
		if ((l_downnode = l_current->down) != (struct node_tag *)NULL)
		    l_downnode->up = l_upnode;

		/*
		**  Re-link RIGHT node if the node deleted was the
		**  first child
		**/
		l_leftnode = l_current->left;
		if (l_leftnode->right == l_current)
		{	/**  This is the oldest child  **/
		    if (l_current->down == NULL)		 /**  No more child  **/
			l_leftnode->right = (struct node_tag *)NULL;
		    else if (l_leftnode == l_downnode->left) /**  Same parent    **/
			l_leftnode->right = l_downnode;
		    else
			l_leftnode->right = (struct node_tag *)NULL;
		}

		/**  Free up memory allocated to this node  **/
		lib$free_vm (
			&l_allocate_size,
			&l_current,
			0);
	    }
	}
    }

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	search_queue_node() searches a cached directory tree (rooted at
**	a_n) for the node whose full_path_name exactly matches a_f, trying
**	the current node, then its right siblings (recursively), then
**	descending to the first child row (only when that row's leftmost
**	node shares the same left-parent as a_n, mirroring the tree's own
**	invariant), and recursing there.
**
**  INPUT PARAMETERS:
**
**	a_n  -  node to search from (and its right-siblings/children).
**	a_f  -  full path name to match.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	None (pure tree walk over its arguments).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	The matching node, or NULL if a_f is not found under a_n.
**
**--
**/
static struct node_tag *	search_queue_node(a_n, a_f)
struct node_tag *a_n;
char *a_f;
{
    struct node_tag *l_m;

    if (a_n == (struct node_tag *)NULL)
    {
        return (struct node_tag *)NULL;
    }
    else if (!strcmp(a_n->full_path_name, a_f))
    {
        return a_n;
    }
    else if ((l_m = search_queue_node(a_n->right, a_f)) != (struct node_tag *)NULL)
    {
	return l_m;
    }
    else if (a_n->down->left == a_n->left)
    {
	return search_queue_node(a_n->down, a_f);
    }
    else
	return (struct node_tag *)NULL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	free_root_equ() recursively frees an entire root_equ_names_tag
**	linked list (as built by insert_root_equ_name()) and clears the
**	caller's head pointer.
**
**  INPUT PARAMETERS:
**
**	None beyond a_r.
**
**  OUTPUT PARAMETERS:
**
**	a_r  -  list head to free; set to NULL when done.
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
**	DX__NORMAL always.
**
**--
**/
static int	    free_root_equ(a_r)
struct root_equ_names_tag **a_r;
{
    int l_node_entry_size = sizeof (struct root_equ_names_tag);
    unsigned long l_rc;

    if (*a_r != (struct root_equ_names_tag *)NULL)
    {
        free_root_equ(&((*a_r)->next));
	if (!(1 & (l_rc = lib$free_vm(&l_node_entry_size, a_r, 0))))
	    $LOG(l_rc);		/**  Leaks this node if it happens, but
				**   *a_r is still cleared below so nothing
				**   downstream can dangle/double-free it.  **/
	*a_r = (struct root_equ_names_tag *)NULL;
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	insert_root_equ_name() appends a_name to the end of a
**	root_equ_names_tag linked list, allocating a new node via
**	lib$get_vm().  Used to build up the set of equivalence names a root
**	logical name translates to, for later comparison by cmp_root_equ().
**
**  INPUT PARAMETERS:
**
**	a_name  -  equivalence-name string to append.
**
**  OUTPUT PARAMETERS:
**
**	a_r  -  list head; updated in place if this is the first insertion
**		(the list is otherwise walked to its tail via recursion).
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
**	$LOG(DX__ERROR) if lib$get_vm() failed; DX__NORMAL otherwise.
**
**--
**/
static int		    insert_root_equ_name(a_r, a_name)
struct root_equ_names_tag **a_r;
char *a_name;
{
    int l_allocate_size = sizeof (struct root_equ_names_tag);
    struct root_equ_names_tag *l_new;
    unsigned long l_rc;

    if (*a_r != (struct root_equ_names_tag *)NULL)
        insert_root_equ_name(&((*a_r)->next), a_name);
    else
    {
        if (!(1 & (l_rc = lib$get_vm(&l_allocate_size, &l_new, 0))))
	    return $LOG(DX__ERROR);	/**  l_new would be garbage --
					**   must not dereference it below.  **/
	strcpy(l_new->equ_name, a_name);
	l_new->next = (struct root_equ_names_tag *)NULL;
	*a_r = l_new;
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	s_equ_name_in_list() is a linear membership test: does a_name
**	occur (by strcmp) in a_list?  Factored out of cmp_root_equ(),
**	which used to run this exact search loop twice (once per
**	direction of its subset test) with the body copy-pasted in full.
**
**  INPUT PARAMETERS:
**
**	a_name  -  equivalence-name string to look for.
**	a_list  -  root_equ_names_tag list to search (may be NULL).
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  RETURNS VALUE:
**
**	1 if a_name is found in a_list, 0 otherwise.
**
**--
*/
static int	    s_equ_name_in_list(a_name, a_list)
char *a_name;
struct root_equ_names_tag *a_list;
{
    struct root_equ_names_tag *l_q;

    for (l_q = a_list;  l_q != (struct root_equ_names_tag *)NULL;  l_q = l_q->next)
    {
        if (!strcmp(a_name, l_q->equ_name))
            return 1;
    }
    return 0;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	cmp_root_equ() tests whether two root_equ_names_tag lists represent
**	the same set of equivalence names, by checking (via
**	s_equ_name_in_list()) that every name in a_s occurs in a_t and every
**	name in a_t occurs in a_s.
**
**  INPUT PARAMETERS:
**
**	a_s  -  first equivalence-name list.
**	a_t  -  second equivalence-name list.
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
**	1 if a_s and a_t contain exactly the same set of names, 0 otherwise.
**
**--
**/
static int	    cmp_root_equ(a_s, a_t)
struct root_equ_names_tag *a_s,
			  *a_t;
{
    struct root_equ_names_tag *l_p;

    for (l_p = a_s; l_p != (struct root_equ_names_tag *)NULL;  l_p = l_p->next)
    {
        if (!s_equ_name_in_list(l_p->equ_name, a_t))
        {
            return 0;	    /**  Not found  **/
        }
    }

    for (l_p = a_t; l_p != (struct root_equ_names_tag *)NULL;  l_p = l_p->next)
    {
        if (!s_equ_name_in_list(l_p->equ_name, a_s))
        {
            return 0;	    /**  Not found  **/
        }
    }

    return 1;		/**  Same list  **/
}
