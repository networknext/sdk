@echo off
echo hello batch file world
premake5 vs2022
"c:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com" /Run visualstudio/next.sln /Project test