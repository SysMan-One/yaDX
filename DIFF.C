/*
**++
**  FACILITY:  DIFF.C
**
**  MODULE DESCRIPTION:
**
**      This module compares two arrays of lines (representing
**	files) and reports the sequences of consecutive matching
**	lines between them using the "recursive longest matching
**	sequence" algorithm.  
**
**  AUTHORS:
**	Steppe, Tom. "File Comparison Algorithms." Dr. Dobb's
**	Journal, #131 (Sep 1987): 54-60.
**
**  EDITORS:
**	StarLet Squad and Ruslan R. Laishev (RRL)
**
**  CREATION DATE:  2-21-1991
**
**
**  MODIFICATION HISTORY:
**
**	13-Sep-2026  RRL  a_/l_/$LOG naming convention applied; BOOLEAN
**			   typedef removed project-wide (this file's
**			   compare()/get_inf()/chk_hashes() now return
**			   unsigned long per the VMS condition-status
**			   convention: low bit set = success)
**	25-Sep-2026  RRL  ECO2: __NEW_STARLET prototypes, types fixed (0 CC diags)
**			   max()/min() guarded by #ifndef; txtcmp() prototyped
**	26-Sep-2026  CLD  ECO3: ident sync with DX.C (no code change)
**--
*/

#define __MODULE__	"DIFF"
#define __IDENT__	"X-02-05ECO3"
#define __REV__		"02.05.03"


/*
**
**  INCLUDE FILES
**
*/
#define	__NEW_STARLET	1

#include	<starlet.h>
#include	<rms.h>
#include	<descrip.h>
#include	<lib$routines.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "global.h"
#include "inquire.h"
#include "diff.h"


/*
**
**  MACRO DEFINITIONS
**
*/

#define TRUE	1
#define FALSE	0

/**  Maximum and Minimum macro  **/
#ifndef max		    /**  <stdlib.h> may already define them  **/
#define max(x, y)	(((x) > (y)) ? (x) : (y))
#endif
#ifndef min
#define min(x, y)	(((x) < (y)) ? (x) : (y))
#endif

/** Value to indicate identical strings with strcmp  **/
#define ALIKE	0

/**  Mask for number of bits in hash code (12 bits)  **/
#define MASK	(unsigned int) 0x0FFFF

/** Number of possible hash codes  **/
#define HASHSIZ	(MASK+1)

/*
**
**  GLOBAL DECLARATIONS
**
**/

int diff_sec,   		    /**  Number of diff sections  **/
    diff_line;    		    /**  Number of diff lines  **/

int diff_beg1,    		    /**  First line# for diff sec in file 1  **/
    diff_beg2;    		    /**  First line# for diff sec in file 2  **/

/*
**
**  INTERNAL FUNCTION PROTOTYPE DECLARATIONS
**
**/

