# shkodya-os
Простая Хобби-OC на C и Ассемблере с нуля / A simple hobby x86 OS written from scratch in C and Assembly

Инструкция по запуску / How to Run
Linux (QEMU)
Установите QEMU и запустите команду / Install QEMU and run the command:
sudo apt install qemu-system-x86
qemu-system-x86_64 -m 512 -vga std -cdrom myos64.iso -boot d

Windows (QEMU / PowerShell)
Установите QEMU через winget и запустите команду / Install QEMU via winget and run the command:
winget install --id SoftwareFreedomConservancy.QEMU -e
qemu-system-x86_64.exe -m 512 -cdrom myos64.iso -boot d

VirtualBox
 * Создайте виртуальную машину: тип «Other», версия «Other/Unknown (64-bit)», 512 МБ ОЗУ / Create a VM: type "Other", version "Other/Unknown (64-bit)", 512 MB RAM.
 * Материнская плата: выключите EFI (только BIOS), манипулятор — PS/2 мышь / Motherboard: disable EFI (BIOS only), pointing device — PS/2 Mouse.
 * Дисплей: графический контроллер VBoxVGA, видеопамять 32 МБ, 3D-ускорение выключено / Display: VBoxVGA graphics controller, 32 MB VRAM, 3D acceleration disabled.
 * Носители: подключите myos64.iso к IDE Secondary Master / Storage: attach myos64.iso to IDE Secondary Master.
Управление / Hotkeys
 * Выход из QEMU / Exit QEMU: Ctrl+Alt+2 -> quit
 * Освободить курсор мыши / Release mouse cursor: Ctrl+Alt+G (QEMU) / Right Ctrl (VirtualBox)
/
## How to Run

### Linux (QEMU)
Install QEMU and run:
```bash
sudo apt install qemu-system-x86
qemu-system-x86_64 -m 512 -vga std -cdrom myos64.iso -boot d

Windows (QEMU / PowerShell)
Install QEMU via winget and run:
winget install --id SoftwareFreedomConservancy.QEMU -e
qemu-system-x86_64.exe -m 512 -cdrom myos64.iso -boot d

VirtualBox
 * VM Setup: Type "Other", version "Other/Unknown (64-bit)", 512 MB RAM.
 * Motherboard: Disable EFI (BIOS only), pointing device: PS/2 Mouse.
 * Display: Graphics controller VBoxVGA, 32 MB VRAM, 3D acceleration disabled.
 * Storage: Attach myos64.iso to IDE Secondary Master.
Hotkeys
 * Exit QEMU: Ctrl+Alt+2 -> type quit
 * Release mouse cursor: Ctrl+Alt+G (QEMU) / Right Ctrl (VirtualBox)
## Скриншоты системы / System Screenshots

<img width="1024" height="768" alt="screenshot-window" src="https://github.com/user-attachments/assets/c058be6f-6080-4edd-8a71-bf83a8c718b7" />
<img width="1024" height="768" alt="screenshot-desktop" src="https://github.com/user-attachments/assets/0cf6a949-9ca7-403c-aa64-5784a4f56d7d" />
<img width="1024" height="768" alt="screenshot-aichat" src="https://github.com/user-attachments/assets/d731996d-92ae-4323-b4d2-5c394778662f" />

