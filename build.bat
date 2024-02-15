@echo off
echo hello batch file world
premake5 vs2019
"c:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com" visualstudio/next.sln /Build $CONFIG