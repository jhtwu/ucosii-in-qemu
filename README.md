# uC/OS-II x86 QEMU Demo

> **中文說明請見每個段落下方；English description is mirrored after each heading.**

## 簡介 | Overview
本專案展示一個可在 32-bit x86 架構上執行的精簡版 uC/OS-II 類即時作業系統，透過 GRUB 開機並於 QEMU 虛擬平台上運行。系統啟動後會建立兩個測試任務，分別每秒與每 250 毫秒印出訊息，示範多任務排程與排隊切換。
This project is a compact uC/OS-II inspired RTOS targeting 32-bit x86. It boots through GRUB, runs inside QEMU, and starts two demo tasks that print to the serial console at 1 s and 250 ms intervals to showcase multitasking and preemptive scheduling.

## 需求 | Prerequisites
在建置與執行前，請確認系統已安裝下列工具（皆需支援 32-bit 目標）：
- `gcc`（需含 `-m32` 支援）
- `nasm`
- `grub-mkrescue`
- `qemu-system-i386`

You will need the following tools (with 32-bit support) before building and running:
- `gcc` with `-m32` capabilities
- `nasm`
- `grub-mkrescue`
- `qemu-system-i386`

在 Debian/Ubuntu 上可透過 `sudo apt install gcc-multilib nasm grub-pc-bin grub-common xorriso qemu-system-i386` 安裝；其他發行版請依套件管理工具調整。
On Debian/Ubuntu you can install them with `sudo apt install gcc-multilib nasm grub-pc-bin grub-common xorriso qemu-system-i386`. Adjust accordingly for other distributions.

## 建置流程 | Build Steps
於專案根目錄執行：
```bash
make
```
此指令會：
1. 編譯 C 與組語原始碼為 32-bit 可攜系統程式。
2. 使用自訂 linker script 產出符合 Multiboot 規範的 `kernel.elf`。
3. 以 `grub-mkrescue` 建立 `build/ucosii.iso`，可直接在 QEMU 或實體機器上啟動。

Run this command from the project root:
```bash
make
```
It performs the following:
1. Compiles the C and assembly sources for a freestanding 32-bit target.
2. Links them via the custom linker script into a Multiboot-compliant `kernel.elf`.
3. Invokes `grub-mkrescue` to produce `build/ucosii.iso`, ready to boot in QEMU or on real hardware.

## 執行示範 | Run the Demo
```bash
make run
```
`make run` 會啟動 QEMU，並將序列埠輸出重導至終端機（使用 `-serial stdio` 與 `-display none`）。預期輸出如下：
```
uC/OS-II x86 demo starting...
Starting scheduler...
[Task A] tick 0
[Task B] heartbeat
...
```
按下 `Ctrl+C` 以結束 QEMU。

The `make run` target launches QEMU with the ISO, redirects the serial port to the terminal (`-serial stdio`, `-display none`), and shows output similar to the snippet above. Press `Ctrl+C` to terminate QEMU.

如需保留輸出，可改用：
```bash
qemu-system-i386 -cdrom build/ucosii.iso -serial file:serial.log -no-reboot -no-shutdown -display none
```
這會把序列埠資料寫入 `serial.log`。
To keep the trace, run QEMU manually as shown; it stores serial output in `serial.log`.

## 原始碼架構 | Repository Layout
- `src/boot.asm` – Multiboot 入口，建立堆疊並跳往核心。
- `src/kernel/` – 平台初始化（IDT、PIC、PIT、序列埠）與示範任務。
- `src/os/` – 精簡版 uC/OS-II 核心：任務控制區、排程器、上下文切換。
- `src/hw/` – x86 週邊驅動：序列埠、PIT、PIC、IDT。
- `include/` – 公用標頭檔，涵蓋核心 API、硬體介面、CPU 封裝。
- `Makefile` – 建置規則與 `make run` 目標。

Explanation in English:
- `src/boot.asm` – Multiboot entry, stack setup, transfer to `kernel_main`.
- `src/kernel/` – Platform bring-up (IDT/PIC/PIT/serial) plus demo tasks.
- `src/os/` – Lightweight uC/OS-II style core: TCBs, scheduler, context switch code.
- `src/hw/` – Minimal device drivers for serial, PIT, PIC, and IDT.
- `include/` – Shared headers for the OS API, hardware helpers, and CPU support.
- `Makefile` – Build orchestration and the `make run` shortcut.

## 排程與中斷 | Scheduling & Interrupts
- PIT 以 100 Hz 觸發 `irq0`，在 `irq0_handler_c` 中呼叫 `OSTimeTick()`。
- `OSIntEnter()`／`OSIntExit()` 保持巢狀中斷層級，必要時請求延遲切換。
- `OSTaskCreate()` 透過 `OSTaskStkInit()` 建立 i386 相容堆疊框架，並在 `OSStartHighRdy()`/`OSCtxSw()` 以 `iretd` 還原。
- 任務進入點位於 `OSTaskThunk()`，在呼叫任務前先執行 `sti` 以啟用中斷。

Scheduling & interrupt highlights:
- The PIT runs at 100 Hz, driving `irq0` and `OSTimeTick()`.
- `OSIntEnter()`/`OSIntExit()` maintain nesting depth and request deferred context switches when required.
- `OSTaskCreate()` builds i386-compatible frames via `OSTaskStkInit()`; `OSStartHighRdy()`/`OSCtxSw()` restore them with `iretd`.
- `OSTaskThunk()` enables interrupts with `sti` before invoking the task entry point.

## 已知限制 | Known Limitations
- 僅實作最基本的任務管理與延遲 API，未含郵件、訊號量與記憶體管理。
- 未支援 SMP 與高階除錯（如 GDB stub）。
- 目前採用簡單的 ready-bit scheduler，僅適用於最多 8 個任務。

Current caveats:
- Only basic task management and delay APIs are provided; no mailboxes, semaphores, or memory management.
- SMP and advanced debugging (e.g., GDB stub) are not implemented.
- Scheduler uses a simple ready bitmask with a maximum of eight tasks.

## 上傳至 GitHub | Publish to GitHub
由於執行環境限制，我無法直接存取網際網路替您 push。可依下列步驟自行上傳：
1. 在 GitHub 建立新倉庫（例如 `ucosii-x86-demo`）。
2. 於專案資料夾初始化 Git 並提交：
   ```bash
   git init
   git add .
   git commit -m "Add uC/OS-II x86 QEMU demo"
   ```
3. 加入遠端並推送：
   ```bash
   git remote add origin https://github.com/<your-account>/<repo>.git
   git branch -M main
   git push -u origin main
   ```
若需使用 SSH 或 Access Token，請依個人環境調整遠端 URL。

Due to the sandbox, I cannot push on your behalf. Follow the steps above to create a Git repository locally and push it to GitHub using either HTTPS or SSH credentials.

## 進一步延伸 | Next Steps
- 增加更多 OS 特性（郵件、訊號量、事件旗標）。
- 將序列輸出擴充為簡單命令介面，示範任務同步。
- 建立自動化測試或 QEMU snapshot，確保行為一致。

Possible follow-ups:
- Add richer kernel services (mailboxes, semaphores, event flags).
- Extend the serial console into a mini monitor to showcase IPC primitives.
- Script QEMU-based regression tests to verify scheduler behavior.
