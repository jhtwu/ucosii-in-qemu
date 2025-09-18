# uC/OS-II AArch64 QEMU Demo

> **中文說明與英文解說交錯排列；Chinese and English descriptions live side by side in every section.**

## 簡介 | Overview
此專案示範一個以 ARMv8-A (AArch64) 為目標的精簡版 uC/OS-II 類 RTOS，直接以裸機模式啟動在 QEMU `virt` 平台上。核心啟動後會建立數個任務，透過 virtio-console 輸出訊息，並使用 virtio-net 收送 ARP/ICMP 封包，展示排程與網路堆疊整合。
This project ports the lightweight uC/OS-II inspired RTOS to bare-metal ARMv8-A and runs it on QEMU's `virt` machine. At boot the kernel spawns a couple of demo tasks, talks over virtio-console, and exercises a virtio-net stack that answers ARP and ICMP echo requests to showcase scheduling plus networking.

## 需求 | Prerequisites
建置與執行需要下列工具（皆須支援 AArch64 目標）：
- `aarch64-elf-gcc` 或等效的裸機交叉編譯器
- `qemu-system-aarch64`

You will need the following toolchain components:
- An AArch64 bare-metal cross compiler such as `aarch64-elf-gcc`
- `qemu-system-aarch64`

若使用 Debian/Ubuntu，可透過 `sudo apt install gcc-aarch64-linux-gnu qemu-system-aarch64` 安裝，再依需求配置裸機交叉工具鏈。
On Debian/Ubuntu run `sudo apt install gcc-aarch64-linux-gnu qemu-system-aarch64` and configure your bare-metal toolchain accordingly.

## 建置流程 | Build Steps
於專案根目錄輸入：
```bash
make
```
指令會：
1. 以 AArch64 參數編譯所有 C 與組語原始碼。
2. 使用自訂 linker script 產生 `build/kernel.elf`。
3. 透過 `objcopy` 匯出可由 QEMU `-kernel` 直接載入的 `build/kernel8.img`。

From the project root simply run:
```bash
make
```
The rule compiles every C/assembly translation unit for AArch64, links them into `build/kernel.elf`, and finally emits a raw `build/kernel8.img` image suitable for `qemu-system-aarch64 -kernel`.

## 執行示範 | Run the Demo
```bash
sudo make run
```
此指令會：
1. 啟動 `qemu-system-aarch64 -M virt -cpu cortex-a53 -display none -monitor none -serial none`。
2. 將 `virtio-console` 連到目前的終端裝置，作為 RTOS 的序列輸出。
3. 透過 TAP 介面（預設 `qemu-lan`）接上 virtio-net 裝置。
4. 於主控台顯示類似以下訊息：
```
uC/OS-II arm64 demo starting...
[NetTxTask] start
[NetTxTask] tick
[NET] ARP probe sent to 192.168.1.103
...
```
按下 `Ctrl+C` 以關閉 QEMU。

The `make run` target launches QEMU in headless mode, wires the guest virtio-console to your terminal, attaches the virtio-net device to an existing TAP interface (default `qemu-lan`), and prints the demo task log shown above. Hit `Ctrl+C` when you're done.

> **建立 TAP 介面 | Creating the TAP Interface**
> ```bash
> sudo ip tuntap add dev qemu-lan mode tap
> sudo ip addr add 192.168.1.2/24 dev qemu-lan
> sudo ip link set qemu-lan up
> # 視需求將 qemu-lan 納入 bridge，或直接與 host 溝通
> ```
> 若需自訂介面名稱與 MAC，執行 `make run TAP_IFACE=<iface> NETDEV_ID=<id> NET_MAC=<mac>`。

### 手動啟動 | Manual Launch
若想保留主控台輸出或客製化參數，可參考：
```bash
sudo qemu-system-aarch64 -M virt -cpu cortex-a53 -display none \
    -kernel build/kernel8.img \
    -monitor none \
    -serial none \
    -chardev stdio,id=virtiocon0,signal=off \
    -device virtio-serial-device,id=virtio-serial0 \
    -device virtconsole,chardev=virtiocon0,id=virtio-console0,bus=virtio-serial0.0 \
    -netdev tap,id=network-lan,ifname=qemu-lan,script=no,downscript=no \
    -device virtio-net-device,netdev=network-lan,mac=02:00:00:00:00:01
```
This command mirrors the Makefile recipe and keeps full control over QEMU arguments.

## Virtio 裝置說明 | Virtio Devices
本移植版全面採用 virtio-mmio：
- `virtio-console` 取代 x86 上的 16550A，所有 `serial_*` API 會經由 transmit queue 寫到 host 端標準輸出，並在中斷或傳送前主動回收完成的描述元。
- `virtio-net` 採 legacy/mmio 介面，queue0 做 RX、queue1 做 TX；驅動於啟動時就緒 128 個描述元和 2 KiB 緩衝區，並於每次輪詢與傳送時回收及重掛。

This port targets the virtio-mmio transport used by QEMU's `virt` machine:
- `virtio-console` replaces the legacy 16550A. The serial helper APIs feed bytes into the TX virtqueue and reclaim descriptors proactively so the host sees a continuous log.
- `virtio-net` runs in legacy/mmio mode with queue 0 serving RX and queue 1 TX. The driver provisions 128 descriptors backed by 2 KiB buffers and repopulates them after each poll.

