# Week 0 · Feasibility

## Checklist

| # | Criterion | Status | Evidence |
| --- | --- | --- | --- |
| 1 | Environment recorded; camera detected as imx708 | ☑ | [env.md](env.md) |
| 2 | Reference pipeline works | ☑ | §A2 |
| 3 | Reference latency (20 samples, median, test conditions) | ☑ | §A3, `data/latency_ref.csv` |
| 4 | Reference CPU (rpicam-vid and gst-launch averages) | ☑ | §A3, `data/pidstat_ref.txt`, `data/mpstat_ref.txt` |
| 5 | Formats: full list, chosen format and rationale, primary vs overlay | ☑ (plane id pending a monitor) | §A4 |
| 6 | Clock source: conclusion and evidence | ☑ | §A6 |
| 7 | V4L2 probe tool committed | ☑ | `tools/m2m_probe.c`, `data/m2m_probe.txt` |
| 8 | Proposal submitted; metrics calibrated | ◐ metrics calibrated; proposal submission TODO confirm | §Metrics |

## A2 · Reference pipeline

- Pi: `scripts/ref_pi.sh <MAC_IP>`
- Mac: `scripts/recv_mac.sh gst` (receiver fixed to GStreamer for all measurements)
- Result: live 1280x720@30 video on the Mac.
- Issues:
  - First attempt failed on the Pi with `ERROR: *** no cameras available ***`
    (`rpicam-hello --list-cameras` also reported none). A reboot fixed it; root
    cause not identified (suspect the ribbon cable or a sensor probe failure at
    boot). If it recurs, check `dmesg | grep -iE 'imx708|unicam'` before rebooting.
  - When `rpicam-vid` fails, the script does not stop: `gst-launch-1.0` reads
    EOS and exits right away, so look for errors at the top of the output.
- Stability (PREEMPT_RT smoke test): ran for 10 min. `dmesg --level=err,warn`
  filtered for `BUG|sleeping function|scheduling while atomic|vchiq|unicam|codec`
  was empty. `vcgencmd get_throttled` was `0x0` before and after.

## A3 · Reference latency and CPU

Test conditions:

- Network: Wi-Fi (Pi → Mac ping RTT 11.7–91.0 ms, avg 47.4 ms)
- Mac display refresh rate: TODO Hz
- Receiver command: `scripts/recv_mac.sh gst`
- Sender: `SHUTTER=5000 scripts/ref_pi.sh 192.168.1.142` (1280x720@30, H.264 baseline, intra 30)
- Kernel: `6.18.58-rt9-v8+` (PREEMPT_RT); 10 min run with a clean dmesg (see §A2)
- `vcgencmd get_throttled`: `0x0` before and after the run

Method: the Pi camera points at `scripts/clock.html` on the Mac screen, and the received video
window sits next to the live clock. A phone photo captures both numbers, and
latency = live clock − clock seen in the video.

| Samples | Discarded | Median (ms) | Min (ms) | Max (ms) | Mean (ms) |
| --- | --- | --- | --- | --- | --- |
| 22 | 0 | **400** | 317 | 1102 | 441 |
| 20 (stalls excluded) | 2 | 400 | 317 | 434 | — (stdev 25) |

Notes:

- Readings are quantized by the 60 Hz display (16.7 ms steps). The phone exposure often
  spans two refreshes, which shows up as ghosted digits on the live clock. When that happened,
  the brighter value was used. Per-sample uncertainty is about ±17 ms.
- Two samples are stalls: #5 at 749 ms and #10 at 1102 ms. Both photos are readable, so
  they were kept in the main row. They are consistent with Wi-Fi bursts: the receiver uses
  `rtpjitterbuffer latency=0 ! ... sync=false`, so late packets are displayed late instead
  of being dropped.
- A steady-state latency of about 400 ms is about 12 frames at 30 fps, which is much more
  than capture, encode and Wi-Fi alone would explain. A likely contributor is frame-threaded
  decoding in `avdec_h264` on the Mac, which adds roughly one frame of delay per decoder
  thread. Next step: try `avdec_h264 max-threads=1`. If it lowers the median noticeably,
  fix the receiver at that setting and re-measure this reference before using it for comparison.

