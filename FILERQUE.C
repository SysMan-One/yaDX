/* Program Name            : FILERQUE.C                                 */
/*   Original Author       : C. K. Hung					*/
/*   Editor                : StarLet Squad and Ruslan R. Laishev (RRL)  */
/*   Date                  : AUG-5-1991					*/
/*   Program Description   : Filer cache + directory scan/sort/filter   */
/*                           -- consolidated 14-Sep-2026 from           */
/*                           FILERQUE.C + FINDFILE.C + FILERSRT.C +     */
/*                           FILERFLT.C.                                */
/*                         :                                            */
/* Revision History follows                                             */
/*  25-Sep-2026 RRL ECO2: __NEW_STARLET prototypes, types fixed (0 CC diags)  */
/*                        FIBDEF/SBKDEF typedefs; sys$qiow IOSB/P2-P6 casts  */
/*  26-Sep-2026 CLD ECO3: ident sync with DX.C (no code change)         */

#define __MODULE__	"FILERQUE"
#define __IDENT__	"X-02-05ECO3"
#define __REV__		"02.05.03"
 
#define	__NEW_STARLET	1
#include	<starlet.h>
#include	<lib$routines.h>
#include	<str$routines.h>
#include	<libdef.h>
#include	<atrdef.h>
#include	<fibdef.h>
#include	<iodef.h>
#include	<sbkdef.h>
#include	<strdef.h>
#include	<signal.h>

#include "global.h"
#include "dx.h"
#include "inquire.h"
#include "filerque.h"
#include "filer.h"
#include "filersta.h"
#include "filercpy.h"
 
/*
**
**  INTERNAL FUNCTION PROTOTYPING
**
**/
 
