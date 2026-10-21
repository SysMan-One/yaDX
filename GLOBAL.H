/* Program Name            : GLOBAL.H                               	*/
/*   Original Author       : C. K. Hung					*/
/*   Date                  : 7-12-89					*/
/*   Program Description   :                                            */
/*                         :                                            */
/* __MODULE__              : GLOBAL_H                                   */
/* __IDENT__               : X-02-05ECO8                                */
/* __REV__                 : 02.05.08                                   */
/* Revision History follows                                             */
/*	19Mar90(CKH): Add enum type rings.				*/
/*	05Apr91(CKH): Changes for upgrading to DX II			*/
/*	03-Jun-1999 : JL cleaning, some include were missing.		*/
/*	29-MAR-2007	RRL	Some changes for IA64			*/
/*  28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header */
 
 
 
#include <ctype.h>
#include <descrip.h>
#include <rms.h>
#include <smgdef.h>
#include <smg$routines.h>
#include <lib$routines.h>
#include <ssdef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unixio.h>
#include "dx_msg.h"
 
 
#if defined __ALPHA || defined __ia64
#include <builtins.h>
#include <tpu$routines.h>
#else
#pragma builtins
#endif
 
 
 
/* **  MACRO DEFINITIONS ** */
#define HELPLIB			"DX$HOME:"	  		/**  DX defaults		**/
#define DX$HELPLIB		"DX$HELPLIB"	 		/**  DX help (full path logical)**/
#define DEFAULT_STARTUP		"SYS$LOGIN:DX$STARTUP.DAT"	/**  DX default startup file    **/
 
 
#define MAXFILESPEC		255		/**  Max file spec (all cap) **/
#define DX_MAX_PB_WIDTH		MAXFILESPEC	/**  Pasteboard width cap:   **/
						/**  every full-line buffer  **/
						/**  is MAXFILESPEC+1 wide   **/
 
#define MAXVMSNODE		63		/**  Max NODE in chars       **/
#define MAXVMSDEVICE		63		/**  Max DEVICE in chars     **/
#define MAXVMSDIRECTORY		39		/**  Max DIRECTORY in chars  **/
#define MAXVMSFILENAME		39		/**  Max FILENAME in chars   **/
#define MAXVMSFILETYPE		39		/**  Max FILETYPE in chars   **/
#define MAXVMSFILEVERSION	5		/**  Max VERSION in chars    **/
 
#define MAXVMSFULLNAME		(MAXVMSFILENAME+MAXVMSFILETYPE+MAXVMSFILEVERSION)
 
#define EOS			'\0'		/**  End of string symbol    **/
 
#define DX__DONTCARE		0		/**  Don't care - do nothing **/
#define DX__NORMAL		1		/**  Success return status   **/
#define DX__ERROR		-1		/**  Generic error	     **/
 
#define MAXFILEITEMS		4096		/**  Max. files in a direct  **/
 
 
#define MAXCOMMANDSITEMNUM	20		/**  Max. no. of options     **/
#define MAXCOMMANDSITEMLENGTH	40		/**  Max. length of options  **/
#define MENU_SEPARATOR		"-----"		/**  Menu separator	     **/
 
#define MAXWINDOWS		10		/**  Max number of directory **/
 
 
#define min(n1, n2)		((int)n1 < (int)n2? n1 : n2)
#define max(n1, n2)		((int)n1 > (int)n2? n1 : n2)
 
static unsigned long int ss_status;		/**  RTL condition value    **/
#define check_OK(func)				/**  RTL function checker   **/\
    {									       \
	ss_status = func;						       \
	if ( !(ss_status & 1) )						       \
	{								       \
	    fprintf(stderr, "\033[2J");		/**  ANSI erase screen	    **/\
	    lib$signal(DXM__SFC,2,__LINE__,__FILE__);                          \
	    exit(ss_status);		        /**  Exit!		    **/\
	}								       \
    }

/**  $LOG(status) -- same DXM__SFC report as check_OK's failure path, but
**   does NOT exit(): evaluates to "status", so "return $LOG(l_rc);" logs
**   the failure and returns it to the caller instead of crashing the
**   whole program.  For functions where an RTL/system-service failure
**   is not fatal to DX as a whole (the caller can recover or the failure
**   is expected/handled) -- check_OK stays the right choice wherever a
**   failure genuinely should be unconditionally fatal.
**/
#define $LOG(status)		(lib$signal(DXM__SFC,2,__LINE__,__FILE__), (status))

