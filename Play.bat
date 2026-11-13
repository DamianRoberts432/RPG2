@echo off
rem Double-click to update the game to the latest main, rebuild it and play.
cd /d "%~dp0"
git checkout main
git pull
gcc *.c -o game.exe -lgdi32 -lxinput -lmsimg32
if errorlevel 1 (pause & exit /b)
start "" game.exe
