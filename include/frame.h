/* ==========================================================================
 * frame.h — the shared contract between every module of the gateway
 * ==========================================================================
 *
 * WHAT THIS FILE IS
 *   This header defines three things that all modules agree on:
 *     1. frame_t        what "one camera frame" looks like in memory;
 *     2. capture_*()    the API of the capture module (camera -> us);
 *     3. frame_get/put  the reference counting that decides when a camera
 *                       buffer can be given back to the camera.
 *
 *   It is included from both C and C++:
 *     src/capture.cpp   C++, talks to libcamera, fills in frame_t   (TODO B3)
 *     src/bufmgr.c      C, implements frame_get / frame_put         (TODO B5)
 *     src/main.c        C, epoll main loop                          (TODO B4)
 *     display / RTP     teammate's code, consumes frame_t
 *   Because of that, the file contains a small C/C++ compatibility layer
 *   (see "C / C++ COMPATIBILITY" below).
 *
 * LIFECYCLE OF ONE FRAME
 *
 *   camera sensor
 *        |
 *        v
 *   libcamera (its own thread) --> capture.cpp fills frame_t,
 *        ^                         takes a reference (frame_get),
 *        |                         calls your callback cb(f, user)
 *        |                                   |
 *        |                                   v
 *        |                         consumers (display, encoder, RTP)
 *        |                         each one that keeps f calls frame_get(f),
 *        |                         and frame_put(f) when it is done
 *        |                                   |
 *        |                                   v  refcnt drops to 0
 *        +------ capture_requeue(f) <--------+
 *                (buffer goes back to the camera and is refilled)
 *
 *   The pixels are NEVER copied. frame_t only describes where the pixels
 *   are (a dmabuf), how they are laid out, and when they were produced.
 *   The camera has a small, fixed pool of buffers (a handful), so every
 *   frame must eventually be released, or capture stalls.
 *
 * QUICK START (intended usage once capture/bufmgr are implemented)
 *
 *   static void on_frame(frame_t *f, void *user) {
 *       // Runs on libcamera's thread. Do not block, do not do real work
 *       // here. Hand f to the main loop (e.g. push to a queue + write an
 *       // eventfd). Capture already holds one reference for you.
 *   }
 *
 *   capture_open(1280, 720, 30, DRM_FORMAT_YUV420, on_frame, &app);
 *   capture_start();
 *   // main loop, for each frame f taken from the queue:
 *   //   - give it to every consumer; a consumer that keeps f beyond this
 *   //     point calls frame_get(f) first;
 *   //   - drop the reference that came with the callback: frame_put(f).
 *   capture_stop();
 *   capture_close();
 *
 * More background: docs/handoff_display.md, docs/week0.md (§A4, §A6).
 * ========================================================================== */

/* Include this file at most once per translation unit (same effect as the
 * classic #ifndef FRAME_H / #define FRAME_H / #endif guard). */
#pragma once
#include <stdint.h>   /* uint32_t, uint64_t: fixed-width integers           */
#include <time.h>     /* CLOCK_MONOTONIC, clock_gettime()                   */

/* --------------------------------------------------------------------------
 * C / C++ COMPATIBILITY
 * --------------------------------------------------------------------------
 * __cplusplus is defined only by a C++ compiler, so the two branches below
 * pick the right definitions for each language.
 *
 * frame_refcnt_t — the reference count must be ATOMIC.
 *   The libcamera thread and the main thread can both touch the same frame.
 *   A plain "count++" is really read / add / write, and two threads can
 *   interleave and lose an update:
 *       thread A reads 1         thread B reads 1
 *       thread A writes 2        thread B writes 2     <- should be 3
 *   An atomic integer makes each increment/decrement indivisible.
 *   C++ spells it std::atomic<int>, C11 spells it atomic_int. Both get the
 *   same name here, so the rest of the file does not care which language is
 *   compiling. We rely on the two having the same size and layout (one int),
 *   which holds for GCC/Clang on the Pi; that is what lets one frame_t be
 *   shared between C and C++ code.
 *   Consequence: never copy a frame_t by value (you would copy an atomic and
 *   create a second, unrelated frame). Always pass frame_t *.
 *
 * extern "C" { ... }
 *   A C++ compiler "mangles" function names to support overloading
 *   (frame_put becomes something like _Z9frame_putP5frame); a C compiler
 *   does not. Without extern "C", C code calling capture_open() would fail
 *   to link against the C++ implementation ("undefined reference").
 *   extern "C" tells the C++ compiler to use plain C names for everything
 *   declared inside the braces. The brace opened here is closed at the very
 *   end of this file.
 * -------------------------------------------------------------------------- */
