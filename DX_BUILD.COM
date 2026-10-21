$! DX_BUILD.COM -- build DX.EXE and DXHELP.HLB (no MMS/MMK needed)
$!
$!	__MODULE__	DX_BUILD
$!	__IDENT__	X-02-05ECO12
$!	__REV__		02.05.12
$!
$!	AUTHORS:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
$!
$!	MODIFICATION HISTORY:
$!
$!	26-SEP-2026	CLD	X-02-05ECO4: builds in its own directory and gives the
$!				caller's default directory back on every exit (FINISH);
$!				CC/LINK severity from the saved status (a %CC-F- was
$!				counted as "Compiled OK"); error exit without %NONAME-E-NOMSG.
$!	28-SEP-2026	RRL	X-02-05ECO9: DX.EXE gets the image ident (IDENTIFICATION=
$!				in DX_LINK.OPT), taken from __IDENT__ of the head module DX.C.
$!	21-OCT-2026	CLD	X-02-05ECO12: SYS$LIB_C.TLB as a text library on every CC
$!				(FILERSTA.C includes <fh2def.h>, it is not in SYS$STARLET_C).
$!
$! Simple compile-all-modules build script, for use where
$! neither MMS nor MMK is available.  Compiles every DX .C module listed
$! in DX_MODULES.LIS (one name per line, no extension), reports any that
$! failed, then (if all succeeded) compiles DX_MSG.MSG and DX_CLI.CLD and
$! links DX.EXE directly from the .OBJ files -- no intermediate DX.OLB
$! library step (skipped on purpose: nothing else ever pulls single
$! modules back out of it, so it is pure wasted build time here).  Finally
$! rebuilds DXHELP.HLB from DX.HLP for the standard "HELP DX" command.
$!
$! Usage:  $ @DX_BUILD          (release build -> DX.EXE)
$!         $ @DX_BUILD DEBUG    (debug build, /DEBUG/NOOPTIMIZE + linker
$!                               /DEBUG -> DX_DEBUG.EXE, does not touch
$!                               DX.EXE)
$!
$! NOTE: a DEBUG build recompiles every module with debug qualifiers, so
$! it overwrites the release .OBJ files with debug-flavor ones (VMS keeps
$! the previous release .OBJ as an older file version, but it is no longer
$! the version LINK picks up by default) -- run a plain "$ @DX_BUILD"
$! afterward to get back to a clean release DX.EXE.
$!
$! NOTE: sets its own default directory explicitly -- when run as a batch
$! job, SYS$SYLOGIN/LOGIN.COM runs first and leaves the default directory
$! wherever the account's login procedure puts it (e.g. SYS$COMMON:[SYSMGR]
$! for SYSTEM), NOT wherever the submitting process's default was.  The
$! directory is the one this procedure itself lives in (the sources are
$! expected next to it), so the kit builds wherever it is unpacked.  The
$! caller's default directory is restored on every way out (success,
$! failure or CTRL/Y) -- all exits go through FINISH below.
$!
$ ORIG_DEFAULT = F$ENVIRONMENT("DEFAULT")
$ BUILD_STATUS = %X10000002		! error, STS$M_INHIB_MSG: the reason is already shown
$ ON CONTROL_Y THEN GOTO FINISH
$ HERE = F$PARSE(F$ENVIRONMENT("PROCEDURE"),,,"DEVICE") + -
	 F$PARSE(F$ENVIRONMENT("PROCEDURE"),,,"DIRECTORY")
