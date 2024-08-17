#include "SDK.h"

HWND appWindow = NULL;
DWORD appPid = 0;
SDK* sdk = NULL;

uintptr_t appDll = 0;

int main()
{
findTarget:
	appWindow = SDK::getTargetHwnd((wchar_t*)L"This PC");
	if (appWindow)
	{
		appPid = SDK::getTargetProcId(appWindow);
		wprintf(L"\nFound APP, pid: %d\n\n", appPid);
	}
	else
	{
		Sleep(1000);
		wprintf(L".");
		goto findTarget;
	}

	wprintf(L"DMA init\n");
	sdk = new SDK(appPid);

	// This is example to find module without any handles from DMA)
	wprintf(L"Looking for DLL\n");
findDll:
	try
	{
		appDll = sdk->GetModuleAddress((wchar_t*)L"ntdll.dll");
	}
	catch (const std::runtime_error& e)
	{
		Sleep(1000);
		wprintf(L".");
		goto findDll;
	}
	wprintf(L"appDll: %#llx\n", appDll);


	// This is example to read virtual memory without handles
	uintptr_t addrToRead = appDll + 0x0;
	uint64_t value;  sdk->ReadVirtual(addrToRead, value);
	wprintf(L"addrToRead:%llx value:%llx\n", addrToRead, value);

}
