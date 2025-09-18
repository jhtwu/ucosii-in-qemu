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
sudo make run
```
`make run` 會啟動 QEMU、接上既有的 TAP 介面（預設 `qemu-lan`）、並將序列埠輸出重導至終端機。啟動後會看到 demo 任務與網路任務定期送出 ARP probe：
```
uC/OS-II x86 demo starting...
[NetTxTask] start
[NetTxTask] tick
[NET] Sending ARP probe...
[NET] ARP probe sent to 192.168.1.103
...
```
按下 `Ctrl+C` 以結束 QEMU。

The `make run` target now expects an existing TAP device (default `qemu-lan`), wires the emulated `virtio-net` NIC into it, and redirects serial output to the terminal. Press `Ctrl+C` when you want to stop QEMU.

> **建立 TAP 介面 | Creating the TAP Interface**
> ```bash
> sudo ip tuntap add dev qemu-lan mode tap
> sudo ip addr add 192.168.1.2/24 dev qemu-lan
> sudo ip link set qemu-lan up
> # 可視需求把 qemu-lan 納入 bridge，或直接與 host 溝通
> ```
> 若要使用不同的介面名稱或 MAC 位址，可在 `make run` 時帶入 `TAP_IFACE=<iface>`、`NETDEV_ID=<id>` 與 `NET_MAC=<mac>`。

To keep the serial log while enabling networking, you can launch QEMU manually:
```bash
sudo qemu-system-i386 -cdrom build/ucosii.iso -serial file:serial.log -no-reboot -no-shutdown -display none \
    -netdev tap,id=network-lan,ifname=qemu-lan,script=no,downscript=no \
    -device virtio-net-pci,disable-modern=on,netdev=network-lan,mac=02:00:00:00:00:01
```
This writes the serial output to `serial.log`.

## 網路測試 | Network Test
- uC/OS-II demo 內建靜態 IP `192.168.1.1/24`，MAC 預設 `02:00:00:00:00:01`。（目前驅動為 `virtio-net`，請在 QEMU 指令中維持相同網卡類型。）
- 啟動後，從 host 端或 bridge 上的其他節點即可測試：
  ```bash
  ping 192.168.1.1
  ```
- 系統會處理 ARP 請求並回覆 ICMP Echo，連線成功會看到一般 ping 回應。

The demo exposes a static IP `192.168.1.1/24` (default MAC `02:00:00:00:00:01`). The in-guest driver targets the emulated `virtio-net`, so keep the same device type when launching QEMU. Once the VM is up, issue `ping 192.168.1.1`; the stack replies to ARP and ICMP Echo packets, so you should see normal ping responses.

## Virtio-Net 驅動說明 | Virtio-Net Driver
本版本將原本的 e1000 驅動改寫為 legacy 模式的 virtio-net，透過 PCI I/O 空間完成特徵協商，配置 RX(隊列0)/TX(隊列1) 環形佇列並預載緩衝區。驅動維持靜態 MAC (`02:00:00:00:00:01`)，並在每次輪詢時回收完成的傳送描述元、重新掛載接收描述元。
This release replaces the former e1000 driver with a legacy-mode virtio-net implementation. The driver negotiates features via PCI I/O registers, wires queue 0/1 as RX/TX rings, pre-populates buffers, and recycles descriptors during polling.

為了保持示範程式簡潔，驅動採用固定大小的 2 KiB 緩衝區與 256 個描述元上限，足以支援 ARP 與 ICMP Echo 測試。若需擴充，可調整 `VIRTQ_MAX` 或改用 DMA 友善的記憶體配置器。
To keep the demo compact the driver ships with 2 KiB buffers and up to 256 descriptors, which is plenty for ARP/ICMP echo. For advanced workloads, grow `VIRTQ_MAX` or switch to an allocator designed for DMA.

實測建議同時啟動 `sudo timeout 40s tcpdump -i qemu-lan -n -vv -l` 與 `sudo make run`，即可在主機上觀察 virtio 送出的 ARP/ICMP 封包。若使用不同的 TAP 名稱，請改以 `make run TAP_IFACE=<iface>`。
During verification, run `sudo timeout 40s tcpdump -i qemu-lan -n -vv -l` alongside `sudo make run` to observe ARP/ICMP frames emitted through virtio on the host. Use `make run TAP_IFACE=<iface>` when testing with a custom TAP device name.

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
- `net_init()` 啟動 `virtio-net` 網卡，`net_poll()` 周期性輪詢 ARP/ICMP，提供 192.168.1.1 的 ping 回覆。

Scheduling & interrupt highlights:
- The PIT runs at 100 Hz, driving `irq0` and `OSTimeTick()`.
- `OSIntEnter()`/`OSIntExit()` maintain nesting depth and request deferred context switches when required.
- `OSTaskCreate()` builds i386-compatible frames via `OSTaskStkInit()`; `OSStartHighRdy()`/`OSCtxSw()` restore them with `iretd`.
- `OSTaskThunk()` enables interrupts with `sti` before invoking the task entry point.
- `net_init()` sets up the emulated `virtio-net` NIC, and `net_poll()` services ARP requests plus ICMP echo (ping) for 192.168.1.1.

## 已知限制 | Known Limitations
- 僅實作最基本的任務管理與延遲 API，未含郵件、訊號量與記憶體管理。
- 未支援 SMP 與高階除錯（如 GDB stub）。
- 目前採用簡單的 ready-bit scheduler，僅適用於最多 8 個任務。

Current caveats:
- Only basic task management and delay APIs are provided; no mailboxes, semaphores, or memory management.
- SMP and advanced debugging (e.g., GDB stub) are not implemented.
- Scheduler uses a simple ready bitmask with a maximum of eight tasks.

## 進一步延伸 | Next Steps
- 增加更多 OS 特性（郵件、訊號量、事件旗標）。
- 將序列輸出擴充為簡單命令介面，示範任務同步。
- 建立自動化測試或 QEMU snapshot，確保行為一致。

Possible follow-ups:
- Add richer kernel services (mailboxes, semaphores, event flags).
- Extend the serial console into a mini monitor to showcase IPC primitives.
- Script QEMU-based regression tests to verify scheduler behavior.
