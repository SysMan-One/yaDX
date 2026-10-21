/*
**++
**  FACILITY:
**
**	FILERCPY.H
**
**  ABSTRACT:
**
**	File copy/delete/rename/purge/append/compare commands --
**	consolidated 14-Sep-2026 from FILERCPY.H + FILERDEL.H + FILERREN.H +
**	FILERPUR.H + FILERAPP.H + FILERDIF.H.
**
**  AUTHORS:
**
**      C. K. Hung
**
**
**  CREATION DATE:      21-MAY-1991
**
**  __MODULE__:	FILERCPY_H
**  __IDENT__:	X-02-05ECO8
**  __REV__:	02.05.08
**
**  MODIFICATION HISTORY:
**
**	28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header
**
**--
*/

    int		    filer_copy(void);
    int		    filer_multiple_copy(void);
    int		    filer_single_copy(void);
    int		    filer_copy$1(char *, char *, char *);
    int		    add_to_filer(struct fil_dx_tag, char *, unsigned short int);

    /* ---- (from FILERDEL.H) ---- */
    int		    filer_delete(void);
    int		    filer_multiple_delete(void);
    int		    filer_single_delete(void);
    int		    filer_delete$1(char *, char *, char *);
    int		    filer_delete$2(char *, char *, unsigned short int);
    int		    recompute_filer_beg_xy(struct fil_dx_tag *, int, int, unsigned short);

    /* ---- (from FILERREN.H) ---- */
    int		    filer_rename(void);
    int		    filer_multiple_rename(void);
    int		    filer_single_rename(void);
    int		    filer_rename$1(char *, char *, char *);

    /* ---- (from FILERPUR.H) ---- */
    int		    filer_purge(void);
    int		    filer_multiple_purge(void);
    int		    filer_single_purge(void);
    int		    filer_purge$1(char *, char *, char *);

    /* ---- (from FILERAPP.H) ---- */
    int		    filer_append(void);
    int		    filer_multiple_append(void);
    int		    filer_single_append(void);
    int		    filer_append$1(char *, char *, char *);
    int		    change_to_filer(struct fil_dx_tag, char *, unsigned short);

    /* ---- (from FILERDIF.H) ---- */
    int		    filer_diff(void);
    int		    filer_multiple_diff(void);
    int		    filer_single_diff(void);
    int		    filer_diff$1(char *, char *, char *);
