/*
**++
**  FACILITY:
**
**	INQUIRE.H
**
**  ABSTRACT:
**
**	[@tbs@]...  Consolidated 13-Sep-2026 with DXSCR.H (shared SMG$
**	screen-management helpers) -- see INQUIRE.C for the merge.
**
**  AUTHORS:
**
**      C. K. Hung
**
**
**  CREATION DATE:      20-APR-1990
**
**  __MODULE__:	INQUIRE_H
**  __IDENT__:	X-02-05ECO8
**  __REV__:	02.05.08
**
**  MODIFICATION HISTORY:
**
**	28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header
**
**--
*/

/*
**
**  MACRO DEFINITIONS
**
**/

/**  USERINPUT virtual display dimensions -- full terminal width (was a
**   fixed 80); an int lvalue, so "&USERINPUT_RIGHTMOST" still works as an
**   SMG$ by-reference argument.  The input line's visible viewport grows
**   with it and always fits the FIELD_LENGTH+1 (256) read-area display.  **/
#define USERINPUT_BOTTOM		10
#define USERINPUT_RIGHTMOST		cntrl_info_block.pasteboard_width
#define USERINPUT_PASTEBOARD_COLUMN	1

/*
**  GLOBAL DECLARATIONS
**/

enum rings		    /**  Instruct SIGNAL_ERR() to ring a bell	     **/
    {
	bell, silence
    };

/*
**
**	FUNCTION PROTOTYPING
**
**/
    int		    get_userinput_and_execute(int (*)(), char *, char *, char *, char *, char *);
    int		    signal_err(char *, enum rings);
    int		    multi_get_userinput_and_exec(int (*)(), char *, char *, char *, char *);

    /* ---- (from DXSCR.H) ---- */
    unsigned int	dxscr_clock_display_id(void);
    unsigned int	dxscr_working_start(unsigned int, int, unsigned int, unsigned int, void (**)(int));
    int			dxscr_tick_clock(unsigned int);
    int			dxscr_dispatch_filemode(int (*)(), int (*)());
    int			dxscr_yes_or_no(char *);
    int			dxscr_confirm_yes(char *, char *, char *);
    int			dxscr_find_chan_for_did(unsigned short int *, unsigned short int *, char *, struct cur_filter_tag *);
    int			dxscr_count_win_dirs(struct cur_filter_tag *);
    int			dxscr_count_win_devices(struct cur_filter_tag *);
    int			dxscr_dir_open_elsewhere(unsigned short int *, int);
    char		*dxscr_vms_date_string(char *, DATE_TIME *);
    char		*dxscr_ljust(char *, char *, int);
    void		dxscr_pb_exit(unsigned long int);
    void		dxscr_ctrly_ast(void);
    int			dxscr_enable_ctrly_cleanup(void);
