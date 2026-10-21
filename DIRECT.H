/* Program Name            : DIRECT.H                                   */
/*   Original Author       : C. K. Hun                                  */
/*   Date                  : 20-MAY-1991                                */
/*   Program Description   : Directory tree navigator -- consolidated   */
/*                           13-Sep-2026 from DIRECT.H + DIRECTKPD.H +  */
/*                           DIRECTQUE.H.                               */
/*                         :                                            */
/* __MODULE__              : DIRECT_H                                   */
/* __IDENT__               : X-02-05ECO8                                */
/* __REV__                 : 02.05.08                                   */
/* Revision History follows                                             */
/*  28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header */

/* **	MACRO DEFINITIONS ** */

#define DIRTREE_PBD_ROW			4
#define DIRTREE_PBD_COLUMN		1
#define DIRTREE_CMDS_PBD_ROW		1
#define DIRTREE_CMDS_PBD_COLUMN		1
#define DIRTREE_DIRECT_ROW		3
#define DIRTREE_CMDS_RSV_LNS		5

/**  Header-line path field width: the line is " <path> | <Forward/
**   Backward> " -- 1+3+8+1 = 13 fixed chars, the rest is the path; never
**   below the original 67 (80-column terminal).  **/
#define DIRTREE_TITLE_FIXED_WIDTH	13
#define DIRTREE_TITLE_PATH_WIDTH \
	((cntrl_info_block.pasteboard_width - DIRTREE_TITLE_FIXED_WIDTH > 67) ? \
	 (cntrl_info_block.pasteboard_width - DIRTREE_TITLE_FIXED_WIDTH) : 67)

#define LINESPACE			2	/**  Vertical distance	     **/
#define LINE_NODE_DISTANCE		2
#define LINE_COUNT			3	/**  Horizontal distance     **/
#define DISTANCE_COUNT			11	/**  MAXDIRSPEC + LINE_COUNT **/
#define MAXDIRSPEC			8	/**  Max char # in dir name  **/

#define DIRTREE_PFS    	"1HELP   2DIRECT 3FIND/F 4FIND/N 5FIRST  6LAST   7INITAL 8PAGE   9       0CANCEL "

#define MAX_DIRECT_BUFFER	20	/**  Size of the dir cache   **/


/* ****	GLOBAL DECLARATIONS *** */
typedef struct node_tag {
	char full_path_name[MAXFILESPEC+1];
	char short_path_name[MAXFILESPEC+1];
	int row,
	    column;
	struct node_tag *left,
			*right,
			*up,
			*down;
} TD_NODE_TAG;

enum dirtree_directions_tag {
	left, right, up, down, dummy
};

struct root_equ_names_tag
    {
	char equ_name[MAXFILESPEC+1];		/**  Equivalent name	     **/
	struct root_equ_names_tag *next;	/**  Next name		     **/
    };

struct direct_cache_tag
    {
	DATE_TIME time_stamp;			/**  Last access time	     **/
	struct root_equ_names_tag *root_equ_names;
						/**  Root directories	     **/
	enum directions direction;		/**  Search direction	     **/
	char pattern[MAXFILESPEC+1];		/**  Search pattern	     **/
	struct node_tag *root;			/**  Directory tree head     **/
    } direct_cache[MAX_DIRECT_BUFFER];   /**  Buffer to save directory info  **/


/* ****	FUNCTION PROTOTYPING *** */

int filer_direct(void);
int direct_process_loop(void);
int find_root(char *, char *);
TD_NODE_TAG *create_node(char *, int, int);
int build_tree(TD_NODE_TAG **, char *);
TD_NODE_TAG *add_node(TD_NODE_TAG **, char *, TD_NODE_TAG *);
TD_NODE_TAG *find_prev_node(TD_NODE_TAG *);
TD_NODE_TAG *find_next_node(TD_NODE_TAG *);
int traverse_tree(TD_NODE_TAG *, TD_NODE_TAG *, int, int, enum directions);
TD_NODE_TAG *search_node(TD_NODE_TAG *, char *);
void paint_dirtree_display(TD_NODE_TAG *);
int free_dirtree(TD_NODE_TAG **, TD_NODE_TAG *);    
int put_dirtree_title(TD_NODE_TAG *);
int direct_helpdir(void);

    /* ---- (from DIRECTKPD.H) ---- */
int	dirtree_find_first(void);
int	dirtree_find_first$1(char *, char *, char *);
int	dirtree_find_next(void);
int	change_dirtree_viewport(int, int, int *, int *);
void	find_down(TD_NODE_TAG *, TD_NODE_TAG *, TD_NODE_TAG **, int *);
TD_NODE_TAG *find_right(TD_NODE_TAG *);
TD_NODE_TAG *dirtree_page_down(TD_NODE_TAG *, TD_NODE_TAG *);
TD_NODE_TAG *search_node_page (TD_NODE_TAG *, int, int);
TD_NODE_TAG *dirtree_page_up (TD_NODE_TAG *);
int	direct_initial(void);

    /* ---- (from DIRECTQUE.H) ---- */
    int			    initialize_direct_cache(void);
    int			    find_LRU_in_direct_cache(void);
    int			    search_direct_cache(char *);
    int			    save_to_direct_cache(int, char *, enum directions, 
				char *, struct node_tag *);
    int			    restore_from_direct_cache(int, 
				enum directions *, char *, struct node_tag **);
    int			    add_to_direct_cache(char *, char *);
    int			    build_dirtree(struct node_tag **, char *);
    int			    delete_from_direct_cache(char *, char *);
