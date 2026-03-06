# EFI Chain Boot USB

A UEFI application that presents a menu of bootable USB devices and chain-loads
the selected device's default EFI bootloader (`\EFI\BOOT\BOOTX64.EFI`).

Designed to be launched as a systemd-boot entry.

## Requirements

- `gnu-efi` — EFI development library
- `make`, `gcc`, `ld`, `objcopy` — standard toolchain

On Arch Linux:

```sh
pacman -S make gcc gnu-efi
```

## Build

```sh
make
```

Produces `usb-boot.efi`.

## Install

Copy the EFI binary to your ESP and add the boot entry:

```sh
# Copy the binary
sudo install -D -m 644 usb-boot.efi /boot/EFI/usb-boot/usb-boot.efi

# Add systemd-boot entry
sudo cp usb-boot.conf /boot/loader/entries/usb-boot.conf
```

Or use the Makefile:

```sh
sudo make install DESTDIR=/efi
```

## Usage

1. Plug in a bootable USB drive
2. Reboot and select **"Boot from USB"** in the systemd-boot menu
3. Use arrow keys to select the USB device
4. Press Enter to boot, or Escape to go back

## How It Works

The program:
1. Enumerates all UEFI filesystem handles backed by removable media
2. Checks each for the presence of `\EFI\BOOT\BOOTX64.EFI`
3. Presents a menu of bootable devices
4. Chain-loads the bootloader from the selected device using `LoadImage`/`StartImage`
