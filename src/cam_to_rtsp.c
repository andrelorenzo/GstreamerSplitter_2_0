#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <glib.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <strings.h>
#include <ctype.h>

typedef enum {
    CAM_BERTIN = 0U,
    CAM_IMX296,
    CAM_IMX296_VC,
    CAM_AR0234
} camera_type_e;

typedef enum {
    CAM_PORT_AUTO = 0U,
    CAM_PORT_MIPI0,
    CAM_PORT_MIPI1,
    CAM_PORT_USB
} camera_port_e;

typedef enum {
    CODEC_H264 = 0U,
    CODEC_H265
} codec_type_e;

typedef enum {
    IO_MODE_AUTO = 0U,
    IO_MODE_MMAP,
    IO_MODE_DMABUF
} io_mode_e;

typedef struct {
    /* RTSP */
    char mount[64];
    uint16_t port;
    uint16_t mtu;

    /* Camera */
    camera_type_e cam;
    camera_port_e cam_port;

    /*
     * Used by V4L2 cameras.
     * Example: /dev/video0
     */
    char device[64];

    /*
     * Optional libcamera camera-name.
     * Empty = let libcamera select the camera.
     */
    char camera_name[256];

    uint16_t width;
    uint16_t height;
    uint16_t fps;

    /*
     * Mainly used by VC Bayer cameras.
     * Example: rggb16le
     */
    char bayer_format[32];

    io_mode_e io_mode;

    /* Encoder */
    codec_type_e codec;
    uint32_t kbps;
    uint16_t key_int;

    char speed_preset[32];

    /* Low latency queues */
    uint16_t queue_buffers;

} Config;


/* -------------------------------------------------------------------------- */
/* DEFAULT CONFIG                                                              */
/* -------------------------------------------------------------------------- */

void configSetDefaults(Config *config)
{
    snprintf(config->mount, sizeof(config->mount), "/live0");

    config->port = 8000;
    config->mtu = 1200;

    config->cam = CAM_AR0234;
    config->cam_port = CAM_PORT_AUTO;

    config->device[0] = '\0';
    config->camera_name[0] = '\0';

    config->width = 1920;
    config->height = 1200;
    config->fps = 30;

    snprintf(config->bayer_format,
             sizeof(config->bayer_format),
             "rggb16le");

    config->io_mode = IO_MODE_MMAP;

    config->codec = CODEC_H264;
    config->kbps = 5000;
    config->key_int = 30;

    snprintf(config->speed_preset,
             sizeof(config->speed_preset),
             "ultrafast");

    config->queue_buffers = 1;
}


/* -------------------------------------------------------------------------- */
/* PROTOTYPES                                                                  */
/* -------------------------------------------------------------------------- */

bool parseConfig(const char *filename, Config *config);

char *GetEthIp(void);

const char *getCodec(codec_type_e codec);

const char *getCameraName(camera_type_e cam);

const char *getCameraPortName(camera_port_e port);

const char *getIoMode(io_mode_e mode);

bool buildPipeline(const Config *config, char *pipe, size_t pipe_size);


/* -------------------------------------------------------------------------- */
/* ENUM -> STRING                                                              */
/* -------------------------------------------------------------------------- */

const char *getCodec(codec_type_e codec)
{
    switch (codec) {
        case CODEC_H264: return "264";
        case CODEC_H265: return "265";
        default:         return "";
    }
}


const char *getCameraName(camera_type_e cam)
{
    switch (cam) {
        case CAM_BERTIN:    return "BERTIN";
        case CAM_IMX296:    return "IMX296";
        case CAM_IMX296_VC: return "IMX296_VC";
        case CAM_AR0234:    return "AR0234";
        default:            return "UNKNOWN";
    }
}


const char *getCameraPortName(camera_port_e port)
{
    switch (port) {
        case CAM_PORT_AUTO:  return "AUTO";
        case CAM_PORT_MIPI0: return "MIPI0";
        case CAM_PORT_MIPI1: return "MIPI1";
        case CAM_PORT_USB:   return "USB";
        default:             return "UNKNOWN";
    }
}


