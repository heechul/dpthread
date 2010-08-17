BEGIN { FS = "|"; printf("ptime, ltime, ctx\n") }
{ printf ("%d,%d,%d\n",$2,$3,$4) }
# $2 = ptime, $3 = ltime, $4 = #ctx 
