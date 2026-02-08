// src/rx.c
#include <stdio.h>
#include <stdint.h>
#include <linux/if_xdp.h>

#include "afxdp.h"

// RXリングからmax分peek
int afxdp_rx_peek(struct afxdp_port *p, uint32_t max)
{
    uint32_t idx = 0;
    int n = xsk_ring_cons__peek(&p->rx, max, &idx);
    if (n > 0)
        p->rx_idx = idx;
    return n;
}

// peekしたaddrのdescを返す
const struct xdp_desc *afxdp_rx_desc(struct afxdp_port *p, uint32_t i)
{
    return xsk_ring_cons__rx_desc(&p->rx, p->rx_idx + i);
}

// パケットを取得
void *afxdp_rx_data(struct afxdp_port *p, const struct xdp_desc *d)
{
    return afxdp_umem_ptr(&p->umem, d->addr);
}

// RXリング消費（cons）
void afxdp_rx_release(struct afxdp_port *p, uint32_t n)
{
    xsk_ring_cons__release(&p->rx, n);
}