static unsigned long get_inf (char *, TXTINF *, int, FILEINF *);
static HASH calc_hash (TXTINF);
static void fnd_seq (FILEINF *, int, int, FILEINF *, int, int, int);
static unsigned long chk_hashes (LINEINF *, LINEINF *, int);
static int cnt_matches (TXTINF *, TXTINF *, int);
static void rpt_seq (FILEINF *, int, FILEINF *, int, int);
static int  rpt_put_line (struct FAB *, struct RAB *, int);
static void rpt_seq_1 (FILEINF *, int, int, FILEINF *, int, int);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      compare() compares two arrays of lines and reports the
**	sequences of consecutive matching lines.  The zeroth
**	element of each array is unused so that the index into
**	the array is identical to the associated line number.
**
**  RETURN VALUE:
**
**      TRUE if comparison succeeded.
**	FALSE if not enough memory.
**
**--
*/
unsigned long compare (a_r1, a_a1, a_n1, a_r2, a_a2, a_n2, a_lngval)
char *a_r1;	/**  File name for #1  **/
TXTINF *a_a1;	/**  Array of lines of text in #1  **/
int a_n1;	/**  Number of lines in a_a1 (does not count 0th element)  **/
char *a_r2;	/**  File name for #2  **/
TXTINF *a_a2;	/**  Array of lines of text in #2  **/
int a_n2;	/**  Number of lines in a_a2 (does not count 0th element)  **/
int a_lngval;	/**  "Long enough" value  **/
{
    FILEINF l_f1 = {0},	/**  File info for #1  **/
	    l_f2 = {0};	/**  File info for #2  **/
    unsigned long l_rtn;	/**  Return value  **/
    int l_bytlen;	/**  Byte count for lib$free_vm  **/

    /**  Initialize  **/
    diff_sec = diff_line = 0;
    diff_beg1 = diff_beg2 = 0;

    /**  Gather info for each file, then compare  **/
    if (l_rtn = (get_inf (a_r1, a_a1, a_n1, &l_f1) && get_inf (a_r2, a_a2, a_n2, &l_f2)))
    {
        fnd_seq (&l_f1, 1, a_n1, &l_f2, 1, a_n2, a_lngval);

	/**  Output remaining difference lines  **/
	if (!(diff_beg1 == a_n1 && diff_beg2 == a_n2))
	{
	    rpt_seq_1 (&l_f1, a_n1, a_n1-diff_beg1, &l_f2, a_n2, a_n2-diff_beg2);
	    diff_line++;
	}
    }

    /**  Output summary report  **/
    {
	static $DESCRIPTOR (l_fao_ctrl1, "\nNumber of difference sections found: !SL\n");
	static $DESCRIPTOR (l_fao_ctrl2, "Number of difference records found: !SL\n");
	struct dsc$descriptor_s l_out_desc;
	char l_scratch[80];
	unsigned short int l_len;
	unsigned long l_rc;

	l_out_desc.dsc$w_length  = sizeof(l_scratch) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_scratch;

	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl1, &l_len, &l_out_desc, diff_sec))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	fputs(l_scratch, stdout);

	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl2, &l_len, &l_out_desc, diff_line))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	fputs(l_scratch, stdout);
    }

    /**
    **	Return dynamic memory to pool.
    **	l_f1/l_f2 are zero-initialized above, and get_inf() clears both
    **	members on entry, so a pointer here is either NULL (nothing to
    **	free -- e.g. get_inf() bailed out before allocating it, or the
    **	file has no lines) or a block actually obtained via lib$get_vm.
    **/
    if (l_f1.line != NULL)
    {
	l_bytlen = (a_n1+1) * sizeof (LINEINF);
	lib$free_vm (&l_bytlen, &l_f1.line, 0);
    }
    if (l_f1.hashtbl != NULL)
    {
	l_bytlen = HASHSIZ * sizeof (TBLENTRY);
	lib$free_vm (&l_bytlen, &l_f1.hashtbl, 0);
    }
    if (l_f2.line != NULL)
    {
	l_bytlen = (a_n2+1) * sizeof (LINEINF);
	lib$free_vm (&l_bytlen, &l_f2.line, 0);
    }
    if (l_f2.hashtbl != NULL)
    {
	l_bytlen = HASHSIZ * sizeof (TBLENTRY);
	lib$free_vm (&l_bytlen, &l_f2.hashtbl, 0);
    }

    return l_rtn;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      get_inf() calculates hash codes and builds a hash table