static int	initialize_filer_cache$1(struct filer_cache_tag *);
static int	search_filer_cache$1(struct cur_filter_tag, struct cur_filter_tag);
static int	cmp_filespec(struct filespec_list_tag *, struct filespec_list_tag *);
static int      display_filer_cache(int, struct display_tag);
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Resets every slot of the filer cache (filer_cache[MAXBUFFER]) to its
**	empty/default state, by calling initialize_filer_cache$1() once per
**	slot.  Called once at DX startup.
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
**	filer_cache[] -- every slot is reset.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	    initialize_filer_cache()
{
    int l_i;
 
    for (l_i = 0;  l_i < MAXBUFFER;  l_i++)
    {
	initialize_filer_cache$1(&filer_cache[l_i]);
    }
     return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Resets one filer_cache_tag slot to its empty/default state: the
**	oldest possible timestamp (so it is picked first by
**	find_LRU_in_filer_cache() until genuinely used), default sort
**	criteria, empty directory names, zeroed file/block counts, an empty
**	filter (no name/exclude patterns, full date range, no size limit),
**	and NULL file lists.
**
**  INPUT PARAMETERS:
**
**	a_e  -  the cache slot to reset.
**
**  OUTPUT PARAMETERS:
**
**	a_e  -  every field is overwritten with its default value.
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
**	DX__NORMAL, always.
**
**--
**/
static int	    initialize_filer_cache$1(a_e)
struct filer_cache_tag *a_e;
{
    convert_date_string (
	"17-NOV-1858 00:00:00.00", &a_e->time_stamp);
    a_e->sortby = sortname;
    a_e->sortorder = ascending;
    a_e->cur_dir[0] = EOS;
    a_e->par_dir[0] = EOS;
    a_e->tot_files = 0;
    a_e->tot_blocks = 0;
    a_e->dpro = 0xFFFF;
    a_e->subdir = 0;
	
    a_e->cur_filter.name_filespec_list = NULL;
    a_e->cur_filter.exclude_filespec_list = NULL;
    a_e->cur_filter.date = DX_CREDATE;
    convert_date_string (
	"31-DEC-2857 00:00:00.00",
	&a_e->cur_filter.before);
    convert_date_string (
	"17-NOV-1858 00:00:00.00",
	&a_e->cur_filter.since);
    a_e->cur_filter.maximum = 1073741823;
    a_e->cur_filter.minimum = 0;
 
    a_e->dir_filelist = NULL;
    a_e->non_dir_filelist = NULL;
    a_e->exclude_filelist = NULL;
     return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      find_LRU_in_filer_cache() find the Least-Recently-Used slot
**	in the directory cache.
**
**--
**/
int	find_LRU_in_filer_cache()
{
    DATE_TIME l_earliest_time,
	      l_cmp;
    int l_m,
        l_n;
 
    l_m = 0;
    memcpy (&l_earliest_time, &filer_cache[0].time_stamp, sizeof (DATE_TIME));
    for (l_n = 1;  l_n < MAXBUFFER;  l_n++)
    {
        if (lib$sub_times(
                &filer_cache[l_n].time_stamp,
                &l_earliest_time,
                &l_cmp) == LIB$_NEGTIM)
	{
	    memcpy(
		&l_earliest_time, &filer_cache[l_n].time_stamp, sizeof (DATE_TIME));
	    l_m = l_n;
	}
    }
 
    /*
    **  Free up memory used by this entry
    **/
 
    free_filespec_list(&filer_cache[l_m].cur_filter.name_filespec_list);
    free_filespec_list(&filer_cache[l_m].cur_filter.exclude_filespec_list);
    free_filelist(&filer_cache[l_m].dir_filelist);
    free_filelist(&filer_cache[l_m].non_dir_filelist);
    free_filelist(&filer_cache[l_m].exclude_filelist);
 
    return l_m;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Searches the filer cache for a slot whose filter (name/exclude
**	patterns, date range, size range) matches a_e exactly, via
**	search_filer_cache$1().
**
**  INPUT PARAMETERS:
**
**	a_e  -  the filter to look for.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	filer_cache[] -- every slot's cur_filter is compared.
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	Index of the matching slot, or -1 if no slot matches.
**
**--
**/
int	search_filer_cache(a_e)
struct cur_filter_tag a_e;
{
    int l_i;
 
    for (l_i = 0;  l_i < MAXBUFFER;  l_i++)
    {
        if (search_filer_cache$1(a_e, filer_cache[l_i].cur_filter))
            return l_i;
    }
 
    return -1;	    /**  Not found  **/
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Compares two cur_filter_tag filters field by field (name/exclude
**	filespec lists via cmp_filespec(), before/since dates, min/max size)
**	to decide whether they are equivalent.
**
**  INPUT PARAMETERS:
**
**	a_e  -  first filter to compare.
**	a_f  -  second filter to compare.
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
**	1 if the two filters are equivalent, 0 otherwise.
**
**--
**/
static int	search_filer_cache$1(a_e, a_f)
struct cur_filter_tag a_e,
		      a_f;
{
    if (cmp_filespec(a_e.name_filespec_list, a_f.name_filespec_list) ||
	cmp_filespec(a_e.exclude_filespec_list, a_f.exclude_filespec_list) ||
	memcmp (&a_e.before, &a_f.before, sizeof (DATE_TIME)) ||
	memcmp (&a_e.since, &a_f.since, sizeof (DATE_TIME)) ||
	a_e.maximum != a_f.maximum ||
	a_e.minimum != a_f.minimum )
	return 0;
    return 1;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Compares two filespec_list_tag lists (name or exclude patterns, each
**	with its own list of equivalent-name/did/chan entries) for set
**	equality: every name in a_l must appear in a_m with the same set of
**	equivalent names, and vice versa.  Order does not matter.
**
**  INPUT PARAMETERS:
**
**	a_l  -  first filespec list to compare (may be NULL).
**	a_m  -  second filespec list to compare (may be NULL).
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
**	0 if the two lists are the same set of names/equivalent-names,
**	-1 if they differ.
**
**--
**/
static int	cmp_filespec(a_l, a_m)
struct filespec_list_tag *a_l,
			 *a_m;
{
    struct filespec_list_tag *l_p,
			     *l_q;
    struct w_dids_tag *l_a,
		      *l_b;
    int l_found, l_found1;
 
    for (l_p = a_l;  l_p != (struct filespec_list_tag *)NULL;  l_p = l_p->next)
    {
	l_found = 0;
	for (l_q = a_m;  !l_found && l_q != (struct filespec_list_tag *)NULL;  l_q = l_q->next)
        {
            if (!strcmp (l_p->name, l_q->name))
            {
		for (l_a = l_p->w_dids;  l_a != (struct w_dids_tag *)NULL;  l_a = l_a->next)
                {
                    l_found1 = 0;
		    for (l_b = l_q->w_dids;  !l_found1 && l_b != (struct w_dids_tag *)NULL;  l_b = l_b->next)
		    {
			if (!strcmp (l_a->equivalent_name, l_b->equivalent_name))
			    l_found1++;
		    }
		    if (!l_found1)
			goto loop1;
		}
		l_found++;
            }
loop1:;
        }
        if (!l_found)
            return -1;
    }
 
    for (l_p = a_m;  l_p != (struct filespec_list_tag *)NULL;  l_p = l_p->next)
    {
	l_found = 0;
	for (l_q = a_l;  !l_found && l_q != (struct filespec_list_tag *)NULL;  l_q = l_q->next)
        {
            if (!strcmp (l_p->name, l_q->name))
            {
		for (l_a = l_p->w_dids;  l_a != (struct w_dids_tag *)NULL;  l_a = l_a->next)
                {
                    l_found1 = 0;
		    for (l_b = l_q->w_dids;  !l_found1 && l_b != (struct w_dids_tag *)NULL;  l_b = l_b->next)
		    {
			if (!strcmp (l_a->equivalent_name, l_b->equivalent_name))
			    l_found1++;
		    }
		    if (!l_found1)
			goto loop2;
		}
		l_found++;
            }
loop2:;
        }
        if (!l_found)
            return -1;
    }
    return 0;	    /**  Two identical namespec list  **/
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Saves the given window's directory state (sort criteria, directory
**	names, file/block counts, filter, and the three file lists) into
**	slot a_n of the filer cache, deep-copying the filter's filespec
**	lists and the file lists so the cache owns independent copies.
**	Also stamps the slot's time_stamp with the current time so it
**	becomes the most-recently-used slot.
**
**  INPUT PARAMETERS:
**
**	a_n  -  index of the cache slot to save into.
**	a_e  -  the dir_dx_tag (window state) to save.
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
**	filer_cache[a_n] -- overwritten with a_e's state.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	    save_to_filer_cache(a_n, a_e)
int a_n;
struct dir_dx_tag a_e;
{
    unsigned long l_rc;
    if (!(1 & (l_rc = sys$gettim (       (struct _generic_64 *) /**  Time stamp  **/
		&filer_cache[a_n].time_stamp))))
        $LOG(l_rc);
    filer_cache[a_n].sortby = a_e.sortby;
    filer_cache[a_n].sortorder = a_e.sortorder;
    strcpy(filer_cache[a_n].cur_dir, a_e.cur_dir);
    strcpy(filer_cache[a_n].par_dir, a_e.par_dir);
    filer_cache[a_n].tot_files = a_e.tot_files;
    filer_cache[a_n].tot_blocks = a_e.tot_blocks;
    filer_cache[a_n].dpro = a_e.dpro;
    filer_cache[a_n].subdir = a_e.subdir;
    filer_cache[a_n].cur_filter.date = a_e.cur_filter.date;
    memcpy(
	&filer_cache[a_n].cur_filter.before,
	&a_e.cur_filter.before,
	sizeof (DATE_TIME));
    memcpy(
	&filer_cache[a_n].cur_filter.since,
	&a_e.cur_filter.since,
	sizeof (DATE_TIME));
    filer_cache[a_n].cur_filter.maximum = a_e.cur_filter.maximum;
    filer_cache[a_n].cur_filter.minimum = a_e.cur_filter.minimum;
 
    copy_filespec_list(
	a_e.cur_filter.name_filespec_list,
	&filer_cache[a_n].cur_filter.name_filespec_list);
    copy_filespec_list(
	a_e.cur_filter.exclude_filespec_list,
	&filer_cache[a_n].cur_filter.exclude_filespec_list);
 
    copy_filelist(
	a_e.dir_filelist,
	&filer_cache[a_n].dir_filelist,
	a_e.sortby,
	a_e.sortorder);
    copy_filelist(
	a_e.non_dir_filelist,
	&filer_cache[a_n].non_dir_filelist,
	a_e.sortby,
	a_e.sortorder);
    copy_filelist(
	a_e.exclude_filelist,
	&filer_cache[a_n].exclude_filelist,
	a_e.sortby,
	a_e.sortorder);
     return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The inverse of save_to_filer_cache(): restores a window's directory
**	state from cache slot a_n into a_d, freeing whatever filespec/file
**	lists a_d already held and deep-copying fresh ones out of the cache
**	slot (the cache slot itself is left untouched).
**
**  INPUT PARAMETERS:
**
**	a_n  -  index of the cache slot to restore from.
**
**  OUTPUT PARAMETERS:
**
**	a_d  -  the dir_dx_tag (window state) to restore into; its previous
**		filespec/file lists are freed first.
**
**  IMPLICIT INPUTS:
**
**	filer_cache[a_n].
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	    restore_from_filer_cache(a_n, a_d)
int a_n;
struct dir_dx_tag *a_d;
{
    a_d->sortby = filer_cache[a_n].sortby;
    a_d->sortorder = filer_cache[a_n].sortorder;
    strcpy(a_d->cur_dir, filer_cache[a_n].cur_dir);
    strcpy(a_d->par_dir, filer_cache[a_n].par_dir);
    a_d->tot_files = filer_cache[a_n].tot_files;
    a_d->tot_blocks = filer_cache[a_n].tot_blocks;
    a_d->dpro = filer_cache[a_n].dpro;
    a_d->subdir = filer_cache[a_n].subdir;
    a_d->cur_filter.date = filer_cache[a_n].cur_filter.date;
    memcpy(
	&a_d->cur_filter.before,
	&filer_cache[a_n].cur_filter.before,
	sizeof (DATE_TIME));
    memcpy(
	&a_d->cur_filter.since,
	&filer_cache[a_n].cur_filter.since,
	sizeof (DATE_TIME));
    a_d->cur_filter.maximum = filer_cache[a_n].cur_filter.maximum;
    a_d->cur_filter.minimum = filer_cache[a_n].cur_filter.minimum;
 
    free_filespec_list(&a_d->cur_filter.name_filespec_list);
    copy_filespec_list(
	filer_cache[a_n].cur_filter.name_filespec_list,
	&a_d->cur_filter.name_filespec_list);
    free_filespec_list(&a_d->cur_filter.exclude_filespec_list);
    copy_filespec_list(
	filer_cache[a_n].cur_filter.exclude_filespec_list,
	&a_d->cur_filter.exclude_filespec_list);
 
    free_filelist(&a_d->dir_filelist);
    copy_filelist(
	filer_cache[a_n].dir_filelist,
	&a_d->dir_filelist,
	filer_cache[a_n].sortby,
	filer_cache[a_n].sortorder);
    free_filelist(&a_d->non_dir_filelist);
    copy_filelist(
	filer_cache[a_n].non_dir_filelist,
	&a_d->non_dir_filelist,
	filer_cache[a_n].sortby,
	filer_cache[a_n].sortorder);
    free_filelist(&a_d->exclude_filelist);
    copy_filelist(
	filer_cache[a_n].exclude_filelist,
	&a_d->exclude_filelist,
	filer_cache[a_n].sortby,
	filer_cache[a_n].sortorder);
     return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Walks every slot of the filer cache looking for entries in each
**	slot's name_filespec_list whose w_dids match a_e's directory-id
**	(a real change to that directory) or, when a_func is
**	delete_from_filer_cache, whose w_dids match a_e's file-id (the
**	file being deleted was itself a merged-in equivalent-name root,
**	so that dead branch is pruned from the cache's tree of
**	equivalence names).  For each did match, calls (*a_func)() (one
**	of add_to_filer_cache()/delete_from_filer_cache()/
**	change_to_filer_cache()) against that cache slot to keep it in
**	sync with a real filesystem change.  If a slot's
**	name_filespec_list becomes completely empty as a result, the slot
**	is reset via initialize_filer_cache$1().
**
**  INPUT PARAMETERS:
**
**	a_e     -  the fil_dx_tag describing the file that changed.
**	a_func  -  which per-slot update to apply: add_to_filer_cache(),
**		   delete_from_filer_cache(), or change_to_filer_cache().
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message if a_func fails.
**
**  IMPLICIT INPUTS:
**
**	None.
**
**  IMPLICIT OUTPUTS:
**
**	filer_cache[] -- every slot's name_filespec_list may be pruned,
**	and (*a_func)() may update the slot's file lists/counts.
**
**  RETURNS VALUE:
**
**	DX__ERROR if any (*a_func)() call fails; DX__NORMAL otherwise.
**
**--
**/
int	    update_filer_cache(a_e, a_func, a_errmsg)
struct fil_dx_tag a_e;
int (*a_func)();
char *a_errmsg;
{
    int l_i;
    struct filespec_list_tag *l_s, *l_s_next, *l_u;
    struct w_dids_tag *l_t, *l_t_next, *l_v;
    int l_allocate_size;
    unsigned long l_rc;

    /*
    **	Search queue for update
    **/

    for (l_i = 0;  l_i < MAXBUFFER;  l_i++)
    {
        for (l_s = filer_cache[l_i].cur_filter.name_filespec_list,  l_u = NULL;
	    l_s;  l_s = l_s_next)
        {
	    l_s_next = l_s->next;
            for (l_t = l_s->w_dids,  l_v = NULL;  l_t;  l_t = l_t_next)
            {
		l_t_next = l_t->next;
                if (l_t->did[0] == a_e.did[0] &&
		    l_t->did[1] == a_e.did[1] &&
		    l_t->did[2] == a_e.did[2])
                {
                    if ((*a_func)(a_e, &filer_cache[l_i], a_errmsg) == DX__ERROR)
                        return DX__ERROR;
		    l_v = l_t;
                }
		else if (l_t->did[0] == a_e.fid[0] &&
			 l_t->did[1] == a_e.fid[1] &&
			 l_t->did[2] == a_e.fid[2] &&
			 *a_func == delete_from_filer_cache)
		{
		    if (l_t == l_s->w_dids)
			l_s->w_dids = l_t->next;
		    else
		    	l_v->next = l_t->next;
		    l_allocate_size = sizeof (struct w_dids_tag);
		    if (!(1 & (l_rc = lib$free_vm (
			         &l_allocate_size,
			         &l_t,
			         0))))
		        $LOG(l_rc);
		    /**  t is freed -- v keeps pointing at the last
		    **   surviving node, do not advance it  **/
		}
		else
		    l_v = l_t;
            }
	    if (l_s->w_dids == NULL)
	    {
		if (l_s == filer_cache[l_i].cur_filter.name_filespec_list)
		    filer_cache[l_i].cur_filter.name_filespec_list = l_s_next;
		else
		    l_u->next = l_s_next;
		l_allocate_size = sizeof (struct filespec_list_tag);
		if (!(1 & (l_rc = lib$free_vm (
			 	&l_allocate_size,
			 	&l_s,
			 	0))))
		    $LOG(l_rc);
		/**  s is freed -- u keeps pointing at the last
		**   surviving node, do not advance it  **/
	    }
	    else
		l_u = l_s;
        }
	if (filer_cache[l_i].cur_filter.name_filespec_list == NULL)
	    initialize_filer_cache$1(&filer_cache[l_i]);
    }

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Called by update_filer_cache() (via the a_func callback) to add a
**	newly-created file into one filer_cache_tag slot's file lists, if
**	that slot's window actually shows the file's directory.  Directory
**	files are inserted into dir_filelist directly; regular files first
**	refresh the slot's exclude list, then are checked against
**	find_queue_file() (does this window's cache already know about this
**	physical file under some other equivalent name?) and filter_OK()
**	before being inserted into non_dir_filelist.  Recomputes the file's
**	display name when the window merges more than one physical
**	directory or device (dxscr_count_win_dirs()/dxscr_count_win_devices()).
**
**  INPUT PARAMETERS:
**
**	a_e       -  the fil_dx_tag describing the new file.
**	a_cachep  -  the cache slot to update.
**
**  OUTPUT PARAMETERS:
**
**	a_cachep  -  its file lists/counts are updated in place.
**	a_errmsg  -  filled in with an error message on DX__ERROR.
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
**	DX__NORMAL if the file was inserted, ignored (directory not shown
**	by this window/cache slot), or filtered out; DX__ERROR if
**	find_exclude_file() or insert_filelist_entry() ran out of memory.
**
**--
**/
int	    add_to_filer_cache(a_e, a_cachep, a_errmsg)
struct fil_dx_tag a_e;
struct filer_cache_tag *a_cachep;
char *a_errmsg;
{
    int l_total_dids;
    int l_total_devices;
    char l_fn[NAM$C_MAXRSS];

    /*
    **	Find out the number of equivalent names of current directory
    **/

    if (!dxscr_find_chan_for_did(a_e.did, (unsigned short int *)NULL, l_fn, &a_cachep->cur_filter))
    {	    /**  Directories not matched  **/
	return DX__NORMAL;
    }
    l_total_dids = dxscr_count_win_dirs(&a_cachep->cur_filter);
    l_total_devices = dxscr_count_win_devices(&a_cachep->cur_filter);

    if (!strcmp(strrchr(a_e.fn, '.'), ".DIR;1"))
    {	    /**  Insert a directory file  **/
	char *l_cp;
	char l_s[MAXFILESPEC+1];
 
	strcpy(l_s, a_e.fn);
	l_cp = strrchr(l_s, '.');
	*l_cp = ']';
	*++l_cp = EOS;
        if (l_total_dids <= 1)
	{
	    static $DESCRIPTOR (l_fao_ctrl, "[.!AZ");
	    struct dsc$descriptor_s l_out_desc;
	    unsigned short int l_len;
	    unsigned long l_rc;

	    l_out_desc.dsc$w_length  = sizeof(a_e.fn) - 1;
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
		a_cachep->dir_filelist,
		a_e,
		a_cachep->sortby,
		a_cachep->sortorder) == DX__NORMAL)
        {
            a_cachep->tot_files++;
	    a_cachep->tot_blocks += a_e.filesize;
	    a_cachep->subdir++;
        }
    }
    else
    {
	if (find_exclude_file(	    /**  Update exclude list  **/
		&(a_cachep->exclude_filelist),
		a_cachep->cur_filter,
		a_cachep->sortby,
		a_cachep->sortorder) == DX__ERROR)
	{
	    strcpy(
		a_errmsg, "Insufficient memory to update the exclude file list");
	    return DX__ERROR;
	}
 
	strcat(l_fn, a_e.fn);	    /**  Get full path name  **/
 
	if (find_queue_file(a_e.fid, a_cachep))
	{
	    if (l_total_dids > 1)
	    {
		if (l_total_devices <= 1)
		    strcpy(a_e.fn, strchr(l_fn, '['));
		else
		    strcpy(a_e.fn, l_fn);
	    }
 
	    if (filter_OK(a_e, a_cachep->exclude_filelist, a_cachep->cur_filter))
	    {
		switch (insert_filelist_entry(
			    a_cachep->non_dir_filelist,
			    a_e,
			    a_cachep->sortby,
			    a_cachep->sortorder))
		{
		    case DX__ERROR:
			strcpy(
			    a_errmsg, "Insufficient memory to insert a new file");
			return DX__ERROR;
		    case DX__NORMAL:
			a_cachep->tot_files++;
			a_cachep->tot_blocks += a_e.filesize;
			break;
		    default:    /**  Don't care  **/
			;
		}
	    }
	}
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Called by update_filer_cache() (via the a_func callback) to remove a
**	deleted file's entry from one filer_cache_tag slot's file lists
**	(directory list searched first, then non-directory list), matching
**	by file-id, and adjust that slot's file/block counts accordingly.
**
**  INPUT PARAMETERS:
**
**	a_e       -  the fil_dx_tag describing the deleted file (only its
**		     fid is used).
**	a_cachep  -  the cache slot to update.
**
**  OUTPUT PARAMETERS:
**
**	a_cachep  -  the matching entry is removed and its counts adjusted.
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
**	DX__NORMAL, always.
**
**--
**/
int	    delete_from_filer_cache(a_e, a_cachep, a_errmsg)
struct fil_dx_tag a_e;
struct filer_cache_tag *a_cachep;
char *a_errmsg;
{
    struct fil_dx_tag *l_p;
 
    /*
    **	Find the file entry.
    **	Search directory file first, then search non-directory file.
    **/
 
    for (l_p = a_cachep->dir_filelist->forward;
	 l_p != a_cachep->non_dir_filelist; l_p = l_p->forward)
    {
        if (l_p == a_cachep->dir_filelist)
            l_p = a_cachep->non_dir_filelist;
	else
	{
	    if (a_e.fid[0] == l_p->fid[0] &&
		a_e.fid[1] == l_p->fid[1] &&
		a_e.fid[2] == l_p->fid[2])
	    {
		a_cachep->tot_files--;
		a_cachep->tot_blocks -= l_p->filesize;
		remove_filelist_entry(&l_p);
		break;
	    }
	}
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Called by update_filer_cache() (via the a_func callback) to refresh
**	a changed file's protection, size, and name in one filer_cache_tag
**	slot's file lists (directory list searched first, then
**	non-directory list), matching by file-id, adjusting the slot's
**	total-block count for the size delta.
**
**  INPUT PARAMETERS:
**
**	a_e       -  the fil_dx_tag with the file's new attributes (matched
**		     to the existing entry by fid).
**	a_cachep  -  the cache slot to update.
**
**  OUTPUT PARAMETERS:
**
**	a_cachep  -  the matching entry's fpro/filesize/fn are overwritten
**		     and tot_blocks is adjusted.
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
**	DX__NORMAL, always.
**
**--
**/
int	    change_to_filer_cache(a_e, a_cachep, a_errmsg)
struct fil_dx_tag a_e;
struct filer_cache_tag *a_cachep;
char *a_errmsg;
{
    struct fil_dx_tag *l_p;
 
    /*
    **	Find the file entry.
    **	Search directory file first, then search non-directory file.
    **/
 
    for (l_p = a_cachep->dir_filelist->forward;
	 l_p != a_cachep->non_dir_filelist; l_p = l_p->forward)
    {
        if (l_p == a_cachep->dir_filelist)
            l_p = a_cachep->non_dir_filelist;
	else
	{
	    if (a_e.fid[0] == l_p->fid[0] &&
		a_e.fid[1] == l_p->fid[1] &&
		a_e.fid[2] == l_p->fid[2])
	    {
		l_p->fpro = a_e.fpro;
		a_cachep->tot_blocks += a_e.filesize - l_p->filesize;
		l_p->filesize = a_e.filesize;
		strcpy(l_p->fn, a_e.fn);
		break;
	    }
	}
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Deep-copies a filespec_list_tag list (as used for a filter's name
**	or exclude patterns): for each entry, copies its name and its whole
**	w_dids equivalent-name/did/chan chain (via insert_dids()), then
**	inserts the copy into *a_to (via insert_filespec_list$1()).
**
**  INPUT PARAMETERS:
**
**	a_from  -  the filespec list to copy (may be NULL).
**
**  OUTPUT PARAMETERS:
**
**	a_to  -  set to point at the head of the newly-built copy (NULL if
**		 a_from was NULL/empty).
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
void	    copy_filespec_list(a_from, a_to)
struct filespec_list_tag *a_from,
			 **a_to;
 
{
    struct filespec_list_tag *l_r;
 
    *a_to = (struct filespec_list_tag *)NULL;
    for (l_r = a_from; l_r != (struct filespec_list_tag *)NULL;  l_r = l_r->next)
    {
	struct filespec_list_tag l_t;
	struct w_dids_tag *l_u;
 
	strcpy(l_t.name, l_r->name);
	l_t.w_dids = (struct w_dids_tag *)NULL;
	l_t.next = (struct filespec_list_tag *)NULL;
        for (l_u = l_r->w_dids;  l_u != (struct w_dids_tag *)NULL;  l_u = l_u->next)
        {
            insert_dids(&l_t.w_dids, l_u->equivalent_name, l_u->did, l_u->chan);
        }
        insert_filespec_list$1(a_to, l_t);
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Deep-copies a fil_dx_tag circular list (as used for dir_filelist/
**	non_dir_filelist/exclude_filelist): creates a fresh MARKER, copies
**	the marker's own fields from *a_from, then re-inserts every real
**	entry one at a time via insert_filelist_entry() using the given
**	sort criteria (so the copy ends up correctly ordered even if
**	a_from's ordering was different).
**
**  INPUT PARAMETERS:
**
**	a_from      -  the list to copy (its own MARKER node).
**	a_sortby    -  sort criterion to use when re-inserting entries.
**	a_sortorder -  sort order (ascending/descending) to use.
**
**  OUTPUT PARAMETERS:
**
**	a_to  -  set to point at the new list's MARKER node.
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
void	    copy_filelist(a_from, a_to, a_sortby, a_sortorder)
struct fil_dx_tag *a_from,
		  **a_to;
enum sortbys a_sortby;
enum sortorders a_sortorder;
{
    struct fil_dx_tag *l_p,
		      *l_q;	
 
    l_q = create_MARKER();
    *l_q = *a_from;
    l_q->backward = l_q->forward = l_q;
    for (l_p = a_from->forward; l_p != a_from; l_p = l_p->forward)
    {
        insert_filelist_entry(l_q, *l_p, a_sortby, a_sortorder);
    }
    *a_to = l_q;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Interactive picker over the whole filer cache (the "show Buffer"
**	command): paints a full-screen virtual display showing one cache
**	slot's filter/settings at a time via display_filer_cache(), and
**	lets the user step through slots with UP/DOWN arrows, select the
**	current one with RETURN/ENTER, or cancel with keypad-0/CTRL-Z.
**	CTRL-W repaints the screen.  Tears the display down before
**	returning in every exit path.
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
**	cntrl_info_block (pasteboard id/rows/width, keyboard id),
**	filer_cache[] (read by display_filer_cache() and to check whether
**	the selected slot is empty).
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	Index of the selected cache slot, or -1 if the user cancelled.
**
**--
**/
int	    select_from_filer_cache()
{
    struct display_tag l_list_cache_display =
	{
	    0,					    /**  id		     **/
	    cntrl_info_block.pasteboard_rows,	    /**  rows		     **/
	    cntrl_info_block.pasteboard_width,	    /**  width		     **/
	    1,					    /**  paste row	     **/
	    1,					    /**  paste column	     **/
	    0,					    /**  unused		     **/
	    0,					    /**  unused		     **/
	    0,					    /**  unused		     **/
	    0					    /**  unused		     **/
	};
    int l_i;
    unsigned short int l_word_terminator_code;
    unsigned long l_rc;
 
    if (!(1 & (l_rc = smg$create_virtual_display (
		  &l_list_cache_display.rows,
		  &l_list_cache_display.width,
		  &l_list_cache_display.id,
		  &SMG$M_TRUNC_ICON,
		  0,
		  0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$paste_virtual_display (
		  &l_list_cache_display.id,
		  &cntrl_info_block.pasteboard_id,
		  &l_list_cache_display.beg_y,
		  &l_list_cache_display.beg_x,
		  0))))
        $LOG(l_rc);
 
    l_i = 0;
    display_filer_cache(l_i, l_list_cache_display);
    if (!(1 & (l_rc = smg$put_chars (
	     	  &l_list_cache_display.id,
	     	  $DESCR (
  "Press ARROW KEYS for prev or next, RETURN to select, or keypad 0 to cancel"),
	     	  &l_list_cache_display.rows,
	     	  &1,
	     	  &SMG$M_ERASE_LINE,
	     	  &SMG$M_REVERSE,
	     	  0,
	     	  0))))
        $LOG(l_rc);
    smg$read_keystroke (
    	&cntrl_info_block.keyboard_id,
    	&l_word_terminator_code,
    	0,
    	0,
	&l_list_cache_display.id,
    	0,
    	0);
 
    /*
    **  Keep typing until exit by the user
    **/
 
    while (1)
    {
        switch (l_word_terminator_code)
        {
            case SMG$K_TRM_UP:
		if (l_i == 0)
		{
		    if (!(1 & (l_rc = smg$ring_bell (
			         &l_list_cache_display.id,
			         0))))
		        $LOG(l_rc);
		    l_i = MAXBUFFER-1;
		}
		else
		    l_i--;
		display_filer_cache (l_i, l_list_cache_display);
		break;
	    case SMG$K_TRM_DOWN:
		if (l_i == MAXBUFFER-1)
		{
		    if (!(1 & (l_rc = smg$ring_bell (
			         &l_list_cache_display.id,
			         0))))
		        $LOG(l_rc);
		    l_i = 0;
		}
		else
		    l_i++;
		display_filer_cache (l_i, l_list_cache_display);
		break;
	    case SMG$K_TRM_CR:
	    case SMG$K_TRM_ENTER:
		if (filer_cache[l_i].cur_filter.name_filespec_list == NULL)
		{	/**  Buffer is empty  **/
                    if (!(1 & (l_rc = smg$ring_bell (
                                 &l_list_cache_display.id,
                                 0))))
                        $LOG(l_rc);
		}
		else
		{
		    if (!(1 & (l_rc = smg$erase_display (
			      &l_list_cache_display.id,
			      0,
			      0,
			      0,
			      0))))
		        $LOG(l_rc);
 
		    if (!(1 & (l_rc = smg$delete_virtual_display (
			      &l_list_cache_display.id))))
		        $LOG(l_rc);
		    return l_i;
		}
		break;
	    case SMG$K_TRM_KP0:
	    case SMG$K_TRM_CTRLZ:
		if (!(1 & (l_rc = smg$erase_display (
			  &l_list_cache_display.id,
			  0,
			  0,
			  0,
			  0))))
		    $LOG(l_rc);
 
		if (!(1 & (l_rc = smg$delete_virtual_display (
			  &l_list_cache_display.id))))
		    $LOG(l_rc);
		return -1;
		break;
	    case SMG$K_TRM_CTRLW:
		if (!(1 & (l_rc = smg$repaint_screen (
			      &l_list_cache_display.id))))
		    $LOG(l_rc);
		if (!(1 & (l_rc = smg$repaint_screen (
			      &l_list_cache_display.id))))
		    $LOG(l_rc);
		break;
	    default :
		if (!(1 & (l_rc = smg$ring_bell (
			     &l_list_cache_display.id,
			     0))))
		    $LOG(l_rc);
		break;
	}
	if (!(1 & (l_rc = smg$put_chars (
		      &l_list_cache_display.id,
		      $DESCR (
  "Press ARROW KEYS for prev or next, RETURN to select, or keypad 0 to cancel"),
		      &l_list_cache_display.rows,
		      &1,
		      &SMG$M_ERASE_LINE,
		      &SMG$M_REVERSE,
		      0,
		      0))))
	    $LOG(l_rc);
	smg$read_keystroke (
	    &cntrl_info_block.keyboard_id,
	    &l_word_terminator_code,
	    0,
	    0,
	    &l_list_cache_display.id,
	    0,
	    0);
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Paints one cache slot's settings into the virtual display used by
**	select_from_filer_cache(): the slot number, its name/exclude filter
**	patterns (with equivalent names), Since/Before dates, Less-than/
**	Larger-than size limits, sort-by/order-by criteria, last-saved
**	timestamp, and total files/blocks/directories.  An empty slot
**	(name_filespec_list == NULL) shows "?????" for the filter and blank
**	fields for everything else.
**
**  INPUT PARAMETERS:
**
**	a_i            -  index of the cache slot to display.
**	a_list_display -  the virtual display to paint into.
**
**  OUTPUT PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS:
**
**	filer_cache[a_i].
**
**  IMPLICIT OUTPUTS:
**
**	None.
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
static int	    display_filer_cache(a_i, a_list_display)
int a_i;
struct display_tag a_list_display;
{
    char l_numstr[3];
    $DESCRIPTOR (l_numstr_descrip, l_numstr);
    int l_numstr_column;
    unsigned short int l_timbuf[7];
    char l_line[256];
    $DESCRIPTOR (l_line_descrip, l_line);
    struct filespec_list_tag *l_p;
    struct w_dids_tag *l_q;
    unsigned long l_rc;

    /*  14-Sep-2026 RRL: batch this whole filter-settings panel redraw (13
    **  smg$ calls below) into one flush instead of each line visibly
    **  appearing separately.  **/
    if (!(1 & (l_rc = smg$begin_display_update(&a_list_display.id))))
        $LOG(l_rc);

    if (!(1 & (l_rc = smg$home_cursor (
	     	  &a_list_display.id,
	     	  0))))
        $LOG(l_rc);
    /**  Filter by  **/
    strcpy(l_line, "Buffer: ");
    if (filer_cache[a_i].cur_filter.name_filespec_list == NULL)
	strcat(l_line, "?????");
    else
    {
	l_p = filer_cache[a_i].cur_filter.name_filespec_list;
	strcat(l_line, l_p->w_dids->equivalent_name);
	strcat(l_line, l_p->name);
	for (l_q = l_p->w_dids->next; l_q;  l_q = l_q->next)
	{
	    strcat(l_line, ", ");
	    strcat(l_line, l_q->equivalent_name);
	    strcat(l_line, l_p->name);
	}
	for (l_p = l_p->next;  l_p;  l_p = l_p->next)
	{
	    for (l_q = l_p->w_dids; l_q; l_q = l_q->next)
	    {
		strcat(l_line, ", ");
		strcat(l_line, l_q->equivalent_name);
		strcat(l_line, l_p->name);
	    }
	}
    }
    l_numstr_column = a_list_display.width-1;
    if (strlen(l_line) >= l_numstr_column-1)
    {
	l_line[l_numstr_column-3] = '~';
	l_line[l_numstr_column-2] = EOS;
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_chars (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  &1,
	     	  &1,
	     	  &SMG$M_ERASE_TO_EOL,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
    {
	static $DESCRIPTOR (l_fao_ctrl, "!2ZL");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	l_out_desc.dsc$w_length  = sizeof(l_numstr) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_numstr;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, (unsigned long) (a_i+1)))))
	    $LOG(l_rc);
	l_numstr[l_len] = EOS;
    }
    if (!(1 & (l_rc = smg$put_chars (
		  &a_list_display.id,
		  &l_numstr_descrip,
		  &1,
		  &l_numstr_column,
		  0,
		  &SMG$M_BOLD,
		  0,
		  0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$set_cursor_abs (
	     	  &a_list_display.id,
	     	  &3,
	     	  &1))))
        $LOG(l_rc);
 
    /**  Excluded  **/
    strcpy(l_line, "               Excluded : ");
    if (filer_cache[a_i].cur_filter.exclude_filespec_list != NULL)
    {
	l_p = filer_cache[a_i].cur_filter.exclude_filespec_list;
	strcat(l_line, l_p->name);
	for (l_p = l_p->next;  l_p;  l_p = l_p->next)
	{
	    strcat(l_line, ", ");
	    strcat(l_line, l_p->name);
	}
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
    /**  Since  **/
    strcpy(l_line, "                  Since : ");
    if (!(1 & (l_rc = sys$numtim (
		  l_timbuf,
		  (struct _generic_64 *) &filer_cache[a_i].cur_filter.since))))
        $LOG(l_rc);
    if (!(l_timbuf[0] == 1858 && l_timbuf[1] == 11 && l_timbuf[2] == 17))
    {
	char l_datestr[12];
	char l_scratch[32];
	static $DESCRIPTOR (l_fao_ctrl, "!AZ !2ZL:!2ZL:!2ZL");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	dxscr_vms_date_string(l_datestr, &filer_cache[a_i].cur_filter.since);
	l_out_desc.dsc$w_length  = sizeof(l_scratch) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_scratch;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_datestr,
		    (unsigned long) l_timbuf[3], (unsigned long) l_timbuf[4], (unsigned long) l_timbuf[5]))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	strcat(l_line, l_scratch);
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);

    /**  Before  **/
    strcpy(l_line, "                 Before : ");
    if (!(1 & (l_rc = sys$numtim (
		  l_timbuf,
		  (struct _generic_64 *) &filer_cache[a_i].cur_filter.before))))
        $LOG(l_rc);
    if (!(l_timbuf[0] == 2857 && l_timbuf[1] == 12 && l_timbuf[2] == 31))
    {
	char l_datestr[12];
	char l_scratch[32];
	static $DESCRIPTOR (l_fao_ctrl, "!AZ !2ZL:!2ZL:!2ZL");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	dxscr_vms_date_string(l_datestr, &filer_cache[a_i].cur_filter.before);
	l_out_desc.dsc$w_length  = sizeof(l_scratch) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_scratch;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_datestr,
		    (unsigned long) l_timbuf[3], (unsigned long) l_timbuf[4], (unsigned long) l_timbuf[5]))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	strcat(l_line, l_scratch);
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
    /**  Maximum  **/
    strcpy(l_line, "              Less than : ");
    if (filer_cache[a_i].cur_filter.name_filespec_list != NULL &&
	filer_cache[a_i].cur_filter.maximum != 1073741823)
    {
	char l_scratch[16];
	static $DESCRIPTOR (l_fao_ctrl, "!SL");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	l_out_desc.dsc$w_length  = sizeof(l_scratch) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_scratch;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
		    (long) filer_cache[a_i].cur_filter.maximum))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	strcat(l_line, l_scratch);
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
    /**  Minimum  **/
    strcpy(l_line, "            Larger than : ");
    if (filer_cache[a_i].cur_filter.name_filespec_list != NULL &&
	filer_cache[a_i].cur_filter.minimum != 0)
    {
	char l_scratch[16];
	static $DESCRIPTOR (l_fao_ctrl, "!SL");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	l_out_desc.dsc$w_length  = sizeof(l_scratch) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_scratch;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
		    (long) filer_cache[a_i].cur_filter.minimum))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	strcat(l_line, l_scratch);
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  $DESCR (""),
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
    /**  Sort by  **/
    strcpy(l_line, "                Sort by : ");
    if (filer_cache[a_i].cur_filter.name_filespec_list != NULL)
    {
	switch (filer_cache[a_i].sortby)
	{
	    case sortname:
		strcat(l_line, "Name");
		break;
	    case sorttype:
		strcat(l_line, "Type");
		break;
	    case sortsize:
		strcat(l_line, "Size");
		break;
	    case sortmark:
		strcat(l_line, "Marked");
		break;
	    case sortcredate:
	    case sortrevdate:
	    case sortexpdate:
	    case sortbakdate:
		strcat(l_line, "Date");
		break;
	    default:
		;
	}
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
    /**  Order by  **/
    strcpy(l_line, "               Order by : ");
    if (filer_cache[a_i].cur_filter.name_filespec_list != NULL)
    {
	if (filer_cache[a_i].sortorder ==  ascending)
	    strcat(l_line, "Ascending");
	else
	    strcat(l_line, "Descending");
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
    /**  Last saved  **/
    strcpy(l_line, "             Last saved : ");
    if (!(1 & (l_rc = sys$numtim (
		  l_timbuf,
		  (struct _generic_64 *) &filer_cache[a_i].time_stamp))))
        $LOG(l_rc);
    if (!(l_timbuf[0] == 1858 && l_timbuf[1] == 11 && l_timbuf[2] == 17))
    {
	char l_datestr[12];
	char l_scratch[32];
	static $DESCRIPTOR (l_fao_ctrl, "!AZ !2ZL:!2ZL:!2ZL");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	dxscr_vms_date_string(l_datestr, &filer_cache[a_i].time_stamp);
	l_out_desc.dsc$w_length  = sizeof(l_scratch) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_scratch;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc, l_datestr,
		    (unsigned long) l_timbuf[3], (unsigned long) l_timbuf[4], (unsigned long) l_timbuf[5]))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	strcat(l_line, l_scratch);
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);
 
    /**  Total files  **/
    strcpy(l_line, "               Total of : ");
    if (filer_cache[a_i].cur_filter.name_filespec_list != NULL)
    {
	char l_scratch[64];
	static $DESCRIPTOR (l_fao_ctrl1, "!SL !AZ, ");
	static $DESCRIPTOR (l_fao_ctrl2, "!SL files, !SL blocks");
	struct dsc$descriptor_s l_out_desc;
	unsigned short int l_len;

	l_out_desc.dsc$w_length  = sizeof(l_scratch) - 1;
	l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
	l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
	l_out_desc.dsc$a_pointer = l_scratch;
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl1, &l_len, &l_out_desc,
		    (long) filer_cache[a_i].subdir,
		    (filer_cache[a_i].subdir <= 1? "directory" : "directories")))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	strcat(l_line, l_scratch);
	if (!(1 & (l_rc = sys$fao(&l_fao_ctrl2, &l_len, &l_out_desc,
		    (long) filer_cache[a_i].tot_files,
		    (long) filer_cache[a_i].tot_blocks))))
	    $LOG(l_rc);
	l_scratch[l_len] = EOS;
	strcat(l_line, l_scratch);
    }
    LENGTH(l_line_descrip) = strlen(l_line);
    if (!(1 & (l_rc = smg$put_line (
	     	  &a_list_display.id,
	     	  &l_line_descrip,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0,
	     	  0))))
        $LOG(l_rc);

    if (!(1 & (l_rc = smg$end_display_update(&a_list_display.id))))
        $LOG(l_rc);

     return DX__NORMAL;
}



/* ---- (from FINDFILE.C -- directory scan / filelist primitives) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FIND_DIRECT_FILE() searchs all the directory files in the directory.
**	Parent directory file is also included.
**
**--
**/
int	find_direct_file()
{
    /*
    **  VARIABLES USED BY ACP SERVICES
    **/
 
    unsigned int l_iosb[2];
    static FIBDEF l_fib;	
    struct dsc$descriptor_s l_fib_descrip =
	{ sizeof (FIBDEF), DSC$K_DTYPE_T, DSC$K_CLASS_S, (char *)&l_fib };
    unsigned short l_len;
    char l_filespec[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descrip, l_filespec);
    unsigned long l_fch;			    /**  File characteristics blocks **/
    SBKDEF l_asb;			    /**  Statistics Block	     **/
    DATE_TIME l_cdat,			    /**  64-bit Creation Date	     **/
	      l_rdat,			    /**  64-bit Revision Date	     **/
	      l_edat,			    /**  64-bit Expiration Date	     **/
	      l_bdat;			    /**  64-bit Backup Date	     **/
    unsigned short int l_fpro;		    /**  File Protection	     **/
    struct
	{				    /**  Access Control Block	     **/
	    unsigned short w_size;
	    unsigned short w_type;
	    void *l_addr;
	} l_acb[] = {
	    { ATR$S_UCHAR, ATR$C_UCHAR, (char *)&l_fch },
	    { ATR$S_STATBLK, ATR$C_STATBLK, (char *)&l_asb },
	    { ATR$S_CREDATE, ATR$C_CREDATE, (char *)&l_cdat },
	    { ATR$S_REVDATE, ATR$C_REVDATE, (char *)&l_rdat },
	    { ATR$S_EXPDATE, ATR$C_EXPDATE, (char *)&l_edat },
	    { ATR$S_BAKDATE, ATR$C_BAKDATE, (char *)&l_bdat },
	    { ATR$S_FPRO, ATR$C_FPRO, (char *)&l_fpro },
	    { 0, 0, (char *)NULL }
	};
 
    /*
    **	    NON-ACP VARABLES
    **/
 
    register int l_i;
    int l_total_dids;
    int l_total_devices;
    struct fil_dx_tag l_temp_entry;
    char l_str[128];
    $DESCRIPTOR (l_str_descrip, l_str);
    struct filespec_list_tag *l_p;
    struct w_dids_tag *l_q;
    long int l_dup_files, l_dup_blocks;
    char l_errmsg[MAXFILESPEC+1];
    unsigned long l_rc;

    /*
    **	Create a new list head.
    **	Free up memory for old list first.
    **/

    free_filelist(&DX_CURRENT_DIRECTORY.dir_filelist);
    if ((DX_CURRENT_DIRECTORY.dir_filelist =
	    create_MARKER()) == (struct fil_dx_tag *)NULL)
    {
	signal_err("Error creating MARKER for directory file list", bell);
	return  DX__ERROR;
    }
 
    /*
    **	Insert parent directory entry
    **/
 
    if (DX_CURRENT_DIRECTORY.par_dir[0] != EOS)
    {
	strcpy (l_temp_entry.fn, "[-]");
	l_temp_entry.state = waiting;
	append_filelist_entry(
	    DX_CURRENT_DIRECTORY.dir_filelist,
	    l_temp_entry);
    }
 
    /*
    **	Find out the number of equivalent names of current directory
    **/
 
    l_total_dids = dxscr_count_win_dirs(&DX_CURRENT_DIRECTORY.cur_filter);
    l_total_devices = dxscr_count_win_devices(&DX_CURRENT_DIRECTORY.cur_filter);
 
    /*
    **	Find all the directory files in this directory
    **/
 
    strcpy(l_str, "*.DIR;1");
    LENGTH(l_str_descrip) = strlen(l_str);
 
    for (l_p = DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list;  l_p;  l_p = l_p->next)
    {
        for (l_q = l_p->w_dids;  l_q;  l_q = l_q->next)
        {
	    for (l_i = 0; l_i <= 2; l_i++)
		__FIB(l_fib,fib$r_did_overlay,fib$w_did[l_i]) = l_q->did[l_i];
 
	    l_filespec[0] = EOS;
 
	    for (l_i = 0;  l_i <= 2;  l_i++)
	    {
		l_temp_entry.did[l_i] = l_q->did[l_i];
	    }
 
	    /*
	    **  Initialize FIB to indicate to ACP that a directory
	    **  look-up operation is desired.
	    **/
 
	    __FIB(l_fib,fib$r_nmctl_overlay,fib$w_nmctl) = FIB$M_WILD;
	    l_fib.fib$l_wcc = 0;
 
	    if (!(1 & (l_rc = sys$qiow (
			0,
			l_q->chan,
			IO$_ACCESS,
			(struct _iosb *) l_iosb,
			0,
			0,
			&l_fib_descrip,
			(__int64) &l_str_descrip,
			(__int64) &l_len,
			(__int64) &l_filespec_descrip,
			(__int64) l_acb,
			0 ))))
	        $LOG(l_rc);
 
	    while (l_iosb[0] == SS$_NORMAL ||
		   ((l_iosb[0] == SS$_NOPRIV || l_iosb[0] == SS$_NOSUCHFILE) &&
		     l_filespec[0]))
	    {
		size_t len1;
 
		if (l_total_dids <= 1)
		{
		    strcpy (l_temp_entry.fn, "[.");
		    len1 = strcspn (l_filespec, ".");
		    strncat (l_temp_entry.fn, l_filespec, len1);
		    strcat (l_temp_entry.fn, "]");
		}
		else
		{
                    if (l_total_devices <= 1)
                        strcpy (l_temp_entry.fn, strchr (l_q->equivalent_name, '['));
                    else
			strcpy (l_temp_entry.fn, l_q->equivalent_name);
		    l_temp_entry.fn[strlen(l_temp_entry.fn)-1] = '.';
		    len1 = strcspn (l_filespec, ".");
		    strncat (l_temp_entry.fn, l_filespec, len1);
		    strcat (l_temp_entry.fn, "]");
		}
 
		DX_CURRENT_DIRECTORY.subdir++;
 
		if (l_iosb[0] == SS$_NOPRIV)
		{
		    l_temp_entry.filesize = 0;
		    l_temp_entry.fpro = 0Xffff;	/**  Insufficient privilege  **/
		    convert_date_string (
			"17-NOV-1858 00:00:00.00",
			&l_temp_entry.cdat);
		    memcpy(&l_temp_entry.rdat, &l_temp_entry.cdat, sizeof (DATE_TIME));
		    memcpy(&l_temp_entry.edat, &l_temp_entry.cdat, sizeof (DATE_TIME));
		    memcpy(&l_temp_entry.bdat, &l_temp_entry.cdat, sizeof (DATE_TIME));
		}
		else if (l_iosb[0] == SS$_NOSUCHFILE)
		{
		    l_temp_entry.filesize = 0;
		    l_temp_entry.fpro = 0Xfffe;	/**  No such file	     **/
		    convert_date_string (
			"17-NOV-1858 00:00:00.00",
			&l_temp_entry.cdat);
		    memcpy(&l_temp_entry.rdat, &l_temp_entry.cdat, sizeof (DATE_TIME));
		    memcpy(&l_temp_entry.edat, &l_temp_entry.cdat, sizeof (DATE_TIME));
		    memcpy(&l_temp_entry.bdat, &l_temp_entry.cdat, sizeof (DATE_TIME));
		}
		else
		{
		    for (l_i = 0;  l_i <= 2;  l_i++)
		    {
			l_temp_entry.fid[l_i] = __FIB(l_fib,fib$r_fid_overlay,fib$w_fid[l_i]);
		    }
		    l_temp_entry.filesize =
			(l_asb.sbk$w_filesizh << 16)+l_asb.sbk$w_filesizl;
		    memcpy(&l_temp_entry.cdat, &l_cdat, sizeof (DATE_TIME));
		    memcpy(&l_temp_entry.rdat, &l_rdat, sizeof (DATE_TIME));
		    memcpy(&l_temp_entry.edat, &l_edat, sizeof (DATE_TIME));
		    memcpy(&l_temp_entry.bdat, &l_bdat, sizeof (DATE_TIME));
		    l_temp_entry.fpro = l_fpro;
		}
 
		l_temp_entry.state = waiting;
 
		if (append_filelist_entry(
			DX_CURRENT_DIRECTORY.dir_filelist,
			l_temp_entry) == DX__NORMAL)
		{
		    DX_CURRENT_DIRECTORY.tot_files++;
		    DX_CURRENT_DIRECTORY.tot_blocks += l_temp_entry.filesize;
		}
 
		if (!(1 & (l_rc = sys$qiow (
			    0,
			    l_q->chan,
			    IO$_ACCESS,
			    (struct _iosb *) l_iosb,
			    0,
			    0,
			    &l_fib_descrip,
			    (__int64) &l_str_descrip,
			    (__int64) &l_len,
			    (__int64) &l_filespec_descrip,
			    (__int64) l_acb,
			    0 ))))
		    $LOG(l_rc);
	    }
        }
    }

    if (finalize_filelist(
	    DX_CURRENT_DIRECTORY.dir_filelist,
	    1,
	    DX_CURRENT_DIRECTORY.sortby,
	    DX_CURRENT_DIRECTORY.sortorder,
	    &l_dup_files,
	    &l_dup_blocks,
	    l_errmsg) != DX__NORMAL)
    {
	signal_err(l_errmsg, bell);
	return DX__ERROR;
    }
    DX_CURRENT_DIRECTORY.tot_files -= l_dup_files;
    DX_CURRENT_DIRECTORY.tot_blocks -= l_dup_blocks;
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FIND_NON_DIRECT_FILE() searchs all the files except directory files
**	in the directory that meet user specified file specification
**
**--
**/
int	find_non_direct_file()
{
    /*
    **  VARIABLES USED BY ACP SERVICES
    **/
 
    unsigned int l_iosb[2];
    static FIBDEF l_fib;
    struct dsc$descriptor_s l_fib_descrip =
	{ sizeof (FIBDEF), DSC$K_DTYPE_T, DSC$K_CLASS_S, (char *)&l_fib };
    unsigned short l_len;
    char l_filespec[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descrip, l_filespec);
    unsigned long l_fch;			    /**  File characteristics blocks **/
    SBKDEF l_asb;			    /**  Statistics Block	     **/
    DATE_TIME l_cdat,			    /**  64-bit Creation Date	     **/
	      l_rdat,			    /**  64-bit Revision Date	     **/
	      l_edat,			    /**  64-bit Expiration Date	     **/
	      l_bdat;			    /**  64-bit Backup Date	     **/
    unsigned short int l_fpro;		    /**  File Protection	     **/
    struct
	{				    /**  Access Control Block	     **/
	    unsigned short w_size;
	    unsigned short w_type;
	    void *l_addr;
	} l_acb[] = {
	    { ATR$S_UCHAR, ATR$C_UCHAR, (char *)&l_fch },
	    { ATR$S_STATBLK, ATR$C_STATBLK, (char *)&l_asb },
	    { ATR$S_CREDATE, ATR$C_CREDATE, (char *)&l_cdat },
	    { ATR$S_REVDATE, ATR$C_REVDATE, (char *)&l_rdat },
	    { ATR$S_EXPDATE, ATR$C_EXPDATE, (char *)&l_edat },
	    { ATR$S_BAKDATE, ATR$C_BAKDATE, (char *)&l_bdat },
	    { ATR$S_FPRO, ATR$C_FPRO, (char *)&l_fpro },
	    { 0, 0, (char *)NULL }
	};
 
    /*
    **	    NON-ACP VARABLES
    **/
 
    register int l_i;
    int l_total_dids;
    int l_total_devices;
    struct fil_dx_tag l_temp_entry;
    char l_str[128];
    $DESCRIPTOR (l_str_descrip, l_str);
    struct filespec_list_tag *l_p;
    struct w_dids_tag *l_q;
    long int l_dup_files, l_dup_blocks;
    char l_errmsg[MAXFILESPEC+1];
    unsigned long l_rc;

    /*
    **	Create a new list head.
    **	Free up memory for old list first.
    **/

    free_filelist(&DX_CURRENT_DIRECTORY.non_dir_filelist);
    if ((DX_CURRENT_DIRECTORY.non_dir_filelist =
	    create_MARKER()) == (struct fil_dx_tag *)NULL)
    {
	signal_err("Error creating MARKER for non-directory file list", bell);
	return  DX__ERROR;
    }
 
    l_total_dids = dxscr_count_win_dirs(&DX_CURRENT_DIRECTORY.cur_filter);
    l_total_devices = dxscr_count_win_devices(&DX_CURRENT_DIRECTORY.cur_filter);
 
    for (l_p = DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list;  l_p;  l_p = l_p->next)
    {
	/*
	**	Find all the files in this directory that meet the criteria
	**/
 
	strcpy(l_str, l_p->name);
	LENGTH(l_str_descrip) = strlen(l_str);
 
        for (l_q = l_p->w_dids;  l_q;  l_q = l_q->next)
        {
	    for (l_i = 0; l_i <= 2; l_i++)
		__FIB(l_fib,fib$r_did_overlay,fib$w_did[l_i]) = l_q->did[l_i];
 
	    l_filespec[0] = EOS;
 
	    for (l_i = 0;  l_i <= 2;  l_i++)
	    {
		l_temp_entry.did[l_i] = l_q->did[l_i];
	    }
 
	    /*
	    **  Initialize FIB to indicate to ACP that a directory
	    **  look-up operation is desired.
	    **/
 
	    __FIB(l_fib,fib$r_nmctl_overlay,fib$w_nmctl) = FIB$M_WILD;
	    l_fib.fib$l_wcc = 0;
 
	    if (!(1 & (l_rc = sys$qiow (
			0,
			l_q->chan,
			IO$_ACCESS,
			(struct _iosb *) l_iosb,
			0,
			0,
			&l_fib_descrip,
			(__int64) &l_str_descrip,
			(__int64) &l_len,
			(__int64) &l_filespec_descrip,
			(__int64) l_acb,
			0 ))))
	        $LOG(l_rc);
 
	    while (l_iosb[0] == SS$_NORMAL ||
		   ((l_iosb[0] == SS$_NOPRIV || l_iosb[0] == SS$_NOSUCHFILE) &&
		     l_filespec[0]))
	    {
		if (!strstr (l_filespec, ".DIR;1"))
		{
		    if (l_total_dids <= 1)
		    {
			strcpy(l_temp_entry.fn, l_filespec);
		    }
		    else if (l_total_devices <= 1)
		    {
			static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ");
			struct dsc$descriptor_s l_out_desc;
			unsigned short int l_fao_len;

			l_out_desc.dsc$w_length  = sizeof(l_temp_entry.fn) - 1;
			l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
			l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
			l_out_desc.dsc$a_pointer = l_temp_entry.fn;
			if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
				    strchr (l_q->equivalent_name, '['), l_filespec))))
			    $LOG(l_rc);
			l_temp_entry.fn[l_fao_len] = EOS;
		    }
		    else
		    {
			static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ");
			struct dsc$descriptor_s l_out_desc;
			unsigned short int l_fao_len;

			l_out_desc.dsc$w_length  = sizeof(l_temp_entry.fn) - 1;
			l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
			l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
			l_out_desc.dsc$a_pointer = l_temp_entry.fn;
			if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
				    l_q->equivalent_name, l_filespec))))
			    $LOG(l_rc);
			l_temp_entry.fn[l_fao_len] = EOS;
		    }
 
		    if (l_iosb[0] == SS$_NOPRIV)
		    {
			l_temp_entry.filesize = 0;
			l_temp_entry.fpro = 0Xffff;	/**  Insufficient privilege  **/
			convert_date_string (
			     "17-NOV-1858 00:00:00.00",
			     &l_temp_entry.cdat);
			memcpy(&l_temp_entry.rdat, &l_temp_entry.cdat, sizeof (DATE_TIME));
			memcpy(&l_temp_entry.edat, &l_temp_entry.cdat, sizeof (DATE_TIME));
			memcpy(&l_temp_entry.bdat, &l_temp_entry.cdat, sizeof (DATE_TIME));
		    }
		    else if (l_iosb[0] == SS$_NOSUCHFILE)
		    {
			l_temp_entry.filesize = 0;
			l_temp_entry.fpro = 0Xfffe;	/**  No such file	     **/
			convert_date_string (
			     "17-NOV-1858 00:00:00.00",
			     &l_temp_entry.cdat);
			memcpy(&l_temp_entry.rdat, &l_temp_entry.cdat, sizeof (DATE_TIME));
			memcpy(&l_temp_entry.edat, &l_temp_entry.cdat, sizeof (DATE_TIME));
			memcpy(&l_temp_entry.bdat, &l_temp_entry.cdat, sizeof (DATE_TIME));
		    }
		    else
		    {
			for (l_i = 0;  l_i <= 2;  l_i++)
			{
			    l_temp_entry.fid[l_i] = __FIB(l_fib,fib$r_fid_overlay,fib$w_fid[l_i]);
			}
			l_temp_entry.filesize =
			    (l_asb.sbk$w_filesizh << 16)+l_asb.sbk$w_filesizl;
			memcpy(&l_temp_entry.cdat, &l_cdat, sizeof (DATE_TIME));
			memcpy(&l_temp_entry.rdat, &l_rdat, sizeof (DATE_TIME));
			memcpy(&l_temp_entry.edat, &l_edat, sizeof (DATE_TIME));
			memcpy(&l_temp_entry.bdat, &l_bdat, sizeof (DATE_TIME));
			l_temp_entry.fpro = l_fpro;
		    }
 
		    l_temp_entry.state = waiting;
 
                    if (l_iosb[0] != SS$_NORMAL)
                    {
                        if (!filter_exclude(
				l_temp_entry.fn, DX_CURRENT_DIRECTORY.exclude_filelist))
                        {
			    if (append_filelist_entry(
				    DX_CURRENT_DIRECTORY.non_dir_filelist,
				    l_temp_entry) == DX__NORMAL)
			    {
				DX_CURRENT_DIRECTORY.tot_files++;
				DX_CURRENT_DIRECTORY.tot_blocks += l_temp_entry.filesize;
			    }
                        }
                    }
                    else if (filter_OK(
				l_temp_entry,
				DX_CURRENT_DIRECTORY.exclude_filelist,
				DX_CURRENT_DIRECTORY.cur_filter))
		    {
			if (append_filelist_entry(
				DX_CURRENT_DIRECTORY.non_dir_filelist,
				l_temp_entry) == DX__NORMAL)
			{
			    DX_CURRENT_DIRECTORY.tot_files++;
			    DX_CURRENT_DIRECTORY.tot_blocks += l_temp_entry.filesize;
			}
		    }
		}
 
		if (!(1 & (l_rc = sys$qiow (
			    0,
			    l_q->chan,
			    IO$_ACCESS,
			    (struct _iosb *) l_iosb,
			    0,
			    0,
			    &l_fib_descrip,
			    (__int64) &l_str_descrip,
			    (__int64) &l_len,
			    (__int64) &l_filespec_descrip,
			    (__int64) l_acb,
			    0 ))))
		    $LOG(l_rc);
	    }
	}
    }

    if (finalize_filelist(
	    DX_CURRENT_DIRECTORY.non_dir_filelist,
	    0,
	    DX_CURRENT_DIRECTORY.sortby,
	    DX_CURRENT_DIRECTORY.sortorder,
	    &l_dup_files,
	    &l_dup_blocks,
	    l_errmsg) != DX__NORMAL)
    {
	signal_err(l_errmsg, bell);
	return DX__ERROR;
    }
    DX_CURRENT_DIRECTORY.tot_files -= l_dup_files;
    DX_CURRENT_DIRECTORY.tot_blocks -= l_dup_blocks;
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FIND_EXCLUDE_FILE() searchs all the files except directory files
**	in the directory that meet "EXCLUDE" qualifier
**
**--
**/
int	find_exclude_file(a_exclude_listp, a_cur_filter, a_sortby, a_sortorder)
struct fil_dx_tag **a_exclude_listp;
struct cur_filter_tag a_cur_filter;
enum sortbys a_sortby;
enum sortorders a_sortorder;
{
    /*
    **  VARIABLES USED BY ACP SERVICES
    **/
 
    unsigned int l_iosb[2];
    static FIBDEF l_fib;
    struct dsc$descriptor_s l_fib_descrip =
	{ sizeof (FIBDEF), DSC$K_DTYPE_T, DSC$K_CLASS_S, (char *)&l_fib };
    unsigned short l_len;
    char l_filespec[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descrip, l_filespec);
 
    /*
    **	    NON-ACP VARABLES
    **/
 
    struct fil_dx_tag *l_new_list;
    register int l_i;
    int l_total_dids;
    int l_total_devices;
    struct fil_dx_tag l_temp_entry;
    char l_str[128];
    $DESCRIPTOR (l_str_descrip, l_str);
    struct filespec_list_tag *l_p;
    struct w_dids_tag *l_q;
    unsigned long l_rc;
 
    /*
    **	Create a new list head.
    **	Free up memory for old list at the end.
    **/
 
    if ((l_new_list = create_MARKER()) == (struct fil_dx_tag *)NULL)
	return DX__ERROR;
 
    l_total_dids = dxscr_count_win_dirs(&a_cur_filter);
    l_total_devices = dxscr_count_win_devices(&a_cur_filter);
 
    for (l_p = a_cur_filter.exclude_filespec_list;  l_p;  l_p = l_p->next)
    {
	/*
	**	Find all the files in this directory that meet the criteria
	**/
 
	strcpy(l_str, l_p->name);
	LENGTH(l_str_descrip) = strlen(l_str);
 
        for (l_q = l_p->w_dids;  l_q;  l_q = l_q->next)
        {
	    for (l_i = 0; l_i <= 2; l_i++)
		__FIB(l_fib,fib$r_did_overlay,fib$w_did[l_i]) = l_q->did[l_i];
 
	    l_filespec[0] = EOS;
 
	    for (l_i = 0;  l_i <= 2;  l_i++)
	    {
		l_temp_entry.did[l_i] = l_q->did[l_i];
	    }
 
	    /*
	    **  Initialize FIB to indicate to ACP that a directory
	    **  look-up operation is desired.
	    **/
 
	    __FIB(l_fib,fib$r_nmctl_overlay,fib$w_nmctl) = FIB$M_WILD;
	    l_fib.fib$l_wcc = 0;
 
	    if (!(1 & (l_rc = sys$qiow (
			0,
			l_q->chan,
			IO$_ACCESS,
			(struct _iosb *) l_iosb,
			0,
			0,
			&l_fib_descrip,
			(__int64) &l_str_descrip,
			(__int64) &l_len,
			(__int64) &l_filespec_descrip,
			0,
			0 ))))
	        $LOG(l_rc);
 
	    while (l_iosb[0] == SS$_NORMAL ||
		   ((l_iosb[0] == SS$_NOPRIV || l_iosb[0] == SS$_NOSUCHFILE) &&
		     l_filespec[0]))
	    {
		if (!strstr (l_filespec, ".DIR;1"))
		{
		    if (l_total_dids <= 1)
		    {
			strcpy(l_temp_entry.fn, l_filespec);
		    }
		    else if (l_total_devices <= 1)
		    {
			static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ");
			struct dsc$descriptor_s l_out_desc;
			unsigned short int l_fao_len;

			l_out_desc.dsc$w_length  = sizeof(l_temp_entry.fn) - 1;
			l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
			l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
			l_out_desc.dsc$a_pointer = l_temp_entry.fn;
			if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
				    strchr (l_q->equivalent_name, '['), l_filespec))))
			    $LOG(l_rc);
			l_temp_entry.fn[l_fao_len] = EOS;
		    }
		    else
		    {
			static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ");
			struct dsc$descriptor_s l_out_desc;
			unsigned short int l_fao_len;

			l_out_desc.dsc$w_length  = sizeof(l_temp_entry.fn) - 1;
			l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
			l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
			l_out_desc.dsc$a_pointer = l_temp_entry.fn;
			if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
				    l_q->equivalent_name, l_filespec))))
			    $LOG(l_rc);
			l_temp_entry.fn[l_fao_len] = EOS;
		    }
 
		    insert_filelist_entry(
			l_new_list, l_temp_entry, a_sortby, a_sortorder);
		}
 
		if (!(1 & (l_rc = sys$qiow (
			    0,
			    l_q->chan,
			    IO$_ACCESS,
			    (struct _iosb *) l_iosb,
			    0,
			    0,
			    &l_fib_descrip,
			    (__int64) &l_str_descrip,
			    (__int64) &l_len,
			    (__int64) &l_filespec_descrip,
			    0,
			    0 ))))
		    $LOG(l_rc);
	    }
	}
    }
 
    free_filelist(a_exclude_listp);
    *a_exclude_listp = l_new_list;
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FIND_MATCH_FILE() matchs the file with all the files that
**	meets the filter 'NAME' criteria.  It return 1
**	if there's a match; otherwise, 0 is returned.
**
**--
**/
int	find_match_file(
		char *a_candidate_file,
		unsigned short int a_win)
{
    /*
    **  VARIABLES USED BY ACP SERVICES
    **/
 
    unsigned int l_iosb[2];
    static FIBDEF l_fib;
    struct dsc$descriptor_s l_fib_descrip =
	{ sizeof (FIBDEF), DSC$K_DTYPE_T, DSC$K_CLASS_S, (char *)&l_fib };
    unsigned short l_len;
    char l_filespec[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descrip, l_filespec);
 
    /*
    **	    NON-ACP VARABLES
    **/
 
    register int l_i;
    char l_str[128];
    $DESCRIPTOR (l_str_descrip, l_str);
    int l_total_dids;
    int l_total_devices;
    struct filespec_list_tag *l_p;
    struct w_dids_tag *l_q;
    char l_fn[MAXFILESPEC+1];
    struct fil_dx_tag *l_head;
    struct fil_dx_tag l_temp_entry, *l_s, *l_t, *l_u;
    int l_found = 0;
    char l_errmsg[256];
    unsigned long l_rc;
 
    if ((l_head = create_MARKER()) == (struct fil_dx_tag *)NULL)
    {
	signal_err("Insufficient virtual memory", bell);
	return  0;
    }
 
    l_total_dids = dxscr_count_win_dirs(&cntrl_info_block.dir_dx[a_win].cur_filter);
    l_total_devices = dxscr_count_win_devices(&cntrl_info_block.dir_dx[a_win].cur_filter);
 
    /*
    **	Search for all files that match the filespec
    **/
 
    for (l_p = cntrl_info_block.dir_dx[a_win].cur_filter.name_filespec_list;  l_p;  l_p = l_p->next)
    {
	/*
	**	Find all the files in this directory that meet the criteria
	**/
 
	strcpy(l_str, l_p->name);
	LENGTH(l_str_descrip) = strlen(l_str);
 
        for (l_q = l_p->w_dids;  l_q;  l_q = l_q->next)
        {
	    for (l_i = 0; l_i <= 2; l_i++)
		__FIB(l_fib,fib$r_did_overlay,fib$w_did[l_i]) = l_q->did[l_i];
 
	    l_filespec[0] = EOS;
 
	    /*
	    **  Initialize FIB to indicate to ACP that a directory
	    **  look-up operation is desired.
	    **/
 
	    __FIB(l_fib,fib$r_nmctl_overlay,fib$w_nmctl) = FIB$M_WILD;
	    l_fib.fib$l_wcc = 0;
 
	    if (!(1 & (l_rc = sys$qiow (
			0,
			l_q->chan,
			IO$_ACCESS,
			(struct _iosb *) l_iosb,
			0,
			0,
			&l_fib_descrip,
			(__int64) &l_str_descrip,
			(__int64) &l_len,
			(__int64) &l_filespec_descrip,
			0,
			0 ))))
	        $LOG(l_rc);
 
	    while (l_iosb[0] == SS$_NORMAL ||
		   ((l_iosb[0] == SS$_NOPRIV || l_iosb[0] == SS$_NOSUCHFILE) &&
		     l_filespec[0]))
	    {
		l_filespec[l_len] = EOS;
		/**  Only full path name is needed  **/
		{
		    static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ");
		    struct dsc$descriptor_s l_out_desc;
		    unsigned short int l_fao_len;

		    l_out_desc.dsc$w_length  = sizeof(l_temp_entry.fn) - 1;
		    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		    l_out_desc.dsc$a_pointer = l_temp_entry.fn;
		    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
				l_q->equivalent_name, l_filespec))))
			$LOG(l_rc);
		    l_temp_entry.fn[l_fao_len] = EOS;
		}
 
		insert_filelist_entry(
		    l_head,
		    l_temp_entry,
		    cntrl_info_block.dir_dx[a_win].sortby,
		    cntrl_info_block.dir_dx[a_win].sortorder);
 
		if (!(1 & (l_rc = sys$qiow (
			    0,
			    l_q->chan,
			    IO$_ACCESS,
			    (struct _iosb *) l_iosb,
			    0,
			    0,
			    &l_fib_descrip,
			    (__int64) &l_str_descrip,
			    (__int64) &l_len,
			    (__int64) &l_filespec_descrip,
			    0,
			    0 ))))
		    $LOG(l_rc);
	    }
	}
    }
 
    /*
    **  Delete a file from non-directory file list if it is no
    **	longer belong to the list due to an insertion of
    **	another file (i.e. remove A.A;13 if A.A;14 was added
    **	in this directory and the file spec is *.*; for the
    **	latest version.)
    **/
 
    for (l_u = l_head->forward;  l_u != l_head;  l_u = l_u->forward)
    {
	if (!strcmp (a_candidate_file, l_u->fn))
	{
	    for (l_s = cntrl_info_block.dir_dx[a_win].non_dir_filelist->forward;
		l_s != cntrl_info_block.dir_dx[a_win].non_dir_filelist;  l_s = l_s->forward)
	    {
		find_full_path_name(l_s->fn, l_fn, a_win);
		for (l_t = l_head->forward;  l_t != l_head;  l_t = l_t->forward)
		{
		    if (!strcmp (l_fn, l_t->fn))
			break;
		}
		if (l_t == l_head)
		{
		    filer_delete$2(l_fn, l_errmsg, a_win);
		    break;
		}
	    }
	    l_found++;
	    break;
	}
    }
 
    free_filelist(&l_head);
 
    return l_found;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FIND_QUEUE_FILE() matchs the file with all the files in the
