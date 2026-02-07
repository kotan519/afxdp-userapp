# afxdp-userapp
AF_XDPを用いたユーザ空間ネットワークアプリケーションです．
RX/TX/Fill/Completion Queue/need_wakeupをユーザ空間で直接扱う最小構成のdatapathを実装しています．

## 概要
本アプリケーションは，XDPによりNICからユーザ空間へパケットをリダイレクトし，AF_XDPのZero-Copy機構を用いてRX→TX転送を行います．
TX完了後はCompletion Queueを回収し，UMEMフレームを再利用することで，ゼロコピーなデータパスを維持します．

## ビルド & 実行
```bash
make clean
make
sudo ./afxdp-userapp
```
## 設定
### NICの設定（AF_XDP Zero-Copy 用）
```bash
sudo ethtool -K enp4s0f1 gro off lro off tso off gso off
sudo ethtool -L enp4s0f1 combined 1
```

### XDPプログラムのロードとattach
#### XDPプログラムのロードとpin
```bash
sudo bpftool prog load xdp/xdp_redirect_kern.o /sys/fs/bpf/xdp_prog
```
#### NICへattach
```bash
sudo ip link set dev enp4s0f1 xdp off
sudo ip link set dev enp4s0f1 xdpdrv pinned /sys/fs/bpf/xdp_prog
```
### xskmapのpin
```bash
sudo bpftool map pin name xsks_map /sys/fs/bpf/xsks_map
```
### IPアドレス設定
```bash
sudo ip addr add 192.168.0.54/24 dev enp4s0f1
sudo ip link set dev enp4s0f1 up
```

## レイテンシ評価（ping RTT）

AF_XDPを用いたユーザ空間処理と，Linuxの通常のネットワークスタックを用いた場合で，ICMP pingによる往復遅延（RTT）を比較した．

### 測定条件
- 同一ホスト・同一NIC
- ARPは事前に静的エントリとして設定
- ICMP Echo(ping -c 10)
- AF_XDPはZero-Copy モード
- 割り込みではなくpoll ベースで処理

### 測定結果

| 処理方式 | RTT min [ms] | RTT avg [ms] | RTT max [ms] | mdev [ms] |
|---------|--------------|--------------|--------------|-----------|
| AF_XDP | 0.134 | 0.178 | 0.198 | 0.017 |
| Linux stack | 0.251 | 0.264 | 0.319 | 0.019 |

### 考察
AF_XDPを用いた場合，平均RTTはLinuxの通常スタックに比べて約 **30% 低減** した．
また，最大遅延も小さく，レイテンシのばらつきが抑えられている．

これはAF_XDPでは以下の処理が省略されるためである．
- skb の生成・解放
- カーネルネットワークスタックの処理
- カーネル／ユーザ空間間のコピー

結果として，NICから受信したパケットをほぼ直接ユーザ空間で処理でき，低レイテンシかつ安定した応答が実現されている．

本結果はICMPによる簡易評価であるが，NFV，パケットフィルタ，ユーザ空間スイッチなどの用途においてAF_XDPが有効であることを示している．

