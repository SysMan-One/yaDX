/* Program Name            : FILERSTA.C                                 */
/*   Original Author       :  C. K. Hung				*/
/*   Editor                :  StarLet Squad and Ruslan R. Laishev (RRL) */
/*   Date                  : 16-MAY-1991				*/
/*   Program Description   : File status/type/edit/print commands --    */
/*                           consolidated 14-Sep-2026 from FILERSTA.C + */
/*                           FILERTYP.C + FILEREDT.C + FILERPRT.C.      */
/* Revision History follows                                             */
/*  03-Jun-1999 JL usage of __FIB 					*/
/*  25-Sep-2026 RRL ECO2: __NEW_STARLET prototypes, types fixed (0 CC diags)  */
/*                        FIBDEF/SBKDEF typedefs; sys$qiow IOSB/P2-P6 casts  */
/*  26-Sep-2026 CLD ECO3: ident sync with DX.C (no code change)         */
/*  26-Sep-2026 CLD ECO5: Type: Home/End go to the first/last page      */
/*  21-Oct-2026 CLD ECO12: file characteristics: system <fh2def.h>   */
/*                        (FH2$M_*); DEC/Shell FCHDEF.H dropped          */
/*  21-Oct-2026 CLD ECO14: Status page painted in one display update     */

#define __MODULE__	"FILERSTA"
#define __IDENT__	"X-02-05ECO14"
#define __REV__		"02.05.14"
 
 
#define	__NEW_STARLET	1
#include	<starlet.h>
#include	<lib$routines.h>
#include	<str$routines.h>
#include	<atrdef.h>
#include	<fibdef.h>
#include	<libclidef.h>
#include	<sbkdef.h>
#include	<iodef.h>
#include	<rmsdef.h>
#include	<jbcmsgdef.h>
#include	<sjcdef.h>
#include	<fh2def.h>	/* FH2$M_* = the ATR$C_UCHAR bits; SYS$LIB_C.TLB, see DX_BUILD.COM */

#include "global.h"
#include "dx.h"
#include "filer.h"
#include "filersta.h"
#include "fileropt.h"
#include "inquire.h"

 
 
/*
**  MACRO DEFINITIONS
**/
 
