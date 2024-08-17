#pragma once

#include "MemoryManager.h"

class SDK
{
private:

	ULONG_PTR eprocessAddress;
	uintptr_t directoryTableBaseAddress;

public:
	static HWND getTargetHwnd(wchar_t* targetWindowName) {
		HWND targetHwnd = FindWindowW(NULL, targetWindowName);
		return targetHwnd;
	};
	static DWORD getTargetProcId(HWND window) {
		static DWORD targetProcId = NULL;
		GetWindowThreadProcessId(window, &targetProcId);
		return targetProcId;
	};

	SDK(DWORD pid) {
		MemoryManager::OpenDrivers();

	dma:
		try {
			eprocessAddress = MemoryManager::FindEprocessByPid(pid);
			uintptr_t kprocessAddress = MemoryManager::GetKProcess(eprocessAddress);
			directoryTableBaseAddress = MemoryManager::GetDirectoryTableBase(kprocessAddress);
		}
		catch (const std::runtime_error& e) {
			Sleep(1000);
			goto dma;
		}

	}
	~SDK() {
		MemoryManager::CloseDrivers();
	}


	uintptr_t GetModuleAddress(wchar_t* targetModuleName) {
		ULONG_PTR pebAddress;
		ULONG_PTR ldrDataAddress;
		ULONG_PTR headAddress, currentAddress;

		// Read the PEB address
		uint64_t pointerPebAddress = eprocessAddress + 0x3f8; // windows dependent offset  struct _PEB* Peb;
		MemoryManager::ReadArbitraryMemory(pointerPebAddress, (unsigned char*)&pebAddress, sizeof(pebAddress));

		// Read the PEB_LDR_DATA address it's a usermode process address
		uint64_t pointerLDRDataAddress = pebAddress + 0x18; // windows dependent offset  struct _PEB_LDR_DATA* Ldr;
		ReadVirtualMemory(pointerLDRDataAddress, (unsigned char*)&ldrDataAddress, 8);

		// Get the head of the InLoadOrderModuleList
		uintptr_t pointerHeadAddress = ldrDataAddress + 0x10; //windows dependent offset  struct _LIST_ENTRY InLoadOrderModuleList; 
		ReadVirtualMemory(pointerHeadAddress, (unsigned char*)&headAddress, 8);


		// Traverse the InLoadOrderModuleList
		currentAddress = headAddress;
		do {
			uintptr_t moduleBaseAddress;
			ReadVirtualMemory(currentAddress + 0x30, (unsigned char*)&moduleBaseAddress, 8); // windows dependent offset LDR_DATA_TABLE_ENTRY::VOID* DllBase; 

			UNICODE_STRING unicodeString;
			ReadVirtualMemory(currentAddress + 0x58, (unsigned char*)&unicodeString, sizeof(UNICODE_STRING)); // windows dependent offset struct LDR_DATA_TABLE_ENTRY::_UNICODE_STRING BaseDllName;

			ReadVirtualMemory(currentAddress, (unsigned char*)&currentAddress, 8);

			wchar_t* moduleName = new wchar_t[unicodeString.Length / sizeof(wchar_t) + 1];

			try {
				ReadVirtualMemory((uintptr_t)unicodeString.Buffer, (unsigned char*)moduleName, unicodeString.Length);

				moduleName[unicodeString.Length / sizeof(wchar_t)] = '\0';

				if (wcscmp(moduleName, targetModuleName) == 0) {
					delete[] moduleName;
					return moduleBaseAddress;
				}
				delete[] moduleName;
			}
			catch (const std::runtime_error& e) {
				delete[] moduleName;
				continue;
			}

		} while (currentAddress != headAddress);

		// Module not found
		throw std::runtime_error("Module of app not found");
	}


	void ReadVirtualMemory(uint64_t virtualAddress, unsigned char* buffer, DWORD sizeToRead) {
		uintptr_t physAddress = MemoryManager::VirtualToPhysical(directoryTableBaseAddress, virtualAddress);
		MemoryManager::ReadPhysicalMemory(physAddress, buffer, sizeToRead);
	}

	template <typename T>
	void ReadVirtual(uint64_t virtualAddress, T& buffer)
	{
		DWORD sizeToRead = sizeof(T);
		ReadVirtualMemory(virtualAddress, reinterpret_cast<unsigned char*>(&buffer), sizeToRead);
	}

};