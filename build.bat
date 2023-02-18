make mingw
md output
robocopy src output *.exe *.dll
robocopy . output COPYRIGHT HISTORY INSTALL README README.md
if ErrorLevel 8 (exit /B 1) else (exit /B 0)