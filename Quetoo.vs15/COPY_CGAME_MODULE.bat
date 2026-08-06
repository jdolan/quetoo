IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%3] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID
IF [%3] == [""] GOTO INVALID
if "%QUETOO_HOME%" == "" GOTO INVALID

set quetoo_folder=%~1
set build_name=%~2
set module_name=%~3

call ROBO_FLAT "bin/%build_name%/%module_name%" "%quetoo_folder%/lib/%module_name%" cgame*
GOTO DONE

:INVALID
echo "No build input folder, no module name, or QUETOO_HOME not defined. Gonedy."

:DONE
