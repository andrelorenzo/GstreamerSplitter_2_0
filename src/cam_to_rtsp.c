#define _POSIX_C_SOURCE 200809L
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <glib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef enum { CAM_BERTIN, CAM_IMX296, CAM_IMX296_VC, CAM_AR0234, CAM_IMX900 } camera_type_e;
typedef enum { CAM_PORT_AUTO, CAM_PORT_MIPI0, CAM_PORT_MIPI1, CAM_PORT_USB } camera_port_e;
typedef enum { CODEC_H264, CODEC_H265 } codec_type_e;
typedef enum { IO_MODE_AUTO, IO_MODE_MMAP, IO_MODE_DMABUF } io_mode_e;

typedef struct {
    char mount[64], device[64], camera_name[256], bayer_format[32], speed_preset[32];
    uint16_t port, mtu, width, height, fps, key_int, queue_buffers;
    uint32_t kbps;
    camera_type_e cam;
    camera_port_e cam_port;
    codec_type_e codec;
    io_mode_e io_mode;
    unsigned int camera_index;
    bool camera_index_set, camera_name_from_index;
} Config;

static const char *cameraName(camera_type_e cam)
{
    switch (cam) {
        case CAM_BERTIN: return "BERTIN";
        case CAM_IMX296: return "IMX296";
        case CAM_IMX296_VC: return "IMX296_VC";
        case CAM_AR0234: return "AR0234";
        case CAM_IMX900: return "IMX900";
        default: return "UNKNOWN";
    }
}

static const char *portName(camera_port_e port)
{
    switch (port) {
        case CAM_PORT_MIPI0: return "MIPI0";
        case CAM_PORT_MIPI1: return "MIPI1";
        case CAM_PORT_USB: return "USB";
        default: return "AUTO";
    }
}

