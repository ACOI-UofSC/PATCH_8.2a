python3 -m venv .venv
REM .venv\scripts\activate.bat
.\.venv\scripts\pip3 install pyside6 pyinstaller
.\.venv\scripts\python -m PyInstaller bin2csv.spec