**
**
**  RETURN VALUE:
**
**      TRUE if get_inf() succeeded.
**	FALSE if not enough memory.
**
**--
*/
static unsigned long get_inf (a_r, a_a, a_n, a_f)
char *a_r;	    /**  File name  **/
TXTINF *a_a;	    /**  Array of lines of text  **/
int  a_n;	    /**  Number of lines in a_a  **/
FILEINF *a_f;	    /**  File info  **/
{
    int l_size;			/**  Size of hash table/line array (bytes)  **/
    register int l_i;		/**  Counter  **/
    TBLENTRY *l_entry;		/**  Entry in hash table  **/

    a_f->filename = a_r;	/**  Assign the file name  **/
    a_f->txt = a_a;		/**  Assign the array of text  **/
    a_f->hashtbl = NULL;	/**  Nothing allocated yet -- keeps compare()'s   **/
    a_f->line = NULL;		/**  clean-up pass safe if we bail out below.     **/

    /**  Allocate and initialize a hash table  **/
    l_size = HASHSIZ * sizeof (TBLENTRY);
    if (!(lib$get_vm (&l_size, &a_f->hashtbl, 0) & 1))
        return (FALSE);
    memset ((char *) a_f->hashtbl, '\0', l_size);

    if (a_n <= 0)
    {	/**  There is no lines  **/
        a_f->line = NULL;
    }
    else
    {
        /**  Allocate an array of line structs  **/
        l_size = (a_n+1) * sizeof (LINEINF);
        if (!(lib$get_vm (&l_size, &a_f->line, 0) & 1))
            return (FALSE);
        for (l_i = 1;  l_i <= a_n;  l_i++)
        {   /**  Loop through the lines  **/
            a_f->line[l_i].hash = calc_hash (a_f->txt[l_i]);	/**  Calculate hash  **/
	    l_entry = a_f->hashtbl + a_f->line[l_i].hash;	/**  Locate entry  **/

	    /**  Update the linked list of lines iwth the same hash code  **/
	    a_f->line[l_entry->last].nxtln = l_i;
	    a_f->line[l_i].nxtln = 0;

	    /** Update the first and last line info in the hash table  **/
            if (l_entry->frst == 0)
                l_entry->frst = l_i;
            l_entry->last = l_i;
        }
    }
    return (TRUE);
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      calc_hash() calculates a hash code for a line of text.
**
**  RETURN VALUE:
**
**      a hash code value.
**
**--
*/
static HASH calc_hash (a_buf)
TXTINF a_buf;	/** Line of text  **/
{
    register unsigned int l_chksum;   /**  Checksum  **/
    register int	  l_i;	    /**  Counter  **/
    char		  *l_s;	    /**  Pointer  **/
    HASH		  l_hash;   /**  Hash code value  **/

    /**  Build up a checksum of the characters in the text   **/
    for (l_chksum = 0, l_s = a_buf.content, l_i = 0;  l_i < a_buf.len;  l_chksum ^= *l_s++, l_i++)
    {
        ;
    }

    /**  Combine the 7-bit checksum and as much of the length as is possible **/
    l_hash = ((l_chksum & 0x7F) | ((l_s - a_buf.content) << 7)) & MASK;

    return (l_hash);
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      fnd_seq() finds a "good sequence" of lines within the given
**	starting and ending line numbers.  fnd_seq() then recursively
**	finds "good sequences" in the sections of lines above the
**	"good sequence" and below it.
**
**  RETURN VALUE:
**
**      none
**
**--
*/
static void fnd_seq (a_f1, a_beg1, a_end1, a_f2, a_beg2, a_end2, a_lngval)
FILEINF	    *a_f1;	/**  File info for #1  **/
int	    a_beg1;	/**  First line # to compare in #1  **/
int	    a_end1;	/**  Last line # to compare in #1  **/
FILEINF     *a_f2;      /**  File info for #2  **/
int         a_beg2;     /**  First line # to compare in #2  **/
int         a_end2;     /**  Last line # to compare in #2  **/
int	    a_lngval;	/**  "Long enough" value  **/
{
    LINEINF *l_line1,	/**  Line info ptr in #1  **/
	    *l_line2;	/**  Line info ptr in #2  **/
    register int l_limit;	/**  Looping limit  **/
    int	    l_ln1,	/**  Line number in #1  **/
	    l_ln2;	/**  Line number in #2  **/
    register int l_ln;	/**  Working line number  **/
    int l_go;	/**  Continue to loop?  **/
    int l_most,		/**  Longest possible seq  **/
	l_most1,	/**  Longest possible due to #1  **/
	l_most2;	/**  Longest possible due to #2  **/
    int l_cnt,		/**  Length of longest seq  **/
	l_oldcnt,	/**  Length of prev longest seq  **/
	l_n,		/**  Length of cur longest seq  **/
	l_m1,		/**  Line of longest seq in #1  **/
        l_m2;           /**  Line of longest seq in #2  **/

    /**  Initialize  **/
    l_go = TRUE;
    l_line1 = a_f1->line;
    l_line2 = a_f2->line;

    /**  Initialize longest sequence info  **/
    l_cnt = 0;		/**  Length of longest seq  **/
    l_m1 = a_beg1 - 1;	/**  Line # of longest seq in #1  **/
    l_m2 = a_beg2 - 1;  /**  Line # of longest seq in #2  **/
    l_oldcnt = 0;	/**  Length of prev longest seq  **/

    /*
    **  Calculate max possible number of consecutive lines that
    **	can match (based on line # range)
    **/
    l_most1 = a_end1 - a_beg1 + 1;
    l_most2 = a_end2 - a_beg2 + 1;

    /*
    **	Scan lines looking for a "good sequence".
    **	Compare lines in the following order of line numbers:
    **
    **			    (1, 1)
    **		    (1, 2), (2, 1), (2, 2)
    **	    (1, 3), (2, 3), (3, 1), (3, 2), (3, 3)
    **			      ...
    **/
    for (l_ln1 = a_beg1, l_ln2 = a_beg2;  TRUE;  l_ln1++, l_ln2++)
    {
        if (l_ln2 <= a_end2 - l_cnt)
	    /*
	    **	There are enough lines left in #2 such that it
	    **	is possible to find a longer sequence.
	    **/
	{
            /*
	    **  Determine the limit in #1 that both
	    **	enforces the order scheme and still makes
	    **	it possible to find a longer sequence.
	    **/
	    l_limit = min (l_ln1 - 1, a_end1 - l_cnt);

	    /**  Calculate first potential match in #1  **/
            for (l_ln = a_f1->hashtbl[l_line2[l_ln2].hash].frst;
		    l_ln && l_ln < a_beg1;  l_ln = l_line1[l_ln].nxtln)
            {
                ;
            }

	    /**  Loop through the lines in #1  **/
            for (;  l_ln && l_ln <= l_limit;  l_ln = l_line1[l_ln].nxtln)
            {
                if (l_line1[l_ln].hash == l_line2[l_ln2].hash &&
		    l_line1[l_ln+l_cnt].hash == l_line2[l_ln2+l_cnt].hash &&
		    !(l_ln-l_m1 == l_ln2-l_m2 && l_ln < l_m1+l_cnt && l_m1 != a_beg1-1))
                {   /*
		    **  A candidate for a longer sequence has been found
		    **  The current lines match, the currents + cnt match,
		    **	and this sequence is not a subset of the longest
		    **  sequence so far.
		    **/

                    /**  Calculate most possible matches  **/
                    l_most = min (a_end1-l_ln+1, l_most2);

		    /*
		    **	First compare hash codes.  If the number of matches
		    **	exceeds the longest sequence so far, then compare
		    **	the actual text.
		    **/
                    if (chk_hashes(l_line1+l_ln, l_line2+l_ln2, l_cnt) &&
			(l_n = cnt_matches (a_f1->txt+l_ln, a_f2->txt+l_ln2, l_most)) > l_cnt)
                    {	/**  This is the longest seq. so far  **/
                        /**  Update longest seq info  **/
			l_oldcnt = l_cnt;
			l_cnt = l_n;
			l_m1 = l_ln;
			l_m2 = l_ln2;

			/**  If it's long enough, end the search  **/
                        if (l_cnt >= a_lngval)
                            break;

			/** Update limit, using new count  **/
			l_limit = min (l_ln1 - 1, a_end1 - l_cnt);
                    }
                }
            }
	    /**  If it's long enough, end the search  **/
            if (l_cnt >= a_lngval)
                break;
	    l_most2--;
        }
        else
            l_go = FALSE;   /**  EOF of this is reached  **/

	/**  Repeat the process for the other file  **/
        if (l_ln1 <= a_end1 - l_cnt)
        {
            l_limit = min (l_ln2, a_end2 - l_cnt);
            for (l_ln = a_f2->hashtbl[l_line1[l_ln1].hash].frst;
		    l_ln && l_ln < a_beg2;  l_ln = l_line2[l_ln].nxtln)
            {
                ;
            }
            for (;  l_ln && l_ln <= l_limit;  l_ln = l_line2[l_ln].nxtln)
            {
                if (l_line1[l_ln1].hash == l_line2[l_ln].hash &&
		    l_line1[l_ln1+l_cnt].hash == l_line2[l_ln+l_cnt].hash &&
		    !(l_ln1-l_m1 == l_ln-l_m2 && l_ln1 < l_m1+l_cnt && l_m2 != a_beg2-1))
                {
                    l_most = min (a_end2 - l_ln + 1, l_most1);
                    if (chk_hashes (l_line1 + l_ln1, l_line2 + l_ln, l_cnt) &&
			(l_n = cnt_matches (a_f1->txt + l_ln1,
					    a_f2->txt + l_ln, l_most)) > l_cnt)
                    {
                        l_oldcnt = l_cnt;
			l_cnt = l_n;
			l_m1 = l_ln1;
			l_m2 = l_ln;

                        if (l_cnt >= a_lngval)
                            break;
			l_limit = min (l_ln2, a_end2 - l_cnt);
                    }
                }
            }
            if (l_cnt >= a_lngval)
                break;
	    l_most1--;
        }
        else if (!l_go)
        {
            break;	/**  EOF of this file is reached  **/
        }
    }

    /*
    **  If the longest seq is shorter than the "long engoug" value,
    **	the "long enough" value can be adjusted for the rest of
    **	the comparison process.
    **/
    if (l_cnt < a_lngval)
        a_lngval = l_cnt;

    if (l_cnt >= 1)	/**  Longest seq exceeds min necessary size  **/
    {
        if (l_m1 != a_beg1 && l_m2 != a_beg2 && l_oldcnt > 0)
	    /*
	    **	There is still something worgh comparing previous to
	    **	the seq.
	    **/
        {
            /**  Use knowledge of the previous longest seq  **/
	    fnd_seq (a_f1, a_beg1, l_m1 - 1, a_f2, a_beg2, l_m2 - 1, l_oldcnt);
        }

	/**  Report the seq  **/
	rpt_seq (a_f1, l_m1, a_f2, l_m2, l_cnt);

        if (l_m1 + l_cnt - 1 != a_end1 && l_m2 + l_cnt - 1 != a_end2)
	    /**  There is still someghing worth comparing subseq to the seq **/
        {
            fnd_seq (a_f1, l_m1 + l_cnt, a_end1, a_f2, l_m2 + l_cnt, a_end2, a_lngval);
        }
    }
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      chk_hashes() determines whether this seq of matching
**	hash codes is longer than cnt.  It knows that the first
**	pair of hash codes is guaranteed to match.
**
**  RETURN VALUE:
**
**      TRUE if this seq is longer than cnt.
**	FALSE if this seq is not longer than cnt.
**
**--
*/
static unsigned long chk_hashes (a_line1, a_line2, a_cnt)
LINEINF	    *a_line1,	    /**  Line info for #1 **/
	    *a_line2;	    /**  Line info for #2 **/
register int a_cnt;	    /**  Counter to try to exceed  **/
{
    register int l_n;	    /**  Counter of consecutive matches  **/

    for (l_n = 1;  l_n <= a_cnt && (++a_line1)->hash == (++a_line2)->hash;  l_n++)
    {
        ;
    }
    return (l_n > a_cnt);
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      cnt_matches() counts the numbner of consecutive matching
**	lines of text.
**
**  RETURN VALUE:
**
**      Number of consecutive matching lines.
**
**--
*/
static int cnt_matches (a_s1, a_s2, a_most)
TXTINF *a_s1,		/**  Starting line in file #1  **/
       *a_s2;		/**  Starting line in file #2  **/
register int a_most;	/**  Most matching lines possible  **/
{
    register int l_n;	/**  Count of consecutive matches  **/

    /**  Count the consecutive matches  **/
    for (l_n = 0;  l_n < a_most && txtcmp (*a_s1++, *a_s2++) == ALIKE;  l_n++)
    {
        ;
    }
    return (l_n);
}

int txtcmp (a_s, a_t)
TXTINF a_s,
       a_t;
{
    register int l_i;
    int l_c;

    if (a_s.len > a_t.len)
    {
    	return 1;
    } else if (a_s.len < a_t.len)
    {
	return -1;
    } else
    {
	for (l_i = 0;  l_i < a_s.len;  l_i++)
	{
	    if ((l_c = a_s.content[l_i]-a_t.content[l_i]))
	    	 return (l_c > 0? 1 : -1);
	}
    }
    return 0;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      rpt_seq() reports a matching seq of lines
**
**  RETURN VALUE:
**
**      None
**
**--
*/
static void rpt_seq (a_f1, a_m1, a_f2, a_m2, a_cnt)
FILEINF	    *a_f1;	/**  File info for #1  **/
int	    a_m1;	/**  Location of matching seq in #1  **/
FILEINF     *a_f2;      /**  File info for #2  **/
int	    a_m2;	/**  Location of matching seq in #2  **/
int	    a_cnt;	/**  Number of lines in matching seq  **/
{
    if (!(a_m1 == 1 && a_m2 == 1))
	rpt_seq_1 (a_f1, a_m1, a_cnt, a_f2, a_m2, a_cnt);

    diff_beg1 = a_m1+a_cnt-1;
    diff_beg2 = a_m2+a_cnt-1;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      rpt_put_line() writes one line to the diff report file (SYS$OUTPUT).
**	RMS errors are reported through the project's own SIGNAL_ERR(), with
**	the real status decoded via LIB$SYS_GETMSG() (the same pattern used
**	for every other RMS error path in this codebase) -- not through
**	fprintf(stderr,...), which has nowhere reliable to go from inside
**	the interactive full-screen SMG session, and not through a fixed
**	"Insufficient privilege" guess regardless of the actual cause.  On
**	error the file is also closed, since every caller needs that done
**	before giving up.
**
**  RETURN VALUE:
**
**      Non-zero if the line was written, zero if the write failed (the
**	file has already been closed in that case).
**
**--
*/
static int	rpt_put_line (a_fab, a_rab, a_len)
struct FAB *a_fab;
struct RAB *a_rab;
int a_len;
{
    unsigned int l_retcode;
    unsigned short l_msglen;
    char l_errmsg[MAXFILESPEC+1];
    $DESCRIPTOR (l_errmsg_descrip, l_errmsg);

    a_rab->rab$w_rsz = a_len;
    if (!((l_retcode = sys$put (a_rab, 0, 0)) & 1))
    {
	lib$sys_getmsg (&l_retcode, &l_msglen, &l_errmsg_descrip, 0, 0);
	l_errmsg[l_msglen] = EOS;
	sys$close (a_fab, 0, 0);
	signal_err(l_errmsg, bell);
	return 0;
    }
    return 1;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      rpt_seq_1() ouputs diff report for a section
**
**  RETURN VALUE:
**
**      None
**
**--
*/
static void rpt_seq_1 (a_f1, a_m1, a_cnt1, a_f2, a_m2, a_cnt2)
FILEINF	    *a_f1;	/**  File info for #1  **/
int	    a_m1;	/**  Location of matching seq in #1  **/
int	    a_cnt1;	/**  Number of lines in matching seq in #1  **/
FILEINF     *a_f2;      /**  File info for #2  **/
int	    a_m2;	/**  Location of matching seq in #2  **/
int	    a_cnt2;	/**  Number of lines in matching seq in #2  **/
{
    struct FAB l_fab;
    struct RAB l_rab;

    unsigned int l_retcode;
    unsigned short l_msglen;
    char l_errmsg[MAXFILESPEC+1];
    $DESCRIPTOR (l_errmsg_descrip, l_errmsg);
    static char l_linebuf[MAXCHARS+1];
    $DESCRIPTOR (l_linebuf_descriptor, l_linebuf);
    char *l_filename = "SYS$OUTPUT";
    int l_i;
    int l_len;
    static $DESCRIPTOR (l_fao_ctrl_file, "File !AZ\r\n");
    static $DESCRIPTOR (l_fao_ctrl_num, "!6ZL   ");
    struct dsc$descriptor_s l_out_desc;
    unsigned short int l_fao_len;
    unsigned long l_rc;

    l_out_desc.dsc$w_length  = sizeof(l_linebuf) - 1;
    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
    l_out_desc.dsc$a_pointer = l_linebuf;

    /*
    **	Initialize FAB
    **/

    l_fab = cc$rms_fab;
    l_fab.fab$l_fna = l_filename;
    l_fab.fab$b_fns = strlen(l_filename);

    /*
    **	    Initialize Source File RAB
    **/

    l_rab = cc$rms_rab;
    l_rab.rab$l_fab = &l_fab;

    /*
    **	Report difference
    **/

    if (!((l_retcode = sys$open (&l_fab, 0, 0)) & 1))
    {
	lib$sys_getmsg (&l_retcode, &l_msglen, &l_errmsg_descrip, 0, 0);
	l_errmsg[l_msglen] = EOS;
	signal_err(l_errmsg, bell);
	return;
    }

    l_rab.rab$b_rac = RAB$C_SEQ;
    l_rab.rab$l_rbf = l_linebuf;

    if (!((l_retcode = sys$connect (&l_rab, 0, 0)) & 1))
    {
	lib$sys_getmsg (&l_retcode, &l_msglen, &l_errmsg_descrip, 0, 0);
	l_errmsg[l_msglen] = EOS;
	sys$close(&l_fab, 0, 0);
	signal_err(l_errmsg, bell);
	return;
    }

    strcpy (l_linebuf, "************\r\n");
    if (!rpt_put_line (&l_fab, &l_rab, sizeof ("************\r\n") - 1))
	return;

    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl_file, &l_fao_len, &l_out_desc, a_f1->filename))))
	$LOG(l_rc);
    l_linebuf[l_fao_len] = EOS;
    if (!rpt_put_line (&l_fab, &l_rab, strlen(l_linebuf)))
	return;

    for (l_i = diff_beg1+1;  l_i <= a_m1;  l_i++)
    {
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl_num, &l_fao_len, &l_out_desc, l_i))))
	    $LOG(l_rc);
	l_linebuf[l_fao_len] = EOS;
	l_len = strlen (l_linebuf);
	memcpy (l_linebuf+l_len, a_f1->txt[l_i].content, a_f1->txt[l_i].len);
	memcpy (l_linebuf+l_len+a_f1->txt[l_i].len, "\r\n", 2);
	if (!rpt_put_line (&l_fab, &l_rab, a_f1->txt[l_i].len+l_len+2))
	    return;
    }

    strcpy (l_linebuf, "******\r\n");
    if (!rpt_put_line (&l_fab, &l_rab, sizeof ("******\r\n") - 1))
	return;

    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl_file, &l_fao_len, &l_out_desc, a_f2->filename))))
	$LOG(l_rc);
    l_linebuf[l_fao_len] = EOS;
    if (!rpt_put_line (&l_fab, &l_rab, strlen(l_linebuf)))
	return;

    for (l_i = diff_beg2+1;  l_i <= a_m2;  l_i++)
    {
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl_num, &l_fao_len, &l_out_desc, l_i))))
	    $LOG(l_rc);
	l_linebuf[l_fao_len] = EOS;
	l_len = strlen (l_linebuf);
	memcpy (l_linebuf+l_len, a_f2->txt[l_i].content, a_f2->txt[l_i].len);
	memcpy (l_linebuf+l_len+a_f2->txt[l_i].len, "\r\n", 2);
	if (!rpt_put_line (&l_fab, &l_rab, a_f2->txt[l_i].len+l_len+2))
	    return;
    }

    /*
    **	Final separator: unlike every write above, a failure here does
    **	not abort -- DIFF_SEC/DIFF_LINE are still updated and the file
    **	still closed, matching the original behaviour of this function.
    */

    strcpy (l_linebuf, "************\r\n");
    l_rab.rab$w_rsz = sizeof ("************\r\n") - 1;
    if (!((l_retcode = sys$put (&l_rab, 0, 0)) & 1))
    {
	lib$sys_getmsg (&l_retcode, &l_msglen, &l_errmsg_descrip, 0, 0);
	l_errmsg[l_msglen] = EOS;
	signal_err(l_errmsg, bell);
    }

    diff_sec++;
    diff_line += max (a_m1-diff_beg1-1, a_m2-diff_beg2-1);

    sys$close (&l_fab, 0, 0);
}
