make mingw
rd /S /Q output
md output
robocopy src output *.exe *.dll
robocopy . output COPYRIGHT HISTORY INSTALL README README.md
rename output\lua.exe esolua.exe
rename output\luac.exe esoluac.exe