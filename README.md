# Software DMA Implementation Example

This example demonstrates how to implement DMA (Direct Memory Access) using the `MMapIOSpace` function. With this approach, you can read any process's virtual memory without using handles, directly accessing physical memory.

## Requirements

- **BRD (Bring Your Own Driver):** You'll need a custom driver if you want to make this work on other Windows versions.
- **MMapIOSpace Drivers:** These drivers work on Windows 10 with versions prior to 1803.
- **Windows 11 Note:** After Windows 10 version 1803, `MMapIoSpace` can only read unpaged memory. For Windows 11, you'll need to use `ZwMapViewOfSection`.

## Example Setup (for Windows 10 1709)

1. **Install and Run RWEverythingx64 1.7:** This tool allows low-level access to hardware registers and memory.
2. **Install and Run MSI Afterburner 4.6.1:** This utility helps monitor and control graphics card settings.
3. **Clone OpenDMA Repository:** Clone the OpenDMA repository and open it in Visual Studio.
4. **Open Target (This PC):** Identify the target process whose memory you want to read.
5. **Run OpenDMA:** Execute the OpenDMA application and enjoy exploring the process's virtual memory!

Feel free to adjust the instructions based on your specific use case. Happy coding! 😊