**	cache queue that meets the filter 'NAME' criteria.  It return 1
**	if there's a match; otherwise, 0 is returned.
**
**--
**/
int	find_queue_file(a_candidate_fid, a_elementp)
unsigned short int a_candidate_fid[3];
struct filer_cache_tag *a_elementp;
{
    /*
    **  VARIABLES USED BY ACP SERVICES
    **/
 
    unsigned int l_iosb[2];
    static FIBDEF l_fib;
    struct dsc$descriptor_s l_fib_descrip =
	{ sizeof (FIBDEF), DSC$K_DTYPE_T, DSC$K_CLASS_S, (char *)&l_fib };
    unsigned short l_len;
    char l_filespec[MAXFILESPEC+1];
    $DESCRIPTOR (l_filespec_descrip, l_filespec);
 
    /*
    **	    NON-ACP VARABLES
    **/
 
    register int l_i;
    char l_str[128];
    $DESCRIPTOR (l_str_descrip, l_str);
    int l_total_dids;
    int l_total_devices;
    struct filespec_list_tag *l_p;
    struct w_dids_tag *l_q;
    char l_fn[MAXFILESPEC+1];
    struct fil_dx_tag *l_head;
    struct fil_dx_tag l_temp_entry, *l_s, *l_t, *l_u;
    int l_found = 0;
    char l_errmsg[256];
    unsigned long l_rc;
 
