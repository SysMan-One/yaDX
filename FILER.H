/* Program Name            : FILER.H                       		*/
/*   Original Author       : C. K. Hung					*/
/*   Date                  : 20-MAY-1991                                */
/*   Program Description   : Main filer driver, windows, GOLD-key      */
/*                           keypad commands and menu navigation --     */
/*                           consolidated 14-Sep-2026 from FILER.H +    */
/*                           FILERWIN.H + FILERKPD.H.                   */
/*                         :                                            */ 
/* __MODULE__              : FILER_H                                    */
/* __IDENT__               : X-02-05ECO8                                */
/* __REV__                 : 02.05.08                                   */
/* Revision History follows                                             */
/*  28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header */


/* ************ MACRO DEFINITIONS ************  */

/**  Filer virtual display  **/
#define FILER_BOTTOM 	cntrl_info_block.dir_dx[cntrl_info_block.cur_win].filer_display.rows
#define FILER_RIGHTMOST	cntrl_info_block.dir_dx[cntrl_info_block.cur_win].filer_display.width
#define FILER_PBD_ROW				5
#define FILER_PBD_COLUMN			1
#define FILER_DIRECT_ROW			1
#define FILER_DIRECT_COLUMN			1
#define FILER_STAT_ROW				2
#define FILER_STAT_COLUMN			1
#define FILER_COLUMNS				4
#define FILER_COLUMN_WIDTH			cntrl_info_block.pasteboard_width/FILER_COLUMNS

/**  Long-form file name field width: whatever the terminal leaves after
**   the fixed part of the line -- the state char, and " DD-MMM-YYYY
**   HH:MM:SS SSSSS   (RWED,RWED,RWED,RWED)" (1+51 chars) -- but never
**   below the original 30 (so an 80-column terminal looks as before;
**   132 columns gives an 80-char name field, 154 -- 102).  **/
#define FILER_FIXED_FIELDS_WIDTH		52
#define FILER_NAME_WIDTH \
	((cntrl_info_block.pasteboard_width - FILER_FIXED_FIELDS_WIDTH > 30) ? \
	 (cntrl_info_block.pasteboard_width - FILER_FIXED_FIELDS_WIDTH) : 30)

/**  Header-line directory field width: the line is " <dir> | <Forward/
**   Backward> | <Single/Multiple> " -- 1+3+8+3+8+1 = 24 fixed chars, the
**   rest is the directory name; never below the original 56 (80-column
**   terminal).  **/
#define FILER_TITLE_FIXED_WIDTH			24
#define FILER_TITLE_DIR_WIDTH \
	((cntrl_info_block.pasteboard_width - FILER_TITLE_FIXED_WIDTH > 56) ? \
	 (cntrl_info_block.pasteboard_width - FILER_TITLE_FIXED_WIDTH) : 56)

#define FILER_PFS    				"1HELP   2DIRECT 3FIND/F 4FIND/N 5FIRST  6LAST   7INC/EX 8PAGE   9MODE   0CANCEL "

/**  FILER timeout				**/
#define FILER_TIMEOUT				10

#define CMDS_RSV_LNS				3
#define DIRINFO_RSV_LNS				3

/**  General commands				**/
#define GEN_CMDS_ROW				1
#define GEN_CMDS_COLUMN				1

/**  Clock position				**/
#define TIME_ROW				2


/* ************ FUNCTION PROTOTYPING ************  */

int		    DX_process_loop(void);
int		    put_general_cmds(menutp *);
int		    write_time(unsigned int);
int		    clear_time(void);
int		    put_filer_pfs(char *, unsigned int);
int		    compute_filer_begin_xy(struct fil_dx_tag *, int *,
			int *, unsigned short int);
int		    write_to_filer(struct fil_dx_tag *);
int		    filer_left(void);
int		    filer_right(void);
void		    format_filer_entry(struct fil_dx_tag *, char *, unsigned short int);
int		    setddir(char *, char *);
int		    highlight_filer_current_file(void);
int		    unhighlight_filer_current_file(void);
int		    put_filer_title(unsigned short int);
int		    put_filer_stat(unsigned short int);
int		    find_full_path_name(char *, char *, unsigned short int);
int		    convert_date_string();

    /* ---- (from FILERWIN.H) ---- */
    int		    filer_split_win(void);
    int		    filer_delete_win(void);
    int		    filer_one_win(void);
    int		    filer_next_win(void);
    int		    filer_prev_win(void);
    void	    filer_exitset(void);

    /* ---- (from FILERKPD.H) ---- */
    int		    change_filer_viewport(unsigned short int);
    int		    filer_top(void);
    int		    filer_bottom(void);
    int		    filer_page_down(void);
    int		    filer_page_up(void);
    int		    filer_up(void);
    int		    filer_down(void);
    int		    filer_find_first(void);
    int		    filer_find_first$1(char *, char *, char *);
    int		    filer_find_next(void);
    int		    invalidate_find_cache(void);
    int		    filer_mark(struct fil_dx_tag *, int);
    int		    filer_unmark(struct fil_dx_tag *, int);
    int             filer_execute(struct fil_dx_tag *);
    int		    filer_multiple(int (*)(), char *, char *);
