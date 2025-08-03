@echo off

set CheckedFiles=*.h *.cpp *.inl *.c

echo -------------
echo STATICS FOUND:
echo -------------
findstr -s -n -i -l "static" %CheckedFiles%

echo --------------
@echo GLOBALS FOUND:
echo --------------
findstr -s -n -i -l "local_persist" %CheckedFiles%
findstr -s -n -i -l "global_variable" %CheckedFiles%
