IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID
if "%QUETOO_HOME%" == "" GOTO INVALID

set quetoo_folder=%~1
set build_name=%~2

call ROBO_FLAT "bin/%build_name%/ctf" "%quetoo_folder%/lib/ctf" cgame*
GOTO DONE

:INVALID
echo "No build input folder, or QUETOO_HOME not defined. Gonedy."

:DONE
