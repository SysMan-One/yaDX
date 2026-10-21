/* Program Name            : DIFF.H                                     */
/*   Original Author       : 						*/
/*	Steppe, Tom. "File Comparison Algorithms." Dr. Dobb's 		*/
/*	Journal, #131 (Sep 1987): 54-60.				*/
/*									*/
/*   Date                  : 1-MAR-1991					*/
/*   Program Description   : This module contains macro definitions 	*/
/*                           for DIFF.C
/*                         :                                            */ 
/* References                                                           */
/*   Files open for Input  :                                            */ 
/*   Files open for Output :                                            */ 
/*   Modules Referenced    :                                            */ 
/* __MODULE__              : DIFF_H                                     */
/* __IDENT__               : X-02-05ECO8                                */
/* __REV__                 : 02.05.08                                   */
/* Revision History follows                                             */
/*  28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header */
/*


/* ************ MACRO DEFINITIONS ************  */

/**  Long enough value for DIFF			**/
#define LNGVAL					25

/**  Maximum lines and chars in a line for DIFF **/
#define MAXLINES				65535
#define MAXCHARS				65535


/* ************ GLOBAL DECLARATION ************  */

typedef unsigned int	HASH;	    /**  Result of hasing function for
					 a line of text  **/
typedef struct tblentry		    /**  Info about an entry in a hash table **/
{
	int frst,		    /**  First line # with this hash code  **/
	    last;		    /**  Last line # with this hash code   **/
} TBLENTRY;

typedef struct txtinf		    /**  Info about a line of text  **/
{
	char *content;		    /**  A line of text  **/
	int  len;		    /**  Line length  **/
} TXTINF;

typedef struct lineinf		    /**  Info about a line of text  **/
{
	HASH hash;		    /**  Hash code value  **/
	int  nxtln;		    /**  Next line with same hash (or 0)  **/
} LINEINF;

typedef struct fileinf		    /**  Info about a file  **/
{
	char	 *filename;	    /**  File name  **/
	TXTINF	 *txt;		    /**  Array of lines of text  **/
	LINEINF	 *line;		    /**  Array of line info structs  **/
	TBLENTRY *hashtbl;	    /**  Hash table  **/
} FILEINF;


/* ************ FUNCTION PROTOTYPING ************  */

unsigned long		    compare(char *, TXTINF *, int, char *, TXTINF *, int, int);
int			    txtcmp(TXTINF, TXTINF);

