[NOTE, 26-Sep-2026: this is the original DX 2.5 README from 1999, kept for
 history.  Its build notes (MMS/MMK) are outdated -- see AAAREADME.TXT or
 README.md for the current build (DX_BUILD.COM) and install.]

PRODUCT NAME
 DX (Directory eXtension), Version 2.5


GENERAL DESCRIPTION
 DX  is a VAX/VMS utility that performs many file operations on a VT100
 or upper type terminal.  It can be used to  delete,  edit,  or  rename
 files in the current directory.  DX can also create a hardcopy listing
 of all the files displayed.

FEATURES

o Flexible display and sorting
o Pull-down menu for point-and-shoot interface
o Display sub-directories and directory contents with flexible "filters"
o Multiple inclusion/exclusion patterns in file operations
o Multiple windows provides different views
o Display a tree-like directory structure in a scrollable window

REQUIREMENTS
 Either MMS or MMK (.COM compilation no longer supported).

COMPILATION
 On AXP node	$ MMS{S|K}/macro=(__AXP__=1)
 On VAX node	$ MMS{S|K}

 To link only, add "LINK" on the command line.
 

 I successfully compiled DX on OpenVMS 6.2 both VAX and Alpha using
 respectively VAXC V3.2-044 and DECC V6.0-001.


INSTALLATION :

 1) Logical
  The logical DX$HOME is used to locate DX default files. System-wide
  installation will require a DEFINE /SYSTEM . Not Mandatory !!!

 2) Executable
  You can chose one of the following :

  o Copy the executable where you want it to be and define a foreign symbol 
    DX :== $disk:[dir]DX
  o Insert DX.CLD in DCLTABLES, change DX$HOME to where the executable is
    installed (if not in DX$HOME), copy the EXEcutable there.
  o Place DX.EXE in DCL$PATH (OpenVMS 6.2 or upper). 
    

 3) Help Library
  Copy DXHELP.HLB  in DX$HOME: . 
  DX will also try to locate DXHELP.HLB in SYS$HELP: .
  The logical DX$HELPLIB may be defined to point on where the .HLB is. 
  Ex :
  	define/system DX$HELPLIB APPSDOC:DXHELP.HLB

 Note : In principle, you do not need to define DX$HELP. I personally place the
 .HLB file in DX$HELP and insert DX.CLD DX$HOME into VMSAPPS: a directory where
 all of our VMS freeware are located.



AUTHOR
 Original Author :  Chau-Kuang Hung (chung@us.oracle.com)
 Maintained by   :  J�r�me Lauret (jlauret@mail.chem.sunysb.edu)
 
 Please send comments bug report and suggestions to
 jlauret@mail.chem.sunysb.edu.
 

