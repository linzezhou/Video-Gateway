/* Reference counting and return of capture buffers.
 * TODO(B5): implement frame_get/frame_put with atomic_fetch_add/sub and
 * track inflight; requeue to libcamera when the count drops to zero. */
#include "frame.h"

void frame_get(frame_t *f) { (void)f; }

void frame_put(frame_t *f) { (void)f; }
