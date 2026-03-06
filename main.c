#include <efi.h>
#include <efilib.h>

#define MAX_DEVICES 32
#define BOOTLOADER_PATH L"\\EFI\\BOOT\\BOOTX64.EFI"

typedef struct {
    EFI_HANDLE handle;
    EFI_DEVICE_PATH *device_path;
    CHAR16 *description;
} BootableDevice;

static CHAR16 *
DevicePathToDescription(EFI_DEVICE_PATH *dp)
{
    /* Use the library's device path to text conversion */
    return DevicePathToStr(dp);
}

static UINTN
FindFilesystemDevices(BootableDevice *devices, UINTN max_devices)
{
    EFI_STATUS status;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    UINTN count = 0;

    /*
     * Find all handles with SimpleFileSystem - then check which ones
     * sit on removable media by walking up to their BlockIO.
     */
    status = LibLocateHandle(ByProtocol, &FileSystemProtocol,
                             NULL, &handle_count, &handles);
    if (EFI_ERROR(status) || handle_count == 0)
        return 0;

    handle_count /= sizeof(EFI_HANDLE);

    for (UINTN i = 0; i < handle_count && count < max_devices; i++) {
        EFI_BLOCK_IO *block_io;

        status = uefi_call_wrapper(BS->HandleProtocol, 3,
                                   handles[i],
                                   &BlockIoProtocol,
                                   (VOID **)&block_io);
        if (EFI_ERROR(status))
            continue;

        if (!block_io->Media->RemovableMedia)
            continue;
        if (!block_io->Media->MediaPresent)
            continue;

        /* Verify the bootloader file exists on this device */
        EFI_FILE_IO_INTERFACE *fs;
        status = uefi_call_wrapper(BS->HandleProtocol, 3,
                                   handles[i],
                                   &FileSystemProtocol,
                                   (VOID **)&fs);
        if (EFI_ERROR(status))
            continue;

        EFI_FILE_HANDLE root;
        status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
        if (EFI_ERROR(status))
            continue;

        EFI_FILE_HANDLE file;
        status = uefi_call_wrapper(root->Open, 5, root, &file,
                                   BOOTLOADER_PATH, EFI_FILE_MODE_READ, 0);
        if (!EFI_ERROR(status)) {
            uefi_call_wrapper(file->Close, 1, file);

            EFI_DEVICE_PATH *dp = DevicePathFromHandle(handles[i]);
            devices[count].handle = handles[i];
            devices[count].device_path = dp;
            devices[count].description = DevicePathToDescription(dp);
            count++;
        }

        uefi_call_wrapper(root->Close, 1, root);
    }

    if (handles)
        FreePool(handles);

    return count;
}

static UINTN
ShowMenu(BootableDevice *devices, UINTN count)
{
    UINTN selected = 0;
    EFI_INPUT_KEY key;

    while (TRUE) {
        uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
        Print(L"=== Boot from USB ===\n\n");

        if (count == 0) {
            Print(L"No bootable USB devices found.\n");
            Print(L"Press any key to exit.\n");
            WaitForSingleEvent(ST->ConIn->WaitForKey, 0);
            return (UINTN)-1;
        }

        Print(L"Select a device to boot from:\n\n");

        for (UINTN i = 0; i < count; i++) {
            if (i == selected)
                Print(L"  > [%d] %s\n", i + 1, devices[i].description);
            else
                Print(L"    [%d] %s\n", i + 1, devices[i].description);
        }

        Print(L"\nUse Up/Down arrows to select, Enter to boot, Escape to exit.\n");

        uefi_call_wrapper(BS->WaitForEvent, 3, 1,
                          &ST->ConIn->WaitForKey, &selected);
        uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);

        if (key.ScanCode == SCAN_UP) {
            if (selected > 0)
                selected--;
        } else if (key.ScanCode == SCAN_DOWN) {
            if (selected < count - 1)
                selected++;
        } else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            return selected;
        } else if (key.ScanCode == SCAN_ESC) {
            return (UINTN)-1;
        }
    }
}

static EFI_DEVICE_PATH *
FileDevicePath2(EFI_HANDLE device, CHAR16 *file_name)
{
    return FileDevicePath(device, file_name);
}

static EFI_STATUS
ChainLoadDevice(EFI_HANDLE image_handle, BootableDevice *device)
{
    EFI_STATUS status;
    EFI_DEVICE_PATH *boot_path;
    EFI_HANDLE new_image;

    Print(L"\nBooting from: %s\n", device->description);
    Print(L"Loading: %s\n", BOOTLOADER_PATH);

    boot_path = FileDevicePath2(device->handle, BOOTLOADER_PATH);
    if (!boot_path) {
        Print(L"Error: Failed to create device path\n");
        return EFI_NOT_FOUND;
    }

    status = uefi_call_wrapper(BS->LoadImage, 6,
                               FALSE,
                               image_handle,
                               boot_path,
                               NULL, 0,
                               &new_image);
    FreePool(boot_path);

    if (EFI_ERROR(status)) {
        Print(L"Error: LoadImage failed: %r\n", status);
        return status;
    }

    status = uefi_call_wrapper(BS->StartImage, 3,
                               new_image, NULL, NULL);
    if (EFI_ERROR(status)) {
        Print(L"Error: StartImage failed: %r\n", status);
    }

    return status;
}

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    BootableDevice devices[MAX_DEVICES];
    UINTN device_count;
    UINTN selection;
    EFI_STATUS status;

    InitializeLib(image_handle, system_table);

    Print(L"\nUSB Chain Boot Initiated\n");
    device_count = FindFilesystemDevices(devices, MAX_DEVICES);

    selection = ShowMenu(devices, device_count);

    if (selection == (UINTN)-1) {
        Print(L"\nNo device selected. Returning to boot manager...\n");
        uefi_call_wrapper(BS->Stall, 1, 2000000); /* 2 seconds */
        return EFI_SUCCESS;
    }

    status = ChainLoadDevice(image_handle, &devices[selection]);

    if (EFI_ERROR(status)) {
        Print(L"\nBoot failed. Press any key to exit.\n");
        WaitForSingleEvent(ST->ConIn->WaitForKey, 0);
        EFI_INPUT_KEY key;
        uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
    }

    return status;
}