Raw data: `data/latency_ref.csv` (one row per photo, with the file name). Photos: `data/latency_photos/`.

CPU, measured with the pipeline running (`pidstat -u -C 'rpicam-vid|gst-launch' 1 60`;
%CPU is relative to one core, so the max is 400% on the 4-core Pi 4):

| Process | %usr | %system | Avg %CPU |
| --- | --- | --- | --- |
| rpicam-vid | 4.58 | 10.07 | 14.65 |
| gst-launch-1.0 | 6.01 | 1.85 | 7.86 |
| **Total** | 10.59 | 11.92 | **22.51** |

System-wide (`mpstat 1 60`, averaged over 4 cores):

| %usr | %sys | %iowait | %irq | %soft | %idle |
| --- | --- | --- | --- | --- | --- |
| 2.83 | 4.40 | 0.01 | 0.00 | 0.11 | 92.63 |

The system is 7.37% busy, which is about 29.5% of one core. That is roughly 7 points more
than the two processes add up to. The difference is kernel threads and background tasks:
under PREEMPT_RT, hard IRQs run as `irq/*` threads, so `%irq` reads 0.00 and that time is
counted in `%sys` instead. kworkers, vchiq threads, sshd and the desktop also contribute.
Use both numbers when comparing against the gateway later.

Observations:

- H.264 encoding runs on the VideoCore hardware encoder, so CPU cost is low.
  `rpicam-vid` already passes ISP dmabufs straight to the encoder without copying them.
- `rpicam-vid` is dominated by `%system` time: V4L2 queue/dequeue ioctls, dmabuf
  handling, and writing the bitstream into the pipe.
- `gst-launch-1.0` is dominated by `%usr` time: `h264parse` re-parses the bitstream and
  `rtph264pay` builds the RTP packets.
- What the gateway can actually save: the pipe between the two processes, the second
  parse of the bitstream, and the extra process. The CPU target should be calibrated
  against 22.5% of one core (see §Metrics).

Raw output: `data/pidstat_ref.txt`, `data/mpstat_ref.txt`.

## A4 · Format intersection

Commands: `v4l2-ctl -d /dev/video11 --list-formats-out`; `modetest -M vc4 -p`.
The libcamera `cam` tool is **not** shipped on this system (`libcamera-tools` only has
`qcam`, `ipa_verify`, `libcamera-bug-report`, `libcamerify`), so the libcamera column
comes from the `python3-libcamera` bindings instead.

| Format | libcamera | Encoder OUTPUT | DRM plane (id / primary or overlay) |
| --- | --- | --- | --- |
| YUV420 (`YU12`) | ✅ (default for VideoRecording) | ✅ | ✅ all primary and overlay planes, `LINEAR` only |
| NV12 | ✅ | ✅ | ✅ all primary and overlay planes, `LINEAR` + `BROADCOM_SAND64/128/256` |

Sources: `data/libcamera_formats.txt` (`python3-libcamera`, `generate_configuration()` for
the VideoRecording and Viewfinder roles), `data/m2m_probe.txt`, `data/modetest_planes.txt`.

- libcamera (rpi/vc4 ISP output) offers NV21, YUV420, NV12, YVU420, XBGR8888, BGR888,
  RGB888, XRGB8888, RGB565 and the 4:2:2 packed formats. The VideoRecording default is
  YUV420 1920x1080.
- Naming: libcamera `YUV420` = DRM/V4L2 fourcc `YU12` (3 planes: Y, U, V).
  `NV12` = Y plane plus an interleaved UV plane (2 planes).
- The intersection of all three columns is {YU12, YV12, NV12, NV21}. The two
  candidates are YU12 and NV12.

DRM planes (`modetest -M vc4 -p`, summarized):

