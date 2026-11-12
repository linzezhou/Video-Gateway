PRETTY_NAME="Debian GNU/Linux 13 (trixie)"
NAME="Debian GNU/Linux"
VERSION_ID="13"
VERSION="13 (trixie)"
VERSION_CODENAME=trixie
DEBIAN_VERSION_FULL=13.6
ID=debian
HOME_URL="https://www.debian.org/"
SUPPORT_URL="https://www.debian.org/support"
BUG_REPORT_URL="https://bugs.debian.org/"

Kernel Version : 6.18.58-rt9-v8+

ii  gstreamer1.0-tools                   1.26.2-2                             arm64        Tools for use with GStreamer
ii  libcamera-dev                        0.7.2+rpt20261009-1                  arm64        complex camera support library (development files)
ii  libcamera-ipa:arm64                  0.7.2+rpt20261009-1                  arm64        complex camera support library (IPA modules)
ii  libcamera-tools                      0.7.2+rpt20261009-1                  arm64        complex camera support library (tools)
ii  libcamera-v4l2:arm64                 0.7.2+rpt20261009-1                  arm64        complex camera support library (V4L2 module)
ii  libcamera0.7:arm64                   0.7.2+rpt20261009-1                  arm64        complex camera support library
ii  librpicam-app1:arm64                 1.14.0-1                             arm64        Framework for rpicam-apps camera applications
ii  libspa-0.2-libcamera:arm64           1.4.2-1+rpt3                         arm64        libraries for the PipeWire multimedia server - libcamera plugin
ii  pipewire-libcamera                   1.4.2-1+rpt3                         all          transitional package for libspa-0.2-libcamera
ii  python3-libcamera                    0.7.2+rpt20261009-1                  arm64        complex camera support library (Python bindings)
ii  rpicam-apps                          1.14.0-1                             all          Camera based applications for Raspberry Pi using the libcamera framework
ii  rpicam-apps-core                     1.14.0-1                             arm64        Camera based applications for Raspberry Pi using the libcamera framework
ii  rpicam-apps-encoder:arm64            1.14.0-1                             arm64        Framework for rpicam-apps camera applications
ii  rpicam-apps-lite                     1.14.0-1                             all          transitional package
ii  rpicam-apps-opencv-postprocess       1.14.0-1                             arm64        Framework for rpicam-apps camera applications
ii  rpicam-apps-preview:arm64            1.14.0-1                             arm64        Framework for rpicam-apps camera applications

pi@ksk94-lz674:~ $ rpicam-hello --list-cameras
Available cameras
-----------------
0 : imx708 [4608x2592 10-bit RGGB] (/base/soc/i2c0mux/i2c@1/imx708@1a)
    Modes: 'SRGGB10_CSI2P' : 1536x864 [120.13 fps - (768, 432)/3072x1728 crop]
                             2304x1296 [56.03 fps - (0, 0)/4608x2592 crop]
                             4608x2592 [14.35 fps - (0, 0)/4608x2592 crop]

pi@ksk94-lz674:~/gateway/Video-Gateway/data $ v4l2-ctl --list-devices
bcm2835-codec-decode (platform:bcm2835-codec):
        /dev/video10
        /dev/video11
        /dev/video12
        /dev/video18
        /dev/video31

bcm2835-isp (platform:bcm2835-isp):
        /dev/video13
        /dev/video14
        /dev/video15
        /dev/video16
        /dev/video20
        /dev/video21
        /dev/video22
        /dev/video23
        /dev/media2
        /dev/media3

unicam (platform:fe801000.csi):
        /dev/video0
        /dev/video1
        /dev/media0

rpi-hevc-dec (platform:rpi-hevc-dec):
        /dev/video19
        /dev/media1

bcm2835-codec (vchiq:bcm2835-codec):
        /dev/media4