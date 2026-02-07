#include "afxdp_config.h"
#include "afxdp.h"

struct afxdp_cfg afxdp_cfg_default(void)
{
    return (struct afxdp_cfg){
        .ifname = "enp4s0f1",
        .queue_id = 0,
        .batch_size = AFXDP_BATCH_SIZE,
        .use_need_wakeup = true,
        .poll_timeout_ms = 1,
    };
}
