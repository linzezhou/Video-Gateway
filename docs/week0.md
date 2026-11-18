# Week 0 · Feasibility

## Checklist

| # | Criterion | Status | Evidence |
| --- | --- | --- | --- |
| 1 | Environment recorded; camera detected as imx708 | ☑ | [env.md](env.md) |
| 2 | Reference pipeline works | ☑ | §A2 |
| 3 | Reference latency (20 samples, median, test conditions) | ☑ | §A3, `data/latency_ref.csv` |
| 4 | Reference CPU (rpicam-vid and gst-launch averages) | ☑ | §A3, `data/pidstat_ref.txt`, `data/mpstat_ref.txt` |
| 5 | Formats: full list, chosen format and rationale, primary vs overlay | ☐ | §A4 |
| 6 | Clock source: conclusion and evidence | ☐ | §A6 |
| 7 | V4L2 probe tool committed | ☑ | `tools/m2m_probe.c`, `data/m2m_probe.txt` |
| 8 | Proposal submitted; metrics calibrated | ☐ | §Metrics |
| 9 | Both consent emails sent | ☐ | — |

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

Commands: `v4l2-ctl -d /dev/video11 --list-formats-out`; `modetest -M vc4 -p`; `cam -c 1 -I`

| Format | libcamera | Encoder OUTPUT | DRM plane (id / primary or overlay) |
| --- | --- | --- | --- |
| YUV420 (`YU12`) |  |  |  |
| NV12 |  |  |  |

**Chosen format**: TODO
**Rationale**: TODO
**Display plane**: TODO primary / overlay, plane id = TODO

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

- libcamera docs, quoted: TODO
- Source location (`src/libcamera/pipeline/rpi/...`): TODO
- Measurement: TODO (only used to rule out REALTIME)
- **Conclusion**: the other three timestamps use `clock_gettime(TODO)`; update `FRAME_CLOCK_ID` in `include/frame.h` to match

## Metrics (calibrated)

| Metric | Original target | Calibrated | Basis |
| --- | --- | --- | --- |
| End-to-end glass-to-glass |  | TODO (reference median: 400 ms, Wi-Fi) | A3 reference median |
| CPU (sender side, % of one core) |  | TODO (reference: 22.5) | A3 pidstat total |
