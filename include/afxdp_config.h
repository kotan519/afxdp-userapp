#pragma once

#include <stdbool.h>
#include <stdint.h>

struct afxdp_cfg {
    const char *ifname;
    uint32_t queue_id;
    uint32_t batch_size;
    bool use_need_wakeup;
    int poll_timeout_ms;
};

// デフォルト設定を返す
struct afxdp_cfg afxdp_cfg_default(void);
