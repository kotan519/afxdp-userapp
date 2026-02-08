#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>    
#include <bpf/xsk.h>   

#include "afxdp.h" 

// TXリングに登録
int afxdp_tx_reserve(struct afxdp_port *p, uint32_t n, uint32_t *idx_out)
{
    int r = xsk_ring_prod__reserve(&p->tx, n, idx_out);
    return r;
}

/*
// TXリングに書き込み
static inline void afxdp_tx_write(struct afxdp_port *p, uint32_t idx,
                                  uint64_t addr, uint32_t len)
{
    struct xdp_desc *d = xsk_ring_prod__tx_desc(&p->tx, idx);
    d->addr = addr;
    d->len  = len;
}
*/

// TXのprodを公開
void afxdp_tx_submit(struct afxdp_port *p, uint32_t n)
{
    xsk_ring_prod__submit(&p->tx, n);
    p->stats.tx_pkts += n;
}

// kick
int afxdp_tx_kick(struct afxdp_port *p)
{
    if (!p->use_need_wakeup)
        return 0;

    // needwakeupだったら通知する
    if (!xsk_ring_prod__needs_wakeup(&p->tx))
        return 0;

    // ユーザ空間からカーネルへ通知
    return sendto(xsk_socket__fd(p->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
}

// cqをdrain
int afxdp_cq_drain(struct afxdp_port *p, uint32_t budget)
{
    uint32_t idx;
    // cqをpeek
    uint32_t n = xsk_ring_cons__peek(&p->cq, budget, &idx);
    if (!n)
        return 0;

    for (uint32_t i = 0; i < n; i++) {
        uint64_t addr = *xsk_ring_cons__comp_addr(&p->cq, idx + i);
        afxdp_free_push(&p->umem, addr);
        p->stats.cq_pkts++;
    }

    xsk_ring_cons__release(&p->cq, n);
    return (int)n;
}