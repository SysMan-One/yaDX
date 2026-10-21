/* Program Name            : FILERQUE.H                                 */
/*   Original Author       : C. K. Hung					*/
/*   Date                  : 1-JUN-1990					*/
/*   Program Description   : Filer cache + directory scan/sort/filter   */
/*                           -- consolidated 14-Sep-2026 from           */
/*                           FILERQUE.H + FINDFILE.H + FILERSRT.H +     */
/*                           FILERFLT.H.                                */
/*                         :                                            */
/* __MODULE__              : FILERQUE_H                                 */
/* __IDENT__               : X-02-05ECO8                                */
/* __REV__                 : 02.05.08                                   */
/* Revision History follows                                             */
/*  03-Jun-1999 JL Cleaned                                              */
/*  28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header */



/* ****  GLOBAL DECLARATIONS *** */

#define		MAXBUFFER		20	/**  Size of the dir cache   **/
typedef struct filer_cache_tag
{
	DATE_TIME time_stamp;			/**  Last access time	     **/
	enum sortbys sortby;			/**  Sort by		     **/
	enum sortorders sortorder;		/**  Sort order		     **/
        char cur_dir[MAXFILESPEC+1];            /**  Directory name          **/
        char par_dir[MAXFILESPEC+1];            /**  Parent directory name   **/
	long int tot_files,			/**  Total files	     **/
		 tot_blocks;			/**  Total blocks	     **/
	unsigned short int dpro;		/**  Directory protection    **/
	short int subdir;			/**  No. of sub-directories  **/
	struct cur_filter_tag cur_filter;
						/**  Filter criteria	     **/
	struct fil_dx_tag *dir_filelist, 
			  *non_dir_filelist,
			  *exclude_filelist;
} TD_FILER_CACHE_TAG;

TD_FILER_CACHE_TAG filer_cache[MAXBUFFER];   /**  Buffer to save directory info **/



/* ****  FUNCTIONS PROTOTYPING *** */

int	initialize_filer_cache(void);
int	find_LRU_in_filer_cache(void);
int	search_filer_cache(struct cur_filter_tag);
int	save_to_filer_cache(int, struct dir_dx_tag);
int	restore_from_filer_cache(int, struct dir_dx_tag *);
int	update_filer_cache(struct fil_dx_tag, int (*)(), char *);
int	add_to_filer_cache(struct fil_dx_tag, struct filer_cache_tag *, char *);
int	delete_from_filer_cache( struct fil_dx_tag, struct filer_cache_tag *, char *);
int	change_to_filer_cache( struct fil_dx_tag, struct filer_cache_tag *, char *);
void	copy_filespec_list(struct filespec_list_tag *, struct filespec_list_tag **);
void	copy_filelist(struct fil_dx_tag *, struct fil_dx_tag **, enum sortbys, enum sortorders);
int	select_from_filer_cache(void);

    /* ---- (from FINDFILE.H) ---- */
int		    find_direct_file(void);
int		    find_non_direct_file(void);
int		    find_exclude_file(
			struct fil_dx_tag **, struct cur_filter_tag,
			enum sortbys, enum sortorders);
int		    find_match_file(char *, unsigned short int);
int		    find_queue_file(unsigned short *, TD_FILER_CACHE_TAG *);
struct fil_dx_tag *create_MARKER(void);
int		    add_filelist_entry(
			struct fil_dx_tag *, struct fil_dx_tag *, 
			struct fil_dx_tag *);
int		    free_filelist(struct fil_dx_tag **);
int		    remove_filelist_entry(struct fil_dx_tag **);
int		    insert_filelist_entry(
			struct fil_dx_tag *, struct fil_dx_tag,
			enum sortbys, enum sortorders);
int		    append_filelist_entry(
			struct fil_dx_tag *, struct fil_dx_tag);

    /* ---- (from FILERSRT.H) ---- */
    int             filer_sort_name(void);
    int             filer_sort_type(void);
    int             filer_sort_date(void);
    int             filer_sort_size(void);
    int             filer_sort_mark(void);
    int             filer_sort$1(char *, char *, char *);

    typedef int	(*CMPFUNC)(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);
    CMPFUNC	    pick_sort_cmpfunc(enum sortbys);
    int		    finalize_filelist(
			struct fil_dx_tag *, int, enum sortbys, enum sortorders,
			long int *, long int *, char *);

    int		    sort_by_dir(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);
    int		    sort_by_name(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);
    int		    sort_by_type(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);
    int		    sort_by_size(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);
    int		    sort_by_credate(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);
    int		    sort_by_revdate(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);
    int		    sort_by_expdate(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);
    int		    sort_by_bakdate(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);
    int		    sort_by_mark(struct fil_dx_tag *, struct fil_dx_tag *, enum sortorders);

    /* ---- (from FILERFLT.H) ---- */
    int             filer_filter_name(void);

    int             filer_filter_exclude(void);
    int             filter_exclude(char *, struct fil_dx_tag *);

    int             filer_filter_since(void);
    int             filter_since(DATE_TIME, DATE_TIME);

    int             filer_filter_before(void);
    int             filter_before(DATE_TIME, DATE_TIME);

    int             filer_filter_maximum(void);
    int             filter_maximum(unsigned int, unsigned int);

    int             filer_filter_minimum(void);
    int             filter_minimum(unsigned int, unsigned int);

    int		    filter_OK(struct fil_dx_tag, struct fil_dx_tag *, struct cur_filter_tag);
    int		    filer_filter$1(char *, char *, char *);

    int		    free_filespec_list(struct filespec_list_tag **);
    int		    free_w_dids(struct w_dids_tag **);
