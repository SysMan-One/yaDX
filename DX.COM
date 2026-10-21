$ ver = f$ver(0)
$! DX.COM -- defines DX$HOME and the DX command
$!
$!	__MODULE__	DX_COM
$!	__IDENT__	X-02-05ECO8
$!	__REV__		02.05.08
$!
$!	MODIFICATION HISTORY:
$!
$!	28-SEP-2026	RRL	X-02-05ECO8: __MODULE__/__IDENT__/__REV__ header added.
$!
$ here = F$PARSE(F$ENVIRONMENT("PROCEDURE"),,,,"NO_CONCEAL")-"]["
$ here = F$EXTRACT(0,F$LOCATE("]",here)+1,here)
$ DEFINE/nolog DX$HOME 'here'
$ SET COMMAND 'HERE'DX
$ DX :== ""
$ WRITE SYS$OUTPUT "DX loaded."
$ ver = f$ver(ver)
