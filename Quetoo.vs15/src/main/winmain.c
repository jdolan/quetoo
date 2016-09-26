// Special file to redirect winmain
#include "config.h"
#include "shared.h"

extern int32_t main(int32_t argc, char **argv);

int CALLBACK WinMain(
  _In_ HINSTANCE hInstance,
  _In_ HINSTANCE hPrevInstance,
  _In_ LPSTR     lpCmdLine,
  _In_ int       nCmdShow
)
{
	return main(__argc, __argv);
}