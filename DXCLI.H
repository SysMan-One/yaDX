/* Program Name            : DXCLI.H                                    */
/*   Original Author       : JLAURET                                    */
/*   Date                  : 03-Jun-1999                                */
/*   Program Description   :                                            */
/*                         :                                            */
/* __MODULE__              : DXCLI_H                                    */
/* __IDENT__               : X-02-05ECO8                                */
/* __REV__                 : 02.05.08                                   */
/* Revision History follows                                             */
/*  28-Sep-2026 RRL ECO8: __MODULE__/__IDENT__/__REV__ added to the header */
#ifndef __DESCRIP_LOADED
#include <descrip.h>
#endif
$DESCRIPTOR(command,"DX ");
#if defined(__DECC) || defined(__GNUC__)
extern char	*DX_CLI;
#else
globalref	DX_CLI;
#endif
