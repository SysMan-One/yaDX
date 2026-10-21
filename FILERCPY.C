/* Program Name            : FILERCPY.C                                 */
/*   Original Author       :  C. K. Hung				*/
/*   Editor                :  StarLet Squad and Ruslan R. Laishev (RRL) */
/*   Date                  : 22-MAY-1991				*/
/*   Program Description   : File copy/delete/rename/purge/append/      */
/*                           compare commands -- consolidated           */
/*                           14-Sep-2026 from FILERCPY.C + FILERDEL.C + */
/*                           FILERREN.C + FILERPUR.C + FILERAPP.C +     */
/*                           FILERDIF.C.                                */
/*                         :                                            */
/* Revision History follows                                             */
/*  25-Sep-2026 RRL ECO2: __NEW_STARLET prototypes, types fixed (0 CC diags)  */
/*                        FIBDEF/SBKDEF typedefs; sys$qiow IOSB/P2-P6 casts;  */
/*                        FIX: lib$sys_getmsg() got &iosb.status (word)  */
/*  26-Sep-2026 CLD ECO3: ident sync with DX.C (no code change)         */

#define __MODULE__	"FILERCPY"
#define __IDENT__	"X-02-05ECO3"
#define __REV__		"02.05.03"


#define	__NEW_STARLET	1
#include	<starlet.h>
#include	<lib$routines.h>
#include	<str$routines.h>
#include	<atrdef.h>
#include	<iodef.h>
#include	<limits.h>
#include	<sbkdef.h>
#include	<fibdef.h>
#include	<libclidef.h>
#include	<signal.h>

#include "global.h"
#include "direct.h"
#include "dx.h"
#include "diff.h"
#include "filer.h"
#include "filercpy.h"
#include "filerque.h"
#include "filersta.h"
#include "inquire.h"


