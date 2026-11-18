#pragma once
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
#include <atomic>
typedef std::atomic<int> frame_refcnt_t;
extern "C" {
#else
#include <stdatomic.h>
typedef atomic_int frame_refcnt_t;
#endif

/* Clock for TS_INGEST / TS_ENCODED / TS_SENT. Must match the clock that
 * libcamera uses for SensorTimestamp. TODO(A6): confirm and cite the source;
 * CLOCK_BOOTTIME is a placeholder until then. */
#define FRAME_CLOCK_ID CLOCK_BOOTTIME

#define FRAME_MAX_PLANES 3
enum { TS_SENSOR, TS_INGEST, TS_ENCODED, TS_SENT, TS_COUNT };

typedef struct frame {
    int      fd[FRAME_MAX_PLANES];      /* dmabuf fd per plane (may be equal) */
    uint32_t offset[FRAME_MAX_PLANES];  /* byte offset of the plane in its dmabuf */
    uint32_t length[FRAME_MAX_PLANES];
    uint32_t stride[FRAME_MAX_PLANES];  /* bytes per line, from the driver */
    uint32_t num_planes;
    uint32_t width, height;
    uint32_t fourcc;                    /* DRM_FORMAT_* value */
    uint32_t seq;                       /* FrameMetadata::sequence */
    uint64_t ts_ns[TS_COUNT];           /* all four on the same clock */
    frame_refcnt_t refcnt;
    void    *request;                   /* libcamera::Request*, opaque to C */
} frame_t;

/* capture (C++ inside, C outside). cb runs on libcamera's thread. */
typedef void (*capture_frame_cb)(frame_t *f, void *user);
int  capture_open(uint32_t width, uint32_t height, uint32_t fps,
                  uint32_t fourcc, capture_frame_cb cb, void *user);
int  capture_start(void);
void capture_requeue(frame_t *f);       /* main thread only */
void capture_stop(void);
void capture_close(void);

/* bufmgr (C) */
void frame_get(frame_t *f);
void frame_put(frame_t *f);             /* at 0: capture_requeue() */

/* display (teammate) — signature to confirm in B2:
 * void display_submit(frame_t *f);
 * Display calls frame_put() on the previous frame after the NEXT page flip
 * completes. */

#ifdef __cplusplus
}
#endif