    if ((l_head = create_MARKER()) == (struct fil_dx_tag *)NULL)
    {
	signal_err("Insufficient virtual memory", bell);
	return  0;
    }
 
    l_total_dids = dxscr_count_win_dirs(&a_elementp->cur_filter);
    l_total_devices = dxscr_count_win_devices(&a_elementp->cur_filter);
 
    /*
    **	Search for all files that match the filespec
    **/
 
    for (l_p = a_elementp->cur_filter.name_filespec_list;  l_p;  l_p = l_p->next)
    {
	/*
	**	Find all the files in this directory that meet the criteria
	**/
 
	strcpy(l_str, l_p->name);
	LENGTH(l_str_descrip) = strlen(l_str);
 
        for (l_q = l_p->w_dids;  l_q;  l_q = l_q->next)
        {
	    for (l_i = 0; l_i <= 2; l_i++)
		__FIB(l_fib,fib$r_did_overlay,fib$w_did[l_i]) = l_q->did[l_i];
 
	    l_filespec[0] = EOS;
 
	    /*
	    **  Initialize FIB to indicate to ACP that a directory
	    **  look-up operation is desired.
	    **/
 
	    __FIB(l_fib,fib$r_nmctl_overlay,fib$w_nmctl) = FIB$M_WILD;
	    l_fib.fib$l_wcc = 0;
 
	    if (!(1 & (l_rc = sys$qiow (
			0,
			l_q->chan,
			IO$_ACCESS,
			(struct _iosb *) l_iosb,
			0,
			0,
			&l_fib_descrip,
			(__int64) &l_str_descrip,
			(__int64) &l_len,
			(__int64) &l_filespec_descrip,
			0,
			0 ))))
	        $LOG(l_rc);
 
	    while (l_iosb[0] == SS$_NORMAL ||
		   ((l_iosb[0] == SS$_NOPRIV || l_iosb[0] == SS$_NOSUCHFILE) &&
		     l_filespec[0]))
	    {
		l_filespec[l_len] = EOS;
		if (l_total_dids <= 1)
		{
		    strcpy(l_temp_entry.fn, l_filespec);
		}
		else if (l_total_devices <= 1)
		{
		    static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ");
		    struct dsc$descriptor_s l_out_desc;
		    unsigned short int l_fao_len;

		    l_out_desc.dsc$w_length  = sizeof(l_temp_entry.fn) - 1;
		    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		    l_out_desc.dsc$a_pointer = l_temp_entry.fn;
		    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
				strchr (l_q->equivalent_name, '['), l_filespec))))
			$LOG(l_rc);
		    l_temp_entry.fn[l_fao_len] = EOS;
		}
		else
		{
		    static $DESCRIPTOR (l_fao_ctrl, "!AZ!AZ");
		    struct dsc$descriptor_s l_out_desc;
		    unsigned short int l_fao_len;

		    l_out_desc.dsc$w_length  = sizeof(l_temp_entry.fn) - 1;
		    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
		    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
		    l_out_desc.dsc$a_pointer = l_temp_entry.fn;
		    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_fao_len, &l_out_desc,
				l_q->equivalent_name, l_filespec))))
			$LOG(l_rc);
		    l_temp_entry.fn[l_fao_len] = EOS;
		}
 
		for (l_i = 0;  l_i <= 2;  l_i++)
		{
		    l_temp_entry.fid[l_i] = __FIB(l_fib,fib$r_fid_overlay,fib$w_fid[l_i]);
		}
 
		insert_filelist_entry(
		    l_head,
		    l_temp_entry,
		    a_elementp->sortby,
		    a_elementp->sortorder);
 
		if (!(1 & (l_rc = sys$qiow (
			    0,
			    l_q->chan,
			    IO$_ACCESS,
			    (struct _iosb *) l_iosb,
			    0,
			    0,
			    &l_fib_descrip,
			    (__int64) &l_str_descrip,
			    (__int64) &l_len,
			    (__int64) &l_filespec_descrip,
			    0,
			    0 ))))
		    $LOG(l_rc);
	    }
	}
    }
 
    /*
    **  Delete a file from non-directory file list if it is no
    **	longer belong to the list due to an insertion of
    **	another file (i.e. remove A.A;13 if A.A;14 was added
    **	in this directory and the file spec is *.*; for the
    **	latest version.)
    **/
 
    for (l_u = l_head->forward;  l_u != l_head;  l_u = l_u->forward)
    {
	if (a_candidate_fid[0] == l_u->fid[0] &&
	    a_candidate_fid[1] == l_u->fid[1] &&
	    a_candidate_fid[2] == l_u->fid[2])
	{
	    for (l_s = a_elementp->non_dir_filelist->forward;
		    l_s != a_elementp->non_dir_filelist;  l_s = l_s->forward)
	    {
		for (l_t = l_head->forward;  l_t != l_head;  l_t = l_t->forward)
		{
		    if (l_s->fid[0] == l_t->fid[0] &&
			l_s->fid[1] == l_t->fid[1] &&
			l_s->fid[2] == l_t->fid[2])
			break;
		}
		if (l_t == l_head)
		{	/**  This file is no longer belong to the list  **/
		    a_elementp->tot_files--;
		    a_elementp->tot_blocks -= l_s->filesize;
		    remove_filelist_entry(&l_s);
		    break;
		}
	    }
	    l_found++;
	    break;
	}
    }
 
    free_filelist(&l_head);
 
    return l_found;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Allocates and initializes a fil_dx_tag MARKER node: the sentinel
