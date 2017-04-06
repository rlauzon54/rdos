10 REM rload - Get file from Ron Disk
20 MAXFILES = 3
30 INPUT "What file to get from drive";F$
40 INPUT "What to name it Tandy";L$
50 OPEN "com:98N1D" FOR OUTPUT AS #1
60 OPEN "com:98N1D" FOR INPUT AS #2
80 REM reference file
90 R$="ZZ" +CHR$(0)+CHR$(26)+F$+SPACE$(24-LEN(F$))+"F"+CHR$(0)
100 PRINT #1,R$;:print ".";
110 C$=INPUT$(1,2)
120 IF ASC(C$) <> 17 THEN PRINT "Invalid response code (ref)":GOTO 385
130 C$=INPUT$(1,2)
140 L=ASC(C$)
150 Q$=INPUT$(L,2)
159 REM open file for read
160 R$="ZZ" +CHR$(1)+CHR$(1)+CHR$(3)
170 PRINT #1,R$;:print ".";
180 C$=INPUT$(1,2)
190 IF ASC(C$) <> 18 THEN PRINT "Invalid response code (open)":GOTO 385
200 C$=INPUT$(1,2):C$=INPUT$(1,2)
210 IF ASC(C$) <> 0 THEN PRINT "Error opening file:",ASC(C$):GOTO 385
215 OPEN L$ FOR OUTPUT AS #3
219 REM Read file
220 R$="ZZ" +CHR$(3)+CHR$(0)
230 PRINT #1,R$;:print ".";
240 C$=INPUT$(1,2)
250 IF ASC(C$) <> 16 THEN PRINT "Invalid response code (read)":GOTO 390
260 C$=INPUT$(1,2)
270 L=ASC(C$)
280 IF L = 0 GOTO 330
290 Q$=INPUT$(L,2)
300 PRINT #3,Q$;
305 Q$=""
310 IF L < 128 THEN 330 ELSE 220
329 REM close file
330 R$="ZZ" +CHR$(2)+CHR$(0)
340 PRINT #1,R$;:print ".";
350 C$=INPUT$(1,2)
360 IF ASC(C$) <> 18 THEN PRINT "Invalid response code (close)":GOTO 390
370 C$=INPUT$(1,2):C$=INPUT$(1,2)
380 IF ASC(C$) <> 0 THEN PRINT "Error closing file:",ASC(C$)
385 CLOSE #3
390 CLOSE #1:CLOSE #2:END
