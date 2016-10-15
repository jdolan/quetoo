IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID

set quetoo_folder=%1
set build_name=%2

copy "..\..\src\client\renderer\shaders\*.glsl" "%quetoo_folder%\lib\default\shaders\*" /y

GOTO DONE

:INVALID
echo "No build input folder. Gonedy."

:DONE