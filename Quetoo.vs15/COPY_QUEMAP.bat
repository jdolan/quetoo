IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID
if "%QUETOO_HOME%" == "" GOTO INVALID

set quetoo_folder=%~1
set build_name=%~2

call ROBO "bin/%build_name%" "%quetoo_folder%/bin/" quemap*

GOTO DONE

:INVALID
echo "No build input folder, or QUETOO_HOME not defined. Gonedy."

:DONE