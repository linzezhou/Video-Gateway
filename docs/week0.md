# Week 0 · Feasibility

## Checklist

| # | Criterion | Status | Evidence |
| --- | --- | --- | --- |
| 1 | Environment recorded; camera detected as imx708 | ☑ | [env.md](env.md) |
| 2 | Reference pipeline works | ☐ | §A2 |
| 3 | Reference latency (20 samples, median, test conditions) | ☐ | §A3, `data/latency_ref.csv` |
| 4 | Reference CPU (rpicam-vid and gst-launch averages) | ☐ | §A3 |
| 5 | Formats: full list, chosen format and rationale, primary vs overlay | ☐ | §A4 |
| 6 | Clock source: conclusion and evidence | ☐ | §A6 |
| 7 | V4L2 probe tool committed | ☐ | `tools/m2m_probe.c`, `data/m2m_probe.txt` |
| 8 | Proposal submitted; metrics calibrated | ☐ | §Metrics |
| 9 | Both consent emails sent | ☐ | — |

## A2 · Reference pipeline

- Pi: `scripts/ref_pi.sh <MAC_IP>`
- Mac: `scripts/recv_mac.sh` (receiver fixed for all measurements: TODO gst / ffplay)
- Result: TODO (live video on the Mac? issues hit)

## A3 · Reference latency and CPU

Test conditions:

- Network: TODO wired / Wi-Fi
- Mac display refresh rate: TODO Hz
- Receiver command: TODO
- Shutter: `SHUTTER=5000`
- Kernel: TODO (RT kernel or not)
- `vcgencmd get_throttled`: TODO

| Samples | Discarded | Median (ms) | Min (ms) | Max (ms) |
| --- | --- | --- | --- | --- |
| 20 |  |  |  |  |

Raw data: `data/latency_ref.csv`. Photos: `data/latency_photos/`.

CPU (`pidstat -u 1 60`):

| Process | Avg %CPU |
| --- | --- |
| rpicam-vid |  |
| gst-launch-1.0 |  |

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

- `./build/m2m_probe /dev/video11 > data/m2m_probe.txt`
- OUTPUT `DMABUF=`: TODO
- Repeat Sequence Header control present: TODO
- H264 Profile control present: TODO
- Matches `v4l2-ctl --list-formats-out --list-formats --list-ctrls`: TODO

## A6 · SensorTimestamp clock source

- libcamera docs, quoted: TODO
- Source location (`src/libcamera/pipeline/rpi/...`): TODO
- Measurement: TODO (only used to rule out REALTIME)
- **Conclusion**: the other three timestamps use `clock_gettime(TODO)`; update `FRAME_CLOCK_ID` in `include/frame.h` to match

## Metrics (calibrated)

| Metric | Original target | Calibrated | Basis |
| --- | --- | --- | --- |
| End-to-end glass-to-glass |  |  | A3 reference median |