| Planes | Type | possible_crtcs |
| --- | --- | --- |
| 48, 67, 79, 91, 103, 115 | Primary (one per CRTC) | 0x01, 0x02, 0x04, 0x08, 0x10, 0x20 |
| 127–292 (16 planes) | Overlay | 0x3e (CRTCs 1–5) |
| 303–644 (32 planes) | Overlay | 0x01 (CRTC 0 only) |
| 655–710 (6 planes) | Cursor | one per CRTC |

All primary and overlay planes accept YU12 and NV12 with the `LINEAR` modifier. libcamera
buffers are linear, so any of these planes can scan them out directly. The SAND (column)
modifiers apply only to NV12 and are not used here.

State when probed: `HDMI-A-1` and `HDMI-A-2` were both **disconnected**, every CRTC was
inactive (size 0x0), and no compositor (labwc / wayfire / Xorg / weston) was running.

**Chosen format**: `YUV420` / DRM `YU12` (3 planes). Fallback: `NV12`.

**Rationale**:

- It is in all three sets with the `LINEAR` modifier.
- It is libcamera's default for the VideoRecording role.
- It is the format `rpicam-vid` already passes to `/dev/video11` by dmabuf in the reference
  pipeline. Using the same encoder input keeps the A3 comparison fair: only the
  architecture changes.
- Risk: the encoder assumes the chroma stride is half the luma stride. At 1280 wide that
  is 640 B, which is 64-aligned, so it should hold. Verify against the real
  `stride[]` values printed in B3. If it does not hold, switch to NV12, whose UV
  plane has the same stride as Y.

**Display plane**: not fixed yet. It depends on which CRTC drives the connected HDMI port,
and that cannot be known while nothing is connected.

- Plan: pick the plane at runtime instead of hard-coding an id. Find the CRTC bound to
  the connected connector, then choose a plane whose `possible_crtcs` includes it and
  whose `type` fits.
- Primary vs overlay: no compositor is running, so the gateway can be DRM master and put
  the video on the CRTC's **primary** plane. If a console/desktop framebuffer must stay
  visible, use an overlay plane instead.
- TODO (display owner): connect an HDMI monitor (HDMI-A-1, the port next to USB-C), re-run
  `modetest -M vc4 -c -e -p`, and record the connector → CRTC → plane mapping here.

(Raw outputs saved under `data/`.)

## A5 · V4L2 probe

- Built standalone: `gcc -Wall -Wextra -o m2m_probe tools/m2m_probe.c`.
  Ran `./m2m_probe /dev/video11 | tee data/m2m_probe.txt`.
- Device: `bcm2835-codec-encode`. `M2M_MPLANE=1`, so the gateway must use the
  multi-planar API (`V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE` / `..._CAPTURE_MPLANE`).
- OUTPUT `DMABUF=1`. The encoder can import libcamera dmabufs directly, which is
  what zero-copy needs. CAPTURE also supports `DMABUF=1`, but the bitstream will use MMAP.
- OUTPUT formats include both `YU12` and `NV12`, the candidates for §A4. `NC12`
  (Broadcom column/SAND layout) is not considered.
- Repeat Sequence Header control present: yes (`0x009909e2`, default 0). Set it to 1
  so that SPS/PPS are re-sent with every IDR frame. This is the equivalent of
  `rpicam-vid --inline`, and a receiver needs it to join mid-stream.
- H264 Profile control present: yes (`0x00990a6b`, default 4 = High). Set it to 0
  (Baseline) to match the reference pipeline (`--profile baseline`).
- Other controls that matter:

  | Control | Range / default | Plan |
  | --- | --- | --- |
  | Video B Frames | max 0 | No B-frames, so no reorder delay. Nothing to set. |
  | H264 I-Frame Period / GOP Size | default 60 | Set to 30 to match `--intra 30`. |
  | Video Bitrate | 25 kbps – 25 Mbps, default 10 Mbps | Keep the same bitrate in the reference run and the gateway run. |
  | Video Bitrate Mode | 0 = VBR (default), 1 = CBR | TODO decide; record the choice. |
  | H264 Level | default 11 = Level 4.0 | Sufficient for 720p30 and 1080p30. |
  | Force Key Frame | button | Useful when a client joins. |

