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
call ROBO "../src/cgame/%module_name%/ui/hud/" "%quetoo_folder%/lib/%module_name%/ui/hud/" *.json
GOTO DONE

:INVALID
echo "No build input folder, no module name, or QUETOO_HOME not defined. Gonedy."

:DONE