const char *getIoMode(io_mode_e mode)
{
    switch (mode) {
        case IO_MODE_MMAP:   return "mmap";
        case IO_MODE_DMABUF: return "dmabuf";
        case IO_MODE_AUTO:
        default:             return "auto";
    }
}


/* -------------------------------------------------------------------------- */
/* PIPELINE BUILDER                                                           */
/* -------------------------------------------------------------------------- */

bool buildPipeline(const Config *config, char *pipe, size_t pipe_size)
{
    const char *codec = getCodec(config->codec);

    if (codec[0] == '\0')
        return false;

    /*
     * x264enc and x265enc don't have exactly the same properties.
     * Keep codec-specific additions here.
     */
    const char *enc_extra =
        (config->codec == CODEC_H264)
            ? " bframes=0 byte-stream=true"
            : "";

    switch (config->cam) {

        /* ------------------------------------------------------------------ */
        /* BERTIN / generic V4L2 camera                                      */
        /* ------------------------------------------------------------------ */

        case CAM_BERTIN: {

            if (config->device[0] == '\0') {
                fprintf(stderr,
                        "CAM_BERTIN requires DEVICE=/dev/videoX\n");
                return false;
            }

            snprintf(
                pipe,
                pipe_size,

                "( "
                "v4l2src device=%s ! "
                "videoconvert ! "
                "video/x-raw,format=I420,"
                "width=%u,height=%u,framerate=%u/1 ! "

                "queue max-size-buffers=%u "
                "max-size-bytes=0 max-size-time=0 leaky=downstream ! "

                "x%senc "
                "tune=zerolatency "
                "speed-preset=%s "
                "bitrate=%u "
                "key-int-max=%u%s ! "

                "video/x-h%s,"
                "stream-format=byte-stream,"
                "alignment=au ! "

                "h%sparse config-interval=1 ! "

                "rtph%spay "
                "name=pay0 "
                "pt=96 "
                "config-interval=1 "
                "mtu=%u "
                ")",

                config->device,

                config->width,
                config->height,
                config->fps,

                config->queue_buffers,

                codec,
                config->speed_preset,
                config->kbps,
                config->key_int,
                enc_extra,

                codec,
                codec,
                codec,

                config->mtu
            );

            return true;
        }


        /* ------------------------------------------------------------------ */
        /* LIBCAMERA: AR0234 / native IMX296                                 */
        /* ------------------------------------------------------------------ */

        case CAM_AR0234:
        case CAM_IMX296: {

            char source[512];

            if (config->camera_name[0] != '\0') {

                snprintf(
                    source,
                    sizeof(source),
                    "libcamerasrc camera-name=\"%s\"",
                    config->camera_name
                );

            } else {

                snprintf(
                    source,
                    sizeof(source),
                    "libcamerasrc"
                );
            }

            snprintf(
                pipe,
                pipe_size,

                "( "
                "%s ! "

                "video/x-raw,"
                "format=NV12,"
                "width=%u,"
                "height=%u,"
                "framerate=%u/1 ! "

                "queue max-size-buffers=%u "
                "max-size-bytes=0 "
                "max-size-time=0 "
                "leaky=downstream ! "

                /*
                 * Keep videoconvert for compatibility.
                 * It can later be removed if the encoder accepts NV12
                 * directly on this installation.
                 */
                "videoconvert ! "

                "video/x-raw,"
                "format=I420,"
                "width=%u,"
                "height=%u,"
                "framerate=%u/1 ! "

                "queue max-size-buffers=%u "
                "max-size-bytes=0 "
                "max-size-time=0 "
                "leaky=downstream ! "

                "x%senc "
                "tune=zerolatency "
                "speed-preset=%s "
                "bitrate=%u "
                "key-int-max=%u%s ! "

                "video/x-h%s,"
                "stream-format=byte-stream,"
                "alignment=au ! "

                "queue max-size-buffers=2 "
                "max-size-bytes=0 "
                "max-size-time=0 "
                "leaky=downstream ! "

                "h%sparse config-interval=1 ! "

                "queue max-size-buffers=2 "
                "max-size-bytes=0 "
                "max-size-time=0 "
                "leaky=downstream ! "

                "rtph%spay "
                "name=pay0 "
                "pt=96 "
                "config-interval=1 "
                "mtu=%u "
                ")",

                source,

                config->width,
                config->height,
                config->fps,

                config->queue_buffers,

                config->width,
                config->height,
                config->fps,

                config->queue_buffers,

                codec,
                config->speed_preset,
                config->kbps,
                config->key_int,
                enc_extra,

                codec,
                codec,
                codec,

                config->mtu
            );

            return true;
        }


        /* ------------------------------------------------------------------ */
        /* VISION COMPONENTS IMX296C                                         */
        /* ------------------------------------------------------------------ */

        case CAM_IMX296_VC: {

            if (config->device[0] == '\0') {
                fprintf(stderr,
                        "CAM_IMX296_VC requires DEVICE=/dev/videoX\n");
                return false;
            }

            snprintf(
                pipe,
                pipe_size,

                "( "
                "v4l2src "
                "device=%s "
                "io-mode=%s ! "

                "video/x-bayer,"
                "format=%s,"
                "width=%u,"
                "height=%u,"
                "framerate=%u/1 ! "

                "queue "
                "max-size-buffers=%u "
                "max-size-bytes=0 "
                "max-size-time=0 "
                "leaky=downstream ! "

                "bayer2rgb ! "
                "videoconvert ! "

                "video/x-raw,"
                "format=I420,"
                "width=%u,"
                "height=%u,"
                "framerate=%u/1 ! "

                "queue "
                "max-size-buffers=%u "
                "max-size-bytes=0 "
                "max-size-time=0 "
                "leaky=downstream ! "

                "x%senc "
                "tune=zerolatency "
                "speed-preset=%s "
                "bitrate=%u "
                "key-int-max=%u%s ! "

                "video/x-h%s,"
                "stream-format=byte-stream,"
                "alignment=au ! "

                "h%sparse "
                "config-interval=1 ! "

                "rtph%spay "
                "name=pay0 "
                "pt=96 "
                "config-interval=1 "
                "mtu=%u "
                ")",

                config->device,
                getIoMode(config->io_mode),

                config->bayer_format,
                config->width,
                config->height,
                config->fps,

                config->queue_buffers,

                config->width,
                config->height,
                config->fps,

                config->queue_buffers,

                codec,
                config->speed_preset,
                config->kbps,
                config->key_int,
                enc_extra,

                codec,
                codec,
                codec,

                config->mtu
            );

            return true;
        }

        default:
            fprintf(stderr, "Camera not supported\n");
            return false;
    }
}


