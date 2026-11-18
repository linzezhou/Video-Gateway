# Video Gateway

Zero-copy, low-latency video gateway on Raspberry Pi: libcamera → dmabuf → (H.264 encoder, DRM display).

## Build (on the Pi)

```bash
cmake -B build && cmake --build build                         # performance runs
cmake -B build-asan -DASAN=ON && cmake --build build-asan     # day-to-day development
```

Targets: `gateway` (zero-copy), `gateway_baseline` (same code + one memcpy per frame), `m2m_probe`.

## Measured buffer layout

TODO(B3/B6): format, stride per plane, number of distinct dmabuf fds, offset of each plane, selected sensor mode.

## Initial CPU comparison

TODO(B7)
