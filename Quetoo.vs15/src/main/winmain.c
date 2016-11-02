// Special file to redirect winmain
#include "config.h"
#include "shared.h"

extern int32_t main(int32_t argc, char **argv);

#if defined(_MSC_VER)
#include <DbgHelp.h>

HMODULE GetCurrentModule()
{
	HMODULE hModule = NULL;
	GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		(LPCTSTR)GetCurrentModule,
		&hModule);

	return hModule;
}

HRESULT GenerateCrashDump(MINIDUMP_TYPE flags, EXCEPTION_POINTERS *seh)
{
	HRESULT error = S_OK;

	// get the time
	SYSTEMTIME sysTime = { 0 };
	GetSystemTime(&sysTime);

	// build the filename: APPNAME_COMPUTERNAME_DATE_TIME.DMP
	char path[MAX_PATH] = { 0 };
	char moduleName[MAX_PATH] = { 0 };

	const size_t len = GetModuleFileName(GetCurrentModule(), moduleName, sizeof(moduleName));
	moduleName[len] = 0;

	const char *slash = strrchr(moduleName, '\\');

	if (!slash)
		slash = moduleName;
	else
		slash++;

	const char *dot = strrchr(moduleName, '.');
	const size_t nameLength = dot ? (dot - slash) : strlen(slash);

	g_strlcpy(moduleName, slash, nameLength + 1);

	sprintf_s(path, ARRAYSIZE(path),
		".\\%s_%04u-%02u-%02u_%02u-%02u-%02u.dmp",
		moduleName,
		sysTime.wYear, sysTime.wMonth, sysTime.wDay,
		sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

	// open the file
	HANDLE hFile = CreateFileA(path, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {

		error = GetLastError();
		error = HRESULT_FROM_WIN32(error);
		return error;
	}

	// get the process information
	HANDLE hProc = GetCurrentProcess();
	DWORD procID = GetProcessId(hProc);

	// if we have SEH info, package it up
	MINIDUMP_EXCEPTION_INFORMATION sehInfo = { 0 };
	MINIDUMP_EXCEPTION_INFORMATION *sehPtr = NULL;

	if (seh) {

		sehInfo.ThreadId = GetCurrentThreadId();
		sehInfo.ExceptionPointers = seh;
		sehInfo.ClientPointers = FALSE;
		sehPtr = &sehInfo;
	}

	// generate the crash dump
	BOOL result = MiniDumpWriteDump(hProc, procID, hFile, flags, sehPtr, NULL, NULL);

	if (!result)
		error = (HRESULT)GetLastError(); // already an HRESULT

	// close the file
	CloseHandle(hFile);

	MessageBox(NULL, "A dump has been generated. Check the bin folder for a .dmp file.", "Crash!", MB_OK | MB_ICONERROR);

	return error;
}
#endif

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR     lpCmdLine,
	_In_ int       nCmdShow
)
{
#if defined(_MSC_VER)
	__try
	{
#endif
		return main(__argc, __argv);
#if defined(_MSC_VER)
	}
	__except (GenerateCrashDump((MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithUnloadedModules), GetExceptionInformation()))
	{
		return 0;
	}
#endif
}