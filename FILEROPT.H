/* Program Name            : FILEROPT.H                                 */
/*   Original Author       : C. K. Hung					*/
/*   Date                  : 3-AUG-1990					*/
/*   Program Description   : Execution and settings commands --        */
/*                           consolidated 14-Sep-2026 from FILEROPT.H + */
/*                           FILEREXE.H + FILERLAU.H + FILERPRO.H +     */
/*                           FILERFND.H + FILERCUS.H.                   */
/*                         :                                            */ 
/* __MODULE__              : FILEROPT_H                                 */
/* __IDENT__               : X-02-05ECO8                                */
/* __REV__                 : 02.05.08                                   */
/* Revision History follows                                             */
/*  28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header */

/* ****	MACRO DEFINITIONS *** */
#define DXVERSION "DX Version 2.5 03-Jun-1999, by C.K. Hung & J. Lauret"


/* ****	FUNCTION PROTOTYPING *** */

int	filer_update(void);
int	filer_help(char *);
int     filer_write(void);
int     print_filer_display(char *, char *, char *);
int     filer_goto(void);
int     filer_goto$1(char *, char *, char *);
int     filer_mark_all(void);
int     filer_unmark_all(void);
int     filer_remark_all(void);
int	filer_short_form(void);
int	filer_long_form(void);
int	get_quota(unsigned long);
int	filer_about(void);
int     filer_mkdir(void);
int     filer_mkdir$1(char *, char *, char *);
int	filer_show_buffer(void);

    /* ---- (from FILEREXE.H) ---- */
    int		    filer_vms_gateway(void);
    int		    filer_vms_command(void);
    int		    vms_command(char *, char *, char *);
    int		    vms_gateway(void);

    /* ---- (from FILERLAU.H) ---- */
    int		    filer_launch(void);
    int		    filer_multiple_launch(void);
    int		    filer_single_launch(void);
    int		    filer_launch$1(char *, char *, char *);

    /* ---- (from FILERPRO.H) ---- */
    int		    filer_protection(void);
    int		    filer_multiple_protection(void);
    int		    filer_single_protection(void);
    int		    filer_protection$1(char *, char *, char *);
    int		    chprot(char *, char *, char *, unsigned short *);
    int		    getprotbits(char *, unsigned short *, unsigned short *);

    /* ---- (from FILERFND.H) ---- */
    int		    filer_find(void);
    int		    filer_multiple_find(void);
    int		    filer_single_find(void);
    int		    filer_find$1(char *, char *, char *);

    /* ---- (from FILERCUS.H) ---- */
int		    custom_editor(void);
int		    select_a_editor(char *, char *, char *);

int		    custom_display_date(void);
int		    select_a_display_date(char *, char *, char *);

int		    custom_delete(void);
int		    confirm_delete(char *, char *, char *);

int		    custom_purge(void);
int		    confirm_purge(char *, char *, char *);

int		    custom_type(void);
int		    select_type_method(char *, char *, char *);

int		    custom_term_rows(void);
int		    set_term_rows(char *, char *, char *);

int		    custom_auto_update(void);
int		    auto_update(char *, char *, char *);

int		    custom_display_clock(void);
int		    display_clock(char *, char *, char *);

int		    custom_update_in_second(void);
int		    update_in_second(char *, char *, char *);

int		    custom_save_setting(void);
int		    save_setting(char *, char *, char *);

int		    custom_restore_setting(void);
int		    restore_setting(char *, char *, char *);

int		    custom_use_default(void);
int		    use_default(char *, char *, char *);