/**  SMG$ key codes vs. <ctype.h>: SMG$READ_KEYSTROKE returns plain
**   characters as 0..255 but every function/keypad/cursor key as a code
**   above 255 (SMG$K_TRM_DOWN = 275, SMG$K_TRM_KP0 = 260, ...).  DEC C's
**   _toupper(c) is (toupper)(c), and toupper()/isalnum() are undefined
**   outside 0..255 -- on a key code they return garbage, which made the
**   pull-down menus reject every arrow key and KP0 (26-Sep-2026 RRL).
**   Only ever pass a key code through these guards.  **/
#define DX_KEY_ISALNUM(k)	(((k) < 256) && isalnum(k))
#define DX_KEY_TOUPPER(k)	(((k) < 256) ? _toupper(k) : (k))

/**  Literal descriptor  **/
static	struct	dsc$descriptor_s _temp_descr_1 =
{
	0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0
};
 
#define LENGTH(descriptor)	descriptor.dsc$w_length
#define POINTER(descriptor)	descriptor.dsc$a_pointer
#define $DESCR(literal)							       \
    (LENGTH (_temp_descr_1) = sizeof literal - 1, POINTER (_temp_descr_1) = literal, &_temp_descr_1)
 
 
 
/* ****  GLOBAL DECLARATIONS *** */
 
typedef struct menutype		    /**  Menu structure				     **/
{
	char *itmstr;				/**  Menu item		     **/
	long int start_row, start_col;		/**  Start row and column    **/
	char *item[MAXCOMMANDSITEMNUM];		/**  Sub-menu items	     **/
	int (*op[MAXCOMMANDSITEMNUM])();	/**  Sub-menu functions	     **/
} menutp;
 
 
 
struct display_tag		    /**  SMG$ virtual display structure	     **/
{
	unsigned int id;			/**  Display id		     **/
	int rows, width;			/**  Size of the display and **/
	int beg_y, beg_x;			/**  Upper-left position     **/
	int view_rows, view_width;		/**  Size of the viewprt and **/
	int view_beg_y, view_beg_x;		/**  Upper-left position     **/
};
 
#if defined __ALPHA || defined __ia64
typedef  unsigned __int64 DATE_TIME;
#define  __FIB(a,b,c) a.c
#else
typedef struct { unsigned : 32; unsigned: 32; } DATE_TIME;
#define  __FIB(a,b,c) a.b.c
#endif
 
enum forms					/**  Display format	     **/
{
	short_form, long_form
};
 
enum sortbys					/**  Sort by                 **/
{
	sortname, sorttype, sortsize, sortmark,
	sortcredate, sortrevdate, sortexpdate, sortbakdate
};
 
enum sortorders					/**  Sort order              **/
{
	ascending, descending
};
 
enum filemodes					/**  File mode (single/multi)**/
{
	single, multiple
};
 
enum directions					/**  Forward or backward     **/
{
	 advance, backup
};
 
struct w_dids_tag		    /**  Data struct used by directory	     **/
{
	char equivalent_name[MAXFILESPEC+1];	/**  Equivalent name	     **/
	unsigned short int did[3];		/**  File id		     **/
	unsigned short int chan;		/**  I/O channel	     **/
	struct w_dids_tag *next;		/**  Next item in list       **/
};
 
struct filespec_list_tag	    /**  Data struct used by filespec list   **/
{
	char name[MAXFILESPEC+1];		/**  Name (may be a logical) **/
	struct w_dids_tag *w_dids;		/**  Directories in the name **/
	struct filespec_list_tag *next;		/**  Next item in list	     **/
};
 
struct cur_filter_tag		    /**  All about file filters		     **/
{
	struct filespec_list_tag
		    *name_filespec_list,	/**  Filter name	     **/
		    *exclude_filespec_list;     /**  Filter exclude	     **/
	short int date;				/**  Date (CREATION,MODIFIED **/
						/**    EXPIRED, or BACKUP)   **/
	DATE_TIME before,			/**  Filter before	     **/
		  since;			/**  Filter after	     **/
	unsigned int maximum,			/**  Filter less than	     **/
		     minimum;			/**  Filter larger than	     **/
};
 
struct fil_dx_tag		    /**  Information about a file	     **/
{
	unsigned short int fid[3],		/**  VMS file-ID	     **/
			   did[3];		/**  Directory ID	     **/
	char fn[MAXFILESPEC+1];			/**  File spec		     **/
	long int filesize;			/**  File size		     **/
	DATE_TIME cdat,				/**  Creation date	     **/
		  rdat,				/**  Revision date	     **/
		  edat,				/**  Expire date	     **/
		  bdat;				/**  Backup date	     **/
	unsigned short int fpro;		/**  File protection	     **/
	enum					/**  File state		     **/
	    {
		waiting, ready, executed
	    } state;				/**  File status	     **/
	int beg_y, beg_x;			/**  File display pos	     **/
	struct fil_dx_tag *forward,		/**  Forward and	     **/
			  *backward;		/**  Backward links	     **/
};
 