/* ****  INTERNAL FUNCTION PROTOTYPING *** */
static int		    filer_copy$2(char *, char *, char *);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      filer_copy() dispatches to filer_multiple_copy() or filer_single_copy(),
**	depending on whether the current window is in single- or multiple-file mode.
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
**	cntrl_info_block.dir_dx[cur_win].filemode.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	Whatever the dispatched routine returns (always DX__NORMAL).
**
**--
**/
int	filer_copy()
{
    return dxscr_dispatch_filemode(filer_multiple_copy, filer_single_copy);
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Prompts once for a single destination filespec (shared by every marked
**	file), then applies filer_copy$1() to each marked file in turn via
**	multi_get_userinput_and_exec().
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
**	The current window's marked-file list.
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_copy$2()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_multiple_copy()
{
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error copying file");
    multi_get_userinput_and_exec(
	filer_copy$1,
	"Multiple Copy",
	"Copy to: ",
	"",
	l_errmsg);
    return DX__NORMAL;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Prompts for a destination filespec for DX_CURRENT_FILE and copies it
**	via get_userinput_and_execute()/filer_copy$1().  Refuses to copy the
**	"[-]" parent-directory placeholder entry.
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
**	None directly (see filer_copy$2()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int filer_single_copy()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Copy a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error copying file");
	get_userinput_and_execute(
	    filer_copy$1,
	    "Copy File",
	    "Copy to: ",
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
**      Thin adapter: get_userinput_and_execute()/multi_get_userinput_and_exec()
**	call their callback as (result, original, errmsg); this reorders the
**	arguments into (from, to, errmsg) and forwards to filer_copy$2(), the
**	actual worker.
**
**  INPUT PARAMETERS:
**
**	a_to  -  destination filespec entered by the user.
**	a_from  -  source filespec (the file being copied).
**	a_errmsg  -  buffer to receive an error message on failure.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg.
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
**	Whatever filer_copy$2() returns (DX__NORMAL or DX__ERROR).
**
**--
**/
int	filer_copy$1(a_to, a_from, a_errmsg)
char *a_to;
char *a_from;
char *a_errmsg;
{
    return filer_copy$2(a_from, a_to, a_errmsg);
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      The actual file copy: reads the source file's RMS/XAB attributes via
**	QIO, creates the destination file with the same characteristics, copies
**	it block-by-block (IO$_READVBLK/IO$_WRITEVBLK), then calls filestat(),
**	update_filer_cache() and add_to_filer() to add the new file to the
**	cache and to every open window's display.
**
**  INPUT PARAMETERS:
**
**	a_from  -  full source filespec.
**	a_to  -  destination filespec, as typed by the user (may be a bare
**		  directory or a partial spec).
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an RMS/QIO error message on failure.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_FILE (source file's directory id), cntrl_info_block
**	(current window, window count).
**
**  IMPLICIT OUTPUTS:
**
**	The filer cache and every open window's display, via
**	update_filer_cache()/add_to_filer().
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success, DX__ERROR if the copy failed at any step
**	(RMS parse/open/create/read/write, or filestat()/add_to_filer()
**	afterward).
**
**--
**/
static int	filer_copy$2(a_from, a_to, a_errmsg)
char *a_from;
char *a_to;
char *a_errmsg;
{
    unsigned short int l_chan;
    unsigned short l_chan_o;		    /**  I/O channel		     **/

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

    /*
    **  struct representing a 64-bit binary value expressing
    **  the date and time
    **/
    DATE_TIME l_edt,
	      l_cdt;
    unsigned short int l_fpro;		    /**  File Protection	     **/

    /**  Access Control Block  **/
    struct {
	unsigned short w_size;
	unsigned short w_type;
	void *l_addr;
    } l_acb[] = {
	{ ATR$S_UCHAR, ATR$C_UCHAR, &l_fch },
	{ ATR$S_RECATTR, ATR$C_RECATTR, &l_raa },
	{ ATR$S_STATBLK, ATR$C_STATBLK, &l_asb },
	{ ATR$S_EXPDATE, ATR$C_EXPDATE, &l_edt },
	{ ATR$S_CREDATE, ATR$C_CREDATE, (char *)&l_cdt },
	{ ATR$S_FPRO, ATR$C_FPRO, (char *)&l_fpro },
	{ 0, 0, (char *)NULL }
    };

    struct {
	unsigned short status;
	unsigned short bytcnt;
	int filler;
    } l_iosb;

    unsigned short l_bytes;
    unsigned int l_status;
    unsigned int l_msgid;			/**  Condition value for lib$sys_getmsg()  **/

    static FIBDEF l_fib;
    $DESCRIPTOR(l_fib_descriptor, (char *)&l_fib);

    char l_filespec[MAXFILESPEC+1];
    unsigned short l_filespec_length;
    $DESCRIPTOR (l_filespec_descriptor, l_filespec);

    char l_buf[BUFSIZ];
    int l_svb;

    struct FAB l_to_fab;
    struct NAM l_to_nam;
    char l_expand_name[NAM$C_MAXRSS];
    char l_resultant_name[MAXFILESPEC+1];
    $DESCRIPTOR (l_resultant_name_descriptor, l_resultant_name);
    unsigned short l_len;
    char l_devname[MAXVMSDEVICE+1];
    $DESCRIPTOR(l_devname_descriptor, l_devname);
    char *l_cp;
    unsigned short l_from_fid[3];
    short l_status_flag;
    int l_i;
    struct fil_dx_tag l_temp_entry;
    char l_errmsg1[256];
    $DESCRIPTOR (l_errmsg1_descrip, l_errmsg1);
    unsigned long l_rc;

    /**  Get directory fid and file name of the output file  **/
    l_to_fab = cc$rms_fab;
    l_to_nam = cc$rms_nam;

    l_to_fab.fab$l_fna = a_to;
    l_to_fab.fab$b_fns = strlen(a_to);
    l_to_fab.fab$l_nam = &l_to_nam;

    l_to_fab.fab$l_dna = a_from;
    l_to_fab.fab$b_dns = strlen(a_from);

    l_to_nam.nam$l_esa = l_expand_name;
    l_to_nam.nam$b_ess = NAM$C_MAXRSS;

    if (!((l_status = sys$parse(&l_to_fab, 0, 0)) & 1))
    {
	lib$sys_getmsg (
	    &l_status,
	    &l_len,
	    &l_errmsg1_descrip,
	    0,
	    0);
	l_errmsg1[l_len] = EOS;
	strcpy(a_errmsg, l_errmsg1);
	return DX__ERROR;
    }

    /**  Get a channel for the output file  **/
    strncpy(l_devname, l_to_nam.nam$l_dev, l_to_nam.nam$b_dev);
    l_devname[l_to_nam.nam$b_dev] = EOS;
    if ((l_chan_o = get_chan(l_devname)) == 0)
    {
	{
	    static $DESCRIPTOR (l_fao_ctrl, "Establish a logical link with !AZ failed");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = MAXFILESPEC;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = a_errmsg;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_to))))
		$LOG(l_rc);
	    a_errmsg[l_len] = EOS;
	}
	return DX__ERROR;
    }


    /**  Directory fid for the input file  **/
    __FIB(l_fib,fib$r_did_overlay,fib$w_did[0]) = DX_CURRENT_FILE->did[0];
    __FIB(l_fib,fib$r_did_overlay,fib$w_did[1]) = DX_CURRENT_FILE->did[1];
    __FIB(l_fib,fib$r_did_overlay,fib$w_did[2]) = DX_CURRENT_FILE->did[2];

    /**  SYS$QIO will not use device and directory name  **/
    if ((l_cp = strchr(a_from, ']')) == 0)
	strcpy(l_filespec, a_from);
    else
    {
	l_cp++;
	strcpy(l_filespec, l_cp);
    }
    LENGTH(l_filespec_descriptor) = strlen(l_filespec);

    /*
    **	Find the I/O channel for this file
    **/
    if (!dxscr_find_chan_for_did(DX_CURRENT_FILE->did, &l_chan, (char *)NULL, &DX_CURRENT_DIRECTORY.cur_filter))
    {
	strcpy(a_errmsg, "Internal error - I/O channel for source file not found");
	return DX__ERROR;
    }

    __FIB(l_fib,fib$r_acctl_overlay,fib$l_acctl) = 0;
    LENGTH(l_fib_descriptor) = sizeof (FIBDEF);
    l_status = sys$qiow (		/**  Read input file attributes  **/
		 0,
		 l_chan,
		 IO$_ACCESS+IO$M_ACCESS,
		 (struct _iosb *) &l_iosb,
		 0,
		 0,
		 &l_fib_descriptor,
		 (__int64) &l_filespec_descriptor,
		 0,
		 0,
		 (__int64) l_acb,
		 0);

    if (!(l_status &1))
    {	/**  No READ access  **/
	lib$sys_getmsg (
	    &l_status,
	    &l_len,
	    &l_errmsg1_descrip,
	    0,
	    0);
	l_errmsg1[l_len] = EOS;
	strcpy(a_errmsg, l_errmsg1);
    	return DX__ERROR;
    }
    else if (!(l_iosb.status & 1))
    {	/**  No READ access  **/
	l_msgid = l_iosb.status;	/**  lib$sys_getmsg() wants a longword  **/
	lib$sys_getmsg (
	    &l_msgid,
	    &l_len,
	    &l_errmsg1_descrip,
	    0,
	    0);
	l_errmsg1[l_len] = EOS;
	strcpy(a_errmsg, l_errmsg1);
    	return DX__ERROR;
    }
    else
    {
	/*
	**  Got input file attributes.
	**  Create a output file use the same attributes.
	**/

	/**  Save input file fid  **/
	for (l_i = 0; l_i <= 2; l_i++)
	    l_from_fid[l_i] = __FIB(l_fib,fib$r_fid_overlay,fib$w_fid[l_i]);

	__FIB(l_fib,fib$r_did_overlay,fib$w_did[0]) = l_to_nam.nam$w_did[0];
	__FIB(l_fib,fib$r_did_overlay,fib$w_did[1]) = l_to_nam.nam$w_did[1];
	__FIB(l_fib,fib$r_did_overlay,fib$w_did[2]) = l_to_nam.nam$w_did[2];

        memset (l_filespec, EOS, sizeof l_filespec);
	strncpy(l_filespec, l_to_nam.nam$l_name, l_to_nam.nam$b_name);
	strncat(l_filespec, l_to_nam.nam$l_type, l_to_nam.nam$b_type);
	if (strchr(a_to, ';') != 0)
	    strncat(l_filespec, l_to_nam.nam$l_ver, l_to_nam.nam$b_ver);
	else
	    strcat(l_filespec, ";0");

	LENGTH(l_filespec_descriptor) = strlen(l_filespec);

	/**  Update FIB information for output file  **/
	__FIB(l_fib,fib$r_acctl_overlay.fib$r_acctl_bits0,fib$v_write) = 1;
	__FIB(l_fib,fib$r_exctl_overlay.fib$r_exctl_bits,fib$v_extend) = 1;
	l_fib.fib$l_exsz = (l_asb.sbk$w_filesizh << 16)+l_asb.sbk$w_filesizl;

	sys$qiow (	/**  Create the output file  **/
	     0,
	     l_chan_o,
	     IO$_CREATE+IO$M_CREATE+IO$M_ACCESS,
	     (struct _iosb *) &l_iosb,
	     0,
	     0,
	     &l_fib_descriptor,
	     (__int64) &l_filespec_descriptor,
	     (__int64) &l_len,
	     (__int64) &l_resultant_name_descriptor,
	     (__int64) l_acb,
	     0);

	if (!(l_iosb.status & 1))
	{    /**  File creation failed **/
	    for (l_i = 0; l_i <= 2; l_i++)
		__FIB(l_fib,fib$r_fid_overlay,fib$w_fid[l_i]) = l_from_fid[l_i];
	    if (!(1 & (l_rc = sys$qiow (	    /**  Close the input file  **/
		     0,
		     l_chan,
		     IO$_DEACCESS,
		     (struct _iosb *) &l_iosb,
		     0,
		     0,
		     &l_fib_descriptor,
		     0,
		     0,
		     0,
		     0,
		     0))))
	        $LOG(l_rc);
	    sys$dassgn (l_chan_o);
	    l_msgid = l_iosb.status;	/**  lib$sys_getmsg() wants a longword  **/
	    lib$sys_getmsg (
		&l_msgid,
		&l_len,
		&l_errmsg1_descrip,
		0,
		0);
	    l_errmsg1[l_len] = EOS;
	    strcpy(a_errmsg, l_errmsg1);
	    return DX__ERROR;
	}

	/*
	**  Copy to the output file by reading the virtual block(s)
	**  From the input file, stores it in a buffer, then writes it
	**  To the output file.
	**/

	l_status_flag = 0;
	l_svb = 1;
	sys$qiow (
	     0,
	     l_chan,
	     IO$_READVBLK,
	     (struct _iosb *) &l_iosb,
	     0,
	     0,
	     l_buf,
	     (__int64) BUFSIZ,
	     (__int64) l_svb,
	     0,
	     0,
	     0);

	while (l_iosb.status == SS$_NORMAL)
	{
	    l_bytes = l_iosb.bytcnt;
	    sys$qiow (
		 0,
		 l_chan_o,
		 IO$_WRITEVBLK,
		 (struct _iosb *) &l_iosb,
		 0,
		 0,
		 l_buf,
		 (__int64) l_bytes,
		 (__int64) l_svb,
		 0,
		 0,
		 0);
	    if (!(l_iosb.status & 1))
	    {	    /**  WRITE A VIRTUAL BLOCK FAILED  **/
		l_status_flag++;
		{
		    static $DESCRIPTOR (l_fao_ctrl, "Error writing to !AZ");
		    struct dsc$descriptor_s l_out_desc;
		    unsigned short int l_len;
		    unsigned long l_rc;

		    l_out_desc.dsc$w_length  = MAXFILESPEC;
		    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		    l_out_desc.dsc$a_pointer = a_errmsg;

		    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_filespec))))
			$LOG(l_rc);
		    a_errmsg[l_len] = EOS;
		}
		break;
	    }
	    l_svb++;
	    sys$qiow (
		 0,
		 l_chan,
		 IO$_READVBLK,
		 (struct _iosb *) &l_iosb,
		 0,
		 0,
		 l_buf,
		 (__int64) BUFSIZ,
		 (__int64) l_svb,
		 0,
		 0,
		 0);
	}

	/*
	**  Close the output file.
	**  No error checking in case the output is SYS$OUTPUT.
	**/

	sys$qiow (
	     0,
	     l_chan_o,
	     IO$_DEACCESS,
	     (struct _iosb *) &l_iosb,
	     0,
	     0,
	     &l_fib_descriptor,
	     0,
	     0,
	     0,
	     0,
	     0);

	sys$dassgn (l_chan_o);

	/**  Directory fid for the input file  **/
	for (l_i = 0; l_i <= 2; l_i++)
	{
	    __FIB(l_fib,fib$r_fid_overlay,fib$w_fid[l_i]) = l_from_fid[l_i];
	}

	sys$qiow (	/**  Close the input file  **/
	     0,
	     l_chan,
	     IO$_DEACCESS,
	     (struct _iosb *) &l_iosb,
	     0,
	     0,
	     &l_fib_descriptor,
	     0,
	     0,
	     0,
	     0,
	     0);
    }

    if (l_status_flag)
	return DX__ERROR;

    /*
    **	Get output file attributes
    **/

    strncpy(l_filespec, l_to_nam.nam$l_dev, l_to_nam.nam$b_dev+l_to_nam.nam$b_dir);
    l_filespec[l_to_nam.nam$b_dev+l_to_nam.nam$b_dir] = EOS;
    strcat(l_filespec, l_resultant_name);
    if (filestat(l_filespec, &l_temp_entry, a_errmsg) == DX__ERROR)
        return DX__ERROR;

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
	    if (!(1 & (l_rc = smg$set_physical_cursor (
                     &cntrl_info_block.pasteboard_id,
                     &cntrl_info_block.pasteboard_rows,
                     &1))))
	        $LOG(l_rc);
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
**      Inserts a newly created/copied/appended file entry into window a_win's
**	cache lists (dir_filelist or non_dir_filelist, whichever applies) and
**	updates that window's on-screen display in place, without a full
**	directory re-scan.
**
**  INPUT PARAMETERS:
**
**	a_e  -  the fil_dx_tag entry describing the new file.
**	a_win  -  which window's cache/display to update.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in on failure.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.dir_dx[a_win] (cache lists, counts, display).
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.dir_dx[a_win] (cache lists, counts, display).
**
**  RETURNS VALUE:
**
**	DX__NORMAL if inserted/displayed, or if the file was correctly
**	determined not to belong in this window's display; DX__ERROR if
**	find_exclude_file() ran out of memory.
**
**--
**/
int	add_to_filer(
	struct fil_dx_tag a_e,
	char *a_errmsg,
	unsigned short int a_win)
{
    int l_rest_col;	/**  1st column after the file name field  **/
    char l_fn[NAM$C_MAXRSS];
    struct fil_dx_tag *l_p, *l_q;
    int l_row, l_col;
    int l_total_dids;
    int l_total_devices;
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR (l_str_descrip, l_str);
    int l_next_entry_row, l_next_entry_column;
    unsigned long l_rc;

    /*
    **	Find out the number of equivalent names of current directory
    **/

    if (!dxscr_find_chan_for_did(a_e.did, (unsigned short int *)NULL, l_fn,
	    &cntrl_info_block.dir_dx[a_win].cur_filter))
    {	    /**  Directories not matched  **/
	return DX__NORMAL;
    }
    l_total_dids = dxscr_count_win_dirs(&cntrl_info_block.dir_dx[a_win].cur_filter);
    l_total_devices = dxscr_count_win_devices(&cntrl_info_block.dir_dx[a_win].cur_filter);

    if (!strcmp(strrchr(a_e.fn, '.'), ".DIR;1"))
    {	    /**  Insert a directory file  **/
	char *l_cp;
	char l_s[MAXFILESPEC+1];

	strcpy(l_s, a_e.fn);
	l_cp = strrchr(l_s, '.');
        *l_cp = ']';      /**  Delete "DIR;1"  **/
	*++l_cp = EOS;
        if (l_total_dids <= 1)
	{
	    static $DESCRIPTOR (l_fao_ctrl, "[.!AZ");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = MAXFILESPEC;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = a_e.fn;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_s))))
		$LOG(l_rc);
	    a_e.fn[l_len] = EOS;
	}
        else
        {
            if (l_total_devices <= 1)
                strcpy (a_e.fn, strchr (l_fn, '['));
	    else
		strcpy (a_e.fn, l_fn);
	    a_e.fn[strlen(a_e.fn)-1] = '.';
	    strcat (a_e.fn, l_s);
        }

        if (insert_filelist_entry(
		cntrl_info_block.dir_dx[a_win].dir_filelist,
		a_e,
		cntrl_info_block.dir_dx[a_win].sortby,
		cntrl_info_block.dir_dx[a_win].sortorder) == DX__NORMAL)
        {
            cntrl_info_block.dir_dx[a_win].tot_files++;
	    cntrl_info_block.dir_dx[a_win].tot_blocks += a_e.filesize;
	    cntrl_info_block.dir_dx[a_win].subdir++;
        }
    }
    else
    {	    /**  Regular file  **/
	if (find_exclude_file(	    /**  Update exclude list  **/
		&cntrl_info_block.dir_dx[a_win].exclude_filelist,
		cntrl_info_block.dir_dx[a_win].cur_filter,
		cntrl_info_block.dir_dx[a_win].sortby,
		cntrl_info_block.dir_dx[a_win].sortorder) == DX__ERROR)
	{
	    strcpy(
		a_errmsg, "Insufficient memory to update the exclude file list");
	    return DX__ERROR;
	}

	strcat(l_fn, a_e.fn);		    /**  Get full path name  **/
	if (!find_match_file(l_fn, a_win))  /**  The new file is not in the list  **/
	    return DX__NORMAL;

	/*
	**	Insert the new file to the list.  Make sure it meets all
	**	the file selection criteria before insertion.
	**/

	if (l_total_dids > 1)
	{
	    if (l_total_devices <= 1)
		strcpy(a_e.fn, strchr(l_fn, '['));
	    else
		strcpy(a_e.fn, l_fn);
	}

	if (!filter_OK(
		a_e,
		cntrl_info_block.dir_dx[a_win].exclude_filelist,
		cntrl_info_block.dir_dx[a_win].cur_filter))
	    return DX__NORMAL;

	if (insert_filelist_entry(
		cntrl_info_block.dir_dx[a_win].non_dir_filelist,
		a_e,
		cntrl_info_block.dir_dx[a_win].sortby,
		cntrl_info_block.dir_dx[a_win].sortorder) == DX__NORMAL)
	{
	    cntrl_info_block.dir_dx[a_win].tot_files++;
	    cntrl_info_block.dir_dx[a_win].tot_blocks += a_e.filesize;
	}
    }

    /*
    **	Update current display.
    **	Find file location in current display.
    **/
    for (l_p = cntrl_info_block.dir_dx[a_win].dir_filelist->forward;
	 l_p != cntrl_info_block.dir_dx[a_win].non_dir_filelist; l_p = l_p->forward)
    {
        if (l_p == cntrl_info_block.dir_dx[a_win].dir_filelist)
            l_p = cntrl_info_block.dir_dx[a_win].non_dir_filelist;
	else
	{
	    if (l_p->fid[0] == a_e.fid[0] &&
		l_p->fid[1] == a_e.fid[1] &&
		l_p->fid[2] == a_e.fid[2])
		break;
	}
    }

    if (l_p == cntrl_info_block.dir_dx[a_win].non_dir_filelist)
        return DX__NORMAL;

    if (l_p->forward == cntrl_info_block.dir_dx[a_win].non_dir_filelist)
    {	    /**  p is the last entry in non-directory file list  **/
        l_next_entry_row = l_next_entry_column = INT_MAX;
    }
    else if (l_p->forward == cntrl_info_block.dir_dx[a_win].dir_filelist)
    {	    /**  p is the last entry in directory file list  **/
        l_next_entry_row = cntrl_info_block.dir_dx[a_win].non_dir_filelist->forward->beg_y;
        l_next_entry_column = cntrl_info_block.dir_dx[a_win].non_dir_filelist->forward->beg_x;
    }
    else
    {
        l_next_entry_row = l_p->forward->beg_y;
        l_next_entry_column = l_p->forward->beg_x;
    }

    /*
    **	Re-compute the position of the files in this directory
    **/

    l_row = l_col = 1;
    compute_filer_begin_xy (
	cntrl_info_block.dir_dx[a_win].dir_filelist, &l_row, &l_col, a_win);
    cntrl_info_block.dir_dx[a_win].non_dir_filelist->beg_y =
	cntrl_info_block.dir_dx[a_win].dir_filelist->backward->beg_y;
    cntrl_info_block.dir_dx[a_win].non_dir_filelist->beg_x =
	cntrl_info_block.dir_dx[a_win].dir_filelist->backward->beg_x;
    compute_filer_begin_xy (
	cntrl_info_block.dir_dx[a_win].non_dir_filelist, &l_row, &l_col, a_win);

    /*
    **	Adjust FILER_DISPLAY rows
    **/

    cntrl_info_block.dir_dx[a_win].filer_display.rows =
	max (cntrl_info_block.dir_dx[a_win].non_dir_filelist->backward->beg_y,
	     cntrl_info_block.dir_dx[a_win].filer_display.view_rows);
    if (!(1 & (l_rc = smg$change_virtual_display (
		 &cntrl_info_block.dir_dx[a_win].filer_display.id,
		 &cntrl_info_block.dir_dx[a_win].filer_display.rows,
		 &cntrl_info_block.dir_dx[a_win].filer_display.width,
		 0,
		 0,
		 0))))
        $LOG(l_rc);

    /*
    **	Update screen with batch mode
    **/

    if (!(1 & (l_rc = smg$begin_display_update (
		 &cntrl_info_block.dir_dx[a_win].filer_display.id))))
        $LOG(l_rc);

    if (cntrl_info_block.dir_dx[a_win].form == long_form)
    {
	if (l_p->beg_y < cntrl_info_block.dir_dx[a_win].filer_display.rows)
	{
	    if (!(1 & (l_rc = smg$insert_line (
			 &cntrl_info_block.dir_dx[a_win].filer_display.id,
			 &l_p->beg_y,
			 0,
			 &SMG$M_DOWN,
			 0,
			 0,
			 0,
			 0))))
	        $LOG(l_rc);
        }
	format_filer_entry(l_p, l_str, a_win);

        if (l_p->fn[ strlen(l_p->fn)-1 ] != ']')
	{
	    LENGTH(l_str_descrip) = strlen(l_str);
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
			0,
			0,
			0,
			0))))
	        $LOG(l_rc);
        }
    }
    else
    {	    /**  Short form  **/
        if (l_next_entry_row < l_p->beg_y ||
	    (l_next_entry_row == l_p->beg_y && l_next_entry_column < l_p->beg_x))
        {
	    if (!(1 & (l_rc = smg$erase_display (
		      &cntrl_info_block.dir_dx[a_win].filer_display.id,
		      &l_next_entry_row,
		      &l_next_entry_column,
		      &cntrl_info_block.dir_dx[a_win].filer_display.rows,
		      &cntrl_info_block.dir_dx[a_win].filer_display.width))))
	        $LOG(l_rc);
        }
        else
        {
	    if (!(1 & (l_rc = smg$erase_display (
		      &cntrl_info_block.dir_dx[a_win].filer_display.id,
		      &l_p->beg_y,
		      &l_p->beg_x,
		      &cntrl_info_block.dir_dx[a_win].filer_display.rows,
		      &cntrl_info_block.dir_dx[a_win].filer_display.width))))
	        $LOG(l_rc);
        }
        for (l_q = l_p;
	    l_q != cntrl_info_block.dir_dx[a_win].non_dir_filelist;
	    l_q = l_q->forward)
        {
            if (l_q == cntrl_info_block.dir_dx[a_win].dir_filelist)
                l_q = cntrl_info_block.dir_dx[a_win].non_dir_filelist;
            else
            {
		format_filer_entry(l_q, l_str, a_win);
		LENGTH(l_str_descrip) = strlen(l_str);
                if (l_q->fn[ strlen(l_q->fn)-1 ] == ']')
                {
		    if (!(1 & (l_rc = smg$put_chars (
				 &cntrl_info_block.dir_dx[a_win].filer_display.id,
				 &l_str_descrip,
				 &l_q->beg_y,
				 &l_q->beg_x,
				 0,
				 &SMG$M_BOLD,
				 0,
				 0))))
		        $LOG(l_rc);
                }
                else
                {
		    if (!(1 & (l_rc = smg$put_chars (
				 &cntrl_info_block.dir_dx[a_win].filer_display.id,
				 &l_str_descrip,
				 &l_q->beg_y,
				 &l_q->beg_x,
				 0,
				 0,
				 0,
				 0))))
		        $LOG(l_rc);
                }
            }
        }
    }

    change_filer_viewport(a_win);

    if (!(1 & (l_rc = smg$end_display_update (
		 &cntrl_info_block.dir_dx[a_win].filer_display.id))))
        $LOG(l_rc);

    put_filer_stat(a_win);

    return DX__NORMAL;
}



