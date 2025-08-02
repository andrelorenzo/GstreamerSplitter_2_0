#include <gst/gst.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char src_url[512];
    char codec[8];
} Config;

gboolean parse_config(const char *filename, Config *config) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("No se pudo abrir el archivo de configuración");
        return FALSE;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "src_url=", 8) == 0) {
            strncpy(config->src_url, line + 8, sizeof(config->src_url) - 1);
            config->src_url[strcspn(config->src_url, "\r\n")] = 0;
        } else if (strncmp(line, "codec=", 6) == 0) {
            strncpy(config->codec, line + 6, sizeof(config->codec) - 1);
            config->codec[strcspn(config->codec, "\r\n")] = 0;
        }
    }
    fclose(fp);
    return (config->src_url[0] != 0 && config->codec[0] != 0);
}

void retry_connection_with_animation(const char *src_url, const char *depay) {
    const char *dots[] = {".", "..", "..."};
    int attempt = 0;

    while (1) {
        gchar *pipeline_str = g_strdup_printf(
            "rtspsrc location=%s latency=100 protocols=udp ! %s ! decodebin ! autovideosink sync=false",
            src_url, depay
        );

        GstElement *pipeline = gst_parse_launch(pipeline_str, NULL);
        g_free(pipeline_str);

        if (!pipeline) {
            g_printerr("No se pudo crear el pipeline. Reintentando%s\r", dots[attempt % 3]);
            fflush(stderr);
            g_usleep(1000000);
            attempt++;
            continue;
        }

        g_print("Intentando reproducir stream: %s\n", src_url);
        gst_element_set_state(pipeline, GST_STATE_PLAYING);

        GstBus *bus = gst_element_get_bus(pipeline);
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                        GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

        if (msg) gst_message_unref(msg);
        gst_object_unref(bus);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);

        g_printerr("Desconectado del servidor. Reintentando%s\r", dots[attempt % 3]);
        fflush(stderr);
        g_usleep(1000000);
        attempt++;
    }
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    Config config = {0};
    if (argc < 2) {
        g_printerr("Uso: %s <ruta_config.txt>\n", argv[0]);
        return -1;
    }
    if (!parse_config(argv[1], &config)) {
        g_printerr("Error al leer el archivo de configuración: %s.\n", argv[1]);
        return -1;
    }

    const char *depay = NULL;
    if (strcmp(config.codec, "264") == 0)
        depay = "rtph264depay";
    else if (strcmp(config.codec, "265") == 0)
        depay = "rtph265depay";
    else {
        g_printerr("Codec no reconocido: %s\n", config.codec);
        return -1;
    }

    retry_connection_with_animation(config.src_url, depay);
    return 0;
}