/* -------------------------------------------------------------------------- */
/* MAIN                                                                        */
/* -------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s <path-to-config>\n", argv[0]);
        return 1;
    }

    Config config;

    if (!parseConfig(argv[1], &config))
        return 1;

    char pipe[8192];

    if (!buildPipeline(&config, pipe, sizeof(pipe))) {
        fprintf(stderr, "Unable to build GStreamer pipeline\n");
        return 1;
    }

    gst_init(&argc, &argv);

    GstRTSPServer *server = gst_rtsp_server_new();

    char port_str[16];

    snprintf(
        port_str,
        sizeof(port_str),
        "%u",
        config.port
    );

    gst_rtsp_server_set_service(server, port_str);

    GstRTSPMountPoints *mounts =
        gst_rtsp_server_get_mount_points(server);

    GstRTSPMediaFactory *factory =
        gst_rtsp_media_factory_new();

    gst_rtsp_media_factory_set_launch(factory, pipe);

    gst_rtsp_media_factory_set_shared(factory, TRUE);

    gst_rtsp_mount_points_add_factory(
        mounts,
        config.mount,
        factory
    );

    g_object_unref(mounts);

    if (gst_rtsp_server_attach(server, NULL) == 0) {
        fprintf(stderr,
                "Error al inicializar el servidor RTSP.\n");
        g_object_unref(server);
        return 1;
    }

    char *ip = GetEthIp();

    printf("\n");
    printf("Camera    : %s\n", getCameraName(config.cam));
    printf("Port      : %s\n", getCameraPortName(config.cam_port));

    if (config.device[0] != '\0')
        printf("Device    : %s\n", config.device);

    if (config.camera_name[0] != '\0')
        printf("Libcamera : %s\n", config.camera_name);

    printf("Resolution: %ux%u @ %u fps\n",
           config.width,
           config.height,
           config.fps);

    printf("Codec     : H.%s\n",
           getCodec(config.codec));

    printf("Bitrate   : %u kbps\n",
           config.kbps);

    printf(
        "RTSP      : rtsp://%s:%u%s\n",
        ip ? ip : "0.0.0.0",
        config.port,
        config.mount
    );

    printf("\nPipeline:\n%s\n\n", pipe);

    free(ip);

    GMainLoop *loop =
        g_main_loop_new(NULL, FALSE);

    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    g_object_unref(server);

    return 0;
}


/* -------------------------------------------------------------------------- */
/* CONFIG PARSER                                                               */
/* -------------------------------------------------------------------------- */

