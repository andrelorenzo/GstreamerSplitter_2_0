#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <glib.h>          // para gboolean, TRUE/FALSE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stdint.h"
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <strings.h>

typedef enum {
    CAM_BERTIN = 0U,
    CAM_IMX296
}camera_type_e;

typedef enum {
    CODEC_H264 = 0U,
    CODEC_H265
}codec_type_e;

typedef struct{
    char mount[64];
    uint16_t port;
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    codec_type_e codec;
    camera_type_e cam;
    uint8_t cam_idx;

    uint16_t kbps;
    int key_int;
} Config;


void configSetDefaults(Config * config){
    snprintf(config->mount, sizeof(config->mount), "/live0");
    config->port = 8000;
    config->width = 640;
    config->height = 480;
    config->fps = 30;
    config->codec = CODEC_H264;
    config->cam = CAM_IMX296;
    config->cam_idx = 0;

    config->kbps = 500;
    config->key_int = 5;
}



bool parseConfig(const char * filename, Config * config);
char *GetEthIp(void);

const char *getCodec(const Config config)
{
    switch (config.codec)
    {
        case CODEC_H264: return "264";
        case CODEC_H265: return "265";
        default: return "";
    }
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <path-to-file>\n", argv[0]);
        return 1;
    }

    Config config;
    if (!parseConfig(argv[1], &config)) return 1;

    const char *codec = getCodec(config);
    char pipe[8192];

        switch(config.cam){
        case CAM_BERTIN:{
            snprintf(pipe, sizeof(pipe), "( v4l2src device=/dev/video%d ! videoconvert ! video/x-raw,format=I420,width=%d,height=%d,framerate=%d/1 ! "
                "x%senc tune=zerolatency bitrate=%d key-int-max=%d ! h%sparse config-interval=1 ! rtph%spay name=pay0 pt=96 config-interval=1 )", 
                config.cam_idx, config.width, config.height, config.fps, codec, config.kbps, config.key_int, codec, codec);
        }break;

        case CAM_IMX296:{
            const char *enc_extra = (config.codec == CODEC_H264) ? " bframes=0 byte-stream=true" : "";
            snprintf(pipe, sizeof(pipe), "( libcamerasrc ! video/x-raw,format=NV12,width=%d,height=%d,framerate=%d/1 ! "
            "queue max-size-buffers=1 max-size-bytes=0 max-size-time=0 leaky=downstream ! videoconvert ! video/x-raw,format=I420,width=%d,height=%d,framerate=%d/1 ! "
            "queue max-size-buffers=1 max-size-bytes=0 max-size-time=0 leaky=downstream ! x%senc tune=zerolatency speed-preset=ultrafast bitrate=%d key-int-max=%d%s ! "
            "video/x-h%s,stream-format=byte-stream,alignment=au ! queue max-size-buffers=2 max-size-bytes=0 max-size-time=0 leaky=downstream ! h%sparse config-interval=1 ! "
            "queue max-size-buffers=2 max-size-bytes=0 max-size-time=0 leaky=downstream ! rtph%spay name=pay0 pt=96 config-interval=1 mtu=1200 )", 
            config.width, config.height, config.fps, config.width, config.height, config.fps, codec, config.kbps, config.key_int, enc_extra, codec, codec, codec);
        }break;

        default: perror("Unreachable, cam not supported");

    }



    gst_init(&argc, &argv);
    GstRTSPServer *server = gst_rtsp_server_new();

    /* CONFIG PORT */
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", config.port);
    gst_rtsp_server_set_service(server, port_str);

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    gst_rtsp_media_factory_set_launch(factory, pipe);
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    gst_rtsp_mount_points_add_factory(mounts, config.mount, factory);
    g_object_unref(mounts);

    if (gst_rtsp_server_attach(server, NULL) == 0) {
        printf("Error al inicializar el servidor RTSP.\n");
        return -1;
    }

    char *ip = GetEthIp();

    printf("Obteniendo vídeo de cámara %d, %dx%d @ %d fps\n", config.cam_idx, config.width, config.height, config.fps);

    printf("RTSP listo en: rtsp://%s:%d%s usando %s\n", ip ? ip : "0.0.0.0", config.port, config.mount, config.codec == CODEC_H265 ? "H.265" : "H.264");

    printf("Pipeline:\n%s\n", pipe);

    free(ip);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    g_object_unref(server);
    return 0;

}


/* CONFIG PARSING */
static void trim(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) { s[--n] = '\0'; }
}
bool parseConfig(const char * filename, Config * config){

    configSetDefaults(config);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("No se pudo abrir el archivo de configuración");
        return false;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        if (line[0] == '#' || line[0] == '\0') continue;

        char *key   = strtok(line, "=");
        char *value = strtok(NULL, "");
        if (!key || !value) continue;

        while (*value == ' ' || *value == '\t') value++;


        if (strcmp(key, "MOUNT") == 0) {
            if(value[0] == '/'){
                snprintf(config->mount, sizeof config->mount, "%s", value);
            }else{
                snprintf(config->mount, sizeof config->mount, "/%s", value);
            }  
        }else if(strcmp(key, "PORT") == 0) {
            config->port = atoi(value);
        }else if(strcmp(key, "WIDTH") == 0) {
            config->width = atoi(value);
        }else if(strcmp(key, "HEIGHT") == 0) {
            config->height = atoi(value);
        }else if(strcmp(key, "FPS") == 0) {
            config->fps = atoi(value);
        }else if(strcmp(key, "CODEC") == 0) {
            if (strcasecmp(value, "265") == 0 || strcasecmp(value, "h265") == 0) config->codec = CODEC_H265;
            else if (strcasecmp(value, "264") == 0 || strcasecmp(value, "h264") == 0) config->codec = CODEC_H264;
        }else if(strcmp(key, "CAM") == 0) {
            if (strcasecmp(value, "bertin") == 0) config->cam = CAM_BERTIN;
            else if (strcasecmp(value, "imx296") == 0) config->cam = CAM_IMX296;
        }else if(strcmp(key, "IDX") == 0) {
            config->cam_idx = atoi(value);
        }else if(strcmp(key, "KBPS") == 0) {
            config->kbps = atoi(value);
        }else if(strcmp(key, "KEYINT") == 0) {
            config->key_int = atoi(value);
        }else {
            printf("Parámetro no reconocido: %s (se ignora)\n", key);
        }
    }
    fclose(fp);


    if (config->mount[0] == '\0') {
        fprintf(stderr, "Config inválida: MOUNT y CODEC requeridos\n");
        return false;
    }
    return true;
}

/* IP GETTER */
char *GetEthIp(void)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1)
        return NULL;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        if (strcmp(ifa->ifa_name, "eth0") == 0 && ifa->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;

            if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) == NULL)
            {
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