static void trim(char *s)
{
    char *p = s;
    size_t n;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static bool parseConfig(const char *filename, Config *c)
{
    FILE *fp;
    char line[512];
    bool width_set = false, height_set = false, fps_set = false;
    *c = (Config){ .port = 8000, .mtu = 1200, .cam = CAM_AR0234, .width = 1920,
                   .height = 1200, .fps = 30, .io_mode = IO_MODE_MMAP,
                   .codec = CODEC_H264, .kbps = 5000, .key_int = 30, .queue_buffers = 1 };
    snprintf(c->mount, sizeof(c->mount), "/live0");
    snprintf(c->bayer_format, sizeof(c->bayer_format), "rggb16le");
    snprintf(c->speed_preset, sizeof(c->speed_preset), "ultrafast");
    fp = fopen(filename, "r");
    if (!fp) { perror(filename); return false; }
    while (fgets(line, sizeof(line), fp)) {
        char *key, *value, *sep;
        trim(line);
        if (!line[0] || line[0] == '#') continue;
        sep = strchr(line, '=');
        if (!sep) continue;
        *sep = '\0'; key = line; value = sep + 1; trim(key); trim(value);
        if (!strcasecmp(key, "MOUNT")) snprintf(c->mount, sizeof(c->mount), "%s%s", value[0] == '/' ? "" : "/", value);
        else if (!strcasecmp(key, "PORT")) c->port = (uint16_t)atoi(value);
        else if (!strcasecmp(key, "MTU")) c->mtu = (uint16_t)atoi(value);
        else if (!strcasecmp(key, "CAM")) {
            if (!strcasecmp(value, "bertin")) c->cam = CAM_BERTIN;
            else if (!strcasecmp(value, "imx296")) c->cam = CAM_IMX296;
            else if (!strcasecmp(value, "imx296_vc")) c->cam = CAM_IMX296_VC;
            else if (!strcasecmp(value, "ar0234")) c->cam = CAM_AR0234;
            else if (!strcasecmp(value, "imx900")) c->cam = CAM_IMX900;
            else { fprintf(stderr, "CAM desconocida: %s\n", value); fclose(fp); return false; }
        } else if (!strcasecmp(key, "CAM_PORT")) {
            if (!strcasecmp(value, "auto")) c->cam_port = CAM_PORT_AUTO;
            else if (!strcasecmp(value, "mipi0") || !strcasecmp(value, "cam0")) c->cam_port = CAM_PORT_MIPI0;
            else if (!strcasecmp(value, "mipi1") || !strcasecmp(value, "cam1")) c->cam_port = CAM_PORT_MIPI1;
            else if (!strcasecmp(value, "usb")) c->cam_port = CAM_PORT_USB;
            else { fprintf(stderr, "CAM_PORT desconocido: %s\n", value); fclose(fp); return false; }
        } else if (!strcasecmp(key, "DEVICE")) snprintf(c->device, sizeof(c->device), "%s", value);
        else if (!strcasecmp(key, "CAMERA_NAME")) snprintf(c->camera_name, sizeof(c->camera_name), "%s", value);
        else if (!strcasecmp(key, "CAMERA_INDEX")) {
            char *end;
            unsigned long index = strtoul(value, &end, 10);
            if (!value[0] || *end || index > G_MAXUINT) {
                fprintf(stderr, "CAMERA_INDEX debe ser un entero >= 0: %s\n", value);
                fclose(fp);
                return false;
            }
            c->camera_index = (unsigned int)index;
            c->camera_index_set = true;
        }
        else if (!strcasecmp(key, "WIDTH")) { c->width = (uint16_t)atoi(value); width_set = true; }
        else if (!strcasecmp(key, "HEIGHT")) { c->height = (uint16_t)atoi(value); height_set = true; }
        else if (!strcasecmp(key, "FPS")) { c->fps = (uint16_t)atoi(value); fps_set = true; }
        else if (!strcasecmp(key, "BAYER_FORMAT")) snprintf(c->bayer_format, sizeof(c->bayer_format), "%s", value);
        else if (!strcasecmp(key, "IO_MODE")) c->io_mode = !strcasecmp(value, "mmap") ? IO_MODE_MMAP : !strcasecmp(value, "dmabuf") ? IO_MODE_DMABUF : IO_MODE_AUTO;
        else if (!strcasecmp(key, "CODEC")) {
            if (!strcasecmp(value, "264") || !strcasecmp(value, "h264")) c->codec = CODEC_H264;
            else if (!strcasecmp(value, "265") || !strcasecmp(value, "h265")) c->codec = CODEC_H265;
            else { fprintf(stderr, "CODEC desconocido: %s\n", value); fclose(fp); return false; }
        } else if (!strcasecmp(key, "KBPS")) c->kbps = (uint32_t)strtoul(value, NULL, 10);
        else if (!strcasecmp(key, "KEYINT")) c->key_int = (uint16_t)atoi(value);
        else if (!strcasecmp(key, "SPEED_PRESET")) snprintf(c->speed_preset, sizeof(c->speed_preset), "%s", value);
        else if (!strcasecmp(key, "QUEUE_BUFFERS")) c->queue_buffers = (uint16_t)atoi(value);
        else fprintf(stderr, "Parámetro no reconocido: %s\n", key);
    }
    fclose(fp);
    if (c->cam == CAM_IMX900) {
        if (!width_set) c->width = 1280;
        if (!height_set) c->height = 1080;
        if (!fps_set) c->fps = 10;
    }
    if (!c->port || !c->mtu || !c->width || !c->height || !c->fps || !c->kbps || !c->key_int) {
        fprintf(stderr, "PORT, MTU, WIDTH, HEIGHT, FPS, KBPS y KEYINT deben ser > 0\n");
        return false;
    }
    if (!c->queue_buffers) c->queue_buffers = 1;
    return true;
}

static bool buildPipeline(const Config *c, char *pipe, size_t size)
{
    char source[1024];
    const char *codec = c->codec == CODEC_H264 ? "264" : "265";
    const char *extra = c->codec == CODEC_H264 ? " bframes=0 byte-stream=true" : "";
    int n;
    if (c->cam == CAM_BERTIN || c->cam == CAM_IMX296_VC) {
        if (!c->device[0]) { fprintf(stderr, "%s requiere DEVICE=/dev/videoX\n", cameraName(c->cam)); return false; }
        if (c->cam == CAM_BERTIN) n = snprintf(source, sizeof(source), "v4l2src device=%s ! videoconvert ! video/x-raw,format=I420,width=%u,height=%u,framerate=%u/1", c->device, c->width, c->height, c->fps);
        else n = snprintf(source, sizeof(source), "v4l2src device=%s io-mode=%s ! video/x-bayer,format=%s,width=%u,height=%u,framerate=%u/1 ! bayer2rgb ! videoconvert ! video/x-raw,format=I420,width=%u,height=%u,framerate=%u/1", c->device, c->io_mode == IO_MODE_MMAP ? "mmap" : c->io_mode == IO_MODE_DMABUF ? "dmabuf" : "auto", c->bayer_format, c->width, c->height, c->fps, c->width, c->height, c->fps);
    } else {
        char camera[520] = "";
        if (c->camera_index_set && c->camera_name[0] && !c->camera_name_from_index) {
            fprintf(stderr, "Usa CAMERA_INDEX o CAMERA_NAME, no ambos\n");
            return false;
        }
        if (c->camera_name[0]) {
            char *escaped = g_strescape(c->camera_name, NULL);
            if (!escaped) return false;
            snprintf(camera, sizeof(camera), " camera-name=\"%s\"", escaped);
            g_free(escaped);
        }
        n = snprintf(source, sizeof(source), "libcamerasrc%s ! video/x-raw,colorimetry=bt709,format=NV12,width=%u,height=%u,framerate=%u/1 ! videoconvert ! video/x-raw,format=I420,width=%u,height=%u,framerate=%u/1", camera, c->width, c->height, c->fps, c->width, c->height, c->fps);
    }
    if (n < 0 || (size_t)n >= sizeof(source)) return false;
    n = snprintf(pipe, size, "( %s ! queue max-size-buffers=%u max-size-bytes=0 max-size-time=0 leaky=downstream ! x%senc tune=zerolatency speed-preset=%s bitrate=%u key-int-max=%u%s ! video/x-h%s,stream-format=byte-stream,alignment=au ! h%sparse config-interval=1 ! rtph%spay name=pay0 pt=96 config-interval=1 mtu=%u )", source, c->queue_buffers, codec, c->speed_preset, c->kbps, c->key_int, extra, codec, codec, codec, c->mtu);
    return n >= 0 && (size_t)n < size;
}

/* libcamerasrc in the Raspberry Pi image selects cameras by camera-name,
 * not by index. Resolve CAMERA_INDEX through GstDeviceMonitor first. */
static bool resolveCameraIndex(Config *c)
{
    GstDeviceProvider *provider;
    GList *devices, *item;
    unsigned int index = 0;
    bool found = false;

    if (!c->camera_index_set) return true;
    if (c->camera_name[0]) {
        fprintf(stderr, "Usa CAMERA_INDEX o CAMERA_NAME, no ambos\n");
        return false;
    }

    provider = gst_device_provider_factory_get_by_name("libcameraprovider");
    if (!provider) {
        fprintf(stderr, "No se encontró el proveedor de cámaras libcamera de GStreamer\n");
        return false;
    }
    if (!gst_device_provider_start(provider)) {
        fprintf(stderr, "No se pudo iniciar la enumeración de cámaras libcamera\n");
        if (provider) gst_object_unref(provider);
        return false;
    }

    devices = gst_device_provider_get_devices(provider);
    for (item = devices; item; item = item->next) {
        GstDevice *device = GST_DEVICE(item->data);
        const gchar *name = gst_device_get_display_name(device);
        if (index == c->camera_index) {
            snprintf(c->camera_name, sizeof(c->camera_name), "%s", name ? name : "");
            found = c->camera_name[0] != '\0';
            c->camera_name_from_index = found;
            break;
        }
        index++;
    }

    if (!found) {
        fprintf(stderr, "CAMERA_INDEX=%u fuera de rango; libcamera expone %u cámara(s)\n",
                c->camera_index, index);
    }
    g_list_free_full(devices, gst_object_unref);
    gst_device_provider_stop(provider);
    gst_object_unref(provider);
    return found;
}

static char *ethIp(void)
{
    struct ifaddrs *list, *ifa;
    char ip[INET_ADDRSTRLEN], *result = NULL;
    if (getifaddrs(&list) < 0) return NULL;
    for (ifa = list; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && !strcmp(ifa->ifa_name, "eth0") && ifa->ifa_addr->sa_family == AF_INET && inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, ip, sizeof(ip))) { result = strdup(ip); break; }
    }
    freeifaddrs(list);
    return result;
}

