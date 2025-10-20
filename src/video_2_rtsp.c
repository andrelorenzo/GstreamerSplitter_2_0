#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <glib.h>          // para gboolean, TRUE/FALSE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char mount[32];   // sin slash inicial -> lo añadimos al usarlo
    char codec[8];    // "265" o "264"
    int  width_px;
    int  height_px;
    int  framerate;
    int  dev_id;
    int  port;        // número, lo convertimos a string para set_service
} Config;

static void set_defaults(Config *c) {
    snprintf(c->codec, sizeof c->codec, "265"); // por defecto H.265
    c->dev_id    = 0;
    c->framerate = 30;
    c->height_px = 480;
    c->width_px  = 640;
    snprintf(c->mount, sizeof c->mount, "live0");
    c->port      = 8554;
}

static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) { s[--n] = '\0'; }
}

gboolean parse_config(const char *filename, Config *config) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("No se pudo abrir el archivo de configuración");
        return FALSE;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);
        if (line[0] == '#' || line[0] == '\0') continue;

        char *key   = strtok(line, "=");
        char *value = strtok(NULL, "");
        if (!key || !value) continue;

        // limpia espacios iniciales en value
        while (*value == ' ' || *value == '\t') value++;

        if (strcmp(key, "MOUNT") == 0) {
            snprintf(config->mount, sizeof config->mount, "%s", value);
        } else if (strcmp(key, "CODEC") == 0) {
            // acepta "265", "264", "hevc", "h265", "h264"
            if (strcasecmp(value, "265") == 0 || strcasecmp(value, "hevc") == 0 || strcasecmp(value, "h265") == 0) {
                snprintf(config->codec, sizeof config->codec, "265");
            } else {
                snprintf(config->codec, sizeof config->codec, "264");
            }
        } else if (strcmp(key, "WIDTH") == 0) {
            config->width_px = atoi(value);
        } else if (strcmp(key, "HEIGHT") == 0) {
            config->height_px = atoi(value);
        } else if (strcmp(key, "FPS") == 0) {
            config->framerate = atoi(value);
        } else if (strcmp(key, "SRC_ID") == 0) {
            config->dev_id = atoi(value);
        } else if (strcmp(key, "PORT") == 0) {
            config->port = atoi(value);
        } else {
            printf("Parámetro no reconocido: %s (se ignora)\n", key);
        }
    }
    fclose(fp);

    // Validación mínima
    if (config->width_px <= 0 || config->height_px <= 0 || config->framerate <= 0) {
        fprintf(stderr, "Config inválida: width/height/fps deben ser > 0\n");
        return FALSE;
    }
    if (config->mount[0] == '\0' || config->codec[0] == '\0') {
        fprintf(stderr, "Config inválida: MOUNT y CODEC requeridos\n");
        return FALSE;
    }
    return TRUE;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <path-to-file>\n", argv[0]);
        return 1;
    }

    Config config;
    set_defaults(&config);
    if (!parse_config(argv[1], &config)) return 1;

    // Construcción de nombres según codec
    gboolean is265 = (strcmp(config.codec, "265") == 0);
    const char *enc   = is265 ? "x265enc"   : "x264enc";
    const char *parse = is265 ? "h265parse" : "h264parse";
    const char *pay   = is265 ? "rtph265pay": "rtph264pay";

    // Asegurar que el mount lleva '/' al principio
    char mount_with_slash[64];
    if (config.mount[0] == '/')
        snprintf(mount_with_slash, sizeof mount_with_slash, "%s", config.mount);
    else
        snprintf(mount_with_slash, sizeof mount_with_slash, "/%s", config.mount);

    // Cadena de pipeline
    char pipe[512];
    snprintf(
        pipe, sizeof pipe,
        "( v4l2src device=/dev/video%d ! "
        "videoconvert ! video/x-raw,format=I420,width=%d,height=%d,framerate=%d/1 ! "
        "%s tune=zerolatency speed-preset=ultrafast key-int-max=60 ! "
        "%s config-interval=1 ! %s name=pay0 pt=96 )",
        config.dev_id,
        config.width_px, config.height_px, config.framerate,
        enc, parse, pay
    );

    // Inicializa GStreamer/RTSP
    gst_init(&argc, &argv);

    GstRTSPServer *server = gst_rtsp_server_new();

    // convertir port (int) -> string para set_service
    char port_str[16];
    snprintf(port_str, sizeof port_str, "%d", config.port);
    gst_rtsp_server_set_service(server, port_str);

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory, pipe);
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    gst_rtsp_mount_points_add_factory(mounts, mount_with_slash, factory);
    g_object_unref(mounts);

    if (gst_rtsp_server_attach(server, NULL) == 0) {
        printf("Error al inicializar el servidor RTSP.\n");
        return -1;
    }

    printf("Obteniendo vídeo de: /dev/video%d %dx%d @ %d fps\n",
           config.dev_id, config.width_px, config.height_px, config.framerate);
    printf("RTSP listo en: rtsp://127.0.0.1:%d%s usando %s\n",
           config.port, mount_with_slash, is265 ? "H.265" : "H.264");
    printf("Pipeline:\n%s\n", pipe);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    // nunca llega aquí salvo señal/parada
    g_main_loop_unref(loop);
    g_object_unref(server);
    return 0;
}
