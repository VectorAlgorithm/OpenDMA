#pragma once

//RWDrv specific vulnerable driver usage for reading/writing physical memory/cr registers etc
//RWDrv is driver of RewriteEverything tool
//Working version RWEverything (1.7 tested) x64 version needed
#include <windows.h>
#include <cstdint>


struct PhysRW_t
{
    uint64_t PhysicalAddress;
    DWORD Size;
    DWORD Unknown;
    uint64_t Address;
};
struct RegRW_t {
    int Register;
    uint64_t Value;
};

#define DEVICE_NAME_RWDRV "\\\\.\\RwDrv"
#define IOCTL_READ_PHYS_MEM_RWDRV 0x222808 //!reads only first 4gb of ram
#define IOCTL_READ_CR_RWDRV 0x22286C

static HANDLE OpenDriverRWDrv() {
    return CreateFileA(DEVICE_NAME_RWDRV, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
}

static BOOL ReadPhysicalMemoryRWDrv(HANDLE hDevice, uintptr_t addressToRead, unsigned char* buffer, unsigned short sizeToRead)
{
    PhysRW_t readStruct;
    readStruct.PhysicalAddress = addressToRead; //addressToRead
    readStruct.Address = (uint64_t)buffer;
    readStruct.Unknown = 0;
    readStruct.Size = sizeToRead;

    if (!DeviceIoControl(hDevice, IOCTL_READ_PHYS_MEM_RWDRV, &readStruct, sizeof(readStruct), NULL, 0, NULL, NULL))
    {
        return false;
    }
    return true;
}