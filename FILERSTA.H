/*
**++
**  FACILITY:
**
**	FILERSTA.H
**
**  ABSTRACT:
**
**	File status/type/edit/print commands -- consolidated 14-Sep-2026
**	from FILERSTA.H + FILERTYP.H + FILEREDT.H + FILERPRT.H.
**
**  AUTHORS:
**
**      C. K. Hung
**
**
**  CREATION DATE:      16-MAY-1991
**
**  __MODULE__:	FILERSTA_H
**  __IDENT__:	X-02-05ECO8
**  __REV__:	02.05.08
**
**  MODIFICATION HISTORY:
**
**	28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header
**
**--
*/

    int		    filer_status(void);
    int		    filer_multiple_status(void);
    int             filer_single_status(void);
    int             filer_status$1(char *, char *, char *);
    void            getpro(unsigned short, char *, char *, char *, char *);
    int		    filestat(char *, struct fil_dx_tag *, char *);

    /* ---- (from FILERTYP.H) ---- */

/**  FILER_TYPE1 virtual display dimensions -- full terminal width (was a
**   fixed 80, which left the filer screen showing through on the right
**   of a wider terminal); an int lvalue, so "&FILER_TYPE1_RIGHTMOST"
**   still works as an SMG$ by-reference argument.  **/
#define FILER_TYPE1_RIGHTMOST			cntrl_info_block.pasteboard_width
#define FILER_TYPE1_PBD_ROW			1
#define FILER_TYPE1_PBD_COLUMN			1

/**  FILER_TYPE2 virtual display dimensions	**/
#define FILER_TYPE2_BOTTOM			2
#define FILER_TYPE2_RIGHTMOST			cntrl_info_block.pasteboard_width
#define FILER_TYPE2_PBD_COLUMN			1

    int		    filer_type(void);
    int		    filer_multiple_type(void);
    int		    filer_single_type(void);
    int		    filer_type$1(char *, char *, char *);
    int		    filer_type$2(char *, char *, char *, int (*)());

    /* ---- (from FILEREDT.H) ---- */
    int		    filer_edt(void);
    int		    filer_multiple_edt(void);
    int		    filer_single_edt(void);
    int		    filer_edt$1(char *, char *, char *);

    /* ---- (from FILERPRT.H) ---- */
    int		    filer_print(void);
    int		    filer_multiple_print(void);
    int		    filer_single_print(void);
    int		    filer_print$1(char *, char *, char *);
