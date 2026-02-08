#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <linux/if_xdp.h>
#include <linux/if_ether.h>

#include <bpf/xsk.h>

#define AFXDP_FRAME_SIZE 2048u
#define AFXDP_NUM_FRAMES 4096u
#define AFXDP_BATCH_SIZE 32u
#define AFXDP_FILL_WM    (AFXDP_NUM_FRAMES / 2u)

// stats
struct afxdp_stats {
    uint64_t rx_pkts;
    uint64_t rx_bytes;
    uint64_t tx_pkts;
    uint64_t tx_bytes;
    uint64_t fill_pkts;
    uint64_t cq_pkts;

};

// UMEM
struct afxdp_umem {
    struct xsk_umem *umem;
    void   *area;
    size_t  area_size;
    int     shm_fd;
    char    shm_name[64];
    uint64_t *free_addrs;
    uint32_t  free_cnt;
    uint32_t  free_cap;

    uint32_t  frame_size;
};

// port
struct afxdp_port {
    struct xsk_socket *xsk;
    struct xsk_ring_cons rx;
    struct xsk_ring_prod tx;
    struct xsk_ring_prod fill;
    struct xsk_ring_cons cq;
    uint32_t rx_idx;
    struct afxdp_umem umem;
    bool use_need_wakeup;
    struct afxdp_stats stats;
};

// RX
int afxdp_rx_peek(struct afxdp_port *p, uint32_t max);
const struct xdp_desc *afxdp_rx_desc(struct afxdp_port *p, uint32_t i);
void *afxdp_rx_data(struct afxdp_port *p, const struct xdp_desc *d);
void afxdp_rx_release(struct afxdp_port *p, uint32_t n);

// TX
int  afxdp_tx_reserve(struct afxdp_port *p, uint32_t n, uint32_t *idx_out);
void afxdp_tx_submit(struct afxdp_port *p, uint32_t n);
int  afxdp_tx_kick(struct afxdp_port *p);
int  afxdp_tx_kick(struct afxdp_port *p);
int  afxdp_cq_drain(struct afxdp_port *p, uint32_t budget);

// UMEM 
int afxdp_umem_create(struct afxdp_umem *u, uint32_t frame_size, uint32_t num_frames,
                        struct xsk_ring_prod *fill, struct xsk_ring_cons *cq);
void afxdp_umem_destroy(struct afxdp_umem *u);
int afxdp_fill_prepare_all(struct afxdp_port *p);
int afxdp_fill_refill(struct afxdp_port *p);

// util
int afxdp_xskmap_register(int map_fd, struct afxdp_port *p, uint32_t queue_id);
bool afxdp_free_pop(struct afxdp_umem *u, uint64_t *addr);
bool afxdp_free_push(struct afxdp_umem *u, uint64_t addr);
void *afxdp_umem_ptr(struct afxdp_umem *u, uint64_t addr);
void afxdp_port_destroy(struct afxdp_port *p);
void try_icmp_reply(void *data, uint32_t len);