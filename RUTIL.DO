10 rem rlist - Utilities for the Ron Drive
20 maxfiles = 2
30 print "1) Rename file on disk";print "2) Delete file on disk";print "3) Change directory on disk"
40 input "Option:";o
50 if o = 1 then 90
60 if o = 2 then 180
70 if o = 3 then 240
80 goto 30
90 rem rename file
100 input "Old name";f$
110 input "New name";n$
120 gosub 400:gosub 440
140 R$="ZZ" +CHR$(13)+CHR$(25)+F$+SPACE$(24-LEN(F$))+"F"
150 print #1,r$;
160 gosub 520: goto 990
170 rem Delete file
180 input "File to delete";f$
190 gosub 400:gosub 440
200 R$="ZZ" +CHR$(5)+CHR$(0)
210 print #1,r$;
220 gosub 520: goto 990
230 rem cwd
240 print "Not supported yet"
250 goto 990
399 rem open files
400 open "com:98N1D" for output as #1
410 open "com:98N1D" for input as #2
420 return
430 rem reference file
440 R$="ZZ" +CHR$(0)+CHR$(26)+F$+SPACE$(24-LEN(F$))+"F"+CHR$(0)
450 PRINT #1,R$;
460 C$=INPUT$(1,2)
470 IF ASC(C$) <> 17 THEN PRINT "Invalid response code (ref)":GOTO 990
480 C$=INPUT$(1,2)
490 L=ASC(C$)
500 Q$=INPUT$(L,2):q$=""
510 return
520 rem process return
530 C$=INPUT$(1,2)
540 IF ASC(C$) <> 18 THEN PRINT "Invalid response code (open)":GOTO 990
550 C$=INPUT$(1,2):C$=INPUT$(1,2)
560 IF ASC(C$) <> 0 THEN PRINT "Error:",ASC(C$):GOTO 990
570 return
990 close #1:close #2:end
