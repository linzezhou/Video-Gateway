/* Query a V4L2 M2M device (default /dev/video11, the H.264 encoder).
 * OUTPUT queue = raw frames we feed in; CAPTURE queue = H.264 we get back.
 * Reference: rpicam-apps encoder/h264_encoder.cpp. */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>

static void fourcc_str(uint32_t f, char s[5]) {
    s[0] = f & 0xff; s[1] = (f >> 8) & 0xff;
    s[2] = (f >> 16) & 0xff; s[3] = (f >> 24) & 0xff; s[4] = 0;
}

static void enum_fmts(int fd, enum v4l2_buf_type type, const char *name) {
    struct v4l2_fmtdesc d = { .type = type };
    printf("%s formats:\n", name);
    for (d.index = 0; ioctl(fd, VIDIOC_ENUM_FMT, &d) == 0; d.index++) {
        char s[5];
        fourcc_str(d.pixelformat, s);
        printf("  %s  %s\n", s, (const char *)d.description);
    }
}

static void query_bufcaps(int fd, enum v4l2_buf_type type, const char *name) {
    struct v4l2_requestbuffers rb = { .count = 0, .type = type,
                                      .memory = V4L2_MEMORY_MMAP };
    if (ioctl(fd, VIDIOC_REQBUFS, &rb) < 0) { perror("VIDIOC_REQBUFS"); return; }
    printf("%s buffer caps: MMAP=%d DMABUF=%d\n", name,
           !!(rb.capabilities & V4L2_BUF_CAP_SUPPORTS_MMAP),
           !!(rb.capabilities & V4L2_BUF_CAP_SUPPORTS_DMABUF));
}

static void list_ctrls(int fd) {
    struct v4l2_queryctrl q = { .id = V4L2_CTRL_FLAG_NEXT_CTRL };
    while (ioctl(fd, VIDIOC_QUERYCTRL, &q) == 0) {
        if (!(q.flags & V4L2_CTRL_FLAG_DISABLED) &&
            q.type != V4L2_CTRL_TYPE_CTRL_CLASS)
            printf("  0x%08x %-36s min=%d max=%d def=%d\n", q.id,
                   (const char *)q.name, q.minimum, q.maximum, q.default_value);
        q.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
}

int main(int argc, char **argv) {
    const char *dev = argc > 1 ? argv[1] : "/dev/video11";
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror(dev); return 1; }

    struct v4l2_capability cap = {0};
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) { perror("VIDIOC_QUERYCAP"); return 1; }
    uint32_t c = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps
                                                           : cap.capabilities;
    printf("driver=%s card=%s M2M_MPLANE=%d STREAMING=%d\n",
           (const char *)cap.driver, (const char *)cap.card,
           !!(c & V4L2_CAP_VIDEO_M2M_MPLANE), !!(c & V4L2_CAP_STREAMING));

    enum_fmts(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, "OUTPUT (raw frames in)");
    enum_fmts(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, "CAPTURE (H.264 out)");
    query_bufcaps(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, "OUTPUT");
    query_bufcaps(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, "CAPTURE");
    printf("controls:\n");
    list_ctrls(fd);
    close(fd);
    return 0;
}