**	head of a circular doubly-linked file list (dir_filelist,
**	non_dir_filelist, exclude_filelist, and the temporary lists used by
**	find_match_file()/find_queue_file()/find_exclude_file()).  A fresh
**	marker's forward/backward both point at itself (an empty list).
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
**	None.
**
**  RETURNS VALUE:
**
**	Pointer to the new marker node, or NULL if lib$get_vm() failed.
**
**--
**/
struct fil_dx_tag *	create_MARKER()
{
    struct fil_dx_tag *l_marker;
    int l_allocate_size;
    unsigned long l_rc;
 
    /*
    **	Allocate virtual memory for MARKER
    **/
 
    l_allocate_size = sizeof (struct fil_dx_tag);
    if (!(1 & (l_rc = lib$get_vm (
		 &l_allocate_size,
		 &l_marker,
		 0))))
    {
        $LOG(l_rc);
	return (struct fil_dx_tag *)NULL;
    }
 
    /*
    **	Initialize MARKER
    **/
 
    l_marker->fid[0] = l_marker->fid[1] = l_marker->fid[2] = 0;
    l_marker->did[0] = l_marker->did[1] = l_marker->did[2] = 0;
    l_marker->fn[0] = EOS;
    l_marker->filesize = 0;
    l_marker->fpro = 0xffff;
    l_marker->state = waiting;
    l_marker->beg_y = l_marker->beg_x = 0;
    l_marker->forward = l_marker->backward = l_marker;
 
    return l_marker;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      ADD_FILELIST_ENTRY() insert a entry to the list
**
**--
**/
int	add_filelist_entry(a_before, a_after, a_current)
struct fil_dx_tag *a_before;
struct fil_dx_tag *a_after;
struct fil_dx_tag *a_current;
{
 
    /*
    **  Test if FORWARD and BACKWARD links are correct
    **/
 
    if (a_before->forward != a_after)
	return DX__ERROR;
 
    if (a_before != a_after->backward)
	return DX__ERROR;
 
    /*
    **  Re-link with new entry
    **/
 
    a_before->forward = a_current;
    a_current->backward = a_before;
 
    a_after->backward = a_current;
    a_current->forward = a_after;
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FREE_FILELIST() frees up the memory used by the list
**
**--
**/
int	free_filelist(a_marker)
struct fil_dx_tag **a_marker;
{
    struct fil_dx_tag *l_f, *l_g;
 
    if (*a_marker != (struct fil_dx_tag *)NULL)
    {
	l_f = (*a_marker)->forward;
	while (l_f != *a_marker)
	{
	    l_g = l_f->forward;
	    remove_filelist_entry(&l_f);
	    l_f = l_g;
	}
	remove_filelist_entry(a_marker);
	*a_marker = (struct fil_dx_tag *)NULL;
    }
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      REMOVE_FILELIST_ENTRY() removes a entry from the list
**
**--
**/
int	remove_filelist_entry(a_file_entry)
struct fil_dx_tag **a_file_entry;
{
    struct fil_dx_tag *l_before, *l_after;
    int l_allocate_size;
    unsigned long l_rc;
 
    l_before = (*a_file_entry)->backward;
    l_after = (*a_file_entry)->forward;
 
    l_before->forward = l_after;
    l_after->backward = l_before;
 
    l_allocate_size = sizeof (struct fil_dx_tag);
 
    if (!(1 & (l_rc = lib$free_vm(
		&l_allocate_size,
		a_file_entry,
		0))))
        $LOG(l_rc);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      INSERT_FILELIST_ENTRY() finds a slot to insert a new entry.
**	It searchs starting from the list head using the current sorting
**	cirteria and order.
**
**--
**/
int	insert_filelist_entry(a_l, a_e, a_sortby, a_sortorder)
struct fil_dx_tag *a_l;
struct fil_dx_tag a_e;
enum sortbys a_sortby;
enum sortorders a_sortorder;
{
 
    struct fil_dx_tag *l_f, *l_g;
    int l_allocate_size;
    int l_i;
 
    CMPFUNC l_cmpfunc;
    unsigned long l_rc;

    if (a_e.fn[strlen(a_e.fn)-1] == ']')
        l_cmpfunc = sort_by_dir;
    else
    {
	/**  Shared with filer_sort$2() (FILERSRT.C) so the two ways
	**   of ordering this list can never disagree.  **/
	l_cmpfunc = pick_sort_cmpfunc(a_sortby);
    }
 
    for (l_g = a_l->forward; l_g != a_l; l_g = l_g->forward)
 
    {
        if (a_e.did[0] == l_g->did[0] && a_e.did[1] == l_g->did[1] &&
	    a_e.did[2] == l_g->did[2])
        {
	    if (!strcmp (l_g->fn, a_e.fn))
	    {	    /**  A file with the same name found  **/
		return DX__DONTCARE;
	    }
 
	    if (a_sortorder == ascending)
	    {
		if ((*l_cmpfunc)(&a_e, l_g, ascending) <= 0)
		    break;
	    } else
	    {
		if ((*l_cmpfunc)(&a_e, l_g, descending) >= 0)
		    break;
	    }
        }
    }
 
    /*
    **	Allocate virtual memory for the new entry
    **/
 
    l_allocate_size = sizeof (struct fil_dx_tag);
    if (!(1 & (l_rc = lib$get_vm (
	     &l_allocate_size,
	     &l_f,
	     0))))
        return $LOG(l_rc);
 
    for (l_i = 0;  l_i <= 2;  l_i++)
    {
	l_f->fid[l_i] = a_e.fid[l_i];
	l_f->did[l_i] = a_e.did[l_i];
    }
    l_f->filesize = a_e.filesize;
    memcpy(&l_f->cdat, &a_e.cdat, sizeof (DATE_TIME));
    memcpy(&l_f->rdat, &a_e.rdat, sizeof (DATE_TIME));
    memcpy(&l_f->edat, &a_e.edat, sizeof (DATE_TIME));
    memcpy(&l_f->bdat, &a_e.bdat, sizeof (DATE_TIME));
    l_f->fpro = a_e.fpro;
    strcpy(l_f->fn, a_e.fn);
    l_f->state = a_e.state;
    l_f->beg_y = a_e.beg_y;
    l_f->beg_x = a_e.beg_x;
    l_f->forward = a_e.forward;
    l_f->backward = a_e.backward;

    return add_filelist_entry(l_g->backward, l_g, l_f);
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      APPEND_FILELIST_ENTRY() adds a new entry at the tail of the
**	list, unsorted and without a duplicate check -- O(1), unlike
**	INSERT_FILELIST_ENTRY()'s O(n) scan for an insertion slot.
**
**	Used during the initial directory scan (FIND_DIRECT_FILE(),
**	FIND_NON_DIRECT_FILE()), where ordering and same-did/same-name
**	duplicate removal are both deferred to a single bulk pass
**	(FINALIZE_FILELIST() -- FILERSRT.C) run once after the scan
**	completes, instead of re-scanning the growing list on every file
**	found -- which used to make the whole scan O(n^2) for a directory
**	of n files.
**
**--
**/
int	append_filelist_entry(a_l, a_e)
struct fil_dx_tag *a_l;
struct fil_dx_tag a_e;
{
    struct fil_dx_tag *l_f;
    int l_allocate_size;
    int l_i;
    unsigned long l_rc;

    l_allocate_size = sizeof (struct fil_dx_tag);
    if (!(1 & (l_rc = lib$get_vm (
	     &l_allocate_size,
	     &l_f,
	     0))))
        return $LOG(l_rc);

    for (l_i = 0;  l_i <= 2;  l_i++)
    {
	l_f->fid[l_i] = a_e.fid[l_i];
	l_f->did[l_i] = a_e.did[l_i];
    }
    l_f->filesize = a_e.filesize;
    memcpy(&l_f->cdat, &a_e.cdat, sizeof (DATE_TIME));
    memcpy(&l_f->rdat, &a_e.rdat, sizeof (DATE_TIME));
    memcpy(&l_f->edat, &a_e.edat, sizeof (DATE_TIME));
    memcpy(&l_f->bdat, &a_e.bdat, sizeof (DATE_TIME));
    l_f->fpro = a_e.fpro;
    strcpy(l_f->fn, a_e.fn);
    l_f->state = a_e.state;
    l_f->beg_y = a_e.beg_y;
    l_f->beg_x = a_e.beg_x;