/**  FILER_STATUS virtual display dimensions -- full terminal width, same
**   as FILER_TYPE1/2_RIGHTMOST (FILERSTA.H)  **/
#define FILER_STATUS_RIGHTMOST			cntrl_info_block.pasteboard_width
#define FILER_STATUS_PBD_ROW			1
#define FILER_STATUS_PBD_COLUMN			1
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_status() is the "Show" (file characteristics) command entry
**	point.  It dispatches to filer_multiple_status() or
**	filer_single_status() depending on whether the current window is
**	in single- or multiple-file mode.
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
**	DX__NORMAL (the value returned by whichever of the two dispatched
**	routines runs).
**
**--
**/
int	filer_status()
{
    return dxscr_dispatch_filemode(filer_multiple_status, filer_single_status);
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_multiple_status() shows the file-characteristics display, in
**	turn, for every marked file in the current window (multiple-file
**	mode), by handing filer_status$1() to filer_multiple() as the
**	per-file worker routine.
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
**	The current window's marked-file list, walked by filer_multiple().
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
int	filer_multiple_status()
{
    char l_dummy[] = "";
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error showing file characteristics");
    filer_multiple(
	filer_status$1,
	l_dummy,
	l_errmsg);
     return DX__NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_single_status() shows the file-characteristics display for
**	just the current file (single-file mode), refusing the parent-
**	directory placeholder entry "[-]" outright and reporting any
**	error from filer_status$1() through signal_err().
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
**	DX_CURRENT_FILE, cntrl_info_block.cur_win.
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
int	filer_single_status()
{
    char l_dummy[] = "";
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Show details of a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error showing file characteristics");
	if (filer_status$1(l_dummy, l_fn, l_errmsg) == -1)
	    signal_err(l_errmsg, bell);
    }
     return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_status$1() is the actual "Show" worker.  It reads the
**	target file's characteristics via a single ACP QIO
**	(IO$_ACCESS, using the FIB and the same ATR$C_xxx item list as
**	filestat() -- but gathering more fields than filestat() returns,
**	since this is for display rather than for the filer cache), then
**	paints a full-screen virtual display with file name, size, file
**	ID, owner UIC, protection, creation/revision/expiration/backup
**	dates, allocation/extension/global-buffer-count, version limit,
**	organization, record format, and record attributes.  It waits for
**	a keystroke before tearing the display down again.
**
**  INPUT PARAMETERS:
**
**	a_dummy     -  unused (kept only so this matches the standard
**		       (char *, char *, char *) callback signature used by
**		       filer_multiple()/get_userinput_and_execute()).
**	a_filespec  -  full file specification of the file to show.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message if the function
**		     returns -1 or DX__DONTCARE.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_FILE (for the FIB directory-id triple),
**	DX_CURRENT_DIRECTORY.cur_filter, cntrl_info_block (pasteboard
**	rows/id, keyboard id).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	-1 if the ACP QIO reports insufficient read access;
**	DX__DONTCARE if the user cancelled with CTRL/Y;
**	DX__NORMAL otherwise.
**
**--
**/
int	filer_status$1(a_dummy, a_filespec, a_errmsg)
char *a_dummy;
char *a_filespec;
char *a_errmsg;
{
    static char l_str[BUFSIZ+1];
    $DESCRIPTOR (l_str_descrip, l_str);
 
    /**  File Characteristics Bits  **/
    unsigned long l_fch;
 
    /**  Record Attributes Area  **/
    struct {
	unsigned char fat$b_rtype;
	unsigned char fat$b_rattrib;
	unsigned short int fat$w_rsize;
	struct {
	    unsigned short int fat$w_hiblkh;
	    unsigned short int fat$w_hiblkl;
	} fat$l_hiblk;
	struct {
	    unsigned short int fat$w_efblkh;
	    unsigned short int fat$w_efblkl;
	} fat$l_efblk;
	short int fat$w_ffbyte;
	unsigned char fat$b_bktsize;
	unsigned char fat$b_vfcsize;
	short int fat$w_maxrec;
	short int fat$w_defext;
	short int fat$w_gbc;
	short int spares1;
	int spares2;
	short int spares3;
	short int fat$w_versions;
    } l_raa;
 
    /**  Statistics Block  **/
    SBKDEF l_asb;
 
    /**  ASCII DATES  **/
    unsigned char l_adat[35];
 
    /*
    **  struct representing a 64-bit binary value expressing
    **  the date and time
    **/
 
    DATE_TIME l_cdt,
	      l_rdt,
	      l_edt,
	      l_bdt;
 
    /**  File Owner UIC  **/
    unsigned long int l_uic;
    char l_uicbuf[256];
    $DESCRIPTOR (l_uicbuf_descrip, l_uicbuf);
    unsigned short int l_uiclen;
    unsigned long int l_gic;
    char l_gicbuf[256];
    $DESCRIPTOR (l_gicbuf_descrip, l_gicbuf);
    unsigned short int l_giclen;
 
    /**  File Protection  **/
    unsigned short int l_fpro;
 
    /**  Access Control Block  **/
    struct {
	unsigned short w_size;
	unsigned short w_type;
	void *l_addr;
    } l_acb[] = {
	{  4, ATR$C_UCHAR, &l_fch },
	{ 32, ATR$C_RECATTR, &l_raa },
	{ 32, ATR$C_STATBLK, &l_asb },
	{ 35, ATR$C_ASCDATES, l_adat },
	{  8, ATR$C_CREDATE, &l_cdt },
	{  8, ATR$C_REVDATE, &l_rdt },
	{  8, ATR$C_EXPDATE, &l_edt },
	{  8, ATR$C_BAKDATE, &l_bdt },
	{  4, ATR$C_UIC, &l_uic },
	{  2, ATR$C_FPRO, &l_fpro },
	{  0, 0, NULL }
    };
 
    int l_iosb[2];
 
    static FIBDEF l_fib;
    $DESCRIPTOR(l_fib_descriptor, (char *)&l_fib);
 
    char l_filename[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descriptor, l_filename);
 
    char *l_cp;
    int l_i;
    unsigned short int l_rvn;
    unsigned int l_filer_status_display_id;
    unsigned short int l_word_terminator_code;
    int l_lineno;
    char l_lines[120][MAXFILESPEC+1];
    char l_linebuf[MAXFILESPEC+1];
    $DESCRIPTOR (l_linebuf_descriptor, l_linebuf);
    unsigned l_resultant_time;
    static unsigned short l_timlen;
    char l_timbuf[50];
    $DESCRIPTOR (l_timbuf_descriptor, l_timbuf);
    char l_forg[MAXFILESPEC+1];
    char l_frat[MAXFILESPEC+1];
    char l_frfm[MAXFILESPEC+1];
    char l_spro[5], l_opro[5], l_gpro[5], l_wpro[5];
    unsigned int l_old_mask;
    int l_prompt_row;
    unsigned short int l_chan;
    unsigned long l_rc;

    LENGTH(l_fib_descriptor) = sizeof (FIBDEF);
 
    for (l_i = 0; l_i <= 2; l_i++)
	__FIB(l_fib,fib$r_did_overlay,fib$w_did[l_i]) = DX_CURRENT_FILE->did[l_i];
 
    if ((l_cp = strchr(a_filespec, ']')) == NULL)
	strcpy(l_filename, a_filespec);
    else
	strcpy(l_filename, l_cp+1);
    LENGTH(l_filespec_descriptor) = strlen(l_filename);
 
    /*
    **	Find the I/O channel for this file
    **/
    if (!dxscr_find_chan_for_did(DX_CURRENT_FILE->did, &l_chan, (char *)NULL, &DX_CURRENT_DIRECTORY.cur_filter))
    {
	strcpy(a_errmsg, "Internal error - I/O channel for file not found");
	return DX__ERROR;
    }

    if (!(1 & (l_rc = sys$qiow (
		0,
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
    {
	/**  No READ access  **/
	strcpy(a_errmsg, "Insufficient privilege for read operation");
	return -1;
    } else
    {
	if (!(1 & (l_rc = smg$create_virtual_display (
		&cntrl_info_block.pasteboard_rows,
		&FILER_STATUS_RIGHTMOST,
		&l_filer_status_display_id,
		&SMG$M_TRUNC_ICON,
		0,
		0))))
	    $LOG(l_rc);
 
	if (!(1 & (l_rc = smg$paste_virtual_display (
		&l_filer_status_display_id,
		&cntrl_info_block.pasteboard_id,
		&FILER_STATUS_PBD_ROW,
		&FILER_STATUS_PBD_COLUMN,
		0))))
	    $LOG(l_rc);
 
	l_lineno = 0;

	/*
	**  Header info -- 14-Sep-2026 RRL: printf-family removed project-wide
	**  (see dxscr_vms_date_string()/dxscr_ljust() in INQUIRE.C); most of
	**  the labels below are compile-time-known literals, so their
	**  "%23s"/"%26s" right-justified padding is simply baked into the
	**  SYS$FAO control string (or, where nothing numeric needs
	**  formatting at all, a plain strcpy()/strcat() literal -- neither
	**  is a printf-family call).
	**/
	{
	    static $DESCRIPTOR (l_fao_ctrl, "File name: !AZ");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_lines[l_lineno];
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_filespec))))
		$LOG(l_rc);
	    l_lines[l_lineno++][l_len] = EOS;
	}
	strcpy(l_lines[l_lineno++], "");

	/*
	**  # of files & total size
	**/
	{
	    static $DESCRIPTOR (l_fao_ctrl, "                   Size : !UL/!UL blocks");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_lines[l_lineno];
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			(unsigned long) (l_raa.fat$l_efblk.fat$w_efblkl+(l_raa.fat$l_efblk.fat$w_efblkh << 16)),
			(unsigned long) (l_asb.sbk$w_filesizl+(l_asb.sbk$w_filesizh << 16))))))
		$LOG(l_rc);
	    l_lines[l_lineno++][l_len] = EOS;
	}

	/*
	**  File ID
	**/
	{
	    static $DESCRIPTOR (l_fao_ctrl, "                File ID : (!UL,!UL,!UL)");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_lines[l_lineno];
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			(unsigned long) __FIB(l_fib,fib$r_fid_overlay,fib$w_fid[0]),
			(unsigned long) __FIB(l_fib,fib$r_fid_overlay,fib$w_fid[1]),
			(unsigned long) __FIB(l_fib,fib$r_fid_overlay,fib$w_fid[2])))))
		$LOG(l_rc);
	    l_lines[l_lineno++][l_len] = EOS;
	}

	/*
	**  Owner UIC -- no numeric formatting needed, l_gicbuf/l_uicbuf are
	**  already fully-formatted ASCII text from sys$idtoasc(); plain
	**  string concatenation is enough (matches the existing strcat()
	**  just below it).
	**/

	strcpy(l_lines[l_lineno], "              Owner UIC : [");
	l_gic = l_uic | 0xFFFF;
	if (sys$idtoasc(l_gic, &l_giclen, &l_gicbuf_descrip, 0, 0, 0) & 1)
	{
	    l_gicbuf[l_giclen] = EOS;
	    strcat(l_lines[l_lineno], l_gicbuf);
	    strcat(l_lines[l_lineno], ",");
	}
	if (sys$idtoasc(l_uic, &l_uiclen, &l_uicbuf_descrip, 0, 0, 0) & 1)
	{
	    l_uicbuf[l_uiclen] = EOS;
	    strcat(l_lines[l_lineno], l_uicbuf);
	}
	strcat(l_lines[l_lineno], "]");
	l_lineno++;

	/*
	**  File protection
	**/
	getpro(l_fpro, l_spro, l_opro, l_gpro, l_wpro);
	{
	    static $DESCRIPTOR (l_fao_ctrl,
		"        File protection : System:!AZ, Owner:!AZ, Group:!AZ, World:!AZ");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_lines[l_lineno];
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			l_spro, l_opro, l_gpro, l_wpro))))
		$LOG(l_rc);
	    l_lines[l_lineno++][l_len] = EOS;
	}

	/*
	**  Time, time, time, ...  -- l_timbuf is already fully-formatted
	**  ASCII text from sys$asctim(), so "Created"/"Expires"/"Backup"
	**  (no further numeric substitution) are plain literal-label +
	**  strcat(); only "Revised" needs a numeric field (the revision
	**  number), so that one alone uses SYS$FAO.
	**/
	if (!(1 & (l_rc = sys$asctim (
		    &l_timlen,
		    &l_timbuf_descriptor,
		    (struct _generic_64 *) &l_cdt,
		    0))))
	    $LOG(l_rc);
	l_timbuf[ l_timlen ] = EOS;
	strcpy(l_lines[l_lineno], "                Created : ");
	strcat(l_lines[l_lineno++], l_timbuf);

	if (!(1 & (l_rc = sys$asctim (
		    &l_timlen,
		    &l_timbuf_descriptor,
		    (struct _generic_64 *) &l_rdt,
		    0))))
	    $LOG(l_rc);
	l_timbuf[ l_timlen ] = EOS;
	l_rvn = l_adat[0] + (l_adat[1] << 8);
	{
	    static $DESCRIPTOR (l_fao_ctrl, "                Revised : !AZ (!UL)");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_lines[l_lineno];
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			l_timbuf, (unsigned long) l_rvn))))
		$LOG(l_rc);
	    l_lines[l_lineno++][l_len] = EOS;
	}

	if (!(1 & (l_rc = sys$asctim (
		    &l_timlen,
		    &l_timbuf_descriptor,
		    (struct _generic_64 *) &l_edt,
		    0))))
	    $LOG(l_rc);
	l_timbuf[ l_timlen ] = EOS;
	if (!strncmp(l_timbuf, "17-NOV-1858", 11))
	    strcpy(l_timbuf, "<None specified>");
	strcpy(l_lines[l_lineno], "                Expires : ");
	strcat(l_lines[l_lineno++], l_timbuf);

	if (!(1 & (l_rc = sys$asctim (
		    &l_timlen,
		    &l_timbuf_descriptor,
		    (struct _generic_64 *) &l_bdt,
		    0))))
	    $LOG(l_rc);
	l_timbuf[ l_timlen ] = EOS;
	if (!strncmp(l_timbuf, "17-NOV-1858", 11))
	    strcpy(l_timbuf, "<No backup recorded>");
	strcpy(l_lines[l_lineno], "                 Backup : ");
	strcat(l_lines[l_lineno++], l_timbuf);

	/*
	**  File attributes
	**/
	{
	    static $DESCRIPTOR (l_fao_ctrl,
		"        File attributes : Allocation:!UL, Extension:!UL, Global buffer count:!UL");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = l_lines[l_lineno];
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			(unsigned long) (l_asb.sbk$w_filesizl+(l_asb.sbk$w_filesizh << 16)),
			(unsigned long) l_raa.fat$w_defext,
			(unsigned long) l_raa.fat$w_gbc))))
		$LOG(l_rc);
	    l_lines[l_lineno++][l_len] = EOS;
	}

	if (l_fch & FH2$M_DIRECTORY)
	{
	    /**  Directory file  **/
	    if (l_raa.fat$w_versions != 0)
	    {
		static $DESCRIPTOR (l_fao_ctrl,
		    "                          Default version limit: !UL");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_len;
		unsigned long l_rc;

		l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_lines[l_lineno];
		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			    (unsigned long) l_raa.fat$w_versions))))
		    $LOG(l_rc);
		l_lines[l_lineno][l_len] = EOS;
	    }
	    else
		strcpy(l_lines[l_lineno], "                          No version limit");
	    strcat(l_lines[l_lineno], ", Directory file");
	}
	else
	{
	    /**  For non-directory file  **/
	    if (l_fib.fib$w_verlimit == 32767)
		strcpy(l_lines[l_lineno], "                          No version limit");
	    else
	    {
		static $DESCRIPTOR (l_fao_ctrl,
		    "                          Version limit: !UL");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_len;
		unsigned long l_rc;

		l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_lines[l_lineno];
		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			    (unsigned long) l_fib.fib$w_verlimit))))
		    $LOG(l_rc);
		l_lines[l_lineno][l_len] = EOS;
	    }
	}
	if (l_fch & FH2$M_CONTIG)
	    strcat(l_lines[l_lineno], ", Contiguous");
	else if (l_fch & FH2$M_CONTIGB)
	    strcat(l_lines[l_lineno], ", Contiguous best try");
	l_lineno++;

	/*
	**  File organization -- l_forg is already a plain literal word,
	**  no numeric substitution needed.
	**/
	switch (l_raa.fat$b_rtype & 0xF0)
	{
	    case FAB$C_SEQ: strcpy(l_forg, "Sequential");  break;
	    case FAB$C_REL: strcpy(l_forg, "Relative");  break;
	    case FAB$C_IDX: strcpy(l_forg, "Indexed"); break;
	    case FAB$C_HSH: strcpy(l_forg, "Hashed"); break;
	    default:
		;
	}
	strcpy(l_lines[l_lineno], "      File Organization : ");
	strcat(l_lines[l_lineno++], l_forg);

	/*
	**  Record format -- only FIX/VAR/VFC need a numeric field; the
	**  rest are plain literal text.
	**/
	switch (l_raa.fat$b_rtype & 0xF)
	{
	    case FAB$C_UDF: strcpy(l_lines[l_lineno++], "          Record Format : Undefined");
			    break;
	    case FAB$C_FIX:
	    {
		static $DESCRIPTOR (l_fao_ctrl,
		    "          Record Format : Fixed length, !UL byte records");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_len;
		unsigned long l_rc;

		l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_lines[l_lineno];
		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			    (unsigned long) l_raa.fat$w_rsize))))
		    $LOG(l_rc);
		l_lines[l_lineno++][l_len] = EOS;
	    }
			    break;
	    case FAB$C_VAR:
	    {
		static $DESCRIPTOR (l_fao_ctrl,
		    "          Record Format : Variable length, maximum !UL bytes");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_len;
		unsigned long l_rc;

		l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_lines[l_lineno];
		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			    (unsigned long) l_raa.fat$w_rsize))))
		    $LOG(l_rc);
		l_lines[l_lineno++][l_len] = EOS;
	    }
			    break;
	    case FAB$C_VFC:
	    {
		static $DESCRIPTOR (l_fao_ctrl,
		    "          Record Format : VFC, maximum !UL bytes");
		struct dsc$descriptor_s l_out_desc;
		unsigned short int l_len;
		unsigned long l_rc;

		l_out_desc.dsc$w_length  = sizeof(l_lines[0]) - 1;
		l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		l_out_desc.dsc$a_pointer = l_lines[l_lineno];
		if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
			    (unsigned long) max(l_raa.fat$w_maxrec, l_raa.fat$w_rsize)))))
		    $LOG(l_rc);
		l_lines[l_lineno++][l_len] = EOS;
	    }
			    break;
	    case FAB$C_STM: strcpy(l_lines[l_lineno++], "          Record Format : RMS-11 stream");
			    break;
	    case FAB$C_STMLF: strcpy(l_lines[l_lineno++], "          Record Format : Stream_LF");
			    break;
	    case FAB$C_STMCR: strcpy(l_lines[l_lineno++], "          Record Format : Stream_CR");
			    break;
	    default: strcpy(l_lines[l_lineno++], "          Record Format : None");
		     break;
	}

	/*
	**  Record attributes -- l_frat is already a plain literal word,
	**  no numeric substitution needed.
	**/
	switch (l_raa.fat$b_rattrib)
	{
	    case FAB$M_FTN: strcpy(l_frat, "FORTRAN carriage control character");  break;
	    case FAB$M_CR: strcpy(l_frat, "Carriage return carriage control"); break;
	    case FAB$M_PRN: strcpy(l_frat, "Print file carriage control"); break;
	    case FAB$M_BLK: strcpy(l_frat, "No carriage control, Non-spanned"); break;
	    default: strcpy(l_frat, "None"); break;
	}
	strcpy(l_lines[l_lineno], "      Record Attributes : ");
	strcat(l_lines[l_lineno++], l_frat);
    }
 
    /*  21-Oct-2026 CLD: the status lines go out in one display update
    **  (content writes only: the display is already pasted above).  **/
    if (!(1 & (l_rc = smg$begin_display_update (
	      &l_filer_status_display_id))))
        $LOG(l_rc);
    for (l_i = 1; l_i <= l_lineno; l_i++)
    {
	strcpy(l_linebuf, l_lines[l_i-1]);
	LENGTH(l_linebuf_descriptor) = strlen(l_linebuf);
	if (!(1 & (l_rc = smg$put_chars (
			    &l_filer_status_display_id,
			    &l_linebuf_descriptor,
			    &l_i,
			    &1,
			    0,
			    0,
			    0,
			    0))))
	    $LOG(l_rc);
    }
    if (!(1 & (l_rc = smg$end_display_update (
	      &l_filer_status_display_id))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$set_cursor_abs (
	      &l_filer_status_display_id,
	      &cntrl_info_block.pasteboard_rows,
	      &1))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = lib$disable_ctrl (
	      &(unsigned int) LIB$M_CLI_CTRLY,
	      &l_old_mask))))
        $LOG(l_rc);
 
    smg$read_keystroke (
                 &cntrl_info_block.keyboard_id,
                 &l_word_terminator_code,
                 $DESCR (
	     "Press RETURN to continue -or- CTRL/Y to cancel multiple show"),
                 0,
                 &l_filer_status_display_id,
                 &SMG$M_REVERSE,
                 0);
 
    if (!(1 & (l_rc = lib$enable_ctrl (
	      &l_old_mask,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$erase_display (
	      &l_filer_status_display_id,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$delete_virtual_display (
	      &l_filer_status_display_id))))
        $LOG(l_rc);
 
    if (l_word_terminator_code != SMG$K_TRM_CTRLY)
	return DX__NORMAL;
    else
    {
	strcpy(a_errmsg, "ANALYZE canceled by request");
	return DX__DONTCARE;
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	getpro() decodes a packed VMS file-protection code (the bit
**	layout used by XAB$W_PRO/FIB$W_FILE_PROT/l_fpro throughout this
**	file: 4 bits each for System/Owner/Group/World, each a mask of
**	R/W/E/D with a CLEAR bit meaning the right IS granted) into four
**	separate "RWED"-style strings, one per protection class.
**
**  INPUT PARAMETERS:
**
**	a_pcode  -  the packed 16-bit protection code.
**
**  OUTPUT PARAMETERS:
**
**	a_spro  -  filled in with the System class's letters (subset of
**		   "RWED", EOS-terminated).
**	a_opro  -  same, for the Owner class.
**	a_gpro  -  same, for the Group class.
**	a_wpro  -  same, for the World class.
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
**	None (void function).
**
**--
**/
void	    getpro(
		unsigned short a_pcode,
		char *a_spro,char *a_opro,char *a_gpro,char *a_wpro)
{

    /**  SYSTEM PROTECTION  **/
    if (!(a_pcode & (1 << 0)))		*a_spro++ = 'R';
    if (!(a_pcode & (1 << 1)))		*a_spro++ = 'W';
    if (!(a_pcode & (1 << 2)))		*a_spro++ = 'E';
    if (!(a_pcode & (1 << 3)))		*a_spro++ = 'D';
    *a_spro = EOS;

    /**  OWNER PROTECTION  **/
    if (!(a_pcode & (1 << 4)))		*a_opro++ = 'R';
    if (!(a_pcode & (1 << 5)))		*a_opro++ = 'W';
    if (!(a_pcode & (1 << 6)))		*a_opro++ = 'E';
    if (!(a_pcode & (1 << 7)))		*a_opro++ = 'D';
    *a_opro = EOS;

    /**  GROUP PROTECTION  **/
    if (!(a_pcode & (1 << 8)))		*a_gpro++ = 'R';
    if (!(a_pcode & (1 << 9)))		*a_gpro++ = 'W';
    if (!(a_pcode & (1 << 10)))		*a_gpro++ = 'E';
    if (!(a_pcode & (1 << 11)))		*a_gpro++ = 'D';
    *a_gpro = EOS;

    /**  WORLD PROTECTION  **/
    if (!(a_pcode & (1 << 12)))		*a_wpro++ = 'R';
    if (!(a_pcode & (1 << 13)))		*a_wpro++ = 'W';
    if (!(a_pcode & (1 << 14)))		*a_wpro++ = 'E';
    if (!(a_pcode & (1 << 15)))		*a_wpro++ = 'D';
    *a_wpro = EOS;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filestat() is the project's general-purpose "get one file's
**	characteristics via RMS" primitive: it opens a_fn with an XABPRO/
**	XABDAT extension block chained on, immediately closes it again
**	(the open/close pair is only there to make RMS resolve the name
**	and populate the XABs), and copies the resulting name, file ID,
**	directory ID, allocated size, protection, and the four RMS dates
**	into *a_e.  It is a general utility used across the project
**	(the filer cache update path, several FILERxxx.C command files,
**	FILERCPY.C's copy worker, etc.) -- originally part of the now-
**	removed Encode/Decode feature (FILERENC.C), relocated here when
**	that feature was deleted since it had 7 unrelated callers.
**
**  INPUT PARAMETERS:
**
**	a_fn  -  file specification to look up (default directory is the
**		 process's current working directory).
**
**  OUTPUT PARAMETERS:
**
**	a_e  -  filled in with the file's resultant name, fid, did,
**		filesize, fpro, and the four dates (cdat/rdat/edat/bdat).
**		On an open failure, only fid[] (set to 0xffff) and fpro
**		(0xffff) are filled in -- the rest of *a_e is left
**		untouched.
**	a_errmsg  -  filled in with an error message if the open fails.
**
**  IMPLICIT INPUTS:
**
**	None (the current default directory is used implicitly by RMS,
**	but that is standard RMS behavior, not a DX global).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__ERROR if sys$open() fails; DX__NORMAL otherwise.
**
**--
**/
int	    filestat(a_fn, a_e, a_errmsg)
char *a_fn;
struct fil_dx_tag *a_e;
char *a_errmsg;
{
    struct NAM l_nam;
    struct FAB l_fab;
    struct XABPRO l_xabpro;
    struct XABDAT l_xabdat;
    int l_i;
    char l_resultant_name[NAM$C_MAXRSS];
    unsigned int l_status;

    /*
    **	Init RMS$ data structures
    **/

    l_fab = cc$rms_fab;
    l_nam = cc$rms_nam;
    l_xabpro = cc$rms_xabpro;
    l_xabdat = cc$rms_xabdat;

    /*
    **	use RMS service to get the full file spec and
    **	characteristics.  Default directory is cwd.
    **
    **/

    l_fab.fab$l_fna = a_fn;
    l_fab.fab$b_fns = strlen(a_fn);
    l_fab.fab$l_nam = &l_nam;
    l_fab.fab$l_xab = &l_xabpro;
    l_xabpro.xab$l_nxt = &l_xabdat;

    l_nam.nam$l_rsa = l_resultant_name;
    l_nam.nam$b_rss = sizeof l_resultant_name;

    if (!((l_status = sys$open (
			&l_fab,
			0,
			0)) & 1))
    {	    /**  Open failed  **/
	a_e->fid[0] = a_e->fid[1] = a_e->fid[2] = 0Xffff;
	a_e->fpro = 0Xffff;
	{
	    static $DESCRIPTOR (l_fao_ctrl, "Open file !AZ failed");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = MAXFILESPEC;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = a_errmsg;
	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_fn))))
		$LOG(l_rc);
	    a_errmsg[l_len] = EOS;
	}
	return DX__ERROR;
    }

    l_status = sys$close(
		&l_fab,
		0,
		0);

    /**  File name  **/
    memcpy(a_e->fn, l_nam.nam$l_name, l_nam.nam$b_name+l_nam.nam$b_type+l_nam.nam$b_ver);
    a_e->fn[l_nam.nam$b_name+l_nam.nam$b_type+l_nam.nam$b_ver] = EOS;

    /**  File id and directory id  **/
    for (l_i = 0; l_i <= 2; l_i++)
    {
	a_e->fid[l_i] = l_nam.nam$w_fid[l_i];
	a_e->did[l_i] = l_nam.nam$w_did[l_i];
    }

    /**  File size  **/
    a_e->filesize = l_fab.fab$l_alq;

    /**  File protection  **/
    a_e->fpro = l_xabpro.xab$w_pro;

    /**  Creation date  **/
    a_e->cdat = l_xabdat.xab$q_cdt;

    /**  Revision date  **/
    a_e->rdat = l_xabdat.xab$q_rdt;

    /**  Expiration date  **/
    a_e->edat = l_xabdat.xab$q_edt;

    /**  Backup date  **/
    a_e->bdat = l_xabdat.xab$q_bdt;

    return DX__NORMAL;
}



/* ---- (from FILERTYP.C -- file type/view) ---- */

#define MAXLINES 65535
#define MAXBUFSIZ 65535



static int      type_no_page(struct FAB, unsigned int, unsigned int,
			     unsigned short int *, char *, char *);
static int      type_by_page(struct FAB, unsigned int, unsigned int,
			     unsigned short int *, char *, char *);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_type() is the "Type" (view file contents) command entry
**	point; dispatches to filer_multiple_type() or filer_single_type()
**	depending on the current window's file-selection mode.
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
**	DX__NORMAL (the value returned by whichever of the two dispatched
**	routines runs).
**
**--
**/
int	filer_type()
{
    return dxscr_dispatch_filemode(filer_multiple_type, filer_single_type);
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_multiple_type() types (views) every marked file in the
**	current window, in turn, via filer_multiple() with filer_type$1()
**	as the per-file worker.
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
**	The current window's marked-file list, walked by filer_multiple().
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
int	filer_multiple_type()
{
    char l_dummy[] = "";
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error typing file");
    filer_multiple(
	filer_type$1,
	l_dummy,
	l_errmsg);
    return DX__NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_single_type() types (views) just the current file, refusing
**	the parent-directory placeholder entry "[-]" outright and
**	reporting any error from filer_type$1() through signal_err().
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
**	DX_CURRENT_FILE, cntrl_info_block.cur_win.
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
int	filer_single_type()
{
    char l_dummy[] = "";
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Type a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error typing file");
	if (filer_type$1(l_dummy, l_fn, l_errmsg) == -1)
	    signal_err(l_errmsg, bell);
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_type$1() is the "Type" worker callback handed to
**	get_userinput_and_execute()/filer_multiple().  It just forwards to
**	filer_type$2(), picking type_by_page() or type_no_page() as the
**	actual display routine depending on the user's TYPE BY PAGE setting.
**
**  INPUT PARAMETERS:
**
**	a_dummy     -  unused (kept only to match the standard
**		       (char *, char *, char *) callback signature).
**	a_filespec  -  full file specification of the file to type.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message on failure.
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
**	Whatever filer_type$2() returns (DX__NORMAL/DX__ERROR/DX__DONTCARE).
**
**--
**/
int	filer_type$1(a_dummy, a_filespec, a_errmsg)
char *a_dummy;
char *a_filespec;
char *a_errmsg;
{
    return filer_type$2 (
		a_filespec,
		a_errmsg,
		a_dummy,
		(cntrl_info_block.user_pref.type_by_page?
		    type_by_page : type_no_page));
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_type$2() opens a_filespec via RMS, creates the two virtual
**	displays used by the file viewer (a scrolling text area sized to
**	FILER_TYPE1_RIGHTMOST/(pasteboard rows-2), and a one-line prompt/
**	status area below it), disables CTRL/Y while the viewer is active,
**	then hands off the actual line-reading/paging work to a_func (either
**	type_no_page() or type_by_page(), chosen by the caller).  Cleans up
**	both displays and closes the file again before returning.
**
**  INPUT PARAMETERS:
**
**	a_filespec  -  full file specification of the file to type.
**	a_option    -  passed through unchanged to a_func (unused by
**		       filer_type$2() itself).
**	a_func      -  the paging/non-paging display routine to run
**		       (type_by_page or type_no_page).
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message on failure.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block (pasteboard rows/id).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__ERROR if the file could not be opened/closed or a_func failed;
**	DX__DONTCARE if the user cancelled with CTRL/Y; DX__NORMAL otherwise.
**
**--
**/
int	filer_type$2(a_filespec, a_errmsg, a_option, a_func)
char *a_filespec;
char *a_errmsg;
char *a_option;
int (*a_func)();
{
    unsigned int l_filer_type1_display_id;
    unsigned int l_filer_type2_display_id;
 
    struct FAB l_from_fab;
 
    int l_status;
    unsigned short l_len;
    unsigned int l_retcode;
    unsigned short int l_word_terminator_code = 0;
    char l_errmsg1[MAXFILESPEC+1];
    $DESCRIPTOR (l_errmsg1_descrip, l_errmsg1);
    unsigned int l_old_mask;
    int l_filer_type1_bottom = cntrl_info_block.pasteboard_rows-2;
    unsigned long l_rc;
 
    /*
    **	Initialize Source File FAB
    **/
    l_from_fab = cc$rms_fab;
    l_from_fab.fab$l_fna = a_filespec;
    l_from_fab.fab$b_fns = strlen(a_filespec);
 
    /*
    **	Start outputing ...
    **/
    if (!((l_retcode = sys$open (
			&l_from_fab,
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
 
    if (!(1 & (l_rc = smg$create_virtual_display (
	      &l_filer_type1_bottom,
	      &FILER_TYPE1_RIGHTMOST,
	      &l_filer_type1_display_id,
	      &SMG$M_TRUNC_ICON,
	      0,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$create_virtual_display (
	      &FILER_TYPE2_BOTTOM,
	      &FILER_TYPE2_RIGHTMOST,
	      &l_filer_type2_display_id,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = lib$disable_ctrl (
	      &(unsigned int) LIB$M_CLI_CTRLY,
	      &l_old_mask))))
        $LOG(l_rc);
 
    l_status = (*a_func)(l_from_fab,
		     l_filer_type1_display_id,
		     l_filer_type2_display_id,
		     &l_word_terminator_code,
		     a_errmsg,
		     a_option);
 
    if (!(1 & (l_rc = lib$enable_ctrl (
	      &l_old_mask,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$erase_display (
	      &l_filer_type1_display_id,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$delete_virtual_display (
	      &l_filer_type1_display_id))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$erase_display (
	      &l_filer_type2_display_id,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$delete_virtual_display (
	      &l_filer_type2_display_id))))
        $LOG(l_rc);
 
    if (!((l_retcode = sys$close (
			&l_from_fab,
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
 
    if (l_status == DX__ERROR)
    {
	return DX__ERROR;
    }
    else if (l_word_terminator_code == SMG$K_TRM_CTRLY)
    {
	strcpy(a_errmsg, "TYPE canceled by request");
	return DX__DONTCARE;
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	type_no_page() streams a_from_fab's whole content into the "type
**	area" virtual display, one RMS record per smg$put_line(), with no
**	scrolling/paging -- used when the user's TYPE BY PAGE preference is
**	off.  Pastes both viewer displays up, reads records until RMS$_EOF
**	(printing a literal "[EOF] " line at end of file), then prompts and
**	waits for a keystroke before returning (the caller tears the
**	displays down again).
**
**  INPUT PARAMETERS:
**
**	a_from_fab                 -  already-opened FAB for the file to type.
**	a_filer_type1_display_id   -  virtual display id of the scrolling
**				      text area.
**	a_filer_type2_display_id   -  virtual display id of the one-line
**				      prompt/status area.
**	a_dummy                    -  unused (kept only to match the shared
**				      (FAB, id, id, code *, char *, char *)
**				      a_func signature used by filer_type$2()).
**
**  OUTPUT PARAMETERS:
**
**	a_word_terminator_code  -  the keystroke that ended the wait.
**	a_errmsg                -  filled in with an error message on failure.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block (pasteboard id, keyboard id).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__ERROR if an RMS $CONNECT/$GET failed for a reason other than
**	end of file; DX__NORMAL otherwise.
**
**--
**/
static int	type_no_page(a_from_fab,
			     a_filer_type1_display_id,
			     a_filer_type2_display_id,
			     a_word_terminator_code,
			     a_errmsg,
			     a_dummy)
struct FAB a_from_fab;
unsigned int a_filer_type1_display_id,
	      a_filer_type2_display_id;
unsigned short int *a_word_terminator_code;
char *a_errmsg;
char *a_dummy;
{
    char l_linebuf[MAXBUFSIZ+1];
    $DESCRIPTOR (l_linebuf_descrip, l_linebuf);
 
    struct RAB l_from_rab;
 
    char l_errmsg1[MAXFILESPEC+1];
    $DESCRIPTOR (l_errmsg1_descrip, l_errmsg1);
    unsigned short int l_len;
    unsigned int l_retcode;
    int l_filer_type2_pasteboard_row = cntrl_info_block.pasteboard_rows-1;
    unsigned long l_rc;

    /*
    **	Initialize Source File RAB
    **/
 
    l_from_rab = cc$rms_rab;
    l_from_rab.rab$l_fab = &a_from_fab;
    l_from_rab.rab$b_rac = RAB$C_SEQ;
    l_from_rab.rab$l_ubf = l_linebuf;
    l_from_rab.rab$w_usz = MAXBUFSIZ;
 
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
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
	      &a_filer_type1_display_id,
	      &cntrl_info_block.pasteboard_id,
	      &FILER_TYPE1_PBD_ROW,
	      &FILER_TYPE1_PBD_COLUMN,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
	      &a_filer_type2_display_id,
	      &cntrl_info_block.pasteboard_id,
	      &l_filer_type2_pasteboard_row,
	      &FILER_TYPE2_PBD_COLUMN,
	      0))))
        $LOG(l_rc);
 
    while (l_retcode != RMS$_EOF)
    {
	l_retcode = sys$get (
		      &l_from_rab,
		      0,
		      0);
	if (l_retcode == RMS$_EOF)
	{
	    strcpy(l_linebuf, "[EOF] ");
	    l_from_rab.rab$w_rsz = strlen("[EOF] ");
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
	    return DX__ERROR;
	}
	LENGTH(l_linebuf_descrip) = l_from_rab.rab$w_rsz;
	smg$put_line (
		&a_filer_type1_display_id,
		&l_linebuf_descrip,
		0,
		0,
		0,
		0,
		0,
		0);
    }
    smg$put_chars (
	&a_filer_type2_display_id,
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
	&a_filer_type2_display_id,
	0,
	0);
    if (!(1 & (l_rc = smg$erase_line (
		  &a_filer_type2_display_id,
		  &FILER_TYPE2_BOTTOM,
		  &1))))
        $LOG(l_rc);
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	type_by_page() is the paged counterpart of type_no_page(), used
**	when the user's TYPE BY PAGE preference is on.  It reads the first
**	screenful of records from a_from_fab, keeping every line read so
**	far as a separately lib$get_vm()-allocated string in l_lines[] (up
**	to MAXLINES), then lets the user scroll with UP/DOWN (or CR, which
**	behaves like DOWN): UP re-displays already-buffered lines from
**	l_lines[], DOWN either shows more already-buffered lines or reads
**	further records from the file and buffers those too.  CTRL/W
**	repaints the screen.  The loop ends on CTRL/Y, CTRL/C, keypad-0, or
**	CTRL/Z, at which point every buffered line is freed (the "free_vm:"
**	label, also used as the error exit) before returning.
**
**  INPUT PARAMETERS:
**
**	a_from_fab                 -  already-opened FAB for the file to type.
**	a_filer_type1_display_id   -  virtual display id of the scrolling
**				      text area.
**	a_filer_type2_display_id   -  virtual display id of the one-line
**				      prompt/status area.
**	a_dummy                    -  unused (kept only to match the shared
**				      a_func signature used by filer_type$2()).
**
**  OUTPUT PARAMETERS:
**
**	a_word_terminator_code  -  the keystroke that ended the loop.
**	a_errmsg                -  filled in with an error message on failure.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block (pasteboard id, keyboard id).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__ERROR if an RMS $CONNECT/$GET failed for a reason other than
**	end of file; DX__NORMAL otherwise (including normal EOF).
**
**--
**/
static int	type_by_page(a_from_fab,
			     a_filer_type1_display_id,
			     a_filer_type2_display_id,
			     a_word_terminator_code,
			     a_errmsg,
			     a_dummy)
struct FAB a_from_fab;
unsigned int a_filer_type1_display_id,
	      a_filer_type2_display_id;
unsigned short int *a_word_terminator_code;
char *a_errmsg;
char *a_dummy;
{
    char l_linebuf[MAXBUFSIZ+1];
    $DESCRIPTOR (l_linebuf_descrip, l_linebuf);
 
    struct RAB l_from_rab;
 
    int l_filer_type1_bottom = cntrl_info_block.pasteboard_rows-2;
    int l_filer_type2_pasteboard_row = cntrl_info_block.pasteboard_rows-1;
    unsigned int l_old_mask;
    int l_lastline, l_linepos;
    char l_errmsg1[MAXFILESPEC+1];
    $DESCRIPTOR (l_errmsg1_descrip, l_errmsg1);
    unsigned short int l_len;
    unsigned int l_retcode;
    char *l_base_address;
    char *l_lines[MAXLINES];
    int l_i;
    int l_allocate_size;
    unsigned long l_rc;
    int l_to_edge = 0;		/* Home/End: repeat the page step to the edge */

    /*
    **	Initialize Source File RAB
    **/
 
    l_from_rab = cc$rms_rab;
    l_from_rab.rab$l_fab = &a_from_fab;
    l_from_rab.rab$b_rac = RAB$C_SEQ;
    l_from_rab.rab$l_ubf = l_linebuf;
    l_from_rab.rab$w_usz = MAXBUFSIZ;
 
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
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
	      &a_filer_type1_display_id,
	      &cntrl_info_block.pasteboard_id,
	      &FILER_TYPE1_PBD_ROW,
	      &FILER_TYPE1_PBD_COLUMN,
	      0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
	      &a_filer_type2_display_id,
	      &cntrl_info_block.pasteboard_id,
	      &l_filer_type2_pasteboard_row,
	      &FILER_TYPE2_PBD_COLUMN,
	      0))))
        $LOG(l_rc);
 
    l_lastline = 0;
 
    /*
    **	Display the first page and wait for user response
    **/
 
    /*  25-Sep-2026 RRL: batch the whole first page (one smg$put_line()
    **  per record) and, below, each UP/DOWN page move (one
    **  smg$insert_line() per line -- rows-1 of them per keystroke) into a
    **  single physical-terminal flush: unbatched, every line went out as
    **  its own scroll+write, so paging visibly crawled line by line.
    **  Only content writes to the already-pasted type1 display are inside
    **  the batch (see the smg$ begin/end_update rule in FILER.C).  **/
    if (!(1 & (l_rc = smg$begin_display_update (&a_filer_type1_display_id))))
        $LOG(l_rc);
    for (l_i = 1;  l_retcode != RMS$_EOF && l_i <= l_filer_type1_bottom;  l_i++)
    {
	l_retcode = sys$get (
		      &l_from_rab,
		      0,
		      0);
	if (l_retcode == RMS$_EOF)
	{
	    strcpy(l_linebuf, "[EOF] ");
	    l_from_rab.rab$w_rsz = strlen("[EOF] ");
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
	    smg$end_display_update (&a_filer_type1_display_id);
	    goto free_vm;
	}
	LENGTH(l_linebuf_descrip) = l_from_rab.rab$w_rsz;
	smg$put_line (
		&a_filer_type1_display_id,
		&l_linebuf_descrip,
		0,
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
	    $LOG(l_rc);
	strncpy(l_base_address, l_linebuf, l_from_rab.rab$w_rsz);
	l_base_address[l_allocate_size-1] = EOS;
	if (l_lastline < MAXLINES)
	    l_lines[l_lastline++] = l_base_address;
	else
	    lib$free_vm (&l_allocate_size, &l_base_address, 0);
    }
    if (!(1 & (l_rc = smg$end_display_update (&a_filer_type1_display_id))))
        $LOG(l_rc);
 
    smg$put_chars (
	&a_filer_type2_display_id,
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
	&a_filer_type2_display_id,
	0,
	0);
    if (!(1 & (l_rc = smg$erase_line (
		  &a_filer_type2_display_id,
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
	    **  step until the first line / the end of the file.
	    **/
	    case SMG$K_TRM_E1:
		 l_to_edge = 1;
	    case SMG$K_TRM_UP :
		 if (l_linepos == 1)
		 {
		     if (!(1 & (l_rc = smg$ring_bell (
			          &a_filer_type1_display_id,
			          0))))
		         $LOG(l_rc);
		 }
		 else
		 {
		     if (!(1 & (l_rc = smg$begin_display_update (&a_filer_type1_display_id))))
		         $LOG(l_rc);
		     do
		     for (l_i = l_filer_type1_bottom;  l_i > 1 && l_linepos > 1;  l_i--)
		     {
			l_linepos--;
			strcpy(l_linebuf, l_lines[l_linepos-1]);
			LENGTH(l_linebuf_descrip) = strlen(l_linebuf);
			smg$insert_line (
				 &a_filer_type1_display_id,
				 &1,
				 &l_linebuf_descrip,
				 &SMG$M_DOWN,
				 0,
				 0,
				 0,
				 0);
		     }
		     while (l_to_edge && l_linepos > 1);
		     if (!(1 & (l_rc = smg$end_display_update (&a_filer_type1_display_id))))
		         $LOG(l_rc);
		 }
		 l_to_edge = 0;
		 break;
	    case SMG$K_TRM_E4:
		 l_to_edge = 1;
	    case SMG$K_TRM_DOWN:
	    case SMG$K_TRM_CR :	
		if (l_linepos+l_filer_type1_bottom-1 >= l_lastline &&
		     l_retcode == RMS$_EOF)
		{
		    if (!(1 & (l_rc = smg$ring_bell (
				    &a_filer_type1_display_id,
				    0))))
		        $LOG(l_rc);
		}
		else
		{
		    if (!(1 & (l_rc = smg$begin_display_update (&a_filer_type1_display_id))))
		        $LOG(l_rc);
		    do
		    for (l_i = l_filer_type1_bottom;  l_i > 1;  l_i--)
		    {
			if (l_linepos+l_filer_type1_bottom-1 < l_lastline)
			{
			    l_linepos++;
			    strcpy(
				l_linebuf,
				l_lines[l_linepos+l_filer_type1_bottom-2]);
			    LENGTH(l_linebuf_descrip) = strlen(l_linebuf);
			    smg$insert_line (
				     &a_filer_type1_display_id,
				     &l_filer_type1_bottom,
				     &l_linebuf_descrip,
				     &SMG$M_UP,
				     0,
				     0,
				     0,
				     0);
			}
			else if (l_retcode != RMS$_EOF)
			{
			    l_retcode = sys$get (
					&l_from_rab,
					0,
					0);
			    if (l_retcode == RMS$_EOF)
			    {
			       strcpy(l_linebuf, "[EOF] ");
			       l_from_rab.rab$w_rsz = strlen("[EOF] ");
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
				 smg$end_display_update (&a_filer_type1_display_id);
				 goto free_vm;
			    }
			    LENGTH(l_linebuf_descrip) = l_from_rab.rab$w_rsz;
			    l_linepos++;
			    smg$insert_line (
				      &a_filer_type1_display_id,
				      &l_filer_type1_bottom,
				      &l_linebuf_descrip,
				      &SMG$M_UP,
				      0,
				      0,
				      0,
				      0);
			    l_allocate_size = (int) l_from_rab.rab$w_rsz+1;
			    if (!(1 & (l_rc = lib$get_vm (
					    &l_allocate_size,
					    &l_base_address,
					    0))))
			        $LOG(l_rc);
			    memcpy(l_base_address, l_linebuf, l_from_rab.rab$w_rsz);
			    l_base_address[l_allocate_size-1] = EOS;
			    if (l_lastline < MAXLINES)
				l_lines[l_lastline++] = l_base_address;
			    else
				lib$free_vm (&l_allocate_size, &l_base_address, 0);
			}
		    }
		    while (l_to_edge && !(l_linepos+l_filer_type1_bottom-1 >= l_lastline && l_retcode == RMS$_EOF));
		    if (!(1 & (l_rc = smg$end_display_update (&a_filer_type1_display_id))))
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
				  &a_filer_type1_display_id,
				  0))))
		        $LOG(l_rc);
		}
		break;
    }
    if (!(1 & (l_rc = smg$put_chars (
		   &a_filer_type2_display_id,
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
	 &a_filer_type2_display_id,
	 0,
	 0);
    if (!(1 & (l_rc = smg$erase_line (
		   &a_filer_type2_display_id,
		   &FILER_TYPE2_BOTTOM,
		   &1))))
        $LOG(l_rc);
    }
 
free_vm:
    for (l_i = 0;  l_i < l_lastline;  l_i++)
    {
	l_allocate_size = strlen(l_lines[l_i])+1;
    	if (!(1 & (l_rc = lib$free_vm (
		     &l_allocate_size,
		     &l_lines[l_i],
		     0))))
    	    $LOG(l_rc);
    }
 
    return (l_retcode == RMS$_NORMAL || l_retcode == RMS$_EOF?
		DX__NORMAL : DX__ERROR);
}



/* ---- (from FILEREDT.C -- file edit) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_edt() is the "Edit" command entry point.  It dispatches to
**	filer_multiple_edt() or filer_single_edt() depending on whether the
**	current window is in single- or multiple-file mode.
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
**	Whatever the dispatched routine returns (always DX__NORMAL for both).
**
**--
**/
int	filer_edt()
{
    return dxscr_dispatch_filemode(filer_multiple_edt, filer_single_edt);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_multiple_edt() runs the editor, in turn, on every marked file
**	in the current window (multiple-file mode), via filer_multiple()
**	with filer_edt$1() as the per-file worker.  Disables broadcast-
**	message trapping around the whole batch (since the editor itself
**	takes over the screen) and re-enables it afterward; refreshes the
**	filer display if AUTO UPDATE is on.
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
**	The current window's marked-file list, walked by filer_multiple();
**	cntrl_info_block.user_pref.auto_update.
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.pasteboard_id's broadcast-trapping state (disabled
**	then re-enabled).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	filer_multiple_edt()
{
    char l_dummy[] = "";
    char l_errmsg[MAXFILESPEC+1];
    unsigned long l_rc;

    if (!(1 & (l_rc = smg$disable_broadcast_trapping(&cntrl_info_block.pasteboard_id))))
	$LOG(l_rc);
    filer_multiple(
	filer_edt$1,
	l_dummy,
	l_errmsg);
    if (!(1 & (l_rc = smg$set_broadcast_trapping(&cntrl_info_block.pasteboard_id, broadcast_routine, 0))))
	$LOG(l_rc);
    if (cntrl_info_block.user_pref.auto_update)
	filer_update();
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_single_edt() runs the editor on just the current file
**	(single-file mode), refusing the parent-directory placeholder entry
**	"[-]" outright and reporting any error from filer_edt$1() through
**	signal_err().  Disables broadcast-message trapping around the edit
**	session and re-enables it afterward; refreshes the filer display if
**	AUTO UPDATE is on.
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
**	DX_CURRENT_FILE, cntrl_info_block.cur_win,
**	cntrl_info_block.user_pref.auto_update.
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.pasteboard_id's broadcast-trapping state (disabled
**	then re-enabled).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	filer_single_edt()
{
    char l_dummy[] = "";
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];
    unsigned long l_rc;

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
	signal_err("Edit a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	if (!(1 & (l_rc = smg$disable_broadcast_trapping(&cntrl_info_block.pasteboard_id))))
	    $LOG(l_rc);
	strcpy(l_errmsg, "Error editing file");
	if (filer_edt$1(l_dummy, l_fn, l_errmsg) == -1)
	    signal_err(l_errmsg, bell);
	if (!(1 & (l_rc = smg$set_broadcast_trapping(&cntrl_info_block.pasteboard_id, broadcast_routine, 0))))
	    $LOG(l_rc);
	if (cntrl_info_block.user_pref.auto_update)
	    filer_update();
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_edt$1() is the actual editor launcher.  It checks read access
**	to a_fn first, saves the physical screen and erases the pasteboard
**	(the editor takes over the whole screen), then invokes either EDT
**	(edt$edit(), with /NOJOURNAL and /NOCOMMAND) or TPU (tpu$edit())
**	depending on the user's editor preference, and finally restores the
**	saved physical screen.
**
**  INPUT PARAMETERS:
**
**	a_dummy  -  unused (kept only to match the standard
**		    (char *, char *, char *) callback signature).
**	a_fn     -  full file specification of the file to edit.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message if access() fails.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.pasteboard_id, cntrl_info_block.user_pref.editor.
**
**  IMPLICIT OUTPUTS:
**
**	The physical screen (saved and restored around the editor session).
**
**  RETURNS VALUE:
**
**	DX__ERROR if access(a_fn, 4) fails (no read access); DX__NORMAL
**	otherwise (the editor's own success/failure is only logged via
**	$LOG(), not surfaced through the return value).
**
**--
**/
int	filer_edt$1(a_dummy, a_fn, a_errmsg)
char *a_dummy;
char *a_fn;
char *a_errmsg;
{
    unsigned int l_display_id;
    $DESCRIPTOR (l_in_file, a_fn);
    $DESCRIPTOR (l_out_file, "");
    long _align( LONGWORD ) l_options;
    unsigned long l_rc;

    extern edt$edit();

    /*
    **	Check if file access is allowed
    **/

    if (access(a_fn, 4) == EOF)
    {
	/**  No READ access  **/
	strcpy(a_errmsg, "Insufficient privilege for read operation");
	return DX__ERROR;
    }

    if (!(1 & (l_rc = smg$save_physical_screen(&cntrl_info_block.pasteboard_id, &l_display_id, 0, 0))))
	$LOG(l_rc);

    if (!(1 & (l_rc = smg$erase_pasteboard(&cntrl_info_block.pasteboard_id))))
	$LOG(l_rc);

    LENGTH(l_in_file) = strlen(a_fn);
    if (cntrl_info_block.user_pref.editor == DX_EDITOR_EDT) {
	l_options = 0x14;	    /**  /NOJOURNAL and /NOCOMMAND are used  **/
	edt$edit(
	    &l_in_file,
	    0,
	    0,
	    0,
	    &l_options,
	    0,
	    0,
	    0);

    } else {
        if (!(1 & (l_rc = tpu$edit(&l_in_file, &l_out_file))))
	    $LOG(l_rc);
    }

    if (!(1 & (l_rc = smg$restore_physical_screen(&cntrl_info_block.pasteboard_id, &l_display_id))))
	$LOG(l_rc);

    return DX__NORMAL;
}



/* ---- (from FILERPRT.C -- file print) ---- */

#define DEFAULT_PRINTER				"SYS$PRINT"



/*
**
**  INTERNAL FUNCTION PROTOTYPING
**
**/
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_print() is the "Print" command entry point.  It dispatches to
**	filer_multiple_print() or filer_single_print() depending on whether
**	the current window is in single- or multiple-file mode.
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
**	Whatever the dispatched routine returns (always DX__NORMAL for both).
**
**--
**/
int	filer_print()
{
    return dxscr_dispatch_filemode(filer_multiple_print, filer_single_print);
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_multiple_print() prompts once for a target print queue, then
**	sends every marked file in the current window to it (multiple-file
**	mode), via multi_get_userinput_and_exec() with filer_print$1() as
**	the per-file worker.
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
**	The current window's marked-file list, walked by
**	multi_get_userinput_and_exec().
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
int	filer_multiple_print()
{
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error printing file");
    multi_get_userinput_and_exec(
        filer_print$1,
        "Multiple Print",
        "Print to queue: ",
        "",
        l_errmsg);
     return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_single_print() prompts for a target print queue (defaulting
**	to DEFAULT_PRINTER) and sends just the current file (single-file
**	mode), refusing the parent-directory placeholder entry "[-]"
**	outright.
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
**	DX_CURRENT_FILE, cntrl_info_block.cur_win.
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
int	filer_single_print()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Print a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error printing file");
	get_userinput_and_execute(
	    filer_print$1,
	    "Print File",
	    "Ouput to queue: ",
	    DEFAULT_PRINTER,
	    l_errmsg,
	    l_fn);
    }
     return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	filer_print$1() is the actual print-submission worker.  It builds a
**	SJC$_ENTER_FILE item list (file spec, queue name) and submits it via
**	sys$sndjbcw() to the job controller, then reports success/failure to
**	the user via signal_err().
**
**  INPUT PARAMETERS:
**
**	a_queue_name  -  name of the print queue to submit to.
**	a_filespec    -  full file specification of the file to print.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message if the job-controller
**		     request fails.
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
**	DX__ERROR if sys$sndjbcw() reports other than JBC$_NORMAL;
**	DX__NORMAL otherwise.
**
**--
**/
int	filer_print$1(a_queue_name, a_filespec, a_errmsg)
char *a_queue_name;
char *a_filespec;
char *a_errmsg;
{
    unsigned long l_entry_number;
    struct item_list_3
    {
	unsigned short buf_len;
	unsigned short item_code;
	void *buf_addr;
	unsigned short *ret_len;
    } l_itmlst[] =
    {
	{ strlen(a_filespec), SJC$_FILE_SPECIFICATION, a_filespec, 0 },
	{ strlen(a_queue_name), SJC$_QUEUE, a_queue_name, 0 },
	{ sizeof (unsigned long), SJC$_ENTRY_NUMBER_OUTPUT, &l_entry_number, 0 },
	{ 0, 0, 0, 0 }
    };

    unsigned int l_iosb[2];
    char l_msg[512];

    sys$sndjbcw (
	     0,
	     SJC$_ENTER_FILE,
	     0,
	     &l_itmlst,
	     (struct _iosb *) l_iosb,
	     0,
	     0);

    if (l_iosb[0] != JBC$_NORMAL)
    {
	strcpy(a_errmsg, "Insufficient privilege for print operation");
	return DX__ERROR;
    }

    {
	static $DESCRIPTOR (l_fao_ctrl, "!AZ has been sent to \"!AZ\"");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;
	unsigned long l_rc;

	l_out_desc.dsc$w_length  = sizeof(l_msg) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_msg;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_filespec, a_queue_name))))
	    $LOG(l_rc);
	l_msg[l_len] = EOS;
    }
    signal_err(l_msg, silence);

    return DX__NORMAL;
}