$ SET DEFAULT 'HERE'
$ SET NOON
$ IF P1 .EQS. "DEBUG"
$ THEN
$   CCQUAL = "/DEBUG/NOOPTIMIZE"
$   LINKQUAL = "/DEBUG"
$   EXENAME = "DX_DEBUG.EXE"
$   WRITE SYS$OUTPUT "*** DEBUG BUILD -- output will be ''EXENAME', not DX.EXE ***"
$ ELSE
$   CCQUAL = ""
$   LINKQUAL = ""
$   EXENAME = "DX.EXE"
$ ENDIF
$ FAILED = 0
$ GOODCOUNT = 0
$ OPEN/READ MODFILE DX_MODULES.LIS
$LOOP:
$ READ/END_OF_FILE=DONE MODFILE M
$ M = F$EDIT(M, "TRIM")
$ IF M .EQS. "" THEN GOTO LOOP
$ WRITE SYS$OUTPUT "--- Compiling ''M'.C ---"
$ CC'CCQUAL' 'M'.C+SYS$LIBRARY:SYS$LIB_C.TLB/LIBRARY
$ CCSTATUS = $STATUS
$! Severity is taken from the saved status, NOT from $SEVERITY: the
$! assignment above is itself a successful DCL command and resets $SEVERITY
$! to 1, so a %CC-F- (e.g. OPENIN) used to be counted as "Compiled OK".
$ CCSEVERITY = CCSTATUS .AND. 7
$! Severity: 0=warning 1=success 2=error 3=info 4=fatal -- only ERROR/FATAL
$! (2 or 4) are real failures; a WARNING-severity completion (e.g. a single
$! %CC-W-... diagnostic) still produces a usable .OBJ and should not stop
$! the build.
$ IF (CCSEVERITY .EQ. 2) .OR. (CCSEVERITY .EQ. 4)
$ THEN
$   WRITE SYS$OUTPUT "*** FAILED: ''M'.C  (status = ''CCSTATUS')"
$   FAILED = FAILED + 1
$ ELSE
$   GOODCOUNT = GOODCOUNT + 1
$ ENDIF
$ GOTO LOOP
$DONE:
$ CLOSE MODFILE
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "Compiled OK: ''GOODCOUNT'   Failed: ''FAILED'"
$ IF FAILED .GT. 0
$ THEN
$   WRITE SYS$OUTPUT "Not linking ''EXENAME' -- fix the failures above first."
$   GOTO FINISH
$ ENDIF
$!
$ WRITE SYS$OUTPUT "--- Compiling DX_MSG.MSG / DX_CLI.CLD ---"
$ MESSAGE DX_MSG.MSG
$ MSGSEVERITY = $SEVERITY
$ IF (MSGSEVERITY .EQ. 2) .OR. (MSGSEVERITY .EQ. 4)
$ THEN
$   WRITE SYS$OUTPUT "*** FAILED: MESSAGE DX_MSG.MSG (severity = ''MSGSEVERITY')"
$   GOTO FINISH
$ ENDIF
$ SET COMMAND/OBJECT DX_CLI.CLD
$ CLDSEVERITY = $SEVERITY
$ IF (CLDSEVERITY .EQ. 2) .OR. (CLDSEVERITY .EQ. 4)
$ THEN
$   WRITE SYS$OUTPUT "*** FAILED: SET COMMAND/OBJECT DX_CLI.CLD (severity = ''CLDSEVERITY')"
$   GOTO FINISH
$ ENDIF
$!
$ WRITE SYS$OUTPUT "--- Linking ''EXENAME' directly from .OBJ files (no .OLB) ---"
$! The image ident is the product ident: __IDENT__ of the head module DX.C,
$! the text between the quotes of its "#define __IDENT__" line.
$ IDENT = ""
$ OPEN/READ IDFILE DX.C
$IDLOOP:
$ READ/END_OF_FILE=IDDONE IDFILE L
$ IF F$LOCATE("#define __IDENT__", L) .EQ. F$LENGTH(L) THEN GOTO IDLOOP
$ IDENT = F$ELEMENT(1, """", L)
$IDDONE:
$ CLOSE IDFILE
$ IF (IDENT .EQS. "") .OR. (IDENT .EQS. """")
$ THEN
$   WRITE SYS$OUTPUT "*** FAILED: no #define __IDENT__ ""...""  in DX.C"
$   GOTO FINISH
$ ENDIF
$ IF F$SEARCH("DX_LINK.OPT") .NES. "" THEN DELETE/NOLOG DX_LINK.OPT;*
$ OPEN/WRITE OPTFILE DX_LINK.OPT
$ OPEN/READ MODFILE DX_MODULES.LIS
$OPTLOOP:
$ READ/END_OF_FILE=OPTDONE MODFILE M
$ M = F$EDIT(M, "TRIM")
$ IF M .EQS. "" THEN GOTO OPTLOOP
$ WRITE OPTFILE M + ".OBJ"
$ GOTO OPTLOOP
$OPTDONE:
$ CLOSE MODFILE
$ WRITE OPTFILE "DX_MSG.OBJ"
$ WRITE OPTFILE "DX_CLI.OBJ"
$ WRITE OPTFILE "SYS$SHARE:TPUSHR.EXE/SHARE"
$ WRITE OPTFILE "IDENTIFICATION=""''IDENT'"""
$ CLOSE OPTFILE
$ IF F$SEARCH(EXENAME) .NES. "" THEN DELETE/NOLOG 'EXENAME';*
$ LINK/NOTRACE'LINKQUAL'/EXECUTABLE='EXENAME' DX_LINK.OPT/OPTIONS
$ LINKSTATUS = $STATUS
$ LINKSEVERITY = LINKSTATUS .AND. 7
$ IF (LINKSEVERITY .EQ. 2) .OR. (LINKSEVERITY .EQ. 4)
$ THEN
$   WRITE SYS$OUTPUT "*** LINK FAILED (status = ''LINKSTATUS')"
$   GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "''EXENAME' built, image ident ''IDENT'."
$!
$ WRITE SYS$OUTPUT "--- Building DXHELP.HLB from DX.HLP (for HELP DX) ---"
$ IF F$SEARCH("DXHELP.HLB") .NES. "" THEN DELETE/NOLOG DXHELP.HLB;*
$ LIBRARY/CREATE/HELP DXHELP.HLB DX.HLP
$ HLBSEVERITY = $SEVERITY
$ IF (HLBSEVERITY .EQ. 2) .OR. (HLBSEVERITY .EQ. 4)
$ THEN
$   WRITE SYS$OUTPUT "*** FAILED: LIBRARY/CREATE/HELP DXHELP.HLB (severity = ''HLBSEVERITY')"
$   GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "DXHELP.HLB built."
$ BUILD_STATUS = 1
$!
$FINISH:
$ IF F$TRNLNM("MODFILE", "LNM$PROCESS") .NES. "" THEN CLOSE MODFILE
$ IF F$TRNLNM("OPTFILE", "LNM$PROCESS") .NES. "" THEN CLOSE OPTFILE
$ IF F$TRNLNM("IDFILE", "LNM$PROCESS") .NES. "" THEN CLOSE IDFILE
$ SET DEFAULT 'ORIG_DEFAULT'
$ EXIT BUILD_STATUS