建議在 host 端同時執行 `sudo timeout 40s tcpdump -i qemu-lan -n -vv -l` 與 `sudo make run`，即可觀察 ARP/ICMP 封包的收發情況。
Run `sudo timeout 40s tcpdump -i qemu-lan -n -vv -l` alongside `sudo make run` to watch the ARP/ICMP probes emitted by the guest.

## 網路測試 | Network Test
- 核心預設 IP `192.168.1.1/24`、MAC `02:00:00:00:00:01`。
- 啟動後可由 host 或同一 bridge 上的節點進行：
  ```bash
  ping 192.168.1.1
  ```
- 客端會回覆 ARP 與 ICMP Echo；若以 `tcpdump` 監看 TAP，可同時看到請求與回應。

The guest exposes `192.168.1.1/24` (MAC `02:00:00:00:00:01`). Once booted, issue `ping 192.168.1.1` from the host or any bridged peer—the stack will answer ARP requests and ICMP echoes, which you can confirm via `tcpdump` on the TAP device.

## 原始碼架構 | Repository Layout
- `src/boot.S` – AArch64 入口，初始化堆疊、清 BSS、安裝向量表。
- `src/arch/aarch64/vectors.S` – EL1 例外向量與 IRQ 框架，負責儲存/還原 CPU 狀態。
- `src/kernel/` – 平台初始化（GIC、generic timer、序列輸出）與 demo 任務定義。
- `src/os/` – uC/OS-II 核心（TCB、排程器、時間管理）與 AArch64 版 context switch。
- `src/hw/` – 硬體抽象：GIC、generic timer、virtio console、virtio net。
- `include/` – 公用標頭檔與架構特定的 helper。
- `Makefile` – 交叉編譯與 QEMU 啟動規則。

Explanation in English:
- `src/boot.S` – AArch64 entry point that sets up stacks, zeroes `.bss`, and installs the vector base.
- `src/arch/aarch64/vectors.S` – Exception vectors and IRQ frame logic that save/restore CPU context.
- `src/kernel/` – Platform bring-up (GIC, generic timer, serial) and the demo task definitions.
- `src/os/` – The trimmed uC/OS-II core plus the AArch64-specific context switch code.
- `src/hw/` – Hardware support layers for GIC, generic timer, virtio console, and virtio net.
- `include/` – Shared headers covering the OS API and hardware helpers.
- `Makefile` – Cross-build rules and the `make run` shortcut.

## 排程與中斷 | Scheduling & Interrupts
- 透過 ARM generic timer (virtual timer) 以 100 Hz 觸發 IRQ `27`，在 ISR 中呼叫 `OSTimeTick()`。
- GICv2 distributor/cpu interface 由 `gic_init()` 啟動，只開放 timer IRQ；若有需要可額外打開 virtio 中斷。
- `OSIntEnter()` / `OSIntExit()` 管理巢狀中斷深度並在退回時觸發延遲切換。
- `OSTaskStkInit()` 建立 AArch64 相容的堆疊影像，`OSCtxSw()` / `OSStartHighRdy()` 透過 `eret` 還原，任務起始於 `OSTaskThunk()`，該函式會先開啟 IRQ 再呼叫任務本體。

The scheduler is driven by the ARM generic virtual timer firing at 100 Hz (IRQ 27). A minimal GICv2 bring-up enables the timer interrupt; deferred context switches are handled via `OSIntEnter()/OSIntExit()`. Task stacks are prepared by `OSTaskStkInit()`, restored with `OSCtxSw()`/`OSStartHighRdy()` using `eret`, and every task begins inside `OSTaskThunk()` which re-enables IRQs before invoking the user entry point.

## 已知限制 | Known Limitations
- 僅支援最多 8 個任務、基本延遲 API，未實作郵件、訊號量或記憶體管理。
- 尚未串接 MMU、快取或更進階的除錯（例如 GDB stub）。
- virtio-console 僅實作輸出；輸入緩衝目前會被丟棄。

Current caveats:
- Scheduler still caps at eight tasks and provides only basic delay primitives—no mailboxes, semaphores, or memory manager.
- MMU/cache management and advanced debugging hooks (e.g. GDB stubs) are out of scope for now.
- The virtio-console path is transmit-only; inbound characters are discarded.

## 進一步延伸 | Next Steps
- 擴充 OS 服務（郵件、訊號量、事件旗標等）。
- 加入基本網路協定（UDP/TCP 範例）或封包過濾。
- 將 virtio-console 補上輸入隊列，並串接簡易命令殼層。
- 建立自動化測試（例如以 QEMU + expect/tcpdump 驗證）確保回歸品質。

Possible follow-ups:
- Broaden the RTOS feature set with mailboxes, semaphores, or event flags.
- Layer simple UDP/TCP examples on top of the existing Ethernet/ARP/ICMP stack.
- Wire the virtio-console RX path into a tiny shell to interact with demo tasks.
- Script QEMU-based regression tests (expect, tcpdump) to catch behavioural regressions.
