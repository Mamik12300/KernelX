# KernelX
KernelX OS - experimental operating system built from scratch.
## 📸 Screenshots

### Dark Theme
![KernelX Dark](screenshots/KernelX-dark.jpg)

### Light Theme
![KernelX Light](screenshots/KernelX-light.jpg)
# 🧠 KernelX OS

KernelX OS is an experimental operating system built from scratch by  
Mamik Kosyntsev (12–13 y/o developer).

This project focuses on simplicity, control, and building a system without legacy complexity.

---

## 🚀 Features

- Custom kernel (Multiboot)
- Graphical desktop (KernelX Desktop)
- Window system
- Taskbar + Start Menu
- File Explorer (KField concept)
- Terminal (kxsh)
- Built-in commands (help, fastfetch, etc.)
- Keyboard + PS/2 mouse support
- Light / Dark themes
- Window dragging

---

## 🧩 Architecture
KernelX = Custom Kernel
GUI Desktop
Terminal
KField FS (concept)

---

## ⚡ Why KernelX

- Minimal design (no unnecessary layers)
- Full control over the system
- Custom-built GUI (not KDE/GNOME)
- Clean architecture
- Learning-focused OS development

---

## 📦 Download

Source code is currently provided as:
KernelX-source-v0.8.1.zip

ISO builds will be available in future releases.

---

## ▶️ Run (QEMU)

After building:

`bash
make
make run
Or manually:
Bash
qemu-system-x86_64 -cdrom KernelX.iso
🧠 Commands
Inside terminal:

help
fastfetch
files
settings
terminal
clear
poweroff
⚠️ Disclaimer
This is an experimental OS project.
Do NOT use on real hardware.
👨‍💻 Developer
Mamik Kosyntsev
KernelX OS Project (2026)
📜 License
This project uses a custom KernelX License:
Attribution required
Open-source required
Non-commercial use only
See LICENSE file for details.
🔮 Future Plans
Real KField filesystem
ELF loader (run programs)
Drivers
Networking
Linux compatibility layer
More advanced GUI
🏁 Goal
KernelX is an attempt to rethink how operating systems should be built:
Less complexity. More control.
