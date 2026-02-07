#include <bpf/bpf.h>
#include <bpf/xsk.h>
#include <stdint.h>
#include <stdbool.h>

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <netinet/in.h> 
#include <linux/if_arp.h>

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

/*--------------------------ICMP--------------------------------*/
// チェックサム計算
// 16bitに畳込む
static inline uint16_t csum_fold(uint32_t sum)
{
    sum = (sum & 0xffff) + (sum >> 16);
    sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

// 2バイトずつ足していく
static inline uint32_t csum_partial(const void *buf, int len, uint32_t sum)
{
    const uint16_t *p = buf;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len)
        sum += *(uint8_t *)p;
    return sum;
}

// ICMP Request -> Reply
void try_icmp_reply(void *data, uint32_t len)
{
    // Ethrernet判定
    struct ethhdr *eth = data;
    if (len < sizeof(*eth))
        return;

    if (eth->h_proto != htons(ETH_P_IP))
        return;

    // IPv4判定と取得
    struct iphdr *ip = (void *)(eth + 1);
    if (ip->protocol != IPPROTO_ICMP)
        return;

    // ICMP判定と取得
    struct icmphdr *icmp = (void *)(ip + 1);
    if (icmp->type != ICMP_ECHO)
        return;

    // MACアドレス入れ替え（L2）
    uint8_t tmp_mac[ETH_ALEN];
    memcpy(tmp_mac, eth->h_source, ETH_ALEN);
    memcpy(eth->h_source, eth->h_dest, ETH_ALEN);
    memcpy(eth->h_dest, tmp_mac, ETH_ALEN);

    // IPアドレス入れ替え（L3）
    uint32_t tmp_ip = ip->saddr;
    ip->saddr = ip->daddr;
    ip->daddr = tmp_ip;

    // ICMPtypeをReplyに変更
    icmp->type = ICMP_ECHOREPLY;

    // IPチェックサム計算
    ip->check = 0;
    ip->check = csum_fold(csum_partial(ip, ip->ihl * 4, 0));

    // ICMPチェックサム計算
    icmp->checksum = 0;
    uint16_t icmp_len = ntohs(ip->tot_len) - ip->ihl * 4;
    icmp->checksum = csum_fold(csum_partial(icmp, icmp_len, 0));
}