# afxdp-userapp

Minimal AF_XDP-based userspace datapath.

## Build & Run
```bash
make clean
make
sudo ./afxdp-userapp

## 設定
### NICの設定（AF_XDP Zero-Copy 用）
```bash
sudo ethtool -K enp4s0f1 gro off lro off tso off gso off
sudo ethtool -L enp4s0f1 combined 1

### XDPプログラムのロードとattach
#### XDPプログラムのロードとpin
```bash
sudo bpftool prog load xdp/xdp_redirect_kern.o /sys/fs/bpf/xdp_prog

#### NICへattach
```bash
sudo ip link set dev enp4s0f1 xdp off
sudo ip link set dev enp4s0f1 xdpdrv pinned /sys/fs/bpf/xdp_prog

### xskmapのpin
```bash
sudo bpftool map pin name xsks_map /sys/fs/bpf/xsks_map

### IPアドレス設定
```bash
sudo ip addr add 192.168.0.54/24 dev enp4s0f1
sudo ip link set dev enp4s0f1 up


