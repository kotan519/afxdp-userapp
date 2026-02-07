#include <bpf/bpf.h>
#include <bpf/xsk.h>
#include <stdint.h>
#include <stdbool.h>

#include "afxdp.h"

// xskmapにsocket fdを登録
int afxdp_xskmap_register(int map_fd,
                          struct afxdp_port *p,
                          uint32_t queue_id)
{
    int fd = xsk_socket__fd(p->xsk);
    return bpf_map_update_elem(map_fd, &queue_id, &fd, 0);
}

// free frameをpop
bool afxdp_free_pop(struct afxdp_umem *u, uint64_t *addr)
{
    if (u->free_cnt == 0)
        return false;

    *addr = u->free_addrs[--u->free_cnt];
    return true;
}

bool afxdp_free_push(struct afxdp_umem *u, uint64_t addr)
{
    if (u->free_cnt >= u->free_cap)
        return false;

    u->free_addrs[u->free_cnt++] = addr;
    return true;
}

// UMEM offset→ポインタ
void *afxdp_umem_ptr(struct afxdp_umem *u, uint64_t addr)
{
    return (uint8_t *)u->area + addr;
}

// port後始末
void afxdp_port_destroy(struct afxdp_port *p)
{
    if (!p)
        return;

    if (p->xsk)
        xsk_socket__delete(p->xsk);

    afxdp_umem_destroy(&p->umem);
}
