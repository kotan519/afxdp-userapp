// src/umem.c
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>   
#include <sys/stat.h>
#include <fcntl.h>
#include <bpf/xsk.h>

#include "afxdp.h"

struct afxdp_port port = {0};

// UMEM作成（/dev/shm上）
int afxdp_umem_create(struct afxdp_umem *u,
                      uint32_t frame_size,
                      uint32_t num_frames,
                      struct xsk_ring_prod *fill,
                      struct xsk_ring_cons *cq)
{
    int ret;
    memset(u, 0, sizeof(*u));
    
    u->shm_fd = -1;
    u->frame_size = frame_size;
    u->free_cap   = num_frames;
    u->free_cnt   = 0;

    u->area_size = (size_t)frame_size * (size_t)num_frames;

    // 作成
    snprintf(u->shm_name, sizeof(u->shm_name), "/afxdp-umem-%d", getpid());

    u->shm_fd = shm_open(u->shm_name, O_CREAT | O_RDWR, 0666);
    if (u->shm_fd < 0) {
        return -1;
    }

    if (ftruncate(u->shm_fd, u->area_size))
        return -1;

    u->area = mmap(NULL, u->area_size,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, u->shm_fd, 0);
    if (u->area == MAP_FAILED)
        return -1;

    memset(u->area, 0, u->area_size);

    // freeフレームを確保
    u->free_addrs = calloc(num_frames, sizeof(uint64_t));
    if (!u->free_addrs)
        return -1;

    for (uint32_t i = 0; i < num_frames; i++)
        u->free_addrs[u->free_cnt++] = (uint64_t)i * frame_size;

    struct xsk_umem_config cfg = {
        .fill_size      = num_frames,
        .comp_size      = num_frames,
        .frame_size     = frame_size,
        .frame_headroom = 0,
        .flags          = 0,
    };

    ret = xsk_umem__create(&u->umem, u->area, u->area_size,
                           fill, cq, &cfg);
    if (ret)
        return ret;

    return 0;
}

// UMEMを解放
void afxdp_umem_destroy(struct afxdp_umem *u)
{
    if (!u) return;

    if (u->umem) {
        xsk_umem__delete(u->umem);
        u->umem = NULL;
    }

    if (u->area && u->area_size) {
        munmap(u->area, u->area_size);
        u->area = NULL;
        u->area_size = 0;
    }

    if (u->shm_fd >= 0) {
        close(u->shm_fd);
        u->shm_fd = -1;
    }

    if (u->free_addrs) {
        free(u->free_addrs);
        u->free_addrs = NULL;
    }
    u->free_cnt = 0;
    u->free_cap = 0;
}


// freeフレームをfillリングに詰める
int afxdp_fill_prepare_all(struct afxdp_port *p)
{
    struct afxdp_umem *u = &p->umem;

    if (u->free_cnt == 0)
        return 0;

    uint32_t idx;
    int need = u->free_cnt;

    // fillリングに予約
    int n = xsk_ring_prod__reserve(&p->fill, need, &idx);
    if (n != need) {
        return -ENOSPC;
    }

    for (int i = 0; i < n; i++) {
        uint64_t addr;
        // freeフレームのaddrをfillリングに詰める
        if (!afxdp_free_pop(u, &addr))
            break;
        *xsk_ring_prod__fill_addr(&p->fill, idx + i) = addr;
        p->stats.fill_pkts++;
    }

    xsk_ring_prod__submit(&p->fill, n);
    return n;
}

// fillリングに使用済みフレームを格納
int afxdp_fill_refill(struct afxdp_port *p)
{
    struct afxdp_umem *u = &p->umem;

    // check
    uint32_t fill_free = xsk_prod_nb_free(&p->fill, 0);
    if (fill_free == 0)
        return 0;


    uint32_t to_push = fill_free;
    if (to_push > u->free_cnt)
        to_push = u->free_cnt;

    if (to_push == 0)
        return 0;

    uint32_t idx;
    int n = xsk_ring_prod__reserve(&p->fill, to_push, &idx);
    if (n <= 0)
        return 0;

    // fillリングに詰める
    for (int i = 0; i < n; i++) {
        uint64_t addr;
        if (!afxdp_free_pop(u, &addr))
            break;
        *xsk_ring_prod__fill_addr(&p->fill, idx + i) = addr;
        p->stats.fill_pkts++;
    }

    xsk_ring_prod__submit(&p->fill, n);
    return n;
}