/* ---- (from FILERDEL.C -- file delete) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      filer_delete() dispatches to filer_multiple_delete() or
**	filer_single_delete(), depending on whether the current window is in
**	single- or multiple-file mode.
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
**	DX__NORMAL (the value returned by whichever of filer_multiple_delete()/
**	filer_single_delete() runs).
**
**--
**/
int	filer_delete()
{
    return dxscr_dispatch_filemode(filer_multiple_delete, filer_single_delete);
}
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Confirms with the user ("Are you sure? [YES/NO]"), then applies
**	filer_delete$1() to every marked file in turn via
**	multi_get_userinput_and_exec().
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
**	The current window's marked-file list.
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_delete$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_multiple_delete()
{
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error deleting file");
    multi_get_userinput_and_exec(
	filer_delete$1,
	"Multiple Delete",
	"Are you sure? [YES/NO] ",
	"",
	l_errmsg);
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Deletes DX_CURRENT_FILE, honoring the user's delete-confirmation
**	preference (cntrl_info_block.user_pref.delete_confirm).  Refuses on
**	the "[-]" parent-directory placeholder entry.
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
**	DX_CURRENT_FILE, cntrl_info_block.user_pref.delete_confirm,
**	cntrl_info_block.cur_win.
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_delete$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_single_delete()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Delete a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error deleting file");
	if (cntrl_info_block.user_pref.delete_confirm)
	{
	    get_userinput_and_execute(
		filer_delete$1,
		"Confirm Deletion",
		"Delete this file? [YES/NO] ",
		"YES",
		l_errmsg,
		l_fn);
	}
	else if (filer_delete$1("YES", l_fn, l_errmsg) == DX__ERROR)
	    signal_err(l_errmsg, bell);
    }
   return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      The confirmed-delete worker: checks the user's YES/NO answer, refuses