- Matches `v4l2-ctl -d /dev/video11 --list-formats-out --list-formats --list-ctrls`: yes.
  Both list the same 14 OUTPUT formats, the same 2 CAPTURE formats, and the same 12
  controls, with identical IDs, ranges and defaults. `v4l2-ctl` also reports both queues
  as "Multiplanar". What only the probe shows is the per-queue buffer capabilities from
  `REQBUFS count=0` (`DMABUF=1`), which is the reason this tool exists.

## A6 · SensorTimestamp clock source

**What the API promises.** From `src/libcamera/control_ids_core.yaml`, the `SensorTimestamp` entry:

> The time when the first row of the image sensor active array is exposed.
> The timestamp, expressed in nanoseconds, represents a monotonically
> increasing counter since the system boot time, as defined by the
> Linux-specific CLOCK_BOOTTIME clock id.

**What the code actually does.** Source checked on `raspberrypi/libcamera` `main` (2026-11):

1. `src/libcamera/v4l2_videodevice.cpp`, `V4L2VideoDevice::dequeueBuffer()`:
   `metadata.timestamp = buf.timestamp.tv_sec * 1000000000ULL + buf.timestamp.tv_usec * 1000ULL;`
   This copies the kernel's `v4l2_buffer` timestamp unchanged. The code never checks
   `V4L2_BUF_FLAG_TIMESTAMP_*` and never converts between clocks.
2. `src/libcamera/pipeline/rpi/vc4/vc4.cpp`, `Vc4CameraData::unicamBufferDequeue()`
   (Image stream branch):
   `uint64_t sensorTimestamp = buffer->metadata().timestamp;`
   `ctrl.set(controls::SensorTimestamp, sensorTimestamp);`
3. `src/libcamera/pipeline/rpi/common/pipeline_base.cpp`, `CameraData::fillRequestMetadata()`:
   copies `SensorTimestamp` from the buffer's controls into the request metadata.

So on the Pi, `SensorTimestamp` is whatever clock the **unicam kernel driver** stamps its
vb2 buffers with.

**Kernel driver.** The kernel config enables both unicam drivers
(`CONFIG_VIDEO_BCM2835_UNICAM=m` and `CONFIG_VIDEO_BCM2835_UNICAM_LEGACY=m`).
The module that is actually loaded is the upstream one:
`modinfo -n bcm2835_unicam` → `.../drivers/media/platform/broadcom/bcm2835-unicam.ko.xz`.
In `drivers/media/platform/broadcom/bcm2835-unicam.c` (source tree `/home/pi/linux`):

- line 2285: `q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;`
- line 800: `ts = ktime_get_ns();`. This is in the IRQ handler on frame start
  (`UNICAM_FSI`), and it is stored into `vb2_buf.timestamp`. The legacy driver
  (`platform/bcm2835/`, line 942) does the same.

`ktime_get_ns()` reads CLOCK_MONOTONIC. CLOCK_BOOTTIME = CLOCK_MONOTONIC + time spent in
suspend.

**Measurement** (`data/a6_clock.txt`). The command was
`rpicam-vid -t 1000 -n -o /dev/null --metadata - --metadata-format txt`; the clocks were
read with Python right after it exited:

| Value | ns |
| --- | --- |
| last `SensorTimestamp` | 9 029 322 546 000 |
| previous `SensorTimestamp` | 9 029 289 224 000 (Δ = 33.3 ms → 30 fps) |
| `CLOCK_MONOTONIC` | 9 029 803 853 903 (0.48 s later: rpicam-vid shutdown + Python start) |
| `CLOCK_BOOTTIME` | 9 029 803 855 588 |
| `CLOCK_REALTIME` | 1 794 972 323 229 139 045 (epoch-based, so ruled out) |
| BOOTTIME − MONOTONIC | 1 685 ns (just the gap between the two calls; no suspend time) |

Side observations:

- `SensorTimestamp` always ends in `000`. V4L2 hands timestamps to userspace as a
  `struct timeval`, so the resolution is 1 µs.
