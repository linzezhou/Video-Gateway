/* Entry point: signal mask -> signalfd/eventfd/timerfd -> epoll main loop.
 * TODO(B4): implement following the skeleton in the lab manual. */
#include <stdio.h>
#include "frame.h"

int main(void) {
#ifdef BASELINE
    printf("gateway_baseline: skeleton build\n");
#else
    printf("gateway: skeleton build\n");
#endif
    return 0;
}