int main(int argc, char **argv)
{
    Config c;
    char pipe[8192], service[16], *ip;
    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;
    GMainLoop *loop;
    if (argc != 2) { fprintf(stderr, "Uso: %s <config>\n", argv[0]); return 1; }
    gst_init(&argc, &argv);
    if (!parseConfig(argv[1], &c) || !resolveCameraIndex(&c) || !buildPipeline(&c, pipe, sizeof(pipe))) return 1;
    server = gst_rtsp_server_new();
    snprintf(service, sizeof(service), "%u", c.port);
    gst_rtsp_server_set_service(server, service);
    mounts = gst_rtsp_server_get_mount_points(server);
    factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory, pipe);
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    gst_rtsp_mount_points_add_factory(mounts, c.mount, factory);
    g_object_unref(mounts);
    if (!gst_rtsp_server_attach(server, NULL)) { fprintf(stderr, "Error al inicializar el servidor RTSP\n"); g_object_unref(server); return 1; }
    ip = ethIp();
    printf("Camera: %s | Port: %s | %ux%u @ %u fps | H.%s | %u kbps\n", cameraName(c.cam), portName(c.cam_port), c.width, c.height, c.fps, c.codec == CODEC_H264 ? "264" : "265", c.kbps);
    if (c.device[0]) printf("Device: %s\n", c.device);
    if (c.camera_name[0]) printf("Libcamera: %s\n", c.camera_name);
    if (c.camera_index_set) printf("Libcamera index: %u\n", c.camera_index);
    printf("RTSP: rtsp://%s:%u%s\nPipeline: %s\n", ip ? ip : "0.0.0.0", c.port, c.mount, pipe);
    free(ip);
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    g_object_unref(server);
    return 0;
}