static void trim(char *s)
{
    if (!s)
        return;

    /* Remove trailing whitespace / CR / LF */
    size_t n = strlen(s);

    while (n > 0 &&
           isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }

    /* Remove leading whitespace */
    char *start = s;

    while (*start &&
           isspace((unsigned char)*start)) {
        start++;
    }

    if (start != s)
        memmove(s, start, strlen(start) + 1);
}


bool parseConfig(const char *filename, Config *config)
{
    configSetDefaults(config);

    FILE *fp = fopen(filename, "r");

    if (!fp) {
        perror("No se pudo abrir el archivo de configuración");
        return false;
    }

    char line[512];

    while (fgets(line, sizeof(line), fp)) {

        trim(line);

        if (line[0] == '#' ||
            line[0] == '\0')
            continue;

        char *separator = strchr(line, '=');

        if (!separator)
            continue;

        *separator = '\0';

        char *key = line;
        char *value = separator + 1;

        trim(key);
        trim(value);


        /* -------------------------------------------------------------- */
        /* RTSP                                                           */
        /* -------------------------------------------------------------- */

        if (strcasecmp(key, "MOUNT") == 0) {

            if (value[0] == '/') {
                snprintf(
                    config->mount,
                    sizeof(config->mount),
                    "%s",
                    value
                );
            } else {
                snprintf(
                    config->mount,
                    sizeof(config->mount),
                    "/%s",
                    value
                );
            }

        } else if (strcasecmp(key, "PORT") == 0) {

            config->port = (uint16_t)atoi(value);

        } else if (strcasecmp(key, "MTU") == 0) {

            config->mtu = (uint16_t)atoi(value);


        /* -------------------------------------------------------------- */
        /* CAMERA                                                         */
        /* -------------------------------------------------------------- */

        } else if (strcasecmp(key, "CAM") == 0) {

            if (strcasecmp(value, "bertin") == 0) {
                config->cam = CAM_BERTIN;

            } else if (strcasecmp(value, "imx296") == 0) {
                config->cam = CAM_IMX296;

            } else if (strcasecmp(value, "imx296_vc") == 0) {
                config->cam = CAM_IMX296_VC;

            } else if (strcasecmp(value, "ar0234") == 0) {
                config->cam = CAM_AR0234;

            } else {
                fprintf(stderr,
                        "CAM desconocida: %s\n",
                        value);

                fclose(fp);
                return false;
            }

        } else if (strcasecmp(key, "CAM_PORT") == 0) {

            if (strcasecmp(value, "auto") == 0) {
                config->cam_port = CAM_PORT_AUTO;

            } else if (strcasecmp(value, "mipi0") == 0 ||
                       strcasecmp(value, "cam0") == 0) {
                config->cam_port = CAM_PORT_MIPI0;

            } else if (strcasecmp(value, "mipi1") == 0 ||
                       strcasecmp(value, "cam1") == 0) {
                config->cam_port = CAM_PORT_MIPI1;

            } else if (strcasecmp(value, "usb") == 0) {
                config->cam_port = CAM_PORT_USB;

            } else {
                fprintf(stderr,
                        "CAM_PORT desconocido: %s\n",
                        value);

                fclose(fp);
                return false;
            }

        } else if (strcasecmp(key, "DEVICE") == 0) {

            snprintf(
                config->device,
                sizeof(config->device),
                "%s",
                value
            );

        } else if (strcasecmp(key, "CAMERA_NAME") == 0) {

            snprintf(
                config->camera_name,
                sizeof(config->camera_name),
                "%s",
                value
            );

        } else if (strcasecmp(key, "WIDTH") == 0) {

            config->width = (uint16_t)atoi(value);

        } else if (strcasecmp(key, "HEIGHT") == 0) {

            config->height = (uint16_t)atoi(value);

        } else if (strcasecmp(key, "FPS") == 0) {

            config->fps = (uint16_t)atoi(value);

        } else if (strcasecmp(key, "BAYER_FORMAT") == 0) {

            snprintf(
                config->bayer_format,
                sizeof(config->bayer_format),
                "%s",
                value
            );

        } else if (strcasecmp(key, "IO_MODE") == 0) {

            if (strcasecmp(value, "mmap") == 0) {
                config->io_mode = IO_MODE_MMAP;

            } else if (strcasecmp(value, "dmabuf") == 0) {
                config->io_mode = IO_MODE_DMABUF;

            } else {
                config->io_mode = IO_MODE_AUTO;
            }


        /* -------------------------------------------------------------- */
        /* ENCODER                                                        */
        /* -------------------------------------------------------------- */

        } else if (strcasecmp(key, "CODEC") == 0) {

            if (strcasecmp(value, "265") == 0 ||
                strcasecmp(value, "h265") == 0) {

                config->codec = CODEC_H265;

            } else if (strcasecmp(value, "264") == 0 ||
                       strcasecmp(value, "h264") == 0) {

                config->codec = CODEC_H264;

            } else {
                fprintf(stderr,
                        "CODEC desconocido: %s\n",
                        value);

                fclose(fp);
                return false;
            }

        } else if (strcasecmp(key, "KBPS") == 0) {

            config->kbps = (uint32_t)strtoul(
                value,
                NULL,
                10
            );

        } else if (strcasecmp(key, "KEYINT") == 0) {

            config->key_int =
                (uint16_t)atoi(value);

        } else if (strcasecmp(key, "SPEED_PRESET") == 0) {

            snprintf(
                config->speed_preset,
                sizeof(config->speed_preset),
                "%s",
                value
            );

        } else if (strcasecmp(key, "QUEUE_BUFFERS") == 0) {

            config->queue_buffers =
                (uint16_t)atoi(value);

        } else {

            fprintf(
                stderr,
                "Parámetro no reconocido: %s\n",
                key
            );
        }
    }

    fclose(fp);


    /* ------------------------------------------------------------------ */
    /* VALIDATION                                                         */
    /* ------------------------------------------------------------------ */

    if (config->mount[0] == '\0') {
        fprintf(stderr,
                "MOUNT no puede estar vacío\n");
        return false;
    }

    if (config->width == 0 ||
        config->height == 0 ||
        config->fps == 0) {

        fprintf(stderr,
                "WIDTH, HEIGHT y FPS deben ser > 0\n");
        return false;
    }

    if (config->kbps == 0) {
        fprintf(stderr,
                "KBPS debe ser > 0\n");
        return false;
    }

    if (config->queue_buffers == 0)
        config->queue_buffers = 1;

    return true;
}


/* -------------------------------------------------------------------------- */
/* IP GETTER                                                                   */
/* -------------------------------------------------------------------------- */

char *GetEthIp(void)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;

    char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1)
        return NULL;

    for (ifa = ifaddr;
         ifa != NULL;
         ifa = ifa->ifa_next) {

        if (ifa->ifa_addr == NULL)
            continue;

        if (strcmp(
                ifa->ifa_name,
                "eth0") == 0 &&
            ifa->ifa_addr->sa_family == AF_INET) {

            struct sockaddr_in *addr =
                (struct sockaddr_in *)
                ifa->ifa_addr;

            if (inet_ntop(
                    AF_INET,
                    &addr->sin_addr,
                    ip,
                    sizeof(ip)) == NULL) {

                freeifaddrs(ifaddr);
                return NULL;
            }

            freeifaddrs(ifaddr);

            return strdup(ip);
        }
    }

    freeifaddrs(ifaddr);

    return NULL;
}