    return add_filelist_entry(a_l->backward, a_l, l_f);
}



/* ---- (from FILERSRT.C -- sort commands + comparators) ---- */

static int		    filer_sort$2(enum sortbys, enum sortorders, char *);



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"by Name..." menu command: prompts for ASCENDING/DESCENDING via
**	get_userinput_and_execute(), then re-sorts the current window's
**	file list by name through filer_sort$1()/filer_sort$2().
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
**	DX_CURRENT_DIRECTORY.sortby/sortorder (via filer_sort$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_sort_name()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error sorting files by name");
    get_userinput_and_execute(
	filer_sort$1,
	"Sort By Name",
	"By what? [ASCENDING or DESCENDING] ",
	"",
	l_errmsg,
	"Name");
     return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"by Type..." menu command: prompts for ASCENDING/DESCENDING via
**	get_userinput_and_execute(), then re-sorts the current window's
**	file list by file type through filer_sort$1()/filer_sort$2().
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
**	DX_CURRENT_DIRECTORY.sortby/sortorder (via filer_sort$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_sort_type()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error sorting files by type");
    get_userinput_and_execute(
	filer_sort$1,
	"Sort By Type",
	"By what? [ASCENDING or DESCENDING] ",
	"",
	l_errmsg,
	"Type");
     return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"by Date..." menu command: prompts for ASCENDING/DESCENDING via
**	get_userinput_and_execute(), then re-sorts the current window's
**	file list by date (whichever date field is currently selected --
**	creation/revision/expiration/backup) through filer_sort$1()/
**	filer_sort$2().
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
**	DX_CURRENT_DIRECTORY.cur_filter.date (picks which date field).
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.sortby/sortorder (via filer_sort$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_sort_date()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error sorting files by date");
    get_userinput_and_execute(
	filer_sort$1,
	"Sort By Date",
	"By what? [ASCENDING or DESCENDING] ",
	"",
	l_errmsg,
	"Date");
     return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"by Size..." menu command: prompts for ASCENDING/DESCENDING via
**	get_userinput_and_execute(), then re-sorts the current window's
**	file list by size through filer_sort$1()/filer_sort$2().
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
**	DX_CURRENT_DIRECTORY.sortby/sortorder (via filer_sort$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_sort_size()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error sorting files by size");
    get_userinput_and_execute(
	filer_sort$1,
	"Sort By Size",
	"By what? [ASCENDING or DESCENDING] ",
	"",
	l_errmsg,
	"Size");
     return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"by Marked..." menu command: prompts for ASCENDING/DESCENDING via
**	get_userinput_and_execute(), then re-sorts the current window's
**	file list by marked/unmarked state through filer_sort$1()/
**	filer_sort$2().
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
**	DX_CURRENT_DIRECTORY.sortby/sortorder (via filer_sort$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_sort_mark()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error sorting files by marked");
    get_userinput_and_execute(
	filer_sort$1,
	"Sort By Marked",
	"By what? [ASCENDING or DESCENDING] ",
	"",
	l_errmsg,
	"Mark");
     return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Callback for the filer_sort_xxx() commands' user-input prompt:
**	parses a_criteria as "ASCENDING"/"DESCENDING" (any unambiguous
**	prefix) into DX_CURRENT_DIRECTORY.sortorder, maps a_sortby's first
**	letter to the corresponding sortbys enum value into
**	DX_CURRENT_DIRECTORY.sortby (for 'D', the actual date field --
**	creation/revision/expiration/backup -- comes from
**	DX_CURRENT_DIRECTORY.cur_filter.date), then performs the resort via
**	filer_sort$2().
**
**  INPUT PARAMETERS:
**
**	a_criteria  -  "ASCENDING" or "DESCENDING" (or an unambiguous
**		       prefix of either).
**	a_sortby    -  one-letter sort key: 'N'ame, 'T'ype, 'D'ate, 'S'ize,
**		       'M'arked.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message on DX__ERROR.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY.cur_filter.date.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.sortby, DX_CURRENT_DIRECTORY.sortorder.
**
**  RETURNS VALUE:
**
**	DX__ERROR if a_criteria or a_sortby is not recognized; otherwise
**	whatever filer_sort$2() returns.
**
**--
**/
int	filer_sort$1(a_criteria, a_sortby, a_errmsg)
char *a_criteria;
char *a_sortby;
char *a_errmsg;
{
    int l_len;
    int l_status;
 
    l_len = strlen(a_criteria);
    if (!strncmp("ASCENDING", a_criteria, l_len))
    {
	DX_CURRENT_DIRECTORY.sortorder = ascending;
    }
    else if (!strncmp("DESCENDING", a_criteria, l_len))
    {
	DX_CURRENT_DIRECTORY.sortorder = descending;
    }
    else
    {
	strcpy(a_errmsg, "Incorrect response, re-specify sorting order.");
	return DX__ERROR;
    }
 
    switch (a_sortby[0])
    {
	case 'N':
	    DX_CURRENT_DIRECTORY.sortby = sortname;
	    break;
	case 'T':
	    DX_CURRENT_DIRECTORY.sortby = sorttype;
	    break;
	case 'D':
	    switch (DX_CURRENT_DIRECTORY.cur_filter.date)
	    {
	    	 case DX_CREDATE:
		     DX_CURRENT_DIRECTORY.sortby = sortcredate;
		     break;
	    	 case DX_REVDATE:
		     DX_CURRENT_DIRECTORY.sortby = sortrevdate;
		     break;
	    	 case DX_EXPDATE:
		     DX_CURRENT_DIRECTORY.sortby = sortexpdate;
		     break;
	    	 case DX_BAKDATE:
		     DX_CURRENT_DIRECTORY.sortby = sortbakdate;
		     break;
		default:
		    strcpy(a_errmsg, "Invalid sort date key");
		    return DX__ERROR;
	    }
	    break;
	case 'S':
	    DX_CURRENT_DIRECTORY.sortby = sortsize;
	    break;
	case 'M':
	    DX_CURRENT_DIRECTORY.sortby = sortmark;
	    break;
	default:
	    strcpy(a_errmsg, "Invalid sort key");
	    return DX__ERROR;
    }
 
    return filer_sort$2(
		DX_CURRENT_DIRECTORY.sortby,
		DX_CURRENT_DIRECTORY.sortorder,
		a_errmsg);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      PICK_SORT_CMPFUNC() maps a sort criterion to its comparator
**	function -- shared by insert_filelist_entry() (FINDFILE.C) and
**	filer_sort$2() below, so the two stay in sync.
**
**--
**/
CMPFUNC	pick_sort_cmpfunc(a_sortby)
enum sortbys a_sortby;
{
    switch (a_sortby)
    {
	case sortname:    return sort_by_name;
	case sorttype:    return sort_by_type;
	case sortcredate: return sort_by_credate;
	case sortrevdate: return sort_by_revdate;
	case sortexpdate: return sort_by_expdate;
	case sortbakdate: return sort_by_bakdate;
	case sortsize:    return sort_by_size;
	case sortmark:    return sort_by_mark;
    }
    return sort_by_name;	    /**  Should not happen  **/
}


/*
**	filer_sort$2()'s array-sort helpers -- see that function's
**	header for why entries are grouped by "did" before applying the
**	usual comparator.
*/
struct s_sort_elem_tag
	{
	struct fil_dx_tag *entry;
	int group;
	int origindex;
	};

static CMPFUNC		s_sort_cmpfunc;
static enum sortorders	s_sort_sortorder;

static int	s_sort_compare(a_ap, a_bp)
const void *a_ap;
const void *a_bp;
{
    const struct s_sort_elem_tag *l_a = (const struct s_sort_elem_tag *) a_ap;
    const struct s_sort_elem_tag *l_b = (const struct s_sort_elem_tag *) a_bp;
    int l_cmp;

    if (l_a->group != l_b->group)
	return l_a->group - l_b->group;

    l_cmp = (*s_sort_cmpfunc)(l_a->entry, l_b->entry, s_sort_sortorder);
    if (l_cmp != 0)
	return l_cmp;

    /**  Equal by the chosen criterion -- fall back to original list
    **   position so the sort is fully deterministic (qsort() need
    **   not be stable, but with a strict order like this there is
    **   only one possible correctly-sorted permutation -- exactly
    **   the stable one).  **/
    return l_a->origindex - l_b->origindex;
}


/*
**	FINALIZE_FILELIST()'s array-sort helpers.  A separate comparator
**	from S_SORT_COMPARE() above: this one needs to guarantee that two
**	exact-duplicate entries (same "did", same file spec -- the same
**	physical file reached twice through overlapping logical-name
**	equivalences) always sort strictly adjacent, so the caller can
**	drop them with a single pass over the sorted array.  Falling back
**	to the raw file spec (instead of S_SORT_COMPARE()'s scan-order
**	ORIGINDEX) guarantees exactly that.
*/
struct s_final_elem_tag
	{
	struct fil_dx_tag *entry;
	int group;
	};

static CMPFUNC		s_final_cmpfunc;
static enum sortorders	s_final_sortorder;

static int	s_final_compare(a_ap, a_bp)
const void *a_ap;
const void *a_bp;
{
    const struct s_final_elem_tag *l_a = (const struct s_final_elem_tag *) a_ap;
    const struct s_final_elem_tag *l_b = (const struct s_final_elem_tag *) a_bp;
    int l_cmp;

    if (l_a->group != l_b->group)
	return l_a->group - l_b->group;

    l_cmp = (*s_final_cmpfunc)(l_a->entry, l_b->entry, s_final_sortorder);
    if (l_cmp != 0)
	return l_cmp;

    return strcmp(l_a->entry->fn, l_b->entry->fn);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      FINALIZE_FILELIST() is called once after a directory scan has
**	appended all its entries, in arbitrary ACP-return order (via
**	APPEND_FILELIST_ENTRY() -- FINDFILE.C), to sort the list into
**	display order and drop duplicate entries in a single O(n log n)
**	pass.  This replaces the old per-file INSERT_FILELIST_ENTRY()
**	scheme, which did an O(n) list scan -- for both ordering *and*
**	duplicate detection -- on every one of the n files found, i.e.
**	O(n^2) overall for a directory of n files.
**
**	A duplicate is a file reached twice with the same "did" (through
**	two overlapping logical-name equivalences naming the same
**	physical directory) and the same full file spec; the first copy
**	found in sorted order is kept, the rest are discarded -- the same
**	outcome INSERT_FILELIST_ENTRY() gave (its scan always hit the
**	already-inserted copy before deciding where a later duplicate
**	would go).  *DUP_FILES and *DUP_BLOCKS return the count and total
**	size of the discarded duplicates so the caller can back them out
**	of its running DX_CURRENT_DIRECTORY.tot_files/tot_blocks (bumped
**	optimistically as each entry was appended, before duplicates were
**	known).
**
**	IS_DIR_LIST selects the SORT_BY_DIR comparator, matching what
**	INSERT_FILELIST_ENTRY() picked for a directory entry (one whose
**	file spec ends in ']'); otherwise the SORTBY/SORTORDER criterion
**	is used, same as for a resort (see PICK_SORT_CMPFUNC() above).
**
**--
**/
int	finalize_filelist(a_l, a_is_dir_list, a_sortby, a_sortorder, a_dup_files, a_dup_blocks, a_errmsg)
struct fil_dx_tag *a_l;
int a_is_dir_list;
enum sortbys a_sortby;
enum sortorders a_sortorder;
long int *a_dup_files;
long int *a_dup_blocks;
char *a_errmsg;
{
    struct fil_dx_tag *l_f, *l_g, *l_last_kept;
    struct s_final_elem_tag *l_a;
    unsigned short int l_seen_did[MAXWINDOWS+1][3];
    int l_allocate_size, l_free_size;
    int l_n, l_i, l_seen_count;
    unsigned long l_rc;

    *a_dup_files = 0;
    *a_dup_blocks = 0;

    l_n = 0;
    for (l_f = a_l->forward;  l_f != a_l;  l_f = l_f->forward)
    {
	l_n++;
    }

    if (l_n == 0)
	return DX__NORMAL;

    l_allocate_size = l_n * sizeof (struct s_final_elem_tag);
    if (!(lib$get_vm (&l_allocate_size, &l_a, 0) & 1))
    {
	strcpy(a_errmsg, "Insufficient virtual memory, directory scan aborted");
	return DX__ERROR;
    }

    /**  Group by "did", same rationale as FILER_SORT$2() below --
    **   keeps several physical directories merged into one window
    **   each internally sorted, without letting their files
    **   interleave.  **/
    l_seen_count = 0;
    l_i = 0;
    for (l_f = a_l->forward;  l_f != a_l;  l_f = l_f->forward, l_i++)
    {
	int l_k, l_group;

	l_group = -1;
	for (l_k = 0;  l_k < l_seen_count;  l_k++)
	{
	    if (l_seen_did[l_k][0] == l_f->did[0] &&
		l_seen_did[l_k][1] == l_f->did[1] &&
		l_seen_did[l_k][2] == l_f->did[2])
	    {
		l_group = l_k;
		break;
	    }
	}
	if (l_group == -1 && l_seen_count <= MAXWINDOWS)
	{
	    l_group = l_seen_count;
	    l_seen_did[l_seen_count][0] = l_f->did[0];
	    l_seen_did[l_seen_count][1] = l_f->did[1];
	    l_seen_did[l_seen_count][2] = l_f->did[2];
	    l_seen_count++;
	}
	else if (l_group == -1)
	{
	    /**  See the identical fallback in FILER_SORT$2() below.  **/
	    l_group = MAXWINDOWS;
	}

	l_a[l_i].entry = l_f;
	l_a[l_i].group = l_group;
    }

    s_final_cmpfunc = a_is_dir_list? sort_by_dir : pick_sort_cmpfunc(a_sortby);
    s_final_sortorder = a_sortorder;
    qsort(l_a, l_n, sizeof (struct s_final_elem_tag), s_final_compare);

    /*
    **	Relink from the sorted array, dropping any entry that is an
    **	exact duplicate of the last one kept -- S_FINAL_COMPARE()
    **	guarantees such entries sort strictly adjacent, so comparing
    **	each candidate only against LAST_KEPT (never against a slot
    **	that may itself have just been freed as a duplicate) is enough.
    */

    l_g = a_l;
    l_last_kept = (struct fil_dx_tag *) NULL;
    for (l_i = 0;  l_i < l_n;  l_i++)
    {
	if (l_last_kept != (struct fil_dx_tag *) NULL &&
	    l_a[l_i].entry->did[0] == l_last_kept->did[0] &&
	    l_a[l_i].entry->did[1] == l_last_kept->did[1] &&
	    l_a[l_i].entry->did[2] == l_last_kept->did[2] &&
	    !strcmp (l_a[l_i].entry->fn, l_last_kept->fn))
	{
	    (*a_dup_files)++;
	    *a_dup_blocks += l_a[l_i].entry->filesize;

	    l_free_size = sizeof (struct fil_dx_tag);
	    if (!(1 & (l_rc = lib$free_vm (&l_free_size, &l_a[l_i].entry, 0))))
	        $LOG(l_rc);
	    continue;
	}

	l_g->forward = l_a[l_i].entry;
	l_a[l_i].entry->backward = l_g;
	l_g = l_a[l_i].entry;
	l_last_kept = l_a[l_i].entry;
    }
    l_g->forward = a_l;
    a_l->backward = l_g;

    lib$free_vm (&l_allocate_size, &l_a, 0);

    return DX__NORMAL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Re-sorts the current window's non_dir_filelist in place by the
**	given criterion (dir_filelist is never resorted -- directories
**	always stay in create/scan order at the top), by collecting
**	pointers to the existing nodes into an array, grouping them by
**	"did" (so several physical directories merged into one window each
**	stay internally sorted without interleaving -- see the long inline
**	comment below for why), sorting the array with s_sort_compare(),
**	and relinking.  Then recomputes screen positions and repaints the
**	whole filer display.
**
**  INPUT PARAMETERS:
**
**	a_sortby     -  sort criterion (name/type/date/size/mark).
**	a_sortorder  -  ascending or descending.
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message on DX__ERROR.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY (file lists, filer_display), cntrl_info_block.
**	cur_win.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.non_dir_filelist (relinked in new order),
**	DX_CURRENT_DIRECTORY.cur_file, DX_CURRENT_DIRECTORY.filer_display
**	(repainted).
**
**  RETURNS VALUE:
**
**	DX__ERROR if out of virtual memory; DX__NORMAL otherwise.
**
**--
**/
static int	filer_sort$2(a_sortby, a_sortorder, a_errmsg)
enum sortbys a_sortby;
enum sortorders a_sortorder;
char *a_errmsg;
{
    struct fil_dx_tag *l_f, *l_g;
    int l_allocate_size;
    int l_row, l_col;
    int l_n, l_i;
    struct s_sort_elem_tag *l_a;
    unsigned short int l_seen_did[MAXWINDOWS+1][3];
    int l_seen_count;
    unsigned long l_rc;

    /*
    **	Reorder the list IN PLACE (directory filelist will not be
    **	changed).  This used to rebuild the whole list one file at a
    **	time via insert_filelist_entry() into a fresh MARKER list --
    **	for every one of the n files already sitting in memory, that
    **	did a linear scan of the (growing) target list *and* a fresh
    **	lib$get_vm()+field copy, i.e. O(n^2) comparisons plus n
    **	needless allocations just to resort data we already have.
    **
    **	Instead: collect pointers to the existing nodes into an
    **	array, sort the array itself (same node objects -- no
    **	allocation, no copy), then relink.  O(n log n).
    **
    **	insert_filelist_entry() only ever compares two entries when
    **	they share the same "did" (directory ID) -- this is what lets
    **	several physical directories, reached through different
    **	logical-name equivalences, be merged into one filer window
    **	while each keeps its own internal order.  We reproduce the
    **	"each did's files sorted amongst themselves" part exactly by
    **	sorting first on "which did this entry belongs to" (grouped
    **	in the order those did's were first seen), then by the usual
    **	criterion within a group.  NOTE: for the common case of a
    **	single directory (one did for the whole list -- the vast
    **	majority of real usage) this produces IDENTICAL results to
    **	the old algorithm.  For the rarer merged-directories case, the
    **	old algorithm could interleave different did's file-by-file
    **	depending on insertion order, an accidental side effect of
    **	the insertion-sort-via-linked-list implementation rather than
    **	a documented behaviour; this version always keeps each did's
    **	files together as one contiguous, internally sorted block,
    **	which is simpler to reason about and just as correct a sort.
    */

    l_n = 0;
    for (l_f = DX_CURRENT_DIRECTORY.non_dir_filelist->forward;
	 l_f != DX_CURRENT_DIRECTORY.non_dir_filelist; l_f = l_f->forward)
    {
	l_n++;
    }

    /**  Nothing to reorder -- fall straight through to the position/
    **   redraw logic below, same as the old code did when the loop
    **   over insert_filelist_entry() simply ran zero times.  **/
    if (l_n > 0)
    {
	l_allocate_size = l_n * sizeof (struct s_sort_elem_tag);
	if (!(lib$get_vm (&l_allocate_size, &l_a, 0) & 1))
	{
	    strcpy(a_errmsg, "Insufficient virtual memory, SORT operation aborted");
	    return DX__ERROR;
	}

	l_seen_count = 0;
	l_i = 0;
	for (l_f = DX_CURRENT_DIRECTORY.non_dir_filelist->forward;
	     l_f != DX_CURRENT_DIRECTORY.non_dir_filelist; l_f = l_f->forward, l_i++)
	{
	    int l_k, l_group;

	    l_group = -1;
	    for (l_k = 0;  l_k < l_seen_count;  l_k++)
	    {
		if (l_seen_did[l_k][0] == l_f->did[0] &&
		    l_seen_did[l_k][1] == l_f->did[1] &&
		    l_seen_did[l_k][2] == l_f->did[2])
		{
		    l_group = l_k;
		    break;
		}
	    }
	    if (l_group == -1 && l_seen_count <= MAXWINDOWS)
	    {
		l_group = l_seen_count;
		l_seen_did[l_seen_count][0] = l_f->did[0];
		l_seen_did[l_seen_count][1] = l_f->did[1];
		l_seen_did[l_seen_count][2] = l_f->did[2];
		l_seen_count++;
	    }
	    else if (l_group == -1)
	    {
		/**  More distinct directories merged into one window than
		**   we bothered to track individually -- fall back to a
		**   single catch-all group.  With MAXWINDOWS+1 slots this
		**   cannot happen through DX's own UI (see FILERWIN.C),
		**   only guards against some future caller.  **/
		l_group = MAXWINDOWS;
	    }

	    l_a[l_i].entry = l_f;
	    l_a[l_i].group = l_group;
	    l_a[l_i].origindex = l_i;
	}

	s_sort_cmpfunc = pick_sort_cmpfunc(a_sortby);
	s_sort_sortorder = a_sortorder;
	qsort(l_a, l_n, sizeof (struct s_sort_elem_tag), s_sort_compare);

	l_g = DX_CURRENT_DIRECTORY.non_dir_filelist;
	for (l_i = 0;  l_i < l_n;  l_i++)
	{
	    l_g->forward = l_a[l_i].entry;
	    l_a[l_i].entry->backward = l_g;
	    l_g = l_a[l_i].entry;
	}
	l_g->forward = DX_CURRENT_DIRECTORY.non_dir_filelist;
	DX_CURRENT_DIRECTORY.non_dir_filelist->backward = l_g;

	lib$free_vm (&l_allocate_size, &l_a, 0);
    }

    /*
    **	Compute the position of each file
    **/

    DX_CURRENT_DIRECTORY.dir_filelist->beg_y =
	DX_CURRENT_DIRECTORY.dir_filelist->beg_x = 1;
    l_row = l_col = 1;
    compute_filer_begin_xy (
	DX_CURRENT_DIRECTORY.dir_filelist, &l_row, &l_col, cntrl_info_block.cur_win);
    DX_CURRENT_DIRECTORY.non_dir_filelist->beg_y =
	DX_CURRENT_DIRECTORY.dir_filelist->backward->beg_y;
    DX_CURRENT_DIRECTORY.non_dir_filelist->beg_x =
	DX_CURRENT_DIRECTORY.dir_filelist->backward->beg_x;
    compute_filer_begin_xy (
	DX_CURRENT_DIRECTORY.non_dir_filelist, &l_row, &l_col, cntrl_info_block.cur_win);

    /*
    **	Re-disply the screen
    **/

    if (!(1 & (l_rc = smg$unpaste_virtual_display (
	      &DX_CURRENT_DIRECTORY.filer_display.id,
	      &cntrl_info_block.pasteboard_id))))
        $LOG(l_rc);

    if (!(1 & (l_rc = smg$erase_display (
	      &DX_CURRENT_DIRECTORY.filer_display.id,
	      0,
	      0,
	      0,
	      0))))
        $LOG(l_rc);

    DX_CURRENT_DIRECTORY.filer_display.rows =
	max (DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y,
	     DX_CURRENT_DIRECTORY.filer_display.view_rows);

    if (!(1 & (l_rc = smg$change_virtual_display (
                 &DX_CURRENT_DIRECTORY.filer_display.id,
                 &DX_CURRENT_DIRECTORY.filer_display.rows,
                 &DX_CURRENT_DIRECTORY.filer_display.width,
                 0,
                 0,
                 0))))
        $LOG(l_rc);

    DX_CURRENT_DIRECTORY.filer_display.view_beg_y = 1;
    DX_CURRENT_DIRECTORY.filer_display.view_beg_x = 1;

    if (!(1 & (l_rc = smg$change_viewport (
		&DX_CURRENT_DIRECTORY.filer_display.id,
		&DX_CURRENT_DIRECTORY.filer_display.view_beg_y,
		&DX_CURRENT_DIRECTORY.filer_display.view_beg_x,
		0,
		0))))
        $LOG(l_rc);

    /*
    **	Set current file pointer.  Make sure at least one file exists
    **	in the directory file list
    **/

    DX_CURRENT_DIRECTORY.cur_file =
	(DX_CURRENT_DIRECTORY.dir_filelist->forward !=
	    DX_CURRENT_DIRECTORY.dir_filelist?
		DX_CURRENT_DIRECTORY.dir_filelist->forward :
		DX_CURRENT_DIRECTORY.non_dir_filelist->forward);

    /*
    **  Write all the files to screen
    **/

    write_to_filer(DX_CURRENT_DIRECTORY.dir_filelist);
    write_to_filer(DX_CURRENT_DIRECTORY.non_dir_filelist);
    highlight_filer_current_file();

    if (!(1 & (l_rc = smg$paste_virtual_display (
		&DX_CURRENT_DIRECTORY.filer_display.id,
		&cntrl_info_block.pasteboard_id,
		&DX_CURRENT_DIRECTORY.filer_display.beg_y,
		&DX_CURRENT_DIRECTORY.filer_display.beg_x,
		0))))
        $LOG(l_rc);

    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparator for directory entries (dir_filelist): orders by file
**	name alone, since directory files have no useful size/date to sort
**	by for the user's purposes.
**
**  INPUT PARAMETERS:
**
**	a_f          -  first entry to compare.
**	a_g          -  second entry to compare.
**	a_sortorder  -  ascending or descending.
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
**	<0 if a_f sorts before a_g, 0 if equal, >0 if a_f sorts after a_g
**	(direction flipped for descending order), per the strcmp()
**	convention -- matches the CMPFUNC signature used throughout this
**	file.
**
**--
**/
int	sort_by_dir(a_f, a_g, a_sortorder)
struct fil_dx_tag *a_f;
struct fil_dx_tag *a_g;
enum sortorders a_sortorder;
{
    return (a_sortorder == ascending?
		strcmp(a_f->fn, a_g->fn) : strcmp(a_g->fn, a_f->fn));
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparator for sorting by file name: compares the name+type portion
**	(everything before the ';version') with strcmp(); if that ties,
**	breaks the tie by version number (higher version sorts first in
**	ascending order, matching VMS's own newest-first convention).
**
**  INPUT PARAMETERS:
**
**	a_f          -  first entry to compare.
**	a_g          -  second entry to compare.
**	a_sortorder  -  ascending or descending.
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
**	<0/0/>0 per the strcmp() convention (direction flipped for
**	descending order).
**
**--
**/
int	sort_by_name(a_f, a_g, a_sortorder)
struct fil_dx_tag *a_f;
struct fil_dx_tag *a_g;
enum sortorders a_sortorder;
{
    char l_s[MAXVMSFULLNAME+1];
    char l_t[MAXVMSFULLNAME+1];
    char *l_cp;
    int l_cmp;
 
    strcpy(l_s, a_f->fn);
    if ((l_cp = strchr(l_s, ';')) != NULL)
	*l_cp = EOS;
 
    strcpy(l_t, a_g->fn);
    if ((l_cp = strchr(l_t, ';')) != NULL)
	*l_cp = EOS;
 
    if ((l_cmp = strcmp(l_s, l_t)) != 0)
	return l_cmp;
 
    if (a_sortorder == ascending)
	return atoi(strchr(a_g->fn, ';')+1) - atoi(strchr(a_f->fn, ';')+1);
    else
	return atoi(strchr(a_f->fn, ';')+1) - atoi(strchr(a_g->fn, ';')+1);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparator for sorting by file type: compares the ".TYPE" portion
**	of the file spec first (with strcspn()-measured, length-capped
**	copies so a long ";VERSION" suffix can never overflow the fixed
**	buffers), falls back to comparing the name portion if the types
**	tie, and finally breaks any remaining tie by version number.
**
**  INPUT PARAMETERS:
**
**	a_f          -  first entry to compare.
**	a_g          -  second entry to compare.
**	a_sortorder  -  ascending or descending.
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
**	<0/0/>0 per the strcmp() convention (direction flipped for
**	descending order).
**
**--
**/
int	sort_by_type(a_f, a_g, a_sortorder)
struct fil_dx_tag *a_f;
struct fil_dx_tag *a_g;
enum sortorders a_sortorder;
{
    char l_s[MAXVMSFILETYPE+1];
    char l_t[MAXVMSFILETYPE+1];
    char *l_cp;
    int l_cmp;
    int l_len;

    /**  strcpy() into s/t used to happen before the ';version' suffix
    **   was trimmed -- ".TYPE;VERSION" can be longer than s/t
    **   (MAXVMSFILETYPE+1 bytes), overflowing them.  Measure the
    **   length up to ';' in the SOURCE string first, cap it to the
    **   buffer size, and copy only that much.  **/
    l_cp = strchr(a_f->fn, '.')+1;
    l_len = strcspn(l_cp, ";");
    if (l_len > MAXVMSFILETYPE)
	l_len = MAXVMSFILETYPE;
    strncpy(l_s, l_cp, l_len);
    l_s[l_len] = EOS;

    l_cp = strchr(a_g->fn, '.')+1;
    l_len = strcspn(l_cp, ";");
    if (l_len > MAXVMSFILETYPE)
	l_len = MAXVMSFILETYPE;
    strncpy(l_t, l_cp, l_len);
    l_t[l_len] = EOS;
 
    if ((l_cmp = strcmp(l_s, l_t)) != 0)
	return l_cmp;
 
    strcpy(l_s, a_f->fn);
    l_cp = strchr(l_s, '.');
    *l_cp = EOS;
 
    strcpy(l_t, a_g->fn);
    l_cp = strchr(l_t, '.');
    *l_cp = EOS;
 
    if ((l_cmp = strcmp(l_s, l_t)) != 0)
	return l_cmp;
 
    if (a_sortorder == ascending)
	return atoi(strchr(a_g->fn, ';')+1) - atoi(strchr(a_f->fn, ';')+1);
    else
	return atoi(strchr(a_f->fn, ';')+1) - atoi(strchr(a_g->fn, ';')+1);
 
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparator for sorting by file size: compares filesize directly
**	(note: a_sortorder is NOT applied to the size comparison itself --
**	only to the file-name tiebreaker below, matching this codebase's
**	original behavior), falling back to the name+version comparison
**	used by sort_by_name() when two files have the exact same size.
**
**  INPUT PARAMETERS:
**
**	a_f          -  first entry to compare.
**	a_g          -  second entry to compare.
**	a_sortorder  -  ascending or descending (used only for the
**			same-size name tiebreaker).
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
**	Negative if a_f is smaller, positive if larger, 0 if the same size
**	and same name+version.
**
**--
**/
int	sort_by_size(a_f, a_g, a_sortorder)
struct fil_dx_tag *a_f;
struct fil_dx_tag *a_g;
enum sortorders a_sortorder;
{
    char l_s[MAXVMSFULLNAME+1];
    char l_t[MAXVMSFULLNAME+1];
    char *l_cp;
    int l_cmp;
 
    if ((l_cmp = a_f->filesize - a_g->filesize) != 0)
	return l_cmp;
 
    /*
    **	    Both have the same file size, compare file name
    **/
    strcpy(l_s, a_f->fn);
    if ((l_cp = strchr(l_s, ';')) != NULL)
	*l_cp = EOS;
 
    strcpy(l_t, a_g->fn);
    if ((l_cp = strchr(l_t, ';')) != NULL)
	*l_cp = EOS;
 
    if ((l_cmp = strcmp(l_s, l_t)) != 0)
	return l_cmp;
 
    if (a_sortorder == ascending)
	return atoi(strchr(a_g->fn, ';')+1) - atoi(strchr(a_f->fn, ';')+1);
    else
	return atoi(strchr(a_f->fn, ';')+1) - atoi(strchr(a_g->fn, ';')+1);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparator for sorting by creation date: compares cdat via
**	lib$sub_times() (a_sortorder is NOT applied to the date comparison
**	itself -- only to the file-name tiebreaker, matching this
**	codebase's original behavior), falling back to the name+version
**	comparison used by sort_by_name() when two files share the exact
**	same creation date.
**
**  INPUT PARAMETERS:
**
**	a_f          -  first entry to compare.
**	a_g          -  second entry to compare.
**	a_sortorder  -  ascending or descending (used only for the
**			same-date name tiebreaker).
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
**	-1 if a_f is older, 1 if newer, or the name+version comparison
**	result if the dates are identical.
**
**--
**/
int	sort_by_credate(a_f, a_g, a_sortorder)
struct fil_dx_tag *a_f;
struct fil_dx_tag *a_g;
enum sortorders a_sortorder;
{
    DATE_TIME l_cmp;
    char l_s[MAXVMSFULLNAME+1];
    char l_t[MAXVMSFULLNAME+1];
    char *l_cp;
    int l_cmp1;
 
    if (memcmp (&a_f->cdat, &a_g->cdat, sizeof (DATE_TIME)))
    {
	return (lib$sub_times(
		    &a_f->cdat,
		    &a_g->cdat,
		    &l_cmp) == LIB$_NEGTIM? -1 : 1);
    }
    else
    {
	/*
	**    Both have the same date, compare file name
	**/
	strcpy(l_s, a_f->fn);
	if ((l_cp = strchr(l_s, ';')) != NULL)
	    *l_cp = EOS;
 
	strcpy(l_t, a_g->fn);
	if ((l_cp = strchr(l_t, ';')) != NULL)
	    *l_cp = EOS;
 
	if ((l_cmp1 = strcmp(l_s, l_t)) != 0)
	    return l_cmp1;
 
	if (a_sortorder == ascending)
	    return atoi(strchr(a_g->fn, ';')+1) - atoi(strchr(a_f->fn, ';')+1);
	else
	    return atoi(strchr(a_f->fn, ';')+1) - atoi(strchr(a_g->fn, ';')+1);
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparator for sorting by revision date: compares rdat via
**	lib$sub_times() (a_sortorder is NOT applied to the date comparison
**	itself -- only to the file-name tiebreaker, matching this
**	codebase's original behavior), falling back to the name+version
**	comparison used by sort_by_name() when two files share the exact
**	same revision date.
**
**  INPUT PARAMETERS:
**
**	a_f          -  first entry to compare.
**	a_g          -  second entry to compare.
**	a_sortorder  -  ascending or descending (used only for the
**			same-date name tiebreaker).
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
**	-1 if a_f is older, 1 if newer, or the name+version comparison
**	result if the dates are identical.
**
**--
**/
int	sort_by_revdate(a_f, a_g, a_sortorder)
struct fil_dx_tag *a_f;
struct fil_dx_tag *a_g;
enum sortorders a_sortorder;
{
    DATE_TIME l_cmp;
    char l_s[MAXVMSFULLNAME+1];
    char l_t[MAXVMSFULLNAME+1];
    char *l_cp;
    int l_cmp1;
 
    if (memcmp (&a_f->rdat, &a_g->rdat, sizeof (DATE_TIME)))
    {
	return (lib$sub_times(
		    &a_f->rdat,
		    &a_g->rdat,
		    &l_cmp) == LIB$_NEGTIM? -1 : 1);
    }
    else
    {
	/*
	**    Both have the same date, compare file name
	**/
	strcpy(l_s, a_f->fn);
	if ((l_cp = strchr(l_s, ';')) != NULL)
	    *l_cp = EOS;
 
	strcpy(l_t, a_g->fn);
	if ((l_cp = strchr(l_t, ';')) != NULL)
	    *l_cp = EOS;
 
	if ((l_cmp1 = strcmp(l_s, l_t)) != 0)
	    return l_cmp1;
 
	if (a_sortorder == ascending)
	    return atoi(strchr(a_g->fn, ';')+1) - atoi(strchr(a_f->fn, ';')+1);
	else
	    return atoi(strchr(a_f->fn, ';')+1) - atoi(strchr(a_g->fn, ';')+1);
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparator for sorting by expiration date: compares edat via
**	lib$sub_times() (a_sortorder is NOT applied to the date comparison
**	itself -- only to the file-name tiebreaker, matching this
**	codebase's original behavior), falling back to the name+version
**	comparison used by sort_by_name() when two files share the exact
**	same expiration date.
**
**  INPUT PARAMETERS:
**
**	a_f          -  first entry to compare.
**	a_g          -  second entry to compare.
**	a_sortorder  -  ascending or descending (used only for the
**			same-date name tiebreaker).
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
**	-1 if a_f is older, 1 if newer, or the name+version comparison
**	result if the dates are identical.
**
**--
**/
int	sort_by_expdate(a_f, a_g, a_sortorder)
struct fil_dx_tag *a_f;
struct fil_dx_tag *a_g;
enum sortorders a_sortorder;
{
    DATE_TIME l_cmp;
    char l_s[MAXVMSFULLNAME+1];
    char l_t[MAXVMSFULLNAME+1];
    char *l_cp;
    int l_cmp1;
 
    if (memcmp (&a_f->edat, &a_g->edat, sizeof (DATE_TIME)))
    {
	return (lib$sub_times(
		    &a_f->edat,
		    &a_g->edat,
		    &l_cmp) == LIB$_NEGTIM? -1 : 1);
    }
    else
    {
	/*
	**    Both have the same date, compare file name
	**/
	strcpy(l_s, a_f->fn);
	if ((l_cp = strchr(l_s, ';')) != NULL)
	    *l_cp = EOS;
 
	strcpy(l_t, a_g->fn);
	if ((l_cp = strchr(l_t, ';')) != NULL)
	    *l_cp = EOS;
 
	if ((l_cmp1 = strcmp(l_s, l_t)) != 0)
	    return l_cmp1;
 
	if (a_sortorder == ascending)
	    return atoi(strchr(a_g->fn, ';')+1) - atoi(strchr(a_f->fn, ';')+1);
	else
	    return atoi(strchr(a_f->fn, ';')+1) - atoi(strchr(a_g->fn, ';')+1);
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparator for sorting by backup date: compares bdat via
**	lib$sub_times() (a_sortorder is NOT applied to the date comparison
**	itself -- only to the file-name tiebreaker, matching this
**	codebase's original behavior), falling back to the name+version
**	comparison used by sort_by_name() when two files share the exact
**	same backup date.
**
**  INPUT PARAMETERS:
**
**	a_f          -  first entry to compare.
**	a_g          -  second entry to compare.
**	a_sortorder  -  ascending or descending (used only for the
**			same-date name tiebreaker).
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
**	-1 if a_f is older, 1 if newer, or the name+version comparison
**	result if the dates are identical.
**
**--
**/
int	sort_by_bakdate(a_f, a_g, a_sortorder)
struct fil_dx_tag *a_f;
struct fil_dx_tag *a_g;
enum sortorders a_sortorder;
{
    DATE_TIME l_cmp;
    char l_s[MAXVMSFULLNAME+1];
    char l_t[MAXVMSFULLNAME+1];
    char *l_cp;
    int l_cmp1;
 
    if (memcmp (&a_f->bdat, &a_g->bdat, sizeof (DATE_TIME)))
    {
	return (lib$sub_times(
		    &a_f->bdat,
		    &a_g->bdat,
		    &l_cmp) == LIB$_NEGTIM? -1 : 1);
    }
    else
    {
	/*
	**    Both have the same date, compare file name
	**/
	strcpy(l_s, a_f->fn);
	if ((l_cp = strchr(l_s, ';')) != NULL)
	    *l_cp = EOS;
 
	strcpy(l_t, a_g->fn);
	if ((l_cp = strchr(l_t, ';')) != NULL)
	    *l_cp = EOS;
 
	if ((l_cmp1 = strcmp(l_s, l_t)) != 0)
	    return l_cmp1;
 
	if (a_sortorder == ascending)
	    return atoi(strchr(a_g->fn, ';')+1) - atoi(strchr(a_f->fn, ';')+1);
	else
	    return atoi(strchr(a_f->fn, ';')+1) - atoi(strchr(a_g->fn, ';')+1);
    }
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparator for sorting by marked/unmarked state: entries with the
**	same marked-ness are a tie (0); otherwise the marked (state ==
**	ready) entry sorts first in ascending order.  Note this returns
**	DX__ERROR (not -1) as its "sorts after" value in two branches --
**	a pre-existing quirk of this comparator carried forward unchanged,
**	not something introduced here.
**
**  INPUT PARAMETERS:
**
**	a_f          -  first entry to compare.
**	a_g          -  second entry to compare.
**	a_sortorder  -  ascending or descending.
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
**	0 if both entries have the same marked-ness; 1 if a_f sorts before
**	a_g; DX__ERROR if a_f sorts after a_g (see note above).
**
**--
**/
int	sort_by_mark(a_f, a_g, a_sortorder)
struct fil_dx_tag *a_f;
struct fil_dx_tag *a_g;
enum sortorders a_sortorder;
{
    /**  Same "marked-ness" on both sides -- tie, neither before the
    **   other.  (Previously fell through to the branches below and
    **   returned a non-zero value even when a_f and a_g were equal --
    **   an antisymmetric/non-reflexive comparator.)  **/
    if ((a_f->state == ready) == (a_g->state == ready))
	return 0;

    if (a_sortorder == ascending)

	if (a_f->state != ready)
	    return 1;
	else
	    return DX__ERROR;

    else

	if (a_f->state == ready)
	    return DX__ERROR;
	else
	    return 1;
}



/* ---- (from FILERFLT.C -- filter commands + filespec-list helpers) ---- */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"Name..." filter menu command: prompts for a comma/space-separated
**	list of file-name patterns via get_userinput_and_execute(), applied
**	through filer_filter$1() (case 'N').
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
**	DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list (via
**	filer_filter$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_filter_name()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error filtering files by name");
    get_userinput_and_execute(
	filer_filter$1,
	"Filter By Name(s)",
	"File filter: ",
	"",
	l_errmsg,
	"Name");
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"Exclude..." filter menu command: prompts for a comma/space-
**	separated list of file-name patterns to exclude via
**	get_userinput_and_execute(), applied through filer_filter$1()
**	(case 'E').
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
**	DX_CURRENT_DIRECTORY.cur_filter.exclude_filespec_list (via
**	filer_filter$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_filter_exclude()
{
    char l_errmsg[MAXFILESPEC+1];
 
    strcpy(l_errmsg, "Error filtering files by exclude");
    get_userinput_and_execute(
	filer_filter$1,
	"Filter By Exclude(s)",
	"Exclude files: ",
	"",
	l_errmsg,
	"Exclude");
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tests whether a file name appears in an exclude list (a fil_dx_tag
**	list built by find_exclude_file() from the filter's exclude
**	patterns) -- used by find_non_direct_file()/filter_OK() to drop
**	excluded files from the display.
**
**  INPUT PARAMETERS:
**
**	a_n            -  file name to look for.
**	a_exclude_list -  the exclude list to search.
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
**	1 if a_n is in the exclude list, 0 otherwise.
**
**--
**/
int	filter_exclude(a_n, a_exclude_list)
char *a_n;
struct fil_dx_tag *a_exclude_list;
{
    struct fil_dx_tag *l_p;
 
    /*
    **	Find all the files in this directory that in the exclude list
    **/
 
    for (l_p = a_exclude_list->forward; l_p != a_exclude_list;  l_p = l_p->forward)
    {
        if (!strcmp (l_p->fn, a_n))
            return 1;
    }
 
    return 0;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"Since..." filter menu command: prompts for a date/time (defaulted
**	to the current Since setting, formatted via sys$asctim()) via
**	get_userinput_and_execute(), applied through filer_filter$1()
**	(case 'S').
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
**	DX_CURRENT_DIRECTORY.cur_filter.since (for the prompt's default).
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.cur_filter.since (via filer_filter$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_filter_since()
{
    static unsigned short l_timlen;

    char l_errmsg[MAXFILESPEC+1];
    char l_timbuf[50];
    $DESCRIPTOR (l_timbuf_descriptor, l_timbuf);
    unsigned long l_rc;

    if (!(1 & (l_rc = sys$asctim (
		&l_timlen,
		&l_timbuf_descriptor,
		(struct _generic_64 *) &DX_CURRENT_DIRECTORY.cur_filter.since,
		0))))
        $LOG(l_rc);
    l_timbuf[l_timlen] = EOS;
    strcpy(l_errmsg, "Error filtering files by since");
    get_userinput_and_execute(
	filer_filter$1,
	"Filter By Since",
	"Show files dated after: ",
	l_timbuf,
	l_errmsg,
	"Since");
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tests whether a date is on or after the filter's "Since" cutoff, via
**	lib$sub_times().
**
**  INPUT PARAMETERS:
**
**	a_d      -  the date to test (e.g. a file's creation/revision date).
**	a_since  -  the filter's Since cutoff date.
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
**	Non-zero (true) if a_d is on or after a_since; 0 otherwise.
**
**--
**/
int	filter_since(a_d, a_since)
DATE_TIME a_d;
DATE_TIME a_since;
{
    DATE_TIME l_cmp;
 
    return (lib$sub_times(
		&a_d,
		&a_since,
		&l_cmp) != LIB$_NEGTIM);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"Before..." filter menu command: prompts for a date/time (defaulted
**	to the current Before setting, formatted via sys$asctim()) via
**	get_userinput_and_execute(), applied through filer_filter$1()
**	(case 'B').
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
**	DX_CURRENT_DIRECTORY.cur_filter.before (for the prompt's default).
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.cur_filter.before (via filer_filter$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_filter_before()
{
    static unsigned short l_timlen;

    char l_errmsg[MAXFILESPEC+1];
    char l_timbuf[50];
    $DESCRIPTOR (l_timbuf_descriptor, l_timbuf);
    unsigned long l_rc;

    if (!(1 & (l_rc = sys$asctim (
		&l_timlen,
		&l_timbuf_descriptor,
		(struct _generic_64 *) &DX_CURRENT_DIRECTORY.cur_filter.before,
		0))))
        $LOG(l_rc);
    l_timbuf[l_timlen] = EOS;
    strcpy(l_errmsg, "Error filtering files by before");
    get_userinput_and_execute(
	filer_filter$1,
	"Filter By Before",
	"Show files dated before: ",
	l_timbuf,
	l_errmsg,
	"Before");
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tests whether a date is strictly before the filter's "Before"
**	cutoff, via lib$sub_times().
**
**  INPUT PARAMETERS:
**
**	a_d       -  the date to test (e.g. a file's creation/revision date).
**	a_before  -  the filter's Before cutoff date.
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
**	Non-zero (true) if a_d is strictly before a_before; 0 otherwise.
**
**--
**/
int	filter_before(a_d, a_before)
DATE_TIME a_d;
DATE_TIME a_before;
{
    DATE_TIME l_cmp;
 
    return (lib$sub_times(
		&a_d,
		&a_before,
		&l_cmp) == LIB$_NEGTIM);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	"maXimum..." filter menu command: prompts for an upper size limit
**	(defaulted to the current Maximum setting) via
**	get_userinput_and_execute(), applied through filer_filter$1()
**	(case 'X').
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
**	DX_CURRENT_DIRECTORY.cur_filter.maximum (for the prompt's default).
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY.cur_filter.maximum (via filer_filter$1()).
**
**  RETURNS VALUE:
**
**	DX__NORMAL, always.
**
**--
**/
int	filer_filter_maximum()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_numstr[80];
    static $DESCRIPTOR (l_fao_ctrl, "!SL");
    struct dsc$descriptor_s l_out_desc;
    unsigned short int l_len;
    unsigned long l_rc;

    l_out_desc.dsc$w_length  = sizeof(l_numstr) - 1;
    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
    l_out_desc.dsc$a_pointer = l_numstr;
    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
		(long) DX_CURRENT_DIRECTORY.cur_filter.maximum))))
	$LOG(l_rc);
    l_numstr[l_len] = EOS;
    strcpy(l_errmsg, "Error filtering files by maximum");
    get_userinput_and_execute(
	filer_filter$1,
	"Filter By Maximum",
	"Show files smaller than: ",
	l_numstr,
	l_errmsg,
	"Ximum");
    return DX__NORMAL;
}
 
 
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tests whether a file's size is strictly under the filter's Maximum
**	limit.
**
**  INPUT PARAMETERS:
**
**	a_s        -  the file's size (blocks).
**	a_maximum  -  the filter's Maximum size limit.
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
**	Non-zero (true) if a_s < a_maximum; 0 otherwise.
**
**--
**/
int	filter_maximum(a_s, a_maximum)
unsigned int a_s;
unsigned int a_maximum;
{
    return (a_s < a_maximum);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	GOLD-key command handler for "Filter By Minimum".  Prompts the
**	user (via get_userinput_and_execute()) for a new minimum file
**	size, pre-filled with the current filter's Minimum value, and
**	on confirmation applies it through filer_filter$1().
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
**	DX_CURRENT_DIRECTORY.cur_filter.minimum  -  current Minimum filter
**	    value, used to pre-fill the prompt.
**
**  IMPLICIT OUTPUTS:
**
**	None directly; filer_filter$1() (invoked as the input-completion
**	routine) updates DX_CURRENT_DIRECTORY.cur_filter.minimum and
**	redisplays the filer.
**
**  RETURNS VALUE:
**
**	DX__NORMAL always.
**
**--
**/
int	filer_filter_minimum()
{
    char l_errmsg[MAXFILESPEC+1];
    char l_numstr[80];
    static $DESCRIPTOR (l_fao_ctrl, "!SL");
    struct dsc$descriptor_s l_out_desc;
    unsigned short int l_len;
    unsigned long l_rc;

    l_out_desc.dsc$w_length  = sizeof(l_numstr) - 1;
    l_out_desc.dsc$b_dtype   = DSC$K_DTYPE_T;
    l_out_desc.dsc$b_class   = DSC$K_CLASS_S;
    l_out_desc.dsc$a_pointer = l_numstr;
    if (!(1 & (l_rc = sys$fao(&l_fao_ctrl, &l_len, &l_out_desc,
		(long) DX_CURRENT_DIRECTORY.cur_filter.minimum))))
	$LOG(l_rc);
    l_numstr[l_len] = EOS;
    strcpy(l_errmsg, "Error filtering files by minimum");
    get_userinput_and_execute(
	filer_filter$1,
	"Filter By Minimum",
	"Show files larger than: ",
	l_numstr,
	l_errmsg,
	"Minimum");
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tests whether a file's size meets or exceeds the filter's Minimum
**	limit.
**
**  INPUT PARAMETERS:
**
**	a_s        -  the file's size (blocks).
**	a_minimum  -  the filter's Minimum size limit.
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
**	Non-zero (true) if a_s >= a_minimum; 0 otherwise.
**
**--
**/
int	filter_minimum(a_s, a_minimum)
unsigned int a_s;
unsigned int a_minimum;
{
    return (a_s >= a_minimum);
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tests a single directory entry against all of the current filter's
**	criteria at once: not on the Exclude list, and within the Since/
**	Before date range and Maximum/Minimum size range (using whichever
**	of the entry's four dates -- creation, revision, expiration, or
**	backup -- the filter is currently keyed on).
**
**  INPUT PARAMETERS:
**
**	a_e             -  the directory entry to test.
**	a_exclude_list  -  head of the Exclude filespec list.
**	a_cur_filter    -  the current filter criteria (date key, Since,
**	    Before, Maximum, Minimum).
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
**	Non-zero (true) if a_e passes every filter test; 0 if it fails
**	any one of them.
**
**--
**/
int	filter_OK(a_e, a_exclude_list, a_cur_filter)
struct fil_dx_tag a_e;
struct fil_dx_tag *a_exclude_list;
struct cur_filter_tag a_cur_filter;
{
    DATE_TIME l_date_time;
 
    switch (a_cur_filter.date)
    {
        case DX_CREDATE:
            memcpy(&l_date_time, &a_e.cdat, sizeof (DATE_TIME));
	    break;
        case DX_REVDATE:
            memcpy(&l_date_time, &a_e.rdat, sizeof (DATE_TIME));
	    break;
        case DX_EXPDATE:
            memcpy(&l_date_time, &a_e.edat, sizeof (DATE_TIME));
	    break;
        case DX_BAKDATE:
            memcpy(&l_date_time, &a_e.bdat, sizeof (DATE_TIME));
	    break;
        default:
            break;
    }
 
    return (	
	 !filter_exclude(a_e.fn, a_exclude_list) &&
	 filter_since(l_date_time, a_cur_filter.since) &&
	 filter_before(l_date_time, a_cur_filter.before) &&
	 filter_maximum(a_e.filesize, a_cur_filter.maximum) &&
	 filter_minimum(a_e.filesize, a_cur_filter.minimum));
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Common completion routine for every "Filter By ..." GOLD-key
**	command (Name, Exclude, Since, Before, Maximum, Minimum) plus the
**	filer-cache "Update"/"Restore" pseudo-filters.  Saves the current
**	directory into the filer cache, applies the new criterion named by
**	a_filterby to DX_CURRENT_DIRECTORY.cur_filter (or restores a whole
**	cached directory for Update/Restore), re-scans the directory (or
**	pulls the result from the filer cache if a matching entry already
**	exists), and repaints the filer display in a single
**	smg$begin_display_update/smg$end_display_update batch.
**
**  INPUT PARAMETERS:
**
**	a_criteria   -  the user-entered value for the filter item (a
**	    size, a date/time string, a filespec list, or a filer-cache
**	    index for Restore).
**	a_filterby   -  selects which filter item is being set; a_filterby[0]
**	    is one of 'N' (Name), 'E' (Exclude), 'S' (Since), 'B' (Before),
**	    'X' (Maximum), 'M' (Minimum), 'U' (Update), or 'R' (Restore).
**
**  OUTPUT PARAMETERS:
**
**	a_errmsg  -  filled in with an error message if DX__ERROR is
**	    returned.
**
**  IMPLICIT INPUTS:
**
**	DX_CURRENT_DIRECTORY  -  current directory/filter state, read and
**	    conditionally restored from the filer cache.
**	cntrl_info_block.cur_win  -  window the filter applies to.
**
**  IMPLICIT OUTPUTS:
**
**	DX_CURRENT_DIRECTORY  -  updated filter criteria, file lists,
**	    sort order, and display geometry.
**	DX_CURRENT_FILE  -  repositioned to the first file (or, after a
**	    "[-]" parent-directory Name filter, to the entry matching the
**	    directory just left).
**	The filer virtual display is erased and redrawn.
**
**  RETURNS VALUE:
**
**	DX__NORMAL on success; DX__ERROR if the filespec list could not be
**	built, the date/time string was invalid, or the exclude file list
**	could not be updated (a_errmsg is set accordingly).
**
**--
**/
int	filer_filter$1(a_criteria, a_filterby, a_errmsg)
char *a_criteria;
char *a_filterby;
char *a_errmsg;
{
    char l_tok[MAXFILESPEC+1];
    char l_default_filespec[MAXFILESPEC+1];
    size_t l_len;
    struct filespec_list_tag *l_l = NULL;
    char *l_cp;
    char l_sav_criteria[MAXFILESPEC+1];
    char l_old_cwd[MAXFILESPEC+1],
	 l_old_par[MAXFILESPEC+1];
    int l_row,
	l_col;
    char l_dat_tim_string[256];
    $DESCRIPTOR(l_dat_tim_string_descrip, l_dat_tim_string);
    int l_n;
    unsigned long l_rc;

    /*
    **	Save previous directory info
    **/
 
    if ((l_n = search_filer_cache(DX_CURRENT_DIRECTORY.cur_filter)) == -1)
    {
	/*
	**  Current directory info not found in the
	**  directory cache.  Find the LRU slot in the cache.
	**/
 
	l_n = find_LRU_in_filer_cache();
    }
 
    save_to_filer_cache(l_n, DX_CURRENT_DIRECTORY);
 
    /*
    **	Do filtering according to a_criteria specified
    **/
 
    switch (a_filterby[0])
    {
	case 'N':	/**  Name  **/
	    strcpy(l_old_cwd, DX_CURRENT_DIRECTORY.cur_dir);
	    strcpy(l_old_par, DX_CURRENT_DIRECTORY.par_dir);
	    strcpy(l_sav_criteria, a_criteria);
 
	    strcpy(l_default_filespec, "*.*;*");
	    l_l = (struct filespec_list_tag *)NULL;
 
	    l_cp = a_criteria;
	    l_n = 0;
	    while (*l_cp)
	    {
		if ((l_len = strcspn (l_cp, ", ")) == 0)
		    l_len = strlen(l_cp);
		strncpy(l_tok, l_cp, l_len);
		l_tok[l_len] = EOS;
 
		if (insert_filespec_list(
			&l_l,
			l_default_filespec,
			l_tok,
			a_errmsg) != DX__NORMAL)
		{
		    strcpy(DX_CURRENT_DIRECTORY.cur_dir, l_old_cwd);
		    strcpy(DX_CURRENT_DIRECTORY.par_dir, l_old_par);
		    free_filespec_list(&l_l);
		    return DX__ERROR;
		}
		if (l_n++ == 0)
		{
		    /*
		    **	Assign new value to DX_CURRENT_DIRECTORY.cur_dir
		    **	and DX_CURRENT_DIRECTORY.par_dir
		    **/
 
		    set_cwd_and_par(l_default_filespec);
		}
		l_cp += l_len;
		l_len = strspn (l_cp, ", ");
		l_cp += l_len;
	    }
 
	    /*
	    **	Assign new value to the current directory
	    **	control block.
	    **/
 
	    convert_date_string (
		"31-DEC-2857 00:00:00.00",
		&DX_CURRENT_DIRECTORY.cur_filter.before);
	    convert_date_string (
		"17-NOV-1858 00:00:00.00",
		&DX_CURRENT_DIRECTORY.cur_filter.since);
	    DX_CURRENT_DIRECTORY.cur_filter.maximum = 1073741823;
	    DX_CURRENT_DIRECTORY.cur_filter.minimum = 0;
	    if (strcmp(DX_CURRENT_DIRECTORY.cur_dir, l_old_cwd))
	    {
		setddir(DX_CURRENT_DIRECTORY.cur_dir, a_errmsg);
		strcpy(DX_CURRENT_DIRECTORY.search_pattern, "*");
		DX_CURRENT_DIRECTORY.direction = advance;
		DX_CURRENT_DIRECTORY.filemode = single;
	    }
	
	    /*
	    **	Free up memory used by the lists
	    **/
 
	    free_filespec_list(&DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list);
	    DX_CURRENT_DIRECTORY.cur_filter.name_filespec_list = l_l;
	    /**  filer_find_first$2()'l_s cache is built by scanning
	    **   name_filespec_list -- a new Name filter invalidates it.
	    **/
	    invalidate_find_cache();
	    break;
        case 'E':	/**  Exclude  **/
	    strcpy(l_default_filespec, "*.*;*");
	    l_l = (struct filespec_list_tag *)NULL;
 
	    l_cp = a_criteria;
	    while (*l_cp)
	    {
		if ((l_len = strcspn (l_cp, ", ")) == 0)
		    l_len = strlen(l_cp);
		strncpy(l_tok, l_cp, l_len);
		l_tok[l_len] = EOS;
 
		if (insert_filespec_list(
			&l_l,
			l_default_filespec,
			l_tok,
			a_errmsg) != DX__NORMAL)
		{
		    free_filespec_list(&l_l);
		    return DX__ERROR;
		}
		l_cp += l_len;
		l_len = strspn (l_cp, ", ");
		l_cp += l_len;
	    }
	    free_filespec_list(&DX_CURRENT_DIRECTORY.cur_filter.exclude_filespec_list);
	    DX_CURRENT_DIRECTORY.cur_filter.exclude_filespec_list = l_l;
	    break;
        case 'S':	/**  Since  **/
	    strcpy(l_dat_tim_string, a_criteria);
	    LENGTH(l_dat_tim_string_descrip) = strlen(l_dat_tim_string);
	    if (!(lib$convert_date_string (
			 &l_dat_tim_string_descrip,
			 &DX_CURRENT_DIRECTORY.cur_filter.since,
			 0,
			 0,
			 0,
			 0) & 1))
	     {
		strcpy(a_errmsg, "Invalid date/time format");
		return DX__ERROR;
	     }
	    break;
        case 'B':	/**  Before  **/
	    strcpy(l_dat_tim_string, a_criteria);
	    LENGTH(l_dat_tim_string_descrip) = strlen(l_dat_tim_string);
	    if (!(lib$convert_date_string (
			 &l_dat_tim_string_descrip,
			 &DX_CURRENT_DIRECTORY.cur_filter.before,
			 0,
			 0,
			 0,
			 0) & 1))
	     {
		strcpy(a_errmsg, "Invalid date/time format");
		return DX__ERROR;
	     }
	    break;
        case 'X':	/**  Maximum  **/
            DX_CURRENT_DIRECTORY.cur_filter.maximum = atoi(a_criteria);
	    break;
        case 'M':	/**  Minimum  **/
            DX_CURRENT_DIRECTORY.cur_filter.minimum = atoi(a_criteria);
	    break;
	case 'U':	/**  Update  **/
	case 'R':	/**  Restore  **/
	    break;
        default:
            strcpy(a_errmsg, "Invalid filter item");
	    return DX__ERROR;
    }
 
    DX_CURRENT_DIRECTORY.dpro = 0Xffff;
    DX_CURRENT_DIRECTORY.subdir = 0;
    DX_CURRENT_DIRECTORY.tot_files = DX_CURRENT_DIRECTORY.tot_blocks = 0;
    DX_CURRENT_DIRECTORY.sel_files = DX_CURRENT_DIRECTORY.sel_blocks = 0;
 
    if (!strcmp(a_filterby, "Restore"))
    {
	restore_from_filer_cache(atoi(a_criteria), &DX_CURRENT_DIRECTORY);
	setddir(DX_CURRENT_DIRECTORY.cur_dir, a_errmsg);
    }
    else if (strcmp(a_filterby, "Update") &&
	(l_n = search_filer_cache(DX_CURRENT_DIRECTORY.cur_filter)) != -1)
    {
	restore_from_filer_cache(l_n, &DX_CURRENT_DIRECTORY);
	setddir(DX_CURRENT_DIRECTORY.cur_dir, a_errmsg);
    }
    else
    {
	if (!strcmp(a_filterby, "Name"))
	{
	    DX_CURRENT_DIRECTORY.sortby = sortname;
	    DX_CURRENT_DIRECTORY.sortorder = ascending;
	    DX_CURRENT_DIRECTORY.cur_filter.date = DX_CREDATE;
	}
	find_direct_file();
	if (find_exclude_file(
		&DX_CURRENT_DIRECTORY.exclude_filelist,
		DX_CURRENT_DIRECTORY.cur_filter,
		DX_CURRENT_DIRECTORY.sortby,
		DX_CURRENT_DIRECTORY.sortorder) == DX__ERROR)
	{
	    strcpy(
		a_errmsg,
		"Insufficient memory to update the exclude file list");
	    return DX__ERROR;
	}
	find_non_direct_file();
    }
 
    /*
    **	Compute the position of each file
    **/
 
    DX_CURRENT_DIRECTORY.dir_filelist->beg_y =
	DX_CURRENT_DIRECTORY.dir_filelist->beg_x = 1;
    l_row = l_col = 1;
    compute_filer_begin_xy (
	DX_CURRENT_DIRECTORY.dir_filelist, &l_row, &l_col, cntrl_info_block.cur_win);
    DX_CURRENT_DIRECTORY.non_dir_filelist->beg_y =
	DX_CURRENT_DIRECTORY.dir_filelist->backward->beg_y;
    DX_CURRENT_DIRECTORY.non_dir_filelist->beg_x =
	DX_CURRENT_DIRECTORY.dir_filelist->backward->beg_x;
    compute_filer_begin_xy (
	DX_CURRENT_DIRECTORY.non_dir_filelist, &l_row, &l_col, cntrl_info_block.cur_win);
 
    /*
    **	Re-disply the screen with batch mode.
    **	Make sure the pasting order is preserved.
    **/
 
    DX_CURRENT_DIRECTORY.filer_display.rows =
	max (DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y,
	     DX_CURRENT_DIRECTORY.filer_display.view_rows);
 
    if (!(1 & (l_rc = smg$erase_display (
		&DX_CURRENT_DIRECTORY.filer_display.id,
		0,
		0,
		0,
		0))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$begin_display_update (
	     	  &DX_CURRENT_DIRECTORY.filer_display.id))))
        $LOG(l_rc);
 
    if (!(1 & (l_rc = smg$change_virtual_display (
		&DX_CURRENT_DIRECTORY.filer_display.id,
		&DX_CURRENT_DIRECTORY.filer_display.rows,
		0,
		0,
		0,
		0))))
        $LOG(l_rc);
 
    DX_CURRENT_DIRECTORY.filer_display.view_beg_y = 1;
    DX_CURRENT_DIRECTORY.filer_display.view_beg_x = 1;
 
    if (!(1 & (l_rc = smg$change_viewport (
		&DX_CURRENT_DIRECTORY.filer_display.id,
		&DX_CURRENT_DIRECTORY.filer_display.view_beg_y,
		&DX_CURRENT_DIRECTORY.filer_display.view_beg_x,
		0,
		0))))
        $LOG(l_rc);
 
    /*
    **  Write all the files to screen
    **/
 
    write_to_filer(DX_CURRENT_DIRECTORY.dir_filelist);
    write_to_filer(DX_CURRENT_DIRECTORY.non_dir_filelist);
    put_filer_title(cntrl_info_block.cur_win);
    put_filer_stat(cntrl_info_block.cur_win);
 
    /*
    **	Write directory and page info
    **/
/***************************************************************************
    if (!strcmp(a_filterby, "Name"))
    {
	l_div_p = div (DX_CURRENT_DIRECTORY.non_dir_filelist->backward->beg_y,
		       DX_CURRENT_DIRECTORY.filer_display.view_rows);
	l_pages = (l_div_p.rem == 0? l_div_p.quot : l_div_p.quot+1);
	l_all_dirs =
	    (strcmp (DX_CURRENT_DIRECTORY.dir_filelist->forward->fn, "[-]")?
	    DX_CURRENT_DIRECTORY.subdir : DX_CURRENT_DIRECTORY.subdir+1);
	sprintf(l_pages_str, "Total of %d %s, %d %s read",
	    l_all_dirs, (l_all_dirs > 1? "directories" : "directory"),
	    l_pages, (l_pages > 1? "pages" : "page"));
	signal_err(l_pages_str, silence);
    }
****************************************************************************/
 
    /*
    **	Set current file pointer.  Make sure at least one file exists
    **	in the directory file list
    **/
 
    DX_CURRENT_FILE =
	(DX_CURRENT_DIRECTORY.dir_filelist->forward !=
	    DX_CURRENT_DIRECTORY.dir_filelist?
		DX_CURRENT_DIRECTORY.dir_filelist->forward :
		DX_CURRENT_DIRECTORY.non_dir_filelist->forward);
    if (!strcmp(l_sav_criteria, "[-]"))
    {
	struct fil_dx_tag *l_s;
	char *l_cp1;
 
        if ((l_cp = strrchr(l_old_cwd, '.')) == NULL)
	    l_cp = strchr(l_old_cwd, '[');
	l_cp++;
        for (l_s = DX_CURRENT_DIRECTORY.dir_filelist->forward;
		l_s != DX_CURRENT_DIRECTORY.dir_filelist;  l_s = l_s->forward)
        {
	    if ((l_cp1 = strrchr(l_s->fn, '.')) != NULL)
	    {
		l_cp1++;
		if (!strcmp(l_cp, l_cp1))
		{
		    DX_CURRENT_FILE = l_s;
		    change_filer_viewport(cntrl_info_block.cur_win);
		    break;
		}
            }
        }
    }
 
    highlight_filer_current_file();
 
    if (!(1 & (l_rc = smg$end_display_update (
	     	  &DX_CURRENT_DIRECTORY.filer_display.id))))
        $LOG(l_rc);
 
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Recursively frees an entire filespec list, including each node's
**	w_dids sublist (freed via free_w_dids()).
**
**  INPUT PARAMETERS:
**
**	a_l  -  address of the pointer to the head of the filespec list.
**
**  OUTPUT PARAMETERS:
**
**	a_l  -  set to NULL after the list is freed.
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
int	free_filespec_list(a_l)
struct filespec_list_tag **a_l;
{
    int l_allocate_size;
    unsigned long l_rc;

    if (*a_l != (struct filespec_list_tag *)NULL)
    {
	free_filespec_list(&(*a_l)->next);

	free_w_dids(&(*a_l)->w_dids);
	l_allocate_size = sizeof (struct filespec_list_tag);
        if (!(1 & (l_rc = lib$free_vm (
                     &l_allocate_size,
		     a_l,
                     0))))
            $LOG(l_rc);
	*a_l = (struct filespec_list_tag *)NULL;
    }
    return DX__NORMAL;
}
 
 

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Recursively frees an entire w_dids (wildcard directory-ID) list.
**
**  INPUT PARAMETERS:
**
**	a_l  -  address of the pointer to the head of the w_dids list.
**
**  OUTPUT PARAMETERS:
**
**	a_l  -  set to NULL after the list is freed.
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
int	free_w_dids(a_l)
struct w_dids_tag **a_l;
{
    int l_allocate_size;
    unsigned long l_rc;

    if (*a_l != (struct w_dids_tag *)NULL)
    {
	free_w_dids(&((*a_l)->next));

	l_allocate_size = sizeof (struct w_dids_tag);
        if (!(1 & (l_rc = lib$free_vm (
                     &l_allocate_size,
		     a_l,
                     0))))
            $LOG(l_rc);
	*a_l = (struct w_dids_tag *)NULL;
    }
    return DX__NORMAL;
}