- The value is taken when the CSI receiver sees **frame start**, not when the first row
  starts exposing, which is what the libcamera docs describe. It therefore lags the true
  start of exposure by about the exposure time (5 ms with `SHUTTER=5000`) plus the
  first row's readout. This constant offset belongs in the latency budget.

**Conclusion**: `SensorTimestamp` is CLOCK_MONOTONIC on this system. The documentation
says CLOCK_BOOTTIME, but on a Pi that never suspends the two are numerically the same.
The other three timestamps use `clock_gettime(CLOCK_MONOTONIC)`, so it matches the actual
data source even if a suspend ever happened. `FRAME_CLOCK_ID` in `include/frame.h` is
set to `CLOCK_MONOTONIC`.

## Metrics (calibrated)

The original targets come from the 11/1 project context document (basic / stretch). The calibration rule set
there is: if the official pipeline already needs about 130 ms, change the end-to-end target to "close to the
official pipeline" and show the value of zero-copy through the CPU comparison and the latency breakdown instead.

| Metric | Original target (basic / stretch) | Calibrated target (basic / stretch) | Basis |
| --- | --- | --- | --- |
| Resolution and frame rate | 720p30 / 1080p30 | 720p30 / 1080p30 (unchanged; 1080p has low priority given the schedule) | Sensor mode 2304x1296@56 covers 1080p30 (env.md) |
| End-to-end glass-to-glass (median of 20) | ≤ 150 ms / ≤ 120 ms | **≤ reference median under identical conditions** / ≤ 150 ms absolute with a tuned receiver | A3: reference median **400 ms** (Wi-Fi, `recv_mac.sh gst`) |
| Application-level pixel copies | 0 / 0 | 0 / 0 (unchanged) | Code review; no `memcpy` hotspot in `perf` |
| CPU usage | Clearly below `gateway_baseline` / per-stage breakdown | Below `gateway_baseline` beyond run-to-run noise, **and** sender-side total ≤ reference 22.5% of one core / per-stage breakdown | A3: `rpicam-vid` 14.65 + `gst-launch` 7.86 = 22.5% (pidstat); 29.5% system-wide (mpstat) |
| Frame drop rate | < 1% over 30 min / < 0.1% | Unchanged | — |
| Long-run stability | 2 h with no fd or RSS growth / client reconnect without crash | Unchanged | — |
| Per-stage latency breakdown | capture, encode, send / + display submit | Unchanged; all stamps on `CLOCK_MONOTONIC` | A6: `TS_SENSOR` is frame start at the CSI receiver (1 µs resolution), lagging exposure start by about the shutter time |

Why each target changed:

- **End-to-end.** The reference median, 400 ms, is about 2.7× the original 150 ms target. The gateway uses the
  same sensor, ISP, hardware encoder, Wi-Fi link and Mac receiver. It removes only the pipe between two
  processes and a second `h264parse` pass, which is worth a few ms at most. That is below the ±17 ms reading
  error. An absolute ≤ 150 ms target is therefore not something the gateway itself can achieve. The basic target
  becomes "no worse than the official pipeline when measured the same way".
  - The 150 ms figure is kept as the stretch target, but only for a tuned receiver: `avdec_h264 max-threads=1`,
    preferably wired Ethernet. Tuned-receiver results are reported separately from the like-for-like comparison.
- **CPU.** One `memcpy` of a 1280×720 YUV420 frame is 1.38 MB, or about 41 MB/s at 30 fps. Expect the
  zero-copy vs. baseline gap to be only a few percent of one core. To show that it is real and not noise, run
  `pidstat -u 1 60` 3 times for each binary and report the spread. The stronger comparison is against the
  reference pipeline. The 22.5% reference does not include display, so the gateway's number should be reported
  both with and without display.
- **Pending change.** If the `avdec_h264 max-threads=1` check (§A3) changes the receiver, re-measure the reference
  median and update the 400 ms above. The targets are written relative to the reference, so they still apply.
