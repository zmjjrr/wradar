#include "Nt.h"
#include "Misc.h"
#include "Modules.h"
#include <ntifs.h>

#define UEFI_MAPPER

/*
Uefi mapper compatibility stuff
*/

#ifdef UEFI_MAPPER

#define MAPPER_SIZE 14 + 7
#define MAPPER_BASE MAPPER_SIZE
__declspec(dllexport) volatile BYTE qwerty[MAPPER_SIZE + 16];


typedef struct _IMAGE_DOS_HEADER {      // DOS .EXE header
    WORD   e_magic;                     // Magic number
    WORD   e_cblp;                      // Bytes on last page of file
    WORD   e_cp;                        // Pages in file
    WORD   e_crlc;                      // Relocations
    WORD   e_cparhdr;                   // Size of header in paragraphs
    WORD   e_minalloc;                  // Minimum extra paragraphs needed
    WORD   e_maxalloc;                  // Maximum extra paragraphs needed
    WORD   e_ss;                        // Initial (relative) SS value
    WORD   e_sp;                        // Initial SP value
    WORD   e_csum;                      // Checksum
    WORD   e_ip;                        // Initial IP value
    WORD   e_cs;                        // Initial (relative) CS value
    WORD   e_lfarlc;                    // File address of relocation table
    WORD   e_ovno;                      // Overlay number
    WORD   e_res[4];                    // Reserved words
    WORD   e_oemid;                     // OEM identifier (for e_oeminfo)
    WORD   e_oeminfo;                   // OEM information; e_oemid specific
    WORD   e_res2[10];                  // Reserved words
    LONG   e_lfanew;                    // File address of new exe header
} IMAGE_DOS_HEADER, * PIMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {
    WORD    Machine;
    WORD    NumberOfSections;
    DWORD   TimeDateStamp;
    DWORD   PointerToSymbolTable;
    DWORD   NumberOfSymbols;
    WORD    SizeOfOptionalHeader;
    WORD    Characteristics;
} IMAGE_FILE_HEADER, * PIMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
    DWORD   VirtualAddress;
    DWORD   Size;
} IMAGE_DATA_DIRECTORY, * PIMAGE_DATA_DIRECTORY;

typedef struct _IMAGE_OPTIONAL_HEADER64 {
    WORD        Magic;
    BYTE        MajorLinkerVersion;
    BYTE        MinorLinkerVersion;
    DWORD       SizeOfCode;
    DWORD       SizeOfInitializedData;
    DWORD       SizeOfUninitializedData;
    DWORD       AddressOfEntryPoint;
    DWORD       BaseOfCode;
    ULONGLONG   ImageBase;
    DWORD       SectionAlignment;
    DWORD       FileAlignment;
    WORD        MajorOperatingSystemVersion;
    WORD        MinorOperatingSystemVersion;
    WORD        MajorImageVersion;
    WORD        MinorImageVersion;
    WORD        MajorSubsystemVersion;
    WORD        MinorSubsystemVersion;
    DWORD       Win32VersionValue;
    DWORD       SizeOfImage;
    DWORD       SizeOfHeaders;
    DWORD       CheckSum;
    WORD        Subsystem;
    WORD        DllCharacteristics;
    ULONGLONG   SizeOfStackReserve;
    ULONGLONG   SizeOfStackCommit;
    ULONGLONG   SizeOfHeapReserve;
    ULONGLONG   SizeOfHeapCommit;
    DWORD       LoaderFlags;
    DWORD       NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64, * PIMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS64 {
    DWORD Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64, * PIMAGE_NT_HEADERS64;

typedef struct _IMAGE_SECTION_HEADER {
    BYTE    Name[8];
    union {
        DWORD   PhysicalAddress;
        DWORD   VirtualSize;
    } Misc;
    DWORD   VirtualAddress;
    DWORD   SizeOfRawData;
    DWORD   PointerToRawData;
    DWORD   PointerToRelocations;
    DWORD   PointerToLinenumbers;
    WORD    NumberOfRelocations;
    WORD    NumberOfLinenumbers;
    DWORD   Characteristics;
} IMAGE_SECTION_HEADER, * PIMAGE_SECTION_HEADER;


UINT64 ResolveEntryAddress(PDRIVER_OBJECT driver)
{
	PLDR_DATA_TABLE_ENTRY ldr = (PLDR_DATA_TABLE_ENTRY)driver->DriverSection;
	// ldr->DllBase

	unsigned char* ldr_base = (unsigned char*)ldr->DllBase;

	IMAGE_NT_HEADERS64* ntHeaders =
		(IMAGE_NT_HEADERS64*)(ldr_base +
			((IMAGE_DOS_HEADER*)ldr_base)->e_lfanew);

	return (UINT64)(ldr_base + ntHeaders->OptionalHeader.AddressOfEntryPoint);
}

void clear_pe_header(UINT64 map_driver_base)
{
	IMAGE_NT_HEADERS64* nt_headers =
		(IMAGE_NT_HEADERS64*)(map_driver_base +
			((IMAGE_DOS_HEADER*)map_driver_base)->e_lfanew);

	for (DWORD i = nt_headers->OptionalHeader.SizeOfHeaders; i--;)
		((unsigned char*)map_driver_base)[i] = 0;
}

BOOLEAN MemCopyWP(PVOID dest, PVOID src, ULONG length)
{
    PMDL mdl = IoAllocateMdl(dest, length, FALSE, FALSE, NULL);
    if (!mdl) {
        return FALSE;
    }

    MmProbeAndLockPages(mdl, KernelMode, IoModifyAccess);

    PVOID mapped = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, 0, HighPagePriority);
    if (!mapped) {
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);
        return FALSE;
    }

    memcpy(mapped, src, length);

    MmUnmapLockedPages(mapped, mdl);
    MmUnlockPages(mdl);
    IoFreeMdl(mdl);
    return TRUE;
}


#endif
/*
* Uefi end
*/

extern "C" {
    NTKERNELAPI NTSTATUS IoCreateDriver(
        PUNICODE_STRING DriverName,
        PDRIVER_INITIALIZE InitializationFunction
    );


}



namespace driver {
    namespace codes {
        constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
        constexpr ULONG read = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
        constexpr ULONG write = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
        constexpr ULONG get_module_base = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x699, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
    }

    struct Request {

        HANDLE processId;

        PVOID target;
        PVOID buffer;

        SIZE_T size;
        SIZE_T returnSize;
    };

    NTSTATUS create(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
        UNREFERENCED_PARAMETER(DeviceObject);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Irp->IoStatus.Status;
    }

    NTSTATUS deviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
        UNREFERENCED_PARAMETER(DeviceObject);

        Print("[+] Device control called\n");
        NTSTATUS status = STATUS_UNSUCCESSFUL;

        PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

        auto request = reinterpret_cast<Request*>(Irp->AssociatedIrp.SystemBuffer);

        if (!stack || !request) {
            Print("invalid stack or request\n");
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Irp->IoStatus.Status;
        }

        static PEPROCESS targetProcess = nullptr;

        const ULONG controlCode = stack->Parameters.DeviceIoControl.IoControlCode;
        switch (controlCode)
        {
        case codes::attach:
            status = PsLookupProcessByProcessId(request->processId, &targetProcess);
            break;

        case codes::read:
            if (!targetProcess) {
                Print("invalid targetProcess (codes::read)\n");
                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                Irp->IoStatus.Information = 0;
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return Irp->IoStatus.Status;
            }
            status = MmCopyVirtualMemory(
                targetProcess,
                request->target,
                PsGetCurrentProcess(),
                request->buffer,
                request->size,
                KernelMode,
                &request->returnSize
            );
            break;

        case codes::write:
            if (!targetProcess) {
                Print("invalid targetProcess (codes::write)\n");
                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                Irp->IoStatus.Information = 0;
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return Irp->IoStatus.Status;
            }
            status = MmCopyVirtualMemory(
                PsGetCurrentProcess(),
                request->buffer,
                targetProcess,
                request->target,
                request->size,
                KernelMode,
                &request->returnSize
            );
            break;





        default:
            Print("invalid control code\n");
            break;


        }

        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = sizeof(Request);

        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        return Irp->IoStatus.Status;
    }

    NTSTATUS close(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
        UNREFERENCED_PARAMETER(DeviceObject);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Irp->IoStatus.Status;
    }


}

NTSTATUS DriverMain(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    UNICODE_STRING deviceName = {};
    RtlInitUnicodeString(&deviceName, L"\\Device\\SexyDriver");

    PDEVICE_OBJECT deviceObject = nullptr;
    NTSTATUS status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );
    if (!NT_SUCCESS(status)) {
        Print("[-] IoCreateDevice failed\n");
        return status;
    }

    Print("[+] Device created\n");

    UNICODE_STRING symbolicLink = {};
    RtlInitUnicodeString(&symbolicLink, L"\\DosDevices\\SexyDriver");

    status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        Print("[-] IoCreateSymbolicLink failed\n");
        IoDeleteDevice(deviceObject);
        return status;
    }

    Print("[+] Symbolic link created\n");

    SetFlag(deviceObject->Flags, DO_BUFFERED_IO);

    DriverObject->MajorFunction[IRP_MJ_CREATE] = driver::create;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = driver::close;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver::deviceControl;

    ClearFlag(deviceObject->Flags, DO_DEVICE_INITIALIZING);

    Print("[+] Driver initialized\n");


    return STATUS_SUCCESS;
}



extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registryPath)
{

#ifdef UEFI_MAPPER
    NTSTATUS(*func)(
        _In_ struct _DRIVER_OBJECT* DriverObject,
        _In_ PUNICODE_STRING RegistryPath
        );
    *(UINT64*)&func = ResolveEntryAddress(driver);
    if (func == 0)
        *(int*)(0x10A0) = 0;

    if (!MemCopyWP((PVOID)func, (PVOID)qwerty, MAPPER_SIZE)) {
        /* fail to load fix entry, cause bluescreen */
        *(int*)(0x10A0) = 0;
    }
#endif

    Print("Hello from IOCTL wradar!\n");

    UNICODE_STRING driverName = {};
    RtlInitUnicodeString(&driverName, L"\\Driver\\SexyDriver");


    IoCreateDriver(&driverName, &DriverMain);


#ifdef UEFI_MAPPER
    return func(driver, registryPath);
#else
	return STATUS_SUCCESS;
#endif
}


