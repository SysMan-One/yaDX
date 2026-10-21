/* Program Name            : DX.H                                       */
/*   Original Author       : C. K. Hung					*/
/*   Date                  : 20-APR-1990				*/
/*   Program Description   :                                            */  
/*                         :                                            */ 
/* __MODULE__              : DX_H                                       */
/* __IDENT__               : X-02-05ECO8                                */
/* __REV__                 : 02.05.08                                   */
/* Revision History follows                                             */
/*  28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header */


/* **	MACRO DEFINITIONS ** */

/**  Commands virtual display  **/
#define COMMANDS_PBD_ROW			1
#define COMMANDS_PBD_COLUMN			1

/**  Status virtual display  **/
#define STATUS_BOTTOM				1
#define STATUS_PBD_ROW				2
#define STATUS_PBD_COLUMN			1

/**  Dir_Info virtual display (one window)  **/
#define SINGLE_WINDOW_DIR_INFO_PBD_ROW		3
#define SINGLE_WINDOW_DIR_INFO_PBD_COLUMN	1

/**  Broadcast message buffer length  **/
#define BROADMSGLENG				255



/* **  GLOBAL DECLARATIONS ** */

struct logical_queue_entry_tag		/**  Queue entry		     **/
{
	int link[2];
	char *equivalent_name;
};


/* ****  FUNCTIONS PROTOTYPING *** */

int		    trnlnm(const char *, int *);
int		    insert_dids(struct w_dids_tag **, char *, 
			unsigned short int *, unsigned short int);
int		    set_cwd_and_par(char *);
unsigned short  get_chan(char *);
int		    insert_filespec_list(struct filespec_list_tag **, 
			char *, char *, char *);
int		    insert_filespec_list$1(struct filespec_list_tag **,
			struct filespec_list_tag);
int		    broadcast_routine(void);