**	to delete a directory file that is currently open (mapped) in another
**	window, calls the C RTL remove(), then removes the entry from every
**	window's display via filer_delete$2() and updates the filer and
**	directory-tree caches.
**
**  INPUT PARAMETERS:
**
**	a_confirm  -  the user's YES/NO answer.
**	a_fn  -  full filespec of the file to delete.
**	a_errmsg  -  buffer to receive an error message on failure.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block (current/other windows).
**
**  IMPLICIT OUTPUTS:
**
**	The filer cache, the directory-tree cache (if a directory was
**	deleted), and every open window's display.
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success; DX__ERROR on failure; DX__DONTCARE if the
**	user answered anything other than YES (via dxscr_confirm_yes()).
**
**--
**/
int	filer_delete$1(a_confirm, a_fn, a_errmsg)
char *a_confirm;
char *a_fn;
char *a_errmsg;
{
    struct fil_dx_tag l_temp_entry;
    char *l_cp;
    int l_i;
    int l_status;

    /*
    **	Verify that the user wants to continue with the
    **	deletion operation.  Return to calling
    **	routine if the user does not enter "YES".
    **/

    if ((l_status = dxscr_confirm_yes(a_confirm, "DELETE", a_errmsg)) != DX__NORMAL)
	return l_status;

    /*
    **	Make sure the file is not a directory file that is being
    **	used by the other window
    **/

    if (filestat(a_fn, &l_temp_entry, a_errmsg) == DX__ERROR)
        return DX__ERROR;

    if (strstr(l_temp_entry.fn, ".DIR;1"))
    {
	if (dxscr_dir_open_elsewhere(l_temp_entry.fid, cntrl_info_block.cur_win))
	{
	    strcpy(a_errmsg, "File in use by other window - deletion failed");
	    return DX__ERROR;
	}
    }

    if (remove(a_fn) != 0)
    {
	strcpy(a_errmsg, "Insufficient privilege for delete operation");
	return DX__ERROR;
    }

    /**  Delete it from the FILER  **/
    if (filer_delete$2(a_fn, a_errmsg, cntrl_info_block.cur_win) == DX__ERROR)
        return DX__ERROR;

    /**  Update other windows  **/
    for (l_i = 0;  l_i < cntrl_info_block.windows;  l_i++)
    {
        if (l_i != cntrl_info_block.cur_win)
        {
            if (filer_delete$2(a_fn, a_errmsg, l_i) == DX__ERROR)
                 return DX__ERROR;
        }
    }

    /*
    **  Update the cache queue.
    **	Must call it after filer_delete$2()
    **/

    update_filer_cache(l_temp_entry, delete_from_filer_cache, a_errmsg);

    /*
    **	Update the directory queue.
    **	Convert the file name to directory name.
    **/

    if (strlen(a_fn) > 6 && !strcmp(a_fn+strlen(a_fn)-6, ".DIR;1"))
    {
        l_cp = strchr (a_fn, ']');
	*l_cp = '.';
        l_cp = strrchr (a_fn, '.');
	*l_cp++ = ']';
	*l_cp = EOS;
        delete_from_direct_cache(a_fn, a_errmsg);
    }

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Removes one file's entry from window a_win's display list and cache
**	counters, and updates the on-screen list in place (a line delete for
**	long form, shift-and-redraw for short form).  Does not touch the file
**	on disk -- the caller is responsible for the actual deletion.
**
**  INPUT PARAMETERS:
**
**	a_fn  -  full filespec of the file being removed (used only to locate
**		  the display entry).
**	a_win  -  which window's cache/display to update.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  unused on this path (kept for signature symmetry with
**		      the other filer_xxx$2() workers).
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.dir_dx[a_win] (cache lists, counts, display).
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.dir_dx[a_win] (cache lists, counts, display).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always (a no-op if the file is not currently shown in
**	this window).
**
**--
**/
int	filer_delete$2(
		char *a_fn,
		char *a_errmsg,
		unsigned short int a_win)
{
    struct fil_dx_tag *l_p, *l_q;
    char l_buf[MAXFILESPEC+1];
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR (l_str_descrip, l_str);
    unsigned long l_rc;
 
    /*
    **	Find the file entry from the link-list
    **/
 
    for (l_p = cntrl_info_block.dir_dx[a_win].dir_filelist->forward;
	 l_p != cntrl_info_block.dir_dx[a_win].non_dir_filelist; l_p = l_p->forward)
    {
        if (l_p == cntrl_info_block.dir_dx[a_win].dir_filelist)
            l_p = cntrl_info_block.dir_dx[a_win].non_dir_filelist;
	else
	{
	    find_full_path_name(l_p->fn, l_buf, a_win);
	    if (!strcmp(l_buf, a_fn))
		break;
	}
    }
 
    if (l_p == cntrl_info_block.dir_dx[a_win].non_dir_filelist)
        return DX__NORMAL;
 
    if (l_p->state == ready)
    {	    /**  Update directory selection files and blocks  **/
	cntrl_info_block.dir_dx[a_win].sel_files--;
	cntrl_info_block.dir_dx[a_win].sel_blocks -= l_p->filesize;
    }
 
    cntrl_info_block.dir_dx[a_win].tot_files--;
    cntrl_info_block.dir_dx[a_win].tot_blocks -= l_p->filesize;
 
    /*
    **	Re-compute the position of the files that following
    **	the file being deleted
    **/
 
    recompute_filer_beg_xy(l_p->forward, l_p->beg_y, l_p->beg_x, a_win);
 
    cntrl_info_block.dir_dx[a_win].non_dir_filelist->beg_y =
	cntrl_info_block.dir_dx[a_win].dir_filelist->backward->beg_y;
    cntrl_info_block.dir_dx[a_win].non_dir_filelist->beg_x =
	cntrl_info_block.dir_dx[a_win].dir_filelist->backward->beg_x;
 
    if (l_p == cntrl_info_block.dir_dx[a_win].cur_file)
    {
	cntrl_info_block.dir_dx[a_win].cur_file = l_p->forward;
        if (cntrl_info_block.dir_dx[a_win].cur_file ==
		cntrl_info_block.dir_dx[a_win].dir_filelist)
        {
            if (cntrl_info_block.dir_dx[a_win].non_dir_filelist->forward !=
		cntrl_info_block.dir_dx[a_win].non_dir_filelist)
            {	    /**  At least one non-directory file presented  **/
                cntrl_info_block.dir_dx[a_win].cur_file =
		    cntrl_info_block.dir_dx[a_win].non_dir_filelist->forward;
            }
            else if (l_p->backward != cntrl_info_block.dir_dx[a_win].dir_filelist)
            {	    /**  At least one directory file still remaining  **/
                cntrl_info_block.dir_dx[a_win].cur_file = l_p->backward;
            }
	    else
	    {	    /**  No files in this directory  **/
		cntrl_info_block.dir_dx[a_win].cur_file =
		    cntrl_info_block.dir_dx[a_win].non_dir_filelist;
	    }
        }
        else if (cntrl_info_block.dir_dx[a_win].cur_file ==
		    cntrl_info_block.dir_dx[a_win].non_dir_filelist)
        {
            if (l_p->backward != cntrl_info_block.dir_dx[a_win].non_dir_filelist)
            {	    /**  At least one non_directory file still remaining  **/
                cntrl_info_block.dir_dx[a_win].cur_file = l_p->backward;
            }
            else if (cntrl_info_block.dir_dx[a_win].dir_filelist->backward !=
		cntrl_info_block.dir_dx[a_win].dir_filelist)
            {	    /**  At least one directory file presented  **/
                cntrl_info_block.dir_dx[a_win].cur_file =
		    cntrl_info_block.dir_dx[a_win].dir_filelist->backward;
            }
        }
    }
 
    /*
    **	Update the screen
    **/
 
    if (cntrl_info_block.dir_dx[a_win].form == long_form)
    {
        if (cntrl_info_block.dir_dx[a_win].non_dir_filelist->backward->beg_y <=
		cntrl_info_block.dir_dx[a_win].filer_display.view_rows ||
	    l_p->beg_y <=
		cntrl_info_block.dir_dx[a_win].non_dir_filelist->backward->beg_y)
        {
	    if (!(1 & (l_rc = smg$delete_line (
		      &cntrl_info_block.dir_dx[a_win].filer_display.id,
		      &l_p->beg_y,
		      &1))))
	        $LOG(l_rc);
        }
    }
    else
    {
	if (!(1 & (l_rc = smg$begin_display_update (
		     &cntrl_info_block.dir_dx[a_win].filer_display.id))))
	    $LOG(l_rc);
 
        if (!(1 & (l_rc = smg$erase_display (
                      &cntrl_info_block.dir_dx[a_win].filer_display.id,
                      &l_p->beg_y,
                      &l_p->beg_x,
                      &cntrl_info_block.dir_dx[a_win].filer_display.rows,
                      &cntrl_info_block.dir_dx[a_win].filer_display.width))))
            $LOG(l_rc);
        for (l_q = l_p->forward;
	    l_q != cntrl_info_block.dir_dx[a_win].non_dir_filelist;
	    l_q = l_q->forward)
        {
            if (l_q == cntrl_info_block.dir_dx[a_win].dir_filelist)
                l_q = cntrl_info_block.dir_dx[a_win].non_dir_filelist;
            else
            {
                format_filer_entry(l_q, l_str, a_win);
		LENGTH(l_str_descrip) = strlen(l_str);
                if (l_q->fn[ strlen(l_q->fn)-1 ] == ']')
                {
		    if (!(1 & (l_rc = smg$put_chars (
				 &cntrl_info_block.dir_dx[a_win].filer_display.id,
				 &l_str_descrip,
				 &l_q->beg_y,
				 &l_q->beg_x,
				 0,
				 &SMG$M_BOLD,
				 0,
				 0))))
		        $LOG(l_rc);
                }
                else
                {
		    if (!(1 & (l_rc = smg$put_chars (
				 &cntrl_info_block.dir_dx[a_win].filer_display.id,
				 &l_str_descrip,
				 &l_q->beg_y,
				 &l_q->beg_x,
				 0,
				 0,
				 0,
				 0))))
		        $LOG(l_rc);
                }
            }
        }
	if (!(1 & (l_rc = smg$end_display_update (
		     &cntrl_info_block.dir_dx[a_win].filer_display.id))))
	    $LOG(l_rc);
    }
 
    remove_filelist_entry(&l_p);
 
    cntrl_info_block.dir_dx[a_win].filer_display.rows =
	max (cntrl_info_block.dir_dx[a_win].non_dir_filelist->backward->beg_y,
	     cntrl_info_block.dir_dx[a_win].filer_display.view_rows);
    if (cntrl_info_block.dir_dx[a_win].filer_display.rows <
	 cntrl_info_block.dir_dx[a_win].filer_display.view_rows)
    {
	cntrl_info_block.dir_dx[a_win].filer_display.rows =
	    cntrl_info_block.dir_dx[a_win].filer_display.view_rows;
    }
 
    if (cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y +
	    cntrl_info_block.dir_dx[a_win].filer_display.view_rows - 1 >
	cntrl_info_block.dir_dx[a_win].filer_display.rows)
    {
	cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y--;
	if (!(1 & (l_rc = smg$change_viewport (
		     &cntrl_info_block.dir_dx[a_win].filer_display.id,
		     &cntrl_info_block.dir_dx[a_win].filer_display.view_beg_y,
		     0,
		     0,
		     0))))
	    $LOG(l_rc);
    }
    if (!(1 & (l_rc = smg$change_virtual_display (
		 &cntrl_info_block.dir_dx[a_win].filer_display.id,
		 &cntrl_info_block.dir_dx[a_win].filer_display.rows,
		 &cntrl_info_block.dir_dx[a_win].filer_display.width,
		 0,
		 0,
		 0))))
        $LOG(l_rc);
 
    put_filer_stat(a_win);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Recomputes the row/column screen position of every entry in list a_l,
**	starting from (a_row, a_col) -- the same layout logic as
**	compute_filer_begin_xy() (FILER.C), but continuing from a given entry
**	instead of restarting at the head of the list.  Used after
**	deleting/inserting one entry, to reflow everything that follows it
**	instead of recomputing the whole window from scratch.
**
**  INPUT PARAMETERS:
**
**	a_l  -  first entry to reposition (its own row/column are overwritten).
**	a_row, a_col  -  starting screen position for a_l.
**	a_win  -  which window's display form (long/short) and column width
**		  to use.
**
**  OUTPUT PARAMETERS:
**
**	None (the position fields inside each list entry are updated in place).
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.dir_dx[a_win].form, FILER_COLUMN_WIDTH.
**
**  IMPLICIT OUTPUTS:
**
**	The beg_y/beg_x fields of every entry in a_l.
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	recompute_filer_beg_xy(
		struct fil_dx_tag *a_l,
		int a_row,int a_col,
		unsigned short int a_win)
{
    struct fil_dx_tag *l_p;
    int l_len;
 
    for (l_p = a_l; l_p != cntrl_info_block.dir_dx[a_win].non_dir_filelist; l_p = l_p->forward)
    {
        if (l_p == cntrl_info_block.dir_dx[a_win].dir_filelist)
        {
            l_p = cntrl_info_block.dir_dx[a_win].non_dir_filelist;
        }
	else if (cntrl_info_block.dir_dx[a_win].form == long_form)
	{
	    l_p->beg_y = a_row;
	    l_p->beg_x = a_col;
	    a_row++;
	}
	else
	{
	    l_len = strlen(l_p->fn);
	    if (l_len <= FILER_COLUMN_WIDTH-2)
	    {
		l_p->beg_y = a_row;
		l_p->beg_x = a_col;
		a_col += FILER_COLUMN_WIDTH;
	    }
	    else if (l_len <= 2*FILER_COLUMN_WIDTH-2)
	    {
		if (a_col > 2*FILER_COLUMN_WIDTH+1)
		{
		    a_row++;
		    a_col = 1;
		}
		l_p->beg_y = a_row;
		l_p->beg_x = a_col;
		a_col += 2*FILER_COLUMN_WIDTH;
	    }
	    else if (l_len <= 3*FILER_COLUMN_WIDTH-2)
	    {
		if (a_col > FILER_COLUMN_WIDTH+1)
		{
		    a_row++;
		    a_col = 1;
		}
		l_p->beg_y = a_row;
		l_p->beg_x = a_col;
		a_col += 3*FILER_COLUMN_WIDTH;
	    }
	    else
	    {
		if (a_col > 1)
		{
		    a_row++;
		    a_col = 1;
		}
		l_p->beg_y = a_row;
		l_p->beg_x = a_col;
		a_col += 4*FILER_COLUMN_WIDTH;
	    }
	    if (a_col > 4*FILER_COLUMN_WIDTH)
	    {
		a_row++;
		a_col = 1;
	    }
	}
    }
    return DX__NORMAL;
}



/* ---- (from FILERREN.C -- file rename) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      filer_rename() dispatches to filer_multiple_rename() or
**	filer_single_rename(), depending on whether the current window is in
**	single- or multiple-file mode.
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
**	DX__NORMAL (the value returned by whichever of filer_multiple_rename()/
**	filer_single_rename() runs).
**
**--
**/
int	filer_rename()
{
    return dxscr_dispatch_filemode(filer_multiple_rename, filer_single_rename);
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Prompts once for a new name (pattern), then applies it to every marked
**	file in turn via filer_rename$1(), using multi_get_userinput_and_exec().
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
**	The current window's marked-file list.
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_rename$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_multiple_rename()
{
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error renaming file");
    multi_get_userinput_and_exec(
	filer_rename$1,
	"Multiple Rename",
	"Rename to: ",
	"",
	l_errmsg);
     return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Prompts for a new name for DX_CURRENT_FILE and renames it via
**	get_userinput_and_execute()/filer_rename$1().  Refuses on the "[-]"
**	parent-directory placeholder entry.
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
**	None directly (see filer_rename$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_single_rename()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Type a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error renaming file");
	get_userinput_and_execute(
	    filer_rename$1,
	    "Rename File",
	    "New name: ",
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
**      The actual rename: refuses if the file is a directory currently open
**	in another window, calls sys$rename(), then removes the old display
**	entry and re-adds the renamed one (preserving its original size/dates/
**	protection) in every window, keeping the directory-tree cache in sync
**	if a directory file was renamed.
**
**  INPUT PARAMETERS:
**
**	a_to  -  new filespec.
**	a_from  -  original filespec being renamed.
**	a_errmsg  -  buffer to receive an error message on failure.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_FILE, cntrl_info_block (current/other windows).
**
**  IMPLICIT OUTPUTS:
**
**	The filer cache, the directory-tree cache (if applicable), and every
**	open window's display.
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success, DX__ERROR on failure.
**
**--
**/
int	filer_rename$1(a_to, a_from, a_errmsg)
char *a_to;
char *a_from;
char *a_errmsg;
{
    struct FAB l_from_fab;
    struct FAB l_to_fab;
    struct NAM l_to_nam;
    char l_dna[NAM$C_MAXRSS];
    char l_resultant_name[NAM$C_MAXRSS];
    char *l_cp;
    long int l_i;
    struct fil_dx_tag *l_f;
    struct fil_dx_tag l_temp_entry;
    int l_status;
    unsigned long l_rc;
    char l_direct_name[MAXFILESPEC+1];

    /*
    **	Make sure this file is not a directory file that is being
    **	used by the other window
    **/

    if (DX_CURRENT_FILE->fn[strlen(DX_CURRENT_FILE->fn) - 1] == ']')
    {
	if (dxscr_dir_open_elsewhere(DX_CURRENT_FILE->fid, cntrl_info_block.cur_win))
	{
	    strcpy(a_errmsg, "File in use by other window - renaming failed");
	    return DX__ERROR;
	}
    }

    /*
    **	Get old file characteristics before delete it.
    **/

    l_temp_entry.filesize = DX_CURRENT_FILE->filesize;
    memcpy(&(l_temp_entry.cdat), &DX_CURRENT_FILE->cdat, sizeof (DATE_TIME));
    memcpy(&(l_temp_entry.rdat), &DX_CURRENT_FILE->rdat, sizeof (DATE_TIME));
    memcpy(&(l_temp_entry.edat), &DX_CURRENT_FILE->edat, sizeof (DATE_TIME));
    memcpy(&(l_temp_entry.bdat), &DX_CURRENT_FILE->bdat, sizeof (DATE_TIME));
    l_temp_entry.fpro = DX_CURRENT_FILE->fpro;
    l_temp_entry.state = waiting;
    l_temp_entry.beg_y = l_temp_entry.beg_x = 0;
    l_temp_entry.forward = l_temp_entry.backward = NULL;

    /*
    **	Use RMS service to rename the file so that after completion,
    **	we can get the new full file specification.
    **/

    l_from_fab = cc$rms_fab;
    l_to_fab = cc$rms_fab;
    l_to_nam = cc$rms_nam;

    l_from_fab.fab$l_fna = a_from;
    l_from_fab.fab$b_fns = strlen(a_from);

    strcpy(l_dna, a_from);
    l_cp = strchr(l_dna, ';');
    *l_cp = EOS;

    l_to_fab.fab$l_fna = a_to;
    l_to_fab.fab$b_fns = strlen(a_to);
    l_to_fab.fab$l_dna = l_dna;
    l_to_fab.fab$b_dns = strlen(l_dna);
    l_to_fab.fab$l_nam = &l_to_nam;

    l_to_nam.nam$l_rsa = l_resultant_name;
    l_to_nam.nam$b_rss = NAM$C_MAXRSS;

    if (!(sys$rename (
	    &l_from_fab,
	    0,
	    0,
	    &l_to_fab) & 1))
    {
	{
	    static $DESCRIPTOR (l_fao_ctrl, "Error renaming !AZ");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = MAXFILESPEC;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = a_errmsg;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_from))))
		$LOG(l_rc);
	    a_errmsg[l_len] = EOS;
	}
	return DX__ERROR;
    }

    for (l_i = 0;  l_i <= 2;  l_i++)
    {
	l_temp_entry.fid[l_i] = l_to_nam.nam$w_fid[l_i];
	l_temp_entry.did[l_i] = l_to_nam.nam$w_did[l_i];
    }
    l_resultant_name[l_to_nam.nam$b_rsl] = EOS;
    strcpy(l_temp_entry.fn, strchr(l_resultant_name, ']')+1);

    /*
    **	Delete the file from the filer and udate the screen by
    **	first deleting the renamed file and then adding the new file
    **	to the filer if it is also in current directory.
    **/

    if (!(1 & (l_rc = smg$begin_pasteboard_update(&cntrl_info_block.pasteboard_id))))
	return $LOG(DX__ERROR);

    l_status = DX__ERROR;
    if (filer_delete$2(a_from, a_errmsg, cntrl_info_block.cur_win) != DX__ERROR)
    {
	/*
	**  Update the cache queue.
	**  Only the file name field needs to be changes.
	**/

	update_filer_cache(l_temp_entry, add_to_filer_cache, a_errmsg);

	/*
	**  Update directory queue if the file renamed is a
	**  directory file
	**/

	if (strlen(a_from) > 6 && !strcmp(a_from+strlen(a_from)-6, ".DIR;1"))
	{
	    /**  Delete the previous directory from cache  **/
	    strcpy(l_direct_name, a_from);
	    l_cp = strchr (l_direct_name, ']');
	    *l_cp = '.';
	    l_cp = strrchr (l_direct_name, '.');
	    *l_cp++ = ']';
	    *l_cp = EOS;
	    delete_from_direct_cache(l_direct_name, a_errmsg);

	    /**  Add the new directory to cache  **/
            strcpy(l_direct_name, l_resultant_name);
	    l_cp = strchr (l_direct_name, ']');
	    *l_cp = '.';
	    l_cp = strrchr (l_direct_name, '.');
	    *l_cp++ = ']';
	    *l_cp = EOS;
	    add_to_direct_cache(l_direct_name, a_errmsg);
	}

	/**  Add it to the FILER  **/
	if (add_to_filer(
		l_temp_entry, a_errmsg, cntrl_info_block.cur_win) != DX__ERROR)
	{
	    l_status = DX__NORMAL;

	    /**  Update other windows  **/
	    for (l_i = 0;  l_i < cntrl_info_block.windows;  l_i++)
	    {
		if (l_i != cntrl_info_block.cur_win)
		{
		    if (filer_delete$2(a_from, a_errmsg, l_i) != DX__ERROR)
			l_status = add_to_filer(l_temp_entry, a_errmsg, l_i);
		}
	    }
	}
    }

    if (!(1 & (l_rc = smg$end_pasteboard_update(&cntrl_info_block.pasteboard_id))))
	return $LOG(DX__ERROR);

    return l_status;
}



/* ---- (from FILERPUR.C -- file purge) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      filer_purge() dispatches to filer_multiple_purge() or
**	filer_single_purge(), depending on whether the current window is in
**	single- or multiple-file mode.
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
**	DX__NORMAL (the value returned by whichever of filer_multiple_purge()/
**	filer_single_purge() runs).
**
**--
**/
int	filer_purge()
{
    return dxscr_dispatch_filemode(filer_multiple_purge, filer_single_purge);
}
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Confirms with the user ("Are you sure? [YES/NO]"), then purges
**	superseded versions of every marked file in turn via filer_purge$1(),
**	using multi_get_userinput_and_exec().
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
**	The current window's marked-file list.
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_purge$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_multiple_purge()
{
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error purging file");
    multi_get_userinput_and_exec(
	filer_purge$1,
	"Multiple Purge",
	"Are you sure? [YES/NO] ",
	"",
	l_errmsg);

     return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Purges superseded versions of DX_CURRENT_FILE, honoring the user's
**	purge-confirmation preference (cntrl_info_block.user_pref.purge_confirm).
**	Refuses on the "[-]" parent-directory placeholder entry.
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
**	DX_CURRENT_FILE, cntrl_info_block.user_pref.purge_confirm,
**	cntrl_info_block.cur_win.
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_purge$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_single_purge()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Purge a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error purging file");
	if (cntrl_info_block.user_pref.purge_confirm)
	    get_userinput_and_execute(
		filer_purge$1,
		"Confirm Purge",
		"Purge this file? [YES/NO] ",
		"YES",
		l_errmsg,
		l_fn);
	else if (filer_purge$1("YES", l_fn, l_errmsg) == -1)
	    signal_err(l_errmsg, bell);
    }
     return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      After confirmation, scans the current window's directory-file list for
**	every version of a_fn's base name with a lower version number than
**	a_fn itself, and deletes each one via filer_delete$1().
**
**  INPUT PARAMETERS:
**
**	a_confirm  -  the user's YES/NO answer.
**	a_fn  -  filespec whose superseded versions should be purged.
**	a_errmsg  -  buffer to receive an error message on failure.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY (the file list scanned).
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_delete$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success; DX__DONTCARE if the user answered anything
**	other than YES (via dxscr_confirm_yes()).
**
**--
**/
int	filer_purge$1(a_confirm, a_fn, a_errmsg)
char *a_confirm;
char *a_fn;
char *a_errmsg;
{
    struct fil_dx_tag *l_p, *l_q;
    char l_buf[MAXFILESPEC+1];
    char l_name1[MAXFILESPEC+1], l_name2[MAXFILESPEC+1];
    char *l_cp;
    int l_ver1, l_ver2;
    int l_status;

    /*
    **	Verify that the user wants to continue with the
    **	purge operation.  Return to calling
    **	routine if the user does not enter "YES".
    **/

    if ((l_status = dxscr_confirm_yes(a_confirm, "PURGE", a_errmsg)) != DX__NORMAL)
	return l_status;

    /*
    **	Confirmed the purge operation.  Go ahead
    **	purge the file specified.
    **/

    strcpy(l_name1, a_fn);
    l_cp = strchr(l_name1, ';');
    *l_cp = EOS;
    l_ver1 = atoi (l_cp+1);
    l_p = DX_CURRENT_DIRECTORY.dir_filelist->forward;
    while (l_p != DX_CURRENT_DIRECTORY.non_dir_filelist)
    {
        if (l_p == DX_CURRENT_DIRECTORY.dir_filelist)
            l_p = DX_CURRENT_DIRECTORY.non_dir_filelist->forward;
	else
	{
	    l_q = l_p->forward;	/**  Save file to be processed next  **/
	    find_full_path_name(l_p->fn, l_buf, cntrl_info_block.cur_win);
	    strcpy(l_name2, l_buf);
            if ((l_cp = strchr(l_name2, ';')) != NULL)
            {
		*l_cp = EOS;
		if (!strcmp(l_name1, l_name2))
		{
		    l_ver2 = atoi (l_cp+1);
		    if (l_ver1 > l_ver2) {
			if (filer_delete$1("YES", l_buf, a_errmsg) == DX__ERROR)
			    signal_err(a_errmsg, bell);
		    }
		}
            }
	    l_p = l_q;
	}
    }

    return DX__NORMAL;
}



/* ---- (from FILERAPP.C -- file append) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      filer_append() dispatches to filer_multiple_append() or
**	filer_single_append(), depending on whether the current window is in
**	single- or multiple-file mode.
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
**	DX__NORMAL (the value returned by whichever of filer_multiple_append()/
**	filer_single_append() runs).
**
**--
**/
int	filer_append()
{
    return dxscr_dispatch_filemode(filer_multiple_append, filer_single_append);
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Prompts once for a destination, then appends every marked file to it
**	in turn via filer_append$1(), using multi_get_userinput_and_exec().
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
**	The current window's marked-file list.
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_append$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_multiple_append()
{
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error appending file");
    multi_get_userinput_and_exec(
	filer_append$1,
	"Multiple Append",
	"Append to: ",
	"",
	l_errmsg);
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Appends DX_CURRENT_FILE to a user-specified destination via
**	get_userinput_and_execute()/filer_append$1().  Refuses when
**	DX_CURRENT_FILE is itself a directory placeholder entry.
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
**	None directly (see filer_append$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_single_append()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (DX_CURRENT_FILE->fn[ strlen(DX_CURRENT_FILE->fn)-1 ] == ']')
        signal_err("Append to a directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error appending file");
	get_userinput_and_execute(
	    filer_append$1,
	    "Append File",
	    "Append to: ",
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
**      The actual append: opens the destination for GET|PUT access,
**	positions to end-of-file, then copies every record from the source
**	file onto the end of the destination.  Afterward updates the
**	destination's cache entry (size/dates/protection) and every window's
**	display via change_to_filer_cache()/change_to_filer(), since the
**	destination file's identity is unchanged but its size has grown.
**
**  INPUT PARAMETERS:
**
**	a_to  -  destination filespec (the file being appended to).
**	a_from  -  source filespec (the file being appended).
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an RMS error message on failure.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block (current/other windows).
**
**  IMPLICIT OUTPUTS:
**
**	The filer cache and every open window's display.
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success, DX__ERROR on failure.
**
**--
**/
int	filer_append$1(a_to, a_from, a_errmsg)
char *a_to;
char *a_from;
char *a_errmsg;
{
    static char l_linebuf[BUFSIZ+1];

    $DESCRIPTOR (l_linebuf_descriptor, l_linebuf);

    unsigned int l_retcode;
    char l_expand_name[NAM$C_MAXRSS];

    struct FAB l_from_fab;
    struct RAB l_from_rab;
    struct FAB l_to_fab;
    struct RAB l_to_rab;
    struct NAM l_to_nam;
    struct XABPRO l_to_xabpro;
    struct XABDAT l_to_xabdat;

    struct fil_dx_tag l_temp_entry;
    int l_i;

    /*
    **	Initialize Source File FAB
    **/

    l_from_fab = cc$rms_fab;
    l_from_fab.fab$l_fna = a_from;
    l_from_fab.fab$b_fns = strlen(a_from);

    /*
    **	Initialize Source File RAB
    **/

    l_from_rab = cc$rms_rab;
    l_from_rab.rab$l_fab = &l_from_fab;

    if (!(sys$open (
	    &l_from_fab,
	    0,
	    0) & 1))
    {
	strcpy(a_errmsg, "Insufficient privilege for read operation");
	return DX__ERROR;
    }

    /*
    **	Initialize Destination File FAB
    **/

    l_to_fab = cc$rms_fab;
    l_to_nam = cc$rms_nam;
    l_to_xabpro = cc$rms_xabpro;
    l_to_xabdat = cc$rms_xabdat;

    l_to_fab.fab$l_fna = a_to;
    l_to_fab.fab$b_fns = strlen(a_to);
    l_to_fab.fab$b_fac = FAB$M_GET | FAB$M_PUT;

    l_to_fab.fab$l_dna = a_from;
    l_to_fab.fab$b_dns = strcspn (a_from, ";");

    l_to_nam.nam$l_esa = l_expand_name;
    l_to_nam.nam$b_ess = NAM$C_MAXRSS;

    l_to_fab.fab$l_nam = &l_to_nam;
    l_to_fab.fab$l_xab = &l_to_xabpro;
    l_to_xabpro.xab$l_nxt = &l_to_xabdat;

    /*
    **	Initialize Source File RAB
    **/

    l_to_rab = cc$rms_rab;
    l_to_rab.rab$l_fab = &l_to_fab;

    if (!((l_retcode = sys$open (
			&l_to_fab,
			0,
			0)) & 1))
    {
	char l_errmsg1[256];
	$DESCRIPTOR (l_errmsg1_descrip, l_errmsg1);
	unsigned short l_len;

        sys$close (
            &l_from_fab,
            0,
            0);
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

    l_from_rab.rab$b_rac = RAB$C_SEQ;
    l_from_rab.rab$l_ubf = l_linebuf;
    l_from_rab.rab$w_usz = BUFSIZ;

    if (!((l_retcode = sys$connect (
			    &l_from_rab,
			    0,
			    0)) & 1))
    {
	sys$close(&l_from_fab, 0, 0);
	strcpy(a_errmsg, "Error connecting to source file");
	return DX__ERROR;
    }

    l_to_rab.rab$l_ubf = l_linebuf;
    l_to_rab.rab$w_usz = BUFSIZ;
    l_to_rab.rab$b_rac = RAB$C_SEQ;
    l_to_rab.rab$l_rbf = l_linebuf;

    if (!((l_retcode = sys$connect (
			    &l_to_rab,
			    0,
			    0)) & 1))
    {
	sys$close(&l_from_fab, 0, 0);
	sys$close(&l_to_fab, 0, 0);
	strcpy(a_errmsg, "Error connecting to destination file");
	return DX__ERROR;
    }

    while ((l_retcode = sys$get (
                          &l_to_rab,
                          0,
                          0)) != RMS$_EOF)
    {
        if (l_retcode != RMS$_NORMAL)
        {
            strcpy(a_errmsg, "Error positioning to EOF of destination file");
	    sys$close(&l_from_fab, 0, 0);
	    sys$close(&l_to_fab, 0, 0);
	    return DX__ERROR;
        }
    }

    while ((l_retcode = sys$get (
			  &l_from_rab,
			  0,
			  0)) != RMS$_EOF)
    {
	if (l_retcode != RMS$_NORMAL)
	{
	    strcpy(a_errmsg, "Error getting a record from source file");
	    sys$close(&l_from_fab, 0, 0);
	    sys$close(&l_to_fab, 0, 0);
	    return DX__ERROR;
	}

	l_to_rab.rab$w_rsz = l_from_rab.rab$w_rsz;
	if (!((l_retcode = sys$put (
			&l_to_rab,
			0,
			0)) & 1))
	{
	    sys$close(&l_from_fab, 0, 0);
	    sys$close(&l_to_fab, 0, 0);
	    strcpy(a_errmsg, "Error writing to destination file");
	    return DX__ERROR;
	}
    }

    if (!(sys$close (
	    &l_from_fab,
	    0,
	    0) & 1))
    {
	strcpy(a_errmsg, "Close source file failed");
	return DX__ERROR;
    }

    if (!(sys$close (
	    &l_to_fab,
	    0,
	    0) & 1))
    {
	strcpy(a_errmsg, "Close destination file failed");
	return DX__ERROR;
    }

    /**  File name  **/
    memcpy(
	l_temp_entry.fn,
	l_to_nam.nam$l_name,
	l_to_nam.nam$b_name+l_to_nam.nam$b_type+l_to_nam.nam$b_ver);
    l_temp_entry.fn[l_to_nam.nam$b_name+l_to_nam.nam$b_type+l_to_nam.nam$b_ver] = EOS;

    /**  File id and directory id  **/
    for (l_i = 0; l_i <= 2; l_i++)
    {
	l_temp_entry.fid[l_i] = l_to_nam.nam$w_fid[l_i];
	l_temp_entry.did[l_i] = l_to_nam.nam$w_did[l_i];
    }

    /**  File size  **/
    l_temp_entry.filesize = l_to_fab.fab$l_alq;

    /**  File protection  **/
    l_temp_entry.fpro = l_to_xabpro.xab$w_pro;

    /**  Creation date  **/
    l_temp_entry.cdat = l_to_xabdat.xab$q_cdt;

    /**  Revision date  **/
    l_temp_entry.rdat = l_to_xabdat.xab$q_rdt;

    /**  Expiration date  **/
    l_temp_entry.edat = l_to_xabdat.xab$q_edt;

    /**  Backup date  **/
    l_temp_entry.bdat = l_to_xabdat.xab$q_bdt;

    /*
    **	Update filer cache
    **/

    if (update_filer_cache(
	    l_temp_entry, change_to_filer_cache, a_errmsg) == DX__ERROR)
        signal_err(a_errmsg, bell);

    /*
    **	Update filer displays
    **/

    if (change_to_filer(l_temp_entry, a_errmsg, cntrl_info_block.cur_win)
	== DX__ERROR)
        return DX__ERROR;

    for (l_i = 0;  l_i < cntrl_info_block.windows;  l_i++)
    {
    	if (l_i != cntrl_info_block.cur_win)
	{
	    if (change_to_filer(l_temp_entry, a_errmsg, l_i) == DX__ERROR)
	    	 return DX__ERROR;
	}
    }

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Updates one existing display entry in place after its underlying file
**	changed size without changing identity (currently used only after an
**	append): finds the entry by file id, adjusts the window's running
**	total-block count and the entry's cached size, redraws just that one
**	line, and refreshes the status line.
**
**  INPUT PARAMETERS:
**
**	a_e  -  updated fil_dx_tag entry (only a_e.fid and a_e.filesize are
**		 used).
**	a_win  -  which window's cache/display to update.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  unused on this path (kept for signature symmetry with
**		      the other filer_xxx$2()-style workers).
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.dir_dx[a_win] (cache lists, counts, display).
**
**  IMPLICIT OUTPUTS:
**
**	cntrl_info_block.dir_dx[a_win] (cache lists, counts, display).
**
**  RETURNS VALUE:
**
**	DX__NORMAL always (a no-op if the file is not currently shown in
**	this window).
**
**--
**/
int	change_to_filer(
		struct fil_dx_tag a_e,
		char *a_errmsg,
		unsigned short int a_win)
{
    struct fil_dx_tag *l_p;
    char l_str[MAXFILESPEC+1];
    $DESCRIPTOR (l_str_descrip, l_str);
    unsigned long l_rc;

    /*
    **	Update current display.
    **	Find file location in current display.
    **/

    for (l_p = cntrl_info_block.dir_dx[a_win].dir_filelist->forward;
	 l_p != cntrl_info_block.dir_dx[a_win].non_dir_filelist; l_p = l_p->forward)
    {
        if (l_p == cntrl_info_block.dir_dx[a_win].dir_filelist)
            l_p = cntrl_info_block.dir_dx[a_win].non_dir_filelist;
	else
	{
	    if (a_e.fid[0] == l_p->fid[0] &&
		a_e.fid[1] == l_p->fid[1] &&
		a_e.fid[2] == l_p->fid[2])
		break;
	}
    }

    if (l_p == cntrl_info_block.dir_dx[a_win].non_dir_filelist)
    {	    /**  Modified file not in this display  **/
	return DX__NORMAL;
    }

    cntrl_info_block.dir_dx[a_win].tot_blocks += a_e.filesize - l_p->filesize;
    l_p->filesize = a_e.filesize;

    format_filer_entry(l_p, l_str, a_win);

    LENGTH(l_str_descrip) = strlen(l_str);
    if (!(1 & (l_rc = smg$put_chars(&cntrl_info_block.dir_dx[a_win].filer_display.id,
				     &l_str_descrip, &l_p->beg_y, &l_p->beg_x, 0, 0, 0, 0))))
	$LOG(l_rc);	/**  Display glitch, not fatal -- still update the
			**   status line below.  **/

    put_filer_stat(a_win);

    return DX__NORMAL;
}



/* ---- (from FILERDIF.C -- file compare) ---- */

static int		    filer_diff$2(char *, char *, char *);
static int		    diff_read(TXTINF a_f[], int *a_n, char *a_m, char *a_dna, char *a_errmsg);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      filer_diff() dispatches to filer_multiple_diff() or
**	filer_single_diff(), depending on whether the current window is in
**	single- or multiple-file mode.
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
**	DX__NORMAL (the value returned by whichever of filer_multiple_diff()/
**	filer_single_diff() runs).
**
**--
**/
int	filer_diff()
{
    return dxscr_dispatch_filemode(filer_multiple_diff, filer_single_diff);
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Prompts once for a file to compare every marked file against, then
**	applies filer_diff$1() to each marked file in turn via
**	multi_get_userinput_and_exec().
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
**	The current window's marked-file list.
**
**  IMPLICIT OUTPUTS:
**
**	None directly (see filer_diff$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_multiple_diff()
{
    char l_errmsg[MAXFILESPEC+1];

    strcpy(l_errmsg, "Error comparing file");
    multi_get_userinput_and_exec(
	filer_diff$1,
	"Multiple diff",
	"Compare with: ",
	"",
	l_errmsg);
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Compares DX_CURRENT_FILE against a user-specified file via
**	get_userinput_and_execute()/filer_diff$1().  Refuses on the "[-]"
**	parent-directory placeholder entry.
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
**	None directly (see filer_diff$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL.
**
**--
**/
int	filer_single_diff()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_fn[MAXFILESPEC+1];

    if (!strcmp(DX_CURRENT_FILE->fn, "[-]"))
        signal_err("Compare a parent directory file not allowed", bell);
    else
    {
	find_full_path_name(DX_CURRENT_FILE->fn, l_fn, cntrl_info_block.cur_win);
	strcpy(l_errmsg, "Error comparing file");
	get_userinput_and_execute(
	    filer_diff$1,
	    "Compare File",
	    "Compare with: ",
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
**      Thin adapter: get_userinput_and_execute()/multi_get_userinput_and_exec()
**	call their callback as (result, original, errmsg); this reorders the
**	arguments into (from, to, errmsg) and forwards to filer_diff$2().
**
**  INPUT PARAMETERS:
**
**	a_to  -  filespec to compare against, entered by the user.
**	a_from  -  filespec being compared (the marked/current file).
**	a_errmsg  -  buffer to receive an error message on failure.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg.
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
**	Whatever filer_diff$2() returns (DX__NORMAL, DX__ERROR or
**	DX__DONTCARE).
**
**--
**/
int	filer_diff$1(a_to, a_from, a_errmsg)
char *a_to;
char *a_from;
char *a_errmsg;
{
    return filer_diff$2(a_from, a_to, a_errmsg);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Reads both files into memory with diff_read(), saves the current
**	screen, runs the line-by-line comparison via compare() (DIFF.C),
**	frees the per-line buffers, then waits for the user to press RETURN
**	(or CTRL/Y to cancel a multiple-file compare) before restoring the
**	screen.
**
**  INPUT PARAMETERS:
**
**	a_from  -  first filespec to compare.
**	a_old_to  -  second filespec to compare (kept unmodified, unlike
**		     diff_read()'s own a_m parameter, so a multiple-compare's
**		     user-specified target is not silently rewritten).
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in on failure, or with "DIFF canceled by
**		      request" if the user pressed CTRL/Y.
**
**  IMPLICIT INPUTS:
**
**	cntrl_info_block.pasteboard_id.
**
**  IMPLICIT OUTPUTS:
**
**	The physical screen (saved and restored around the comparison output).
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success, DX__ERROR if either file could not be read or
**	opened, DX__DONTCARE if the user cancelled with CTRL/Y.
**
**--
**/
static int	filer_diff$2(a_from, a_old_to, a_errmsg)
char *a_from;
char *a_old_to;
char *a_errmsg;
{
    /**  file1[]/file2[] used to live on the stack as TXTINF[MAXLINES]
    **   (MAXLINES=65535) -- ~512KB apiece, ~1MB together, a real risk
    **   of stack overflow.  Heap-allocate them instead, same as every
    **   other sizeable buffer in this project.  **/
    TXTINF *l_file1;
    int l_n1;
    TXTINF *l_file2;
    int l_n2;
    unsigned int l_display_id;
    unsigned short l_word_terminator_code;
    int l_i;
    int l_vm_len;
    unsigned int l_old_mask;
    char l_to[MAXFILESPEC+1];
    unsigned long l_rc;

    /*
    **	Do not update the user-specified diff TO filespec
    **/

    strcpy(l_to, a_old_to);

    l_vm_len = MAXLINES * sizeof (TXTINF);
    if (!(lib$get_vm (&l_vm_len, &l_file1, 0) & 1))
    {
	strcpy(a_errmsg, "Insufficient virtual memory");
	return DX__ERROR;
    }
    if (!(lib$get_vm (&l_vm_len, &l_file2, 0) & 1))
    {
	strcpy(a_errmsg, "Insufficient virtual memory");
	lib$free_vm (&l_vm_len, &l_file1, 0);
	return DX__ERROR;
    }

    /*
    **	Read two files to be compared
    **/

    if (diff_read (l_file1, &l_n1, a_from, a_from, a_errmsg) == -1)
    {
	lib$free_vm (&l_vm_len, &l_file1, 0);
	lib$free_vm (&l_vm_len, &l_file2, 0);
	return DX__ERROR;
    }
    if (diff_read (l_file2, &l_n2, l_to, a_from, a_errmsg) == -1)
    {
	/**  file1 was read successfully -- free what it allocated,
	**   or its per-line buffers would leak on this path.  **/
	for (l_i = 1;  l_i <= l_n1;  l_i++)
	{
	    l_vm_len = l_file1[l_i].len+1;
	    lib$free_vm (&l_vm_len, &(l_file1[l_i].content), 0);
	}
	l_vm_len = MAXLINES * sizeof (TXTINF);
	lib$free_vm (&l_vm_len, &l_file1, 0);
	lib$free_vm (&l_vm_len, &l_file2, 0);
	return DX__ERROR;
    }

    /*
    **	Since compare() does not use SMG$ output routines to
    **	show the differences, need to save the current display
    **	and restore back after the comparison.
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

    /*
    **	Set the signal back to system default so that it won't
    **	interfere with smg$read_keystroke() routine.
    **/

    alarm(0);
    signal(SIGALRM, SIG_DFL);

    fputs("DX File Comparison Utility\n", stdout);
    {
	static $DESCRIPTOR (l_fao_ctrl, "    !AZ-!/    !AZ!/!/!/!/");
	struct dsc$descriptor_s l_out_desc;
	char l_scratch[2*MAXFILESPEC+16];
	unsigned short int l_len;
	unsigned long l_rc;

	l_out_desc.dsc$w_length  = sizeof(l_scratch) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_scratch;

	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_from, l_to))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	fputs(l_scratch, stdout);
    }

    /*
    **	Start the comparison of the two files.
    **	No paging mechanism is provided at present.
    **/

    compare (a_from, l_file1, l_n1, l_to, l_file2, l_n2, LNGVAL);

    /*
    **	Free up the dynamic memory used by DIFF
    **/

    for (l_i = 1;  l_i <= l_n1;  l_i++)
    {
	l_vm_len = l_file1[l_i].len+1;
        if (!(1 & (l_rc = lib$free_vm (
		   &l_vm_len,
		   &(l_file1[l_i].content),
		   0))))
            $LOG(l_rc);
    }
    for (l_i = 1;  l_i <= l_n2;  l_i++)
    {
	l_vm_len = l_file2[l_i].len+1;
        if (!(1 & (l_rc = lib$free_vm (
		   &l_vm_len,
		   &(l_file2[l_i].content),
		   0))))
            $LOG(l_rc);
    }

    l_vm_len = MAXLINES * sizeof (TXTINF);
    if (!(1 & (l_rc = lib$free_vm (&l_vm_len, &l_file1, 0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = lib$free_vm (&l_vm_len, &l_file2, 0))))
        $LOG(l_rc);

    /*
    **	Prompt for user input.
    **	CTRL/Y cancel multiple DIFF operations.  All other key
    **	puts the user back to DX.
    **/

    sleep(2);
    fputs("\n\n\nPress RETURN to continue -or- CTRL/Y to cancel multiple DIFF ", stdout);
    if (!(1 & (l_rc = lib$disable_ctrl (
	      &(unsigned int) LIB$M_CLI_CTRLY,
	      &l_old_mask))))
        $LOG(l_rc);
    smg$read_keystroke (
                 &cntrl_info_block.keyboard_id,
                 &l_word_terminator_code,
                 0,
                 0,
                 0,
                 0,
                 0);
    if (!(1 & (l_rc = lib$enable_ctrl (
	      &l_old_mask,
	      0))))
        $LOG(l_rc);

    /*
    **	Cleanup and exit
    **/

    if (!(1 & (l_rc = smg$restore_physical_screen (
	      &cntrl_info_block.pasteboard_id,
	      &l_display_id))))
        $LOG(l_rc);

    if (l_word_terminator_code == SMG$K_TRM_CTRLY)
    {
	strcpy(a_errmsg, "DIFF canceled by request");
	return DX__DONTCARE;
    }

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Reads one file into the a_f[] array of TXTINF records, allocating one
**	heap buffer per line via lib$get_vm() -- used by filer_diff$2() to
**	load both sides of a comparison.
**
**  INPUT PARAMETERS:
**
**	a_m  -  filespec to read (overwritten on return with the RMS-resolved
**		 result name).
**	a_dna  -  default filespec/version to apply when a_m is a bare
**		  wildcard.
**
**  OUTPUT PARAMETERS:
**
**	a_f[]  -  filled in with one entry per line read (1-based; a_f[0] is
**		   an unused sentinel).
**	a_n  -  set to the number of lines read.
**	a_m  -  overwritten with the RMS-resolved file name.
**	a_errmsg  -  filled in on failure.
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
**	DX__NORMAL on success, DX__ERROR if the file cannot be opened, is
**	too big for MAXLINES lines, or virtual memory is exhausted.
**
**--
**/
static int	diff_read(TXTINF a_f[], int *a_n, char *a_m, char *a_dna, char *a_errmsg)
{
    struct FAB l_fab;
    struct RAB l_rab;
    struct NAM l_nam;

    unsigned int l_retcode;
    int l_vm_len;
    char l_s[MAXCHARS+1];
    char *l_t;
    char l_resultant_name[NAM$C_MAXRSS];

    /**  Initialize File FAB  **/
    l_fab = cc$rms_fab;
    l_nam = cc$rms_nam;
    l_fab.fab$l_fna = a_m;
    l_fab.fab$b_fns = strlen(a_m);
    l_fab.fab$l_dna = a_dna;
    l_fab.fab$b_dns = strcspn(a_dna, ";")+1;    /**  Default to highest version  **/
    l_fab.fab$l_nam = &l_nam;

    l_nam.nam$l_rsa = l_resultant_name;
    l_nam.nam$b_rss = NAM$C_MAXRSS;

    /**  Initialize Source File RAB  **/
    l_rab = cc$rms_rab;
    l_rab.rab$l_fab = &l_fab;

    l_rab.rab$b_rac = RAB$C_SEQ;
    l_rab.rab$l_ubf = l_s;
    l_rab.rab$w_usz = MAXCHARS;

    /**  Open and file  **/
    if (!((l_retcode = sys$open (&l_fab, 0, 0)) & 1) ||
	!((l_retcode = sys$connect (&l_rab, 0, 0)) & 1))
    {
	sys$close(&l_fab, 0, 0);
	{
	    static $DESCRIPTOR (l_fao_ctrl, "Error opening !AZ as input file");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = MAXFILESPEC;
	    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	    l_out_desc.dsc$a_pointer = a_errmsg;

	    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_m))))
		$LOG(l_rc);
	    a_errmsg[l_len] = EOS;
	}
	return DX__ERROR;
    }

    l_resultant_name[l_nam.nam$b_rsl] = EOS;
    strcpy(a_m, l_resultant_name);

    *a_n = 0;

    while ((l_retcode = sys$get(&l_rab, 0, 0)) != RMS$_EOF)
    {
	if (*a_n >= MAXLINES-1)
	{
            /**  f[] holds MAXLINES entries, but is 1-based (index 0 is
            **   an unused sentinel -- see compare()'s "does not count
            **   0th element" convention), so the highest entry we may
            **   still write below at f[++(*n)] is f[MAXLINES-1].  **/
            {
                static $DESCRIPTOR (l_fao_ctrl, "File !AZ too big");
                struct dsc$descriptor_s l_out_desc;
                unsigned short int l_len;
                unsigned long l_rc;

                l_out_desc.dsc$w_length  = MAXFILESPEC;
                l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
                l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
                l_out_desc.dsc$a_pointer = a_errmsg;

                if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, a_m))))
                    $LOG(l_rc);
                a_errmsg[l_len] = EOS;
            }
	    sys$close (&l_fab, 0, 0);
	    return DX__ERROR;
	}

	l_vm_len = l_rab.rab$w_rsz + 1;
	if (!((l_retcode = lib$get_vm (
			     &l_vm_len,
			     &l_t,
			     0)) & 1))
	 {
	    strcpy(a_errmsg, "Error allocating virtual memory - exceeded quota");
	    sys$close (&l_fab, 0, 0);
	    return DX__ERROR;
	}

	memcpy (l_t, l_s, l_rab.rab$w_rsz);
	l_t[l_rab.rab$w_rsz] = EOS;
	a_f[++(*a_n)].content = l_t;
	a_f[*a_n].len = l_rab.rab$w_rsz;
    }

    sys$close (&l_fab, 0, 0);

    return DX__NORMAL;
}
