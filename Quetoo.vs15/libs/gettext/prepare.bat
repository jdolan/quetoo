mkdir ..\bin\Win32Debug\
mkdir ..\bin\Win32Release\
mkdir ..\bin\Win64Debug\
mkdir ..\bin\Win64Release\

copy win32\bin\intl.dll ..\bin\Win32Debug\intl.dll /Y
copy win32\bin\intl.dll ..\bin\Win32Release\intl.dll /Y
copy win64\bin\libintl-8.dll ..\bin\Win64Debug\libintl-8.dll /Y
copy win64\bin\libintl-8.dll ..\bin\Win64Release\libintl-8.dll /Y