mkdir ..\bin\Win32Debug\
mkdir ..\bin\Win32Release\
mkdir ..\bin\Win64Debug\
mkdir ..\bin\Win64Release\

copy win32\lib\glib-2.0.lib ..\bin\Win32Debug\glib-2.0.lib /Y
copy win32\lib\glib-2.0.lib ..\bin\Win32Release\glib-2.0.lib /Y

copy win32\bin\libglib-2.0-0.dll ..\bin\Win32Debug\libglib-2.0-0.dll /Y
copy win32\bin\libglib-2.0-0.dll ..\bin\Win32Release\libglib-2.0-0.dll /Y