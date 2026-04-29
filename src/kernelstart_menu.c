#include "menu.h"
#include "bootinfo.h"
#include "elf.h"

EFI_GUID FileSystemGuid  = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID LoadedImageGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID FileInfoGuid    = EFI_FILE_INFO_ID;

static BOOLEAN GuidEqual(const EFI_GUID *a, const EFI_GUID *b)
{
    const UINT8 *pa = (const UINT8 *)a;
    const UINT8 *pb = (const UINT8 *)b;
    for (UINTN i = 0; i < 16; i++)
        if (pa[i] != pb[i]) return FALSE;
    return TRUE;
}

EFI_STATUS KernelStart(VOID)
{
    // Open Loaded Image protocol to get the device handle
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    Status = BS->OpenProtocol(
        Image,
        &LoadedImageGuid,
        (VOID **)&LoadedImage,
        Image,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if (EFI_ERROR(Status)) {
        Printf(u"LoadedImage open failed: 0x%x\r\n", Status);
        return Status;
    }

    // Open Simple File System protocol on the boot device
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
    Status = BS->OpenProtocol(
        LoadedImage->DeviceHandle,
        &FileSystemGuid,
        (VOID **)&FileSystem,
        Image,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if (EFI_ERROR(Status)) {
        Printf(u"FileSystem open failed: 0x%x\r\n", Status);
        BS->CloseProtocol(Image, &LoadedImageGuid, Image, NULL);
        return Status;
    }

    // Open the root volume
    EFI_FILE_PROTOCOL *RootDir = NULL;
    Status = FileSystem->OpenVolume(FileSystem, &RootDir);
    if (EFI_ERROR(Status)) {
        Printf(u"OpenVolume failed: 0x%x\r\n", Status);
        BS->CloseProtocol(LoadedImage->DeviceHandle, &FileSystemGuid, Image, NULL);
        BS->CloseProtocol(Image, &LoadedImageGuid, Image, NULL);
        return Status;
    }

    // Open kernel.elf from the root of the boot partition
    EFI_FILE_PROTOCOL *KernelFile = NULL;
    Status = RootDir->Open(RootDir, &KernelFile, u"kernel.elf", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Printf(u"kernel.elf not found: 0x%x\r\n", Status);
        RootDir->Close(RootDir);
        BS->CloseProtocol(LoadedImage->DeviceHandle, &FileSystemGuid, Image, NULL);
        BS->CloseProtocol(Image, &LoadedImageGuid, Image, NULL);
        return Status;
    }

    // Get the kernel file size via GetInfo
    UINTN KernelInfoBufSize = sizeof(EFI_FILE_INFO) + 256 * sizeof(CHAR16);
    EFI_FILE_INFO *KernelInfo = NULL;
    Status = BS->AllocatePool(EfiLoaderData, KernelInfoBufSize, (VOID **)&KernelInfo);
    if (EFI_ERROR(Status)) {
        Printf(u"AllocatePool(FileInfo) failed: 0x%x\r\n", Status);
        KernelFile->Close(KernelFile);
        RootDir->Close(RootDir);
        BS->CloseProtocol(LoadedImage->DeviceHandle, &FileSystemGuid, Image, NULL);
        BS->CloseProtocol(Image, &LoadedImageGuid, Image, NULL);
        return Status;
    }
    Status = KernelFile->GetInfo(KernelFile, &FileInfoGuid, &KernelInfoBufSize, KernelInfo);
    if (EFI_ERROR(Status)) {
        Printf(u"GetInfo failed: 0x%x\r\n", Status);
        BS->FreePool(KernelInfo);
        KernelFile->Close(KernelFile);
        RootDir->Close(RootDir);
        BS->CloseProtocol(LoadedImage->DeviceHandle, &FileSystemGuid, Image, NULL);
        BS->CloseProtocol(Image, &LoadedImageGuid, Image, NULL);
        return Status;
    }
    UINT64 KernelFileSize = KernelInfo->FileSize;
    BS->FreePool(KernelInfo);

    // Read the entire kernel file into a pool buffer
    UINT8 *KernelBuf = NULL;
    Status = BS->AllocatePool(EfiLoaderData, KernelFileSize, (VOID **)&KernelBuf);
    if (EFI_ERROR(Status)) {
        Printf(u"AllocatePool(kernel) failed: 0x%x\r\n", Status);
        KernelFile->Close(KernelFile);
        RootDir->Close(RootDir);
        BS->CloseProtocol(LoadedImage->DeviceHandle, &FileSystemGuid, Image, NULL);
        BS->CloseProtocol(Image, &LoadedImageGuid, Image, NULL);
        return Status;
    }
    UINTN ReadSize = KernelFileSize;
    Status = KernelFile->Read(KernelFile, &ReadSize, KernelBuf);
    KernelFile->Close(KernelFile);
    RootDir->Close(RootDir);
    BS->CloseProtocol(LoadedImage->DeviceHandle, &FileSystemGuid, Image, NULL);
    BS->CloseProtocol(Image, &LoadedImageGuid, Image, NULL);
    if (EFI_ERROR(Status)) {
        Printf(u"Read kernel failed: 0x%x\r\n", Status);
        BS->FreePool(KernelBuf);
        return Status;
    }

    // Validate ELF64 magic, class, and machine
    Elf64_Ehdr *Ehdr = (Elf64_Ehdr *)KernelBuf;
    if (Ehdr->e_ident[0] != ELFMAG0 || Ehdr->e_ident[1] != ELFMAG1 ||
        Ehdr->e_ident[2] != ELFMAG2 || Ehdr->e_ident[3] != ELFMAG3) {
        Printf(u"Invalid ELF magic\r\n");
        BS->FreePool(KernelBuf);
        return EFI_LOAD_ERROR;
    }
    if (Ehdr->e_ident[EI_CLASS] != ELFCLASS64 || Ehdr->e_machine != EM_X86_64) {
        Printf(u"Not a 64-bit x86 ELF\r\n");
        BS->FreePool(KernelBuf);
        return EFI_LOAD_ERROR;
    }

    Printf(u"ELF64 kernel, entry=0x%x, %u phdrs\r\n",
           (UINT32)Ehdr->e_entry, (UINT32)Ehdr->e_phnum);

    // Load each PT_LOAD segment into memory at its physical address
    for (UINT16 i = 0; i < Ehdr->e_phnum; i++) {
        Elf64_Phdr *Phdr = (Elf64_Phdr *)(KernelBuf + Ehdr->e_phoff +
                                           (UINT64)i * Ehdr->e_phentsize);
        if (Phdr->p_type != PT_LOAD) continue;

        // Get number of pages (adding 0xFFFu rounds up, left shift 12 is division by 2^12 or 4096)
        UINTN Pages = (Phdr->p_memsz + 0xFFFu) >> 12;
        EFI_PHYSICAL_ADDRESS PhysAddr = Phdr->p_paddr;
        Status = BS->AllocatePages(AllocateAddress, EfiLoaderData, Pages, &PhysAddr);
        if (EFI_ERROR(Status)) {
            Printf(u"AllocatePages at 0x%x failed: 0x%x\r\n",
                   (UINT32)Phdr->p_paddr, Status);
            BS->FreePool(KernelBuf);
            return Status;
        }
        // Copy file data from kernel buffer to its physical address
        BS->CopyMem((VOID *)Phdr->p_paddr, KernelBuf + Phdr->p_offset, Phdr->p_filesz);
        // Zero out BSS (the part of the segment that's in memory but not in the file)
        BS->SetMem((VOID *)(Phdr->p_paddr + Phdr->p_filesz), Phdr->p_memsz - Phdr->p_filesz, 0);
    }

    UINT64 KernelEntry = Ehdr->e_entry;
    BS->FreePool(KernelBuf);

    // Capture framebuffer info before ExitBootServices invalidates boot services
    UINT64 FbBase   = GOP->Mode->FrameBufferBase;
    UINT32 FbWidth  = GOP->Mode->Info->HorizontalResolution;
    UINT32 FbHeight = GOP->Mode->Info->VerticalResolution;
    UINT32 FbPitch  = GOP->Mode->Info->PixelsPerScanLine * 4;
    UINT32 FbFormat = (UINT32)GOP->Mode->Info->PixelFormat;

    // Search configuration table for the ACPI 2.0 RSDP
    EFI_GUID Acpi20Guid = EFI_ACPI_20_TABLE_GUID;
    UINT64 AcpiRsdp = 0;
    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
        if (GuidEqual(&ST->ConfigurationTable[i].VendorGuid, &Acpi20Guid)) {
            AcpiRsdp = (UINT64)ST->ConfigurationTable[i].VendorTable;
            break;
        }
    }

    // Fetch the memory map; allocate with extra slack so the AllocatePool call
    // itself doesn't stale the MapKey we'll pass to ExitBootServices
    UINTN MemMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemMap = NULL;
    UINTN MapKey = 0;
    UINTN DescSize = 0;
    UINT32 DescVer = 0;

    BS->GetMemoryMap(&MemMapSize, NULL, &MapKey, &DescSize, &DescVer);
    MemMapSize += DescSize * 8;
    Status = BS->AllocatePool(EfiLoaderData, MemMapSize, (VOID **)&MemMap);
    if (EFI_ERROR(Status)) {
        Printf(u"AllocatePool(MemMap) failed: 0x%x\r\n", Status);
        return Status;
    }
    Status = BS->GetMemoryMap(&MemMapSize, MemMap, &MapKey, &DescSize, &DescVer);
    if (EFI_ERROR(Status)) {
        Printf(u"GetMemoryMap failed: 0x%x\r\n", Status);
        return Status;
    }

    // Exit boot services; retry once with a fresh MapKey if stale
    Status = BS->ExitBootServices(Image, MapKey);
    if (Status == EFI_INVALID_PARAMETER) {
        Status = BS->GetMemoryMap(&MemMapSize, MemMap, &MapKey, &DescSize, &DescVer);
        if (!EFI_ERROR(Status))
            Status = BS->ExitBootServices(Image, MapKey);
    }
    if (EFI_ERROR(Status)) {
        // Boot services may be in a partial state; halt unconditionally
        while (1) __asm__ volatile("hlt");
    }

    // Boot services are now gone.  Build BootInfo and jump to kernel entry.
    BootInfo Info;
    Info.FramebufferBase   = FbBase;
    Info.FramebufferWidth  = FbWidth;
    Info.FramebufferHeight = FbHeight;
    Info.FramebufferPitch  = FbPitch;
    Info.FramebufferFormat = FbFormat;
    Info.MemoryMapBase     = (UINT64)MemMap;
    Info.MemoryMapSize     = MemMapSize;
    Info.MemoryMapDescSize = DescSize;
    Info.MemoryMapDescVer  = DescVer;
    Info._reserved         = 0;
    Info.AcpiRsdp          = AcpiRsdp;

    // Use EFIAPI (MS ABI) so the kernel entry point receives Info in RCX,
    // matching the __attribute__((ms_abi)) declaration in kmain.c
    typedef void (EFIAPI *KernelEntryFn)(BootInfo *);
    ((KernelEntryFn)KernelEntry)(&Info);

    while (1) __asm__ volatile("hlt");
    return EFI_SUCCESS;
}

EFI_MENU_STATE KernelStartMenuProcessInput(EFI_MENU_PAGE *Base, EFI_INPUT_KEY *Key)
{
    (VOID)Base;
    if (Key->ScanCode == ScanCodeEscape)
        return EfiMainMenuState;
    return EfiKernelStartState;
}

VOID KernelStartMenuUpdate(EFI_MENU_PAGE *Base)
{
    if (!Base->RedrawNeeded) return;
    Base->RedrawNeeded = FALSE;

    cOut->ClearScreen(cOut);
    Printf(u"Launching kernel...\r\n");

    Status = KernelStart();

    // Only reached on failure
    Printf(u"Kernel launch failed: 0x%x\r\nPress ESC to go back.\r\n", Status);
}

EFI_MENU_PAGE *KernelStartMenu(VOID)
{
    EFI_MENU_PAGE *KernelStartMenuPtr;
    BS->AllocatePool(EfiLoaderData, sizeof(EFI_MENU_PAGE), (VOID **)&KernelStartMenuPtr);
    *KernelStartMenuPtr = DefaultPage;
    KernelStartMenuPtr->ProcessInput = KernelStartMenuProcessInput;
    KernelStartMenuPtr->Update = KernelStartMenuUpdate;
    return KernelStartMenuPtr;
}
