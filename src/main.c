// src/main.c
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include <linux/if_link.h>
#include <bpf/bpf.h>
#include <bpf/xsk.h>

#include "afxdp.h"
#include "afxdp_config.h"

static volatile int running = 1;

static void on_sigint(int signo)
{
    (void)signo;
    running = 0;
}

static void die(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char **argv)
{
    // 設定
    struct afxdp_cfg cfg = afxdp_cfg_default();

    struct afxdp_port port;
    memset(&port, 0, sizeof(port));

    signal(SIGINT, on_sigint);

    // XSKMAP取得
    int xsks_map_fd = bpf_obj_get("/sys/fs/bpf/xsks_map");
    if (xsks_map_fd < 0)
        die("bpf_obj_get(/sys/fs/bpf/xsks_map)");

    // UMEM作成
    if (afxdp_umem_create(&port.umem, AFXDP_FRAME_SIZE, AFXDP_NUM_FRAMES,
                          &port.fill, &port.cq))
        die("afxdp_umem_create");

    // fillリングにfreeを詰める
    if (afxdp_fill_prepare_all(&port) < 0)
        die("afxdp_fill_prepare_all");

    // xsksocket作成
    {
        struct xsk_socket_config xcfg = {
            .rx_size = AFXDP_NUM_FRAMES,
            .tx_size = AFXDP_NUM_FRAMES,
            .libbpf_flags = 0,
            .xdp_flags = XDP_FLAGS_DRV_MODE,
            .bind_flags = XDP_ZEROCOPY,
        };

        if(cfg.use_need_wakeup)
            xcfg.bind_flags |= XDP_USE_NEED_WAKEUP;

        if (xsk_socket__create(&port.xsk, cfg.ifname, cfg.queue_id,
                               port.umem.umem, &port.rx, &port.tx, &xcfg))
            die("xsk_socket__create");

        port.use_need_wakeup = cfg.use_need_wakeup;
    }

    // xskmapにsocket登録
    if (afxdp_xskmap_register(xsks_map_fd, &port, cfg.queue_id))
        die("afxdp_xskmap_register");

    printf("Started on %s q=%u \n", cfg.ifname, cfg.queue_id);

    struct pollfd pfd = {
        .fd = xsk_socket__fd(port.xsk),
        .events = POLLIN,
    };

    // メインループ
    while (running) {
        // Completionリングを回収しfreeへ
        afxdp_cq_drain(&port, cfg.batch_size);

        // fillリングに補充
        afxdp_fill_refill(&port, AFXDP_FILL_WM);

        // RX peek
        int n = afxdp_rx_peek(&port, cfg.batch_size);
        if (n <= 0) {
            // polling
            poll(&pfd, 1, cfg.poll_timeout_ms);
            continue;
        }

        // TXリングに格納
        uint32_t tx_idx;
        int tn = xsk_ring_prod__reserve(&port.tx, (uint32_t)n, &tx_idx);
        if (tn != n) {
            afxdp_tx_kick(&port);
            afxdp_rx_release(&port, n);
            continue;
        }

        for (int i = 0; i < n; i++) {
            struct xdp_desc *rd = afxdp_rx_desc(&port, i);
            void *data = afxdp_rx_data(&port, rd);

            // ICMP書き換え
            try_icmp_reply(data, rd->len);

            port.stats.rx_pkts++;
            port.stats.rx_bytes += rd->len;

            // RX→TX（descコピー）
            struct xdp_desc *td = xsk_ring_prod__tx_desc(&port.tx, tx_idx + (uint32_t)i);
            td->addr = rd->addr;
            td->len  = rd->len;
        }

        xsk_ring_prod__submit(&port.tx, (uint32_t)n);
        port.stats.tx_pkts += (uint64_t)n;

        // RX再利用
        afxdp_rx_release(&port, n);

        // TX kick
        afxdp_tx_kick(&port);
    }

    printf("\nrx_pkts=%lu rx_bytes=%lu tx_pkts=%lu comp=%lu fill=%lu\n",
       port.stats.rx_pkts,
       port.stats.rx_bytes,
       port.stats.tx_pkts,
       port.stats.cq_pkts,
       port.stats.fill_pkts);

    afxdp_port_destroy(&port);
    return 0;
}
