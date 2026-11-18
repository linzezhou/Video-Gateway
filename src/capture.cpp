// libcamera capture behind the C interface in frame.h.
// TODO(B3): implement following the libcamera Application writer's guide.
// Included here so the skeleton build already checks that libcamera headers
// and linking work on the Pi.
#include <libcamera/libcamera.h>

#include "frame.h"

int capture_open(uint32_t width, uint32_t height, uint32_t fps,
                 uint32_t fourcc, capture_frame_cb cb, void *user) {
    (void)width; (void)height; (void)fps; (void)fourcc; (void)cb; (void)user;
    return -1;
}

int capture_start(void) { return -1; }

void capture_requeue(frame_t *f) { (void)f; }

void capture_stop(void) {}

void capture_close(void) {}