struct dir_dx_tag		    /**  Information about a directory	     **/
{
	struct display_tag dir_info_display,	/**  Status display 	     **/
			   filer_display;	/**  Directory display 	     **/
	enum forms form;			/**  Display format	     **/
	enum sortbys sortby;                    /**  Sort by                 **/
	enum sortorders	sortorder;              /**  Sort order              **/
	enum filemodes filemode;                /**  File mode (single/multi)**/
	enum directions	direction;		/**  Forward or backward     **/
	char cur_dir[MAXFILESPEC+1];		/**  Directory name	     **/
	char par_dir[MAXFILESPEC+1];		/**  Parent directory name   **/
	char search_pattern[MAXFILESPEC+1];	/**  Search pattern	     **/
	long int tot_files,			/**  Total files	     **/
		 tot_blocks;			/**  Total blocks	     **/
	long int sel_files,			/**  Files selected	     **/
		 sel_blocks;			/**  Blocks selected	     **/
	unsigned short int dpro;		/**  Directory protection    **/
	unsigned short exitn;			/**  Exit immediatly	     **/
	short int subdir;			/**  # of subdir	     **/
	struct cur_filter_tag cur_filter;       /**  Filter		     **/
	struct fil_dx_tag *dir_filelist,	/**  Directory files	     **/
			  *non_dir_filelist,    /**  Non-directory files     **/
			  *exclude_filelist,    /**  Exclude files	     **/
			  *cur_file;		/**  Current file pointer    **/
};
 
struct user_pref_tag		    /**  Data struct used by user preference **/
{
	unsigned use_short_form: 1;		/**  Display short form?     **/
	short int editor;			/**  Current editor used     **/
#define		  DX_EDITOR_EDT		1	/**    Use EDT		     **/
#define		  DX_EDITOR_TPU		2	/**    Use TPU		     **/
#define		  DX_EDITOR_LSE		3	/**    Use LSE		     **/
	short int display_date;			/**  Date for display	     **/
#define		  DX_CREDATE		1	/**    Use creation date     **/
#define		  DX_REVDATE		2	/**    Use revision date     **/
#define		  DX_EXPDATE		3	/**    Use expiration date   **/
#define		  DX_BAKDATE		4	/**    Use backup date	     **/
	unsigned type_by_page: 1;		/**  Pause at page?	     **/
	unsigned delete_confirm: 1;		/**  Confirm before del?     **/
	unsigned purge_confirm: 1;		/**  Confirm before purg?    **/
	unsigned auto_update: 1;		/**  Update direct auto?     **/
	unsigned display_clock: 1;		/**  Display clock?	     **/
	unsigned update_in_second: 1;           /**  Update clock in second  **/
};
 
struct cntrl_info_block_tag	    /**  Default setting and user pref	     **/
{
	unsigned int pasteboard_id;	/**  Pasteboard id	     **/
	int pasteboard_rows,		/**  Pasteboard rows	     **/
	         pasteboard_width;		/**  Pasteboard width	     **/
	unsigned int keyboard_id;		/**  Keyboard id	     **/
	struct display_tag commands_display,	/**  Commands display 	     **/
			   status_display;	/**  Status display 	     **/
	unsigned short windows;			/**  Number of windows	     **/
	unsigned short cur_win;			/**  Current window	     **/
	struct dir_dx_tag dir_dx[MAXWINDOWS];	/**  Directory structure     **/
	char original_cwd[MAXFILESPEC+1];	/**  Dir before entering DX  **/
	struct user_pref_tag user_pref;		/**  User preference settings**/
} cntrl_info_block;
 
#define DX_CURRENT_DIRECTORY	cntrl_info_block.dir_dx[cntrl_info_block.cur_win]
#define DX_CURRENT_FILE		DX_CURRENT_DIRECTORY.cur_file
 
 
 
 
/* ************ FUNCTION PROTOTYPING ************  */
 
unsigned short int	navigator(struct menutype *, int *, int, int);
unsigned short		get_menu_choice(struct menutype, int *);
int			create_submenu(struct display_tag, char **);
int			select_from_submenu(struct display_tag, menutp,
					int *, char *, unsigned short *);
int			highlight_general_cmd(struct menutype, unsigned int *);
int			unhighlight_general_cmd(struct menutype, unsigned int *);
 
 
 
 
