ARCH		= x86_64
TARGET		= usb-boot.efi

EFIINC		= /usr/include/efi
EFIINCS		= -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
EFILIB		= /usr/lib
EFI_CRT_OBJS	= $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS		= $(EFILIB)/elf_$(ARCH)_efi.lds

CFLAGS		= $(EFIINCS) \
		  -ffreestanding \
		  -fno-stack-protector \
		  -fno-stack-check \
		  -fpic \
		  -fshort-wchar \
		  -mno-red-zone \
		  -maccumulate-outgoing-args \
		  -Wall \
		  -DEFI_FUNCTION_WRAPPER \
		  -DGNU_EFI_USE_MS_ABI

LDFLAGS		= -nostdlib \
		  -znocombreloc \
		  -T $(EFI_LDS) \
		  -shared \
		  -Bsymbolic \
		  -L $(EFILIB) \
		  $(EFI_CRT_OBJS)

LIBS		= -lefi -lgnuefi

OBJCOPY_SECTIONS = -j .text -j .sdata -j .data -j .dynamic \
		   -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* \
		   -j .reloc

OBJCOPY_TARGET	= pei-x86-64

all: $(TARGET)

main.o: main.c
	gcc $(CFLAGS) -c -o $@ $<

main.so: main.o
	ld $(LDFLAGS) $^ -o $@ $(LIBS)

$(TARGET): main.so
	objcopy $(OBJCOPY_SECTIONS) --output-target $(OBJCOPY_TARGET) --subsystem 10 $< $@

clean:
	rm -f main.o main.so $(TARGET)

install: $(TARGET)
	@if [ -z "$(DESTDIR)" ]; then \
		echo "Usage: make install DESTDIR=/boot"; \
		exit 1; \
	fi
	install -D -m 644 $(TARGET) $(DESTDIR)/EFI/usb-boot/$(TARGET)
	@echo "Installed to $(DESTDIR)/EFI/usb-boot/$(TARGET)"

.PHONY: all clean install
