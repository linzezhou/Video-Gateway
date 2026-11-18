# Handoff to the display / RTP owner

These are the Week 0 findings that affect your side. Read this before you start display integration (W2).
Details and raw evidence are in [week0.md](week0.md) §A4 and §A6.

Last updated: 2026-11-17.

## 1. Blocker: no monitor is connected to the Pi

`modetest -M vc4 -c` reports that both HDMI ports are **disconnected**, every CRTC is inactive (size 0x0), and no
compositor is running (no labwc / wayfire / Xorg / weston). The DRM/KMS display path cannot be developed or tested
until a monitor is attached.

**Action (you):**

1. Connect an HDMI monitor to **HDMI-A-1**, the micro-HDMI port next to the USB-C power input.
2. On the Pi:

   ```bash
   modetest -M vc4 -c -e -p > data/modetest_connected.txt 2>&1
   ```

3. In [week0.md](week0.md) §A4, under "Display plane", record:
   - the connector id, its status and its preferred mode;
   - which encoder and CRTC drive that connector;
   - the primary plane id for that CRTC, and at least one overlay plane id.

   Then commit `data/modetest_connected.txt`.

If you cannot get a monitor, tell me now. It blocks W2.

## 2. Pixel format: `DRM_FORMAT_YUV420` (`YU12`), linear

- The frames you receive in `display_submit()` will have `f->fourcc == DRM_FORMAT_YUV420`. That is 3 planes:
  Y, then U, then V.
- The fallback is `DRM_FORMAT_NV12` (2 planes). Code should not assume YU12: read `f->fourcc` and
  `f->num_planes`.
- Every primary and overlay plane on vc4 accepts both formats with the `LINEAR` modifier, which is what libcamera
  produces. Do **not** request a SAND/tiled modifier.
- Build the framebuffer with `drmPrimeFDToHandle()` on each `f->fd[i]`, then call `drmModeAddFB2()` using
  `f->offset[i]` and `f->stride[i]`.
  - The planes usually share one dmabuf fd, so the handles can be equal. This is expected.
  - Do not compute strides yourself. Use the values in `frame_t`, which come from the driver.
- Real stride/offset values will be printed by capture (B3), and I will add them to the README. Until then, do not
  hard-code any layout.

## 3. Plane selection: pick at runtime, do not hard-code an id

The plane layout from `modetest -p` (no monitor attached):

| Planes | Type | possible_crtcs |
| --- | --- | --- |
| 48, 67, 79, 91, 103, 115 | Primary (one per CRTC) | 0x01 … 0x20 |
| 127–292 | Overlay | 0x3e (CRTCs 1–5) |
| 303–644 | Overlay | 0x01 (CRTC 0 only) |

Recommended logic:

1. Find the connected connector.
2. Find the CRTC bound to it.
3. Choose a plane whose `possible_crtcs` bit for that CRTC index is set and whose `type` property is what you
   want.

No compositor is running, so the gateway can become DRM master and use the CRTC's **primary** plane. Use an overlay
plane only if something else must stay visible underneath.

## 4. `include/frame.h` changed: timestamps use `CLOCK_MONOTONIC`

```c
#define FRAME_CLOCK_ID CLOCK_MONOTONIC   /* was CLOCK_BOOTTIME (placeholder) */
```

- Any timestamp you record goes into `f->ts_ns[]`, for example `TS_SENT` on the RTP side. Take it with
  `clock_gettime(FRAME_CLOCK_ID, ...)`. Never use `CLOCK_REALTIME`, and do not hard-code a clock.
- Why: libcamera's `SensorTimestamp` (`TS_SENSOR`) actually comes from the unicam driver's `ktime_get_ns()`, which is
  `CLOCK_MONOTONIC`. This was verified in the kernel source and by measurement; see week0.md §A6.
- `SensorTimestamp` has 1 µs resolution. It marks *frame start at the CSI receiver*, not the start of exposure, so
  it lags exposure by about the shutter time.

## 5. Buffer ownership contract (unchanged; please confirm)

```c
void display_submit(frame_t *f);
```

- Capture calls `frame_get(f)` before handing you `f`.
- You keep `f` on screen until the **next** page flip completes. Then you call `frame_put()` on the *previous* frame,
  never the one currently being scanned out.
- When the refcount reaches 0, the buffer goes back to the camera. If you hold frames too long, capture runs out of
  buffers and stalls. If you release one too early, the screen tears or shows garbage.
- Reply with any change to this signature or contract before W2 integration.

## 6. Sharing the Pi

- Only one process can use the camera at a time. `scripts/ref_pi.sh`, the gateway, and any `rpicam-*` command block
  each other. When you see "no cameras available" or "busy", check `pgrep -a rpicam`.
- If the camera is not detected at all after boot, check `dmesg | grep -iE 'imx708|unicam'`. It happened once and a
  reboot fixed it.
- Kernel is `6.18.58-rt9-v8+` (PREEMPT_RT) and is frozen for the whole project. Do not run `apt full-upgrade`.
  Installing an extra package with `apt install` is fine.
- Only one person should edit `docs/week0.md` at a time, to avoid merge conflicts. Use `git pull --rebase` before
  editing.