#ifdef __cplusplus
#include <atomic>
typedef std::atomic<int> frame_refcnt_t;
extern "C" {
#else
#include <stdatomic.h>
typedef atomic_int frame_refcnt_t;
#endif

/* --------------------------------------------------------------------------
 * TIMESTAMP CLOCK
 * --------------------------------------------------------------------------
 * Clock for TS_INGEST / TS_ENCODED / TS_SENT. Must match the clock that
 * libcamera uses for SensorTimestamp (TS_SENSOR).
 *
 * The libcamera docs say CLOCK_BOOTTIME. The RPi pipeline, however, passes the
 * unicam V4L2 buffer timestamp through unchanged. That timestamp is
 * ktime_get_ns() with V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC, i.e. CLOCK_MONOTONIC.
 * The two clocks differ only by suspend time, which is 0 on the Pi.
 * See docs/week0.md §A6.
 *
 * Why it matters: latency = one timestamp minus another. That subtraction is
 * only meaningful if both came from the same clock.
 *   CLOCK_REALTIME   wall-clock time, can jump (NTP). NEVER use it here.
 *   CLOCK_MONOTONIC  counts up since boot, never jumps.      <- we use this
 *   CLOCK_BOOTTIME   like MONOTONIC but also counts suspend time.
 *
 * How to take a timestamp (always through this macro, never a hard-coded
 * clock, so it can be changed in one place):
 *
 *   struct timespec ts;
 *   clock_gettime(FRAME_CLOCK_ID, &ts);
 *   f->ts_ns[TS_SENT] = (uint64_t)ts.tv_sec * 1000000000u + ts.tv_nsec;
 *
 * Caveats about TS_SENSOR (docs/handoff_display.md §4):
 *   - it has 1 us resolution;
 *   - it marks frame START at the CSI receiver, not the start of exposure,
 *     so it lags the real exposure by roughly the shutter time.
 * -------------------------------------------------------------------------- */
#define FRAME_CLOCK_ID CLOCK_MONOTONIC

/* --------------------------------------------------------------------------
 * CONSTANTS
 * --------------------------------------------------------------------------
 * FRAME_MAX_PLANES
 *   A "plane" is one separately stored part of an image. Planar YUV keeps
 *   brightness and colour apart:
 *     DRM_FORMAT_YUV420 (YU12)  3 planes: Y, then U, then V   (our default)
 *     DRM_FORMAT_NV12           2 planes: Y, then interleaved UV (fallback)
 *     packed RGB                1 plane
 *   3 is the most any format we use needs. num_planes in frame_t says how
 *   many are actually used.
 *
 * Timestamp slots (index into frame_t.ts_ns[]):
 *
 *   slot        meaning                               written by
 *   ----------  ------------------------------------  ----------------------
 *   TS_SENSOR   frame start at the sensor/CSI side    capture (from libcamera
 *               (libcamera SensorTimestamp)            SensorTimestamp)
 *   TS_INGEST   frame handed to our code              capture
 *   TS_ENCODED  H.264 encode finished                 encoder
 *   TS_SENT     packet sent on the network            RTP sender
 *
 *   TS_COUNT is not a timestamp. Enum values start at 0, so the last entry
 *   automatically equals the number of entries before it (4). It is used as
 *   the array size, so adding a new slot before it resizes ts_ns[] for free.
 *   Always add new slots BEFORE TS_COUNT.
 *
 *   Example: capture-to-encode latency in ns
 *     uint64_t lat = f->ts_ns[TS_ENCODED] - f->ts_ns[TS_SENSOR];
 * -------------------------------------------------------------------------- */
#define FRAME_MAX_PLANES 3
enum { TS_SENSOR, TS_INGEST, TS_ENCODED, TS_SENT, TS_COUNT };

/* --------------------------------------------------------------------------
 * frame_t — one camera frame
 * --------------------------------------------------------------------------
 * frame_t does NOT contain pixels. The pixels live in a dmabuf owned by the
 * camera; this struct says where they are, how they are laid out, what
 * format they are in, and when the frame passed each stage.
 *
 * The per-plane arrays (fd, offset, length, stride) are indexed by plane
 * number: [0] = Y, [1] = U (or UV), [2] = V. Only the first num_planes
 * entries are valid; the rest are garbage.
 *
 * Address of the first byte of row y in plane i (if you mmap the dmabuf):
 *     base_of(fd[i]) + offset[i] + y * stride[i]
 * Note: stride, not width. See the stride field.
 *
 * Ownership: capture owns frame_t and its dmabufs. Consumers only borrow a
 * frame (frame_get / frame_put) and must not free it or close its fds.
 * -------------------------------------------------------------------------- */
typedef struct frame {
    /* dmabuf file descriptor for each plane (may be equal).
     * A dmabuf is a kernel buffer represented by a file descriptor; any
     * device (display, encoder) can be handed the fd and read the same
     * memory directly, with zero copies. Usually all planes share ONE dmabuf,
     * so fd[0] == fd[1] == fd[2] and only offset differs. This is expected.
     * Display: drmPrimeFDToHandle() on each fd[i] (equal handles are fine).
     * Do NOT close() these fds; capture owns them. */
    int      fd[FRAME_MAX_PLANES];      /* dmabuf fd per plane (may be equal) */

    /* Byte offset where plane i starts inside its dmabuf.
     * E.g. YU12 1280x720 with no padding: Y at 0, U at 921600 (1280*720).
     * Real values come from the driver; do not compute them yourself. */
    uint32_t offset[FRAME_MAX_PLANES];  /* byte offset of the plane in its dmabuf */

    /* Size of plane i in bytes. */
    uint32_t length[FRAME_MAX_PLANES];

    /* Bytes per row of plane i, INCLUDING padding. Hardware often aligns rows
     * (e.g. to 32 or 64 bytes), so stride can be larger than the row's
     * pixel data:
     *
     *   |<------ width * bytes/pixel ------>|<- pad ->|
     *   |<------------------ stride ----------------->|
     *
     * Always step from row to row with stride, never with width.
     * Pass stride[i] as the pitch to drmModeAddFB2() / the encoder. */
    uint32_t stride[FRAME_MAX_PLANES];  /* bytes per line, from the driver */

    /* Number of planes actually used (1..FRAME_MAX_PLANES).
     * 3 for YU12, 2 for NV12. Loop i < num_planes; do not assume 3. */
    uint32_t num_planes;

    /* Image size in pixels (of the full image / Y plane; chroma planes of
     * 4:2:0 formats are width/2 x height/2). */
    uint32_t width, height;

    /* Pixel format as a DRM fourcc (DRM_FORMAT_* from <drm_fourcc.h>), NOT a
     * V4L2_PIX_FMT_* code: the two code sets look alike but are not the same.
     * DRM is used because display goes through DRM/KMS.
     * Expected: DRM_FORMAT_YUV420 (YU12), fallback DRM_FORMAT_NV12, both with
     * the LINEAR modifier (never request SAND/tiled). Code must read this
     * field and num_planes, not assume a format. */
    uint32_t fourcc;                    /* DRM_FORMAT_* value */

    /* Frame sequence number from libcamera, +1 per frame. A gap
     * (e.g. 101 -> 103) means the camera dropped a frame; use it to count
     * drops. */
    uint32_t seq;                       /* FrameMetadata::sequence */

    /* Timestamps in nanoseconds, indexed by TS_SENSOR..TS_SENT, all taken on
     * FRAME_CLOCK_ID. 64-bit because ns overflow 32 bits after ~4.3 s.
     * A slot is only meaningful after the stage that writes it has run. */
    uint64_t ts_ns[TS_COUNT];           /* all four on the same clock */

    /* Number of current users of this frame. Do NOT read or write it
     * directly; use frame_get() / frame_put(). Initialised by capture. */
    frame_refcnt_t refcnt;

    /* The libcamera::Request* this frame came from. capture.cpp needs it to
     * give the buffer back to libcamera (capture_requeue).
     * "Opaque to C": C does not know C++ classes, so it is stored as void*.
     * C code must only carry this pointer around; never dereference, free or
     * modify it. Only capture.cpp casts it back. */
    void    *request;                   /* libcamera::Request*, opaque to C */
} frame_t;

/* --------------------------------------------------------------------------
 * CAPTURE API — implemented in src/capture.cpp (currently a stub, TODO B3)
 * --------------------------------------------------------------------------
 * capture (C++ inside, C outside). cb runs on libcamera's thread.
 *
 * Call order:  capture_open -> capture_start -> (callbacks...) ->
 *              capture_stop -> capture_close
 * Return convention for int functions: 0 on success, negative on error.
 *
 * capture_frame_cb — type of the "new frame" callback.
 *   Read it as: a pointer to a function taking (frame_t *f, void *user) and
 *   returning nothing. You write such a function and pass it to
 *   capture_open(); capture calls it once per new frame.
 *   - It runs on LIBCAMERA'S THREAD, not the main thread. Keep it short and
 *     non-blocking: record what you need, queue f, wake the main loop (e.g.
 *     write to an eventfd), return. Slow callbacks delay the camera.
 *   - Capture calls frame_get(f) before invoking cb, so f arrives with one
 *     reference that now belongs to you. You must frame_put(f) it exactly
 *     once, eventually (from the main thread, see frame_put below).
 *   - user is whatever pointer you gave capture_open(), returned unchanged.
 *     Use it to reach your own state without globals.
 * -------------------------------------------------------------------------- */
typedef void (*capture_frame_cb)(frame_t *f, void *user);

/* Open the camera and configure it.
 *   width, height  resolution in pixels (e.g. 1280 x 720)
 *   fps            frames per second (e.g. 30)
 *   fourcc         requested DRM pixel format (e.g. DRM_FORMAT_YUV420)
 *   cb, user       callback for each new frame and its context pointer
 * Does not start streaming; call capture_start() for that. */
int  capture_open(uint32_t width, uint32_t height, uint32_t fps,
                  uint32_t fourcc, capture_frame_cb cb, void *user);

/* Start streaming. From now on cb is called for every frame. */
int  capture_start(void);

/* Give f's buffer back to the camera so it can be refilled.
 * Normally you do NOT call this yourself: frame_put() calls it when the last
 * reference is dropped.
 * MAIN THREAD ONLY: calling it from another thread (e.g. inside cb) races
 * with libcamera's internal state. */
void capture_requeue(frame_t *f);       /* main thread only */

/* Stop streaming. No more callbacks after this returns. */
void capture_stop(void);

/* Release the camera and all its buffers. Every frame must have been
 * released (frame_put) before this. */
void capture_close(void);

/* --------------------------------------------------------------------------
 * BUFFER MANAGER — implemented in src/bufmgr.c (currently a stub, TODO B5:
 * atomic_fetch_add / atomic_fetch_sub on refcnt)
 * --------------------------------------------------------------------------
 * Reference counting decides when a camera buffer can be reused.
 * One frame can be used by several consumers at the same time (display AND
 * encoder). It may only go back to the camera when the LAST of them is done.
 *
 *   frame_get(f)  "I am going to keep using f"     refcnt += 1
 *   frame_put(f)  "I am done with f"               refcnt -= 1
 *                 if it reaches 0 -> capture_requeue(f)
 *
 * Rules:
 *   - Every frame_get() must be matched by exactly one frame_put().
 *   - Call frame_get() BEFORE handing f to something that keeps it.
 *   - After your frame_put(), do not touch f again; it may already be
 *     refilled with a new image.
 *   - frame_put() may call capture_requeue(), which is main-thread only,
 *     so call frame_put() from the main thread.
 *
 * Getting it wrong:
 *   - hold frames too long / forget a put -> camera runs out of buffers,
 *     capture stalls (frames are dropped: watch seq for gaps);
 *   - put too early / one put too many    -> buffer is refilled while still
 *     in use: tearing, garbage on screen, corrupted encode.
 * -------------------------------------------------------------------------- */
void frame_get(frame_t *f);
void frame_put(frame_t *f);             /* at 0: capture_requeue() */

/* --------------------------------------------------------------------------
 * DISPLAY CONTRACT (teammate's module, not declared here yet)
 * --------------------------------------------------------------------------
 * display (karan) — signature to confirm in B2:
 * void display_submit(frame_t *f);
 * Display calls frame_put() on the previous frame after the NEXT page flip
 * completes.
 *
 * Why: while frame N is on screen, the display hardware keeps reading its
 * memory. Frame N is only safe to release once frame N+1 is actually on
 * screen, i.e. after the page flip to N+1 has COMPLETED (not just been
 * requested).
 *
 *   display_submit(N)      N is scanned out          display holds N
 *   display_submit(N+1)    flip to N+1 requested     still holds N and N+1
 *   flip-complete event    N+1 is on screen          frame_put(N)
 *
 * Releasing N at submit time instead would let the camera overwrite it while
 * the screen is still reading it (tearing). See docs/handoff_display.md §5.
 * -------------------------------------------------------------------------- */

#ifdef __cplusplus
}   /* closes the extern "C" { opened in C / C++ COMPATIBILITY above */
#endif
