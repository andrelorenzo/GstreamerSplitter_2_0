#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netpacket/packet.h>
#define MAX_INTERFACES 16

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <net/if.h>

typedef struct {
    char name[IFNAMSIZ];
    char ip[INET_ADDRSTRLEN];
} InterfaceInfo;

/* Listar interfaces activas con dirección IPv4 asignada, excluyendo loopback */
int list_interfaces(InterfaceInfo *interfaces, int max) {
    struct ifaddrs *ifaddr, *ifa;
    int count = 0;

    if (getifaddrs(&ifaddr) == -1)
        return 0;

    for (ifa = ifaddr; ifa != NULL && count < max; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        const char *ip_str = inet_ntoa(addr->sin_addr);

        // Excluir loopback (127.0.0.1)
        if (strcmp(ip_str, "127.0.0.1") == 0)
            continue;

        strncpy(interfaces[count].ip, ip_str, INET_ADDRSTRLEN - 1);
        strncpy(interfaces[count].name, ifa->ifa_name, IFNAMSIZ - 1);
        interfaces[count].ip[INET_ADDRSTRLEN - 1] = '\0';
        interfaces[count].name[IFNAMSIZ - 1] = '\0';
        count++;
    }

    freeifaddrs(ifaddr);
    return count;
}


typedef struct {
    gchar *mount_point;
    gchar *port;
    gchar *codec;
} Config;

static void on_client_connected(GstRTSPServer *server, GstRTSPClient *client, gpointer user_data) {
    GstRTSPConnection *conn = gst_rtsp_client_get_connection(client);
    if (conn) {
        const gchar *ip = gst_rtsp_connection_get_ip(conn);
        g_print("Cliente conectado desde IP: %s\n", ip ? ip : "desconocida");
    }
}

/* Función para leer archivo de configuración */
gboolean read_config_file(const gchar *filename, Config *config) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        g_printerr("No se pudo abrir el archivo de configuración: %s\n", filename);
        return FALSE;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        gchar *key = strtok(line, "=");
        gchar *value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, "src_mount") == 0)
                config->mount_point = g_strdup(value);
            else if (strcmp(key, "src_port") == 0)
                config->port = g_strdup(value);
            else if (strcmp(key, "src_codec") == 0)
                config->codec = g_strdup(value);
        }
    }

    fclose(file);
    return config->mount_point && config->port && config->codec;
}

int main(int argc, char *argv[]) {
    Config config = {0};
    gst_init(&argc, &argv);

    if (argc < 2) {
        g_printerr("Uso: %s <ruta_config.txt>\n", argv[0]);
        return -1;
    }

    if (!read_config_file(argv[1], &config)) {
        g_printerr("Error leyendo archivo de configuración.\n");
        return -1;
    }

    gchar *pipeline_str = NULL;
    if (strcmp(config.codec, "264") == 0) {
        pipeline_str = g_strdup(
            "v4l2src device=/dev/video0 do-timestamp=true ! timeoverlay ! queue leaky=2 max-size-buffers=2 ! video/x-raw,format=YUY2,width=640,height=480,framerate=30/1 ! "
            "videoconvert ! video/x-raw,format=I420 ! x264enc tune=zerolatency speed-preset=ultrafast key-int-max=10 ! "
            "video/x-h264,profile=baseline ! rtph264pay name=pay0 pt=96 config-interval=1 sync=false"
        );
    } else if (strcmp(config.codec, "265") == 0) {
        pipeline_str = g_strdup(
            "v4l2src device=/dev/video0 do-timestamp=true ! timeoverlay ! queue leaky=2 max-size-buffers=2 ! video/x-raw,format=YUY2,width=640,height=480,framerate=30/1 ! "
            "videoconvert ! video/x-raw,format=I420 ! x265enc tune=zerolatency speed-preset=ultrafast key-int-max=10 ! "
            "video/x-h265 ! rtph265pay name=pay0 pt=96 config-interval=1 sync=false"
        );
    }else {
        g_printerr("Codec no soportado: %s (solo '264' o '265')\n", config.codec);
        g_free(config.mount_point);
        g_free(config.port);
        g_free(config.codec);
        return -1;
    }

    GstRTSPServer *server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(server, config.port);
    g_signal_connect(server, "client-connected", G_CALLBACK(on_client_connected), NULL);

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory, pipeline_str);
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    gst_rtsp_mount_points_add_factory(mounts, config.mount_point, factory);
    g_object_unref(mounts);

    if (gst_rtsp_server_attach(server, NULL) == 0) {
        g_printerr("Error al iniciar el servidor RTSP.\n");
        g_free(config.mount_point);
        g_free(config.port);
        g_free(config.codec);
        g_free(pipeline_str);
        return -1;
    }

    InterfaceInfo interfaces[MAX_INTERFACES];
    int num_interfaces = list_interfaces(interfaces, MAX_INTERFACES);

    if (num_interfaces == 0) {
        g_printerr("No se encontraron interfaces de red con IP asignada.\n");
        return -1;
    }

    int selected = -1;
    if(num_interfaces == 1){
        g_print("Only one interface connected, selecting: %s -> %s\n", interfaces[0].name, interfaces[0].ip);
        selected = 0;
    }else{
        g_print("Interfaces disponibles:\n");
        for (int i = 0; i < num_interfaces; ++i) {
            g_print("  [%d] %s -> %s\n", i, interfaces[i].name, interfaces[i].ip);
        }

        do {
            g_print("Seleccione una interfaz por índice: ");
            if (scanf("%d", &selected) != 1 || selected < 0 || selected >= num_interfaces) {
                g_print("Índice no válido. Intente de nuevo.\n");
                while (getchar() != '\n'); // limpiar búfer
                selected = -1;
            }
        } while (selected == -1);
    }
    

    const gchar *local_ip = interfaces[selected].ip;
    g_print("Servidor RTSP en rtsp://%s:%s%s (codec: %s)\n", local_ip, config.port, config.mount_point, config.codec);


    g_main_loop_run(g_main_loop_new(NULL, FALSE));

    // Limpieza
    g_free(config.mount_point);
    g_free(config.port);
    g_free(config.codec);
    g_free(pipeline_str);
    return 0;
}
