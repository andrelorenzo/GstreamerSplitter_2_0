# CM5 + Trixie: AR0234 + Vision Components IMX296C simultáneas

## 1. Objetivo y configuración final

Esta guía resume la configuración que se consiguió hacer funcionar en
una **Raspberry Pi Compute Module 5 (CM5) con Debian/Raspberry Pi OS
Trixie 64-bit**, utilizando simultáneamente:

-   **AR0234**:
    -   Driver Kurokesu.
    -   `libcamera`/`rpicam` de Kurokesu.
    -   MIPI `cam0`.
    -   4 lanes.
    -   RAW8.
    -   `link-frequency=360000000`.
    -   Hasta 1920×1200 @ \~120 fps.
    -   Acceso desde GStreamer mediante `libcamerasrc`.
-   **Vision Components IMX296C**:
    -   Driver kernel/V4L2 de Vision Components.
    -   MIPI `cam1`.
    -   1 lane.
    -   RAW10 en el sensor.
    -   El RP1 CFE lo expone como Bayer RGGB16.
    -   Acceso desde GStreamer mediante `v4l2src`.
    -   En la configuración probada aparece como `/dev/video8`.
    -   1440×1080 @ \~60.38 fps.

La arquitectura final es:

``` text
AR0234
  |
  | MIPI cam0, 4 lanes, RAW8
  v
RP1 CFE 1f00110000.csi
  |
  v
libcamera Kurokesu / PiSP
  |
  v
libcamerasrc
  |
  +--------------------> GStreamer


IMX296C VC
  |
  | MIPI cam1, 1 lane, RAW10
  v
RP1 CFE 1f00128000.csi
  |
  | unpack RAW10 -> RGGB16
  v
/dev/video8
  |
  v
v4l2src -> bayer2rgb -> videoconvert
  |
  +--------------------> GStreamer
```

> **Importante:** los números `/dev/videoX` no son una API estable. En
> la configuración probada la IMX296C quedó en `/dev/video8`, pero
> conviene descubrir el dispositivo mediante Media Controller/sysfs en
> software de producción.

------------------------------------------------------------------------

## 2. Paquetes base

Actualizar primero el sistema:

``` bash
sudo apt update
sudo apt full-upgrade -y
sudo reboot
```

Herramientas útiles:

``` bash
sudo apt install -y \
    git \
    dkms \
    device-tree-compiler \
    v4l-utils \
    i2c-tools \
    pkg-config \
    cmake \
    build-essential
```

Si APT funciona por HTTPS pero no por HTTP, usar `https://` en los
`.sources`. En el sistema probado se cambiaron:

``` text
/etc/apt/sources.list.d/debian.sources
/etc/apt/sources.list.d/raspi.sources
```

para usar HTTPS.

Si IPv6 da problemas, se puede probar temporalmente:

``` bash
sudo apt-get -o Acquire::ForceIPv4=true update
```

------------------------------------------------------------------------

# 3. GStreamer

## 3.1 Paquetes de runtime

Instalar:

``` bash
sudo apt install -y \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav
```

Para trabajar con `libcamerasrc`, el sistema final utiliza el paquete
`gstreamer1.0-libcamera` compatible con el stack Kurokesu.

Comprobar:

``` bash
gst-inspect-1.0 v4l2src
gst-inspect-1.0 bayer2rgb
gst-inspect-1.0 libcamerasrc
gst-inspect-1.0 x264enc
```

## 3.2 Paquetes de desarrollo

Para compilar código C que utilice GStreamer y `gst-rtsp-server`:

``` bash
sudo apt install -y \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstrtspserver-1.0-dev
```

Comprobar RTSP Server:

``` bash
pkg-config --modversion gstreamer-rtsp-server-1.0
pkg-config --libs gstreamer-rtsp-server-1.0
```

En CMake, una forma limpia es:

``` cmake
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-rtsp-server-1.0
)

target_link_libraries(qfox_uav PRIVATE
    PkgConfig::GSTREAMER
)
```

Si aparecen errores de linker como:

``` text
undefined reference to `gst_rtsp_server_new'
undefined reference to `gst_rtsp_server_attach'
```

falta enlazar `gstreamer-rtsp-server-1.0`; no es un problema de headers.

------------------------------------------------------------------------

# 4. AR0234 --- Kurokesu

## 4.1 Repositorio APT de Kurokesu

La vía recomendada es utilizar el repositorio de paquetes Kurokesu:

``` bash
curl -fsSLO https://apt.kurokesu.com/setup.sh
sudo sh setup.sh

sudo apt update
sudo apt install -y ar0234-rpi-dkms rpicam-apps
```

El repositorio Kurokesu proporciona el driver DKMS y su stack de cámara
compatible con AR0234.

Comprobar paquetes:

``` bash
dpkg -l | grep -E 'ar0234|libcamera|rpicam|gstreamer1.0-libcamera'
dkms status
```

En la máquina probada se utilizó el stack Kurokesu de `libcamera`,
reconocible por el sufijo `+krks...`.

## 4.2 Configuración del AR0234

Editar:

``` bash
sudo nano /boot/firmware/config.txt
```

Desactivar autodetección:

``` ini
camera_auto_detect=0
```

Y bajo `[all]`:

``` ini
[all]
dtoverlay=ar0234,cam0,4lane,link-frequency=360000000
```

La combinación:

``` text
4lane + link-frequency=360000000
```

configura la AR0234 para **4 lanes y RAW8**.

La configuración probada expuso:

``` text
960x600   ~236.85 fps
1280x720  ~198.49 fps
1920x1080 ~133.58 fps
1920x1200 ~120.45 fps
```

Comprobar después del reboot:

``` bash
dmesg | grep -i ar0234
```

Resultado esperado:

``` text
ar0234 10-0010: extclk: 24000000Hz, link: 360000000Hz, lanes: 4
ar0234 10-0010: Success reading chip id: 0xa56
```

Y:

``` bash
rpicam-hello --list-cameras
```

Resultado esperado similar a:

``` text
0 : ar0234 [1920x1200 GRBG]
    Modes: 'SGRBG8' :
        960x600
        1280x720
        1920x1080
        1920x1200
```

Prueba de captura:

``` bash
rpicam-hello -t 0
```

En un sistema headless es normal que aparezca:

``` text
Failed to create egl preview
Failed to create drm preview
Preview window unavailable
```

Eso no implica que la cámara no capture.

------------------------------------------------------------------------

# 5. Vision Components IMX296C

## 5.1 Instalar SOLO el driver de Vision Components

Para CM5 se necesita el paquete BCM2712.

La configuración probada utilizó:

``` bash
wget https://github.com/VC-MIPI-modules/vc_mipi_raspi/releases/download/v0.6.10/vc-mipi-driver-bcm2712_0.6.10_arm64.deb

sudo apt install ./vc-mipi-driver-bcm2712_0.6.10_arm64.deb
```

Comprobar:

``` bash
dpkg -l | grep -i vc-mipi
dkms status
```

También puede clonarse el repositorio para disponer de los fuentes de
los overlays y herramientas:

``` bash
git clone https://github.com/VC-MIPI-modules/vc_mipi_raspi.git
cd vc_mipi_raspi
```

Self-check:

``` bash
./tools/self_check.sh --no-stream
```

------------------------------------------------------------------------

# 6. MUY IMPORTANTE --- Qué NO instalar de Vision Components

## No ejecutar `make all`

**No ejecutar:**

``` bash
wget https://raw.githubusercontent.com/VC-MIPI-modules/vc_mipi_raspi/main/Makefile
make all
```

Tampoco instalar manualmente el fork de `libcamera` de VC si se quiere
mantener el soporte Kurokesu de la AR0234.

El `Makefile` de VC contiene un target que elimina el stack instalado:

``` text
sudo apt-get remove -y libcamera*
```

y después compila/instala el fork:

``` text
VC-MIPI-modules/libcamera
branch update-to-v0.7.0
```

Eso sustituyó el `libcamera` Kurokesu y provocó que la AR0234 dejase de
estar soportada correctamente.

## No es necesario para nuestra solución

Para la IMX296C **NO necesitamos**:

-   El fork `VC-MIPI-modules/libcamera`.
-   `make all`.
-   Recompilar `rpicam-apps` desde VC.
-   Usar `libcamerasrc` para la IMX296C.
-   Instalar el stack ISP/libcamera de VC.

Sí necesitamos:

-   El `.deb` del driver `vc-mipi-driver-bcm2712`.
-   El módulo DKMS.
-   Los overlays VC.
-   V4L2.
-   GStreamer.
-   `bayer2rgb` si queremos color desde RAW.

La IMX296C se utiliza directamente mediante:

``` text
vc_mipi_camera -> RP1 CFE -> /dev/video8 -> v4l2src
```

## Por qué la documentación VC habla de libcamera

El soporte libcamera/ISP de VC aporta funciones como:

-   Auto exposure.
-   Auto white balance.
-   Colour correction matrix.
-   Procesamiento ISP.

Pero para coexistir con la AR0234 Kurokesu evitamos sustituir
`libcamera`.

Consecuencia: usando la IMX296C directamente por V4L2, la
exposición/ganancia y parte del procesamiento de imagen deben
gestionarse manualmente.

------------------------------------------------------------------------

# 7. Configuración VC para la IMX296C

Editar:

``` bash
sudo nano /boot/firmware/config_vc-mipi-driver-bcm2712.txt
```

La configuración final utiliza **VC cam1**, no cam0:

``` ini
# cam0 VC deshabilitado
#dtoverlay=vc-mipi-bcm2712-cam0
#dtparam=cam0_lanes1
#dtparam=cam0_manu_sony
#dtparam=cam0_libcamera_on

# IMX296C VC en cam1
dtoverlay=vc-mipi-bcm2712-cam1
dtparam=cam1_lanes1
dtparam=cam1_manu_sony
dtparam=cam1_libcamera_on

dtoverlay=vc-mipi-common-memory-contiguous
```

La IMX296 utiliza **1 lane**.

En `/boot/firmware/config.txt` debe incluirse el fichero VC:

``` ini
camera_auto_detect=0

...

[all]
dtoverlay=ar0234,cam0,4lane,link-frequency=360000000
include config_vc-mipi-driver-bcm2712.txt
```

------------------------------------------------------------------------

# 8. Problema encontrado en CM5: `cam1_reg`

Con el overlay original de VC `cam1`, la configuración no coexistía
correctamente en la CM5.

El overlay:

``` text
vc-mipi-bcm2712-cam1-overlay.dts
```

contenía:

``` dts
fragment@4 {
    target = <&cam1_reg>;
    __overlay__ {
        status = "disabled";
    };
};
```

En CM5 se observó que la definición/aliasado del regulador CAM1 no era
independiente de la forma esperada. El overlay podía fallar al aplicarse
o interferir con el regulador utilizado por la AR0234.

La solución probada fue eliminar/comentar **solo `fragment@4`** del
overlay CAM1.

## 8.1 Editar el overlay

``` bash
cd ~/GstreamerSplitter_2_0/drivers/vc_mipi_raspi/overlays/overlays-bcm2712

cp vc-mipi-bcm2712-cam1-overlay.dts \
   vc-mipi-bcm2712-cam1-overlay.dts.backup

nano vc-mipi-bcm2712-cam1-overlay.dts
```

Dejar:

``` dts
/*
fragment@4 {
    target = <&cam1_reg>;
    __overlay__ {
        status = "disabled";
    };
};
*/
```

No modificar los demás fragmentos.

## 8.2 Recompilar únicamente el overlay

``` bash
make vc-mipi-bcm2712-cam1.dtbo
```

Comprobar:

``` bash
ls -lh vc-mipi-bcm2712-cam1.dtbo
```

Verificar que ya no existe referencia a `cam1_reg`:

``` bash
dtc -I dtb -O dts \
    vc-mipi-bcm2712-cam1.dtbo \
    2>/dev/null | grep -A8 -B4 'cam1_reg'
```

No debería devolver nada.

## 8.3 Backup del overlay instalado

``` bash
sudo cp \
    /boot/firmware/overlays/vc-mipi-bcm2712-cam1.dtbo \
    /boot/firmware/overlays/vc-mipi-bcm2712-cam1.dtbo.backup
```

Instalar solamente el overlay modificado:

``` bash
sudo cp \
    vc-mipi-bcm2712-cam1.dtbo \
    /boot/firmware/overlays/vc-mipi-bcm2712-cam1.dtbo
```

**No ejecutar `make install`**, porque no es necesario copiar el resto
de overlays/configuración.

Reiniciar:

``` bash
sudo reboot
```

> Nota: versiones posteriores del driver VC indican cambios específicos
> para CM5/Cam1. Esta guía documenta exactamente la solución validada
> con v0.6.10. Si se migra a una versión nueva, comprobar primero si el
> parche sigue siendo necesario.

------------------------------------------------------------------------

# 9. Resultado correcto con ambas cámaras

Después del reboot:

``` bash
dmesg | grep -iE 'ar0234|vc_mipi_camera|failed|regulator'
```

La configuración funcional mostró simultáneamente:

``` text
ar0234 10-0010: extclk: 24000000Hz, link: 360000000Hz, lanes: 4
ar0234 10-0010: Success reading chip id: 0xa56

vc_mipi_camera 0-001a: vc_probe(): Probing UNIVERSAL VC MIPI Driver
vc_mipi_camera 0-001a: libcamera support enabled
vc_mipi_camera 0-001a: vc_core_set_num_lanes(): Set number of lanes: 1
vc_mipi_camera 0-001a: vc_probe(): Probe successful
```

Esto es importante:

``` text
AR0234   -> I2C bus 10 -> address 0x10
IMX296C  -> I2C bus  0 -> address 0x1a
```

La IMX296C utiliza además su controlador interno en `0x10`, pero al
estar en otro bus I2C ya no existe colisión con la AR0234.

------------------------------------------------------------------------

# 10. Topología final de Linux

Comprobar:

``` bash
v4l2-ctl --list-devices
```

La configuración funcional produjo:

``` text
rp1-cfe (platform:1f00110000.csi):
    /dev/video0
    /dev/video1
    ...
    /dev/video7
    /dev/media0

rp1-cfe (platform:1f00128000.csi):
    /dev/video8
    /dev/video9
    ...
    /dev/video15
    /dev/media3
```

Subdevices:

``` bash
for d in /sys/class/video4linux/v4l-subdev*; do
    echo "=== $(basename "$d") ==="
    cat "$d/name"
done
```

Resultado relevante:

``` text
/dev/v4l-subdev2 -> ar0234 10-0010
/dev/v4l-subdev5 -> vc_mipi_camera 0-001a
```

Media Controller:

``` bash
media-ctl -d /dev/media0 -p
media-ctl -d /dev/media3 -p
```

Mapa final:

``` text
AR0234
    /dev/v4l-subdev2
          |
          v
    /dev/media0
          |
          v
    rp1-cfe-csi2_ch0
          |
          v
    /dev/video0


IMX296C
    /dev/v4l-subdev5
          |
          v
    /dev/media3
          |
          v
    rp1-cfe-csi2_ch0
          |
          v
    /dev/video8
```

------------------------------------------------------------------------

# 11. Probar IMX296C por V4L2

Consultar formato:

``` bash
v4l2-ctl -d /dev/video8 --get-fmt-video
```

Resultado esperado:

``` text
Width/Height : 1440/1080
Pixel Format : 'RG16' (16-bit Bayer RGRG/GBGB)
Bytes per Line: 2880
Size Image: 3110400
```

El sensor transmite RAW10, pero el RP1 CFE lo expone desempaquetado como
RGGB16.

Prueba de streaming:

``` bash
v4l2-ctl \
    -d /dev/video8 \
    --set-fmt-video=width=1440,height=1080,pixelformat=RG16 \
    --stream-mmap=4 \
    --stream-count=100 \
    --stream-to=/dev/null \
    --verbose
```

Resultado funcional:

``` text
VIDIOC_STREAMON returned 0 (Success)
...
~60.37-60.38 fps
```

------------------------------------------------------------------------

# 12. IMX296C con GStreamer

## 12.1 RAW hasta fakesink

``` bash
gst-launch-1.0 -v \
    v4l2src device=/dev/video8 io-mode=mmap ! \
    'video/x-bayer,format=rggb16le,width=1440,height=1080,framerate=60/1' ! \
    fakesink sync=false
```

## 12.2 Debayer + conversión

``` bash
gst-launch-1.0 -v \
    v4l2src device=/dev/video8 io-mode=mmap ! \
    'video/x-bayer,format=rggb16le,width=1440,height=1080,framerate=60/1' ! \
    bayer2rgb ! \
    videoconvert ! \
    'video/x-raw,format=I420' ! \
    fakesink sync=false
```

## 12.3 H.264

``` bash
gst-launch-1.0 -v \
    v4l2src device=/dev/video8 io-mode=mmap ! \
    'video/x-bayer,format=rggb16le,width=1440,height=1080,framerate=60/1' ! \
    bayer2rgb ! \
    videoconvert ! \
    'video/x-raw,format=I420' ! \
    x264enc tune=zerolatency speed-preset=ultrafast \
        bitrate=5000 key-int-max=60 bframes=0 byte-stream=true ! \
    h264parse ! \
    fakesink
```

Esta cadena funcionó a 1440×1080 @ 60 fps.

------------------------------------------------------------------------

# 13. AR0234 con GStreamer

La AR0234 se utiliza a través de libcamera:

``` bash
gst-launch-1.0 -v \
    libcamerasrc ! \
    'video/x-raw,format=NV12,width=1920,height=1200,framerate=120/1' ! \
    fakesink
```

Para codificar:

``` bash
gst-launch-1.0 -v \
    libcamerasrc ! \
    'video/x-raw,format=NV12,width=1920,height=1200,framerate=120/1' ! \
    videoconvert ! \
    'video/x-raw,format=I420' ! \
    x264enc tune=zerolatency speed-preset=ultrafast \
        bitrate=8000 key-int-max=120 bframes=0 byte-stream=true ! \
    h264parse ! \
    fakesink
```

A FPS/resoluciones altas, el límite puede pasar a ser CPU/encoder y no
el enlace MIPI.

------------------------------------------------------------------------

# 14. Exposición y ganancia de la IMX296C

Al usar V4L2 directamente estamos evitando el ISP/libcamera de VC.

Por tanto no tenemos automáticamente:

-   AE (auto exposure).
-   AGC.
-   AWB.
-   CCM.
-   Gamma/tone mapping equivalente al ISP.

Si la imagen aparece muy oscura, consultar controles:

``` bash
v4l2-ctl -d /dev/v4l-subdev5 --list-ctrls
```

En la cámara probada estaban disponibles, entre otros:

``` text
exposure
analogue_gain
black_level
frame_rate
trigger_mode
io_mode
```

Consultar:

``` bash
v4l2-ctl -d /dev/v4l-subdev5 \
    --get-ctrl=exposure,analogue_gain
```

Ejemplo de ajuste manual:

``` bash
v4l2-ctl -d /dev/v4l-subdev5 \
    --set-ctrl=exposure=10000,analogue_gain=6000
```

Los valores deben ajustarse a la escena y al frame rate.

A \~60.38 fps:

``` text
periodo de frame ~= 16.56 ms
```

por lo que la exposición máxima práctica está condicionada por el
periodo de frame.

------------------------------------------------------------------------

# 15. Self-check de Vision Components

Desde el repositorio:

``` bash
cd ~/GstreamerSplitter_2_0/drivers/vc_mipi_raspi

./tools/self_check.sh --no-stream
```

En nuestra arquitectura es posible que el apartado `libcamera` de VC no
sea válido o no liste la cámara, porque **intencionadamente no
instalamos el fork libcamera de VC**.

Lo importante para la IMX296C en esta solución es:

``` text
Driver package      OK
DKMS                OK
Device Tree         OK
Driver probe        OK
V4L2                OK
/dev/video8         OK
/dev/v4l-subdev5    OK
```

------------------------------------------------------------------------

# 16. Diagnóstico rápido

## AR0234

``` bash
dmesg | grep -i ar0234
rpicam-hello --list-cameras
```

Debe aparecer:

``` text
Success reading chip id: 0xa56
```

## IMX296C

``` bash
dmesg | grep -i vc_mipi_camera
```

Debe aparecer:

``` text
vc_probe(): Probe successful
```

## Ambas

``` bash
v4l2-ctl --list-devices
```

Deben aparecer ambos CFE:

``` text
platform:1f00110000.csi
platform:1f00128000.csi
```

## Topología

``` bash
media-ctl -d /dev/media0 -p
media-ctl -d /dev/media3 -p
```

------------------------------------------------------------------------

# 17. Errores encontrados durante la integración

## `ar0234: failed to get regulators`

Causa encontrada:

``` text
cam0_reg
    status = "disabled"
```

al aplicar una configuración VC incompatible sobre los mismos recursos.

Se solucionó finalmente utilizando:

``` text
AR0234 -> cam0
VC     -> cam1
```

y corrigiendo el overlay VC `cam1` para CM5.

## `vc_mod_setup(): Unable to get module I2C client for address 0x10`

Se produjo cuando AR0234 y VC terminaron accidentalmente sobre el mismo
bus I2C.

La AR0234 ocupa:

``` text
bus 10, address 0x10
```

y el módulo VC necesita también acceder a su controlador interno en
`0x10`.

La configuración final los separa:

``` text
AR0234  -> bus 10
IMX296C -> bus 0
```

## `VIDIOC_STREAMON 22 (Invalid argument)`

Puede aparecer si se intenta abrir el nodo de vídeo incorrecto.

Con ambas cámaras activas:

``` text
/dev/video0 -> CFE de AR0234
/dev/video8 -> CFE de IMX296C
```

Por tanto la IMX296C debe abrirse con `/dev/video8` en la configuración
probada.

## GStreamer selecciona formatos absurdos

Ejecutar simplemente:

``` bash
gst-launch-1.0 v4l2src device=/dev/video8 ! fakesink
```

puede hacer que GStreamer negocie un formato que el CFE anuncia pero el
sensor no produce realmente.

Forzar siempre caps:

``` text
video/x-bayer,format=rggb16le,width=1440,height=1080,framerate=60/1
```

------------------------------------------------------------------------

# 18. Configuración final de boot

## `/boot/firmware/config.txt`

Partes relevantes:

``` ini
camera_auto_detect=0

[cm5]
dtoverlay=dwc2,dr_mode=host

[pi5]
dtoverlay=nospi10

[all]
dtoverlay=ar0234,cam0,4lane,link-frequency=360000000
include config_vc-mipi-driver-bcm2712.txt
```

## `/boot/firmware/config_vc-mipi-driver-bcm2712.txt`

Partes relevantes:

``` ini
# VC CAM0 deshabilitado

#dtoverlay=vc-mipi-bcm2712-cam0
#dtparam=cam0_lanes1
#dtparam=cam0_manu_sony
#dtparam=cam0_libcamera_on


# VC CAM1 - IMX296C

dtoverlay=vc-mipi-bcm2712-cam1
dtparam=cam1_lanes1
dtparam=cam1_manu_sony
dtparam=cam1_libcamera_on


# CMA

dtoverlay=vc-mipi-common-memory-contiguous
```

El `.dtbo` de `vc-mipi-bcm2712-cam1` debe ser la versión modificada sin
el `fragment@4` que apunta a `cam1_reg`, si se mantiene exactamente la
instalación v0.6.10 probada.

------------------------------------------------------------------------

# 19. Resumen de qué instalar y qué NO instalar

## Instalar

``` text
Kurokesu:
    ar0234-rpi-dkms
    rpicam-apps
    libcamera Kurokesu (resuelto por su repo)
    gstreamer1.0-libcamera compatible

Vision Components:
    vc-mipi-driver-bcm2712 .deb
    DKMS del driver VC
    overlays VC

Sistema:
    v4l-utils
    device-tree-compiler
    GStreamer tools/plugins
    GStreamer development packages
    gst-rtsp-server development package
```

## NO instalar / NO ejecutar

``` text
NO make all del Makefile principal de VC

NO sudo apt-get remove libcamera*

NO reemplazar libcamera Kurokesu por:
    VC-MIPI-modules/libcamera

NO recompilar rpicam-apps desde el Makefile VC para esta arquitectura

NO es necesario usar libcamerasrc para la IMX296C

NO es necesario instalar el soporte ISP/libcamera VC
```

La regla práctica es:

``` text
AR0234:
    Kurokesu controla kernel + libcamera

IMX296C:
    VC controla kernel/V4L2

NO dejar que VC sustituya libcamera
```

------------------------------------------------------------------------

# 20. Checklist final

``` bash
# 1. Kernel
uname -a

# 2. AR0234
dmesg | grep -i ar0234

# 3. VC
dmesg | grep -i vc_mipi_camera

# 4. libcamera
rpicam-hello --list-cameras

# 5. V4L2
v4l2-ctl --list-devices

# 6. Subdevices
for d in /sys/class/video4linux/v4l-subdev*; do
    echo "=== $(basename "$d") ==="
    cat "$d/name"
done

# 7. Media graphs
media-ctl -d /dev/media0 -p
media-ctl -d /dev/media3 -p

# 8. IMX296C raw stream
v4l2-ctl \
    -d /dev/video8 \
    --set-fmt-video=width=1440,height=1080,pixelformat=RG16 \
    --stream-mmap=4 \
    --stream-count=100 \
    --stream-to=/dev/null

# 9. AR0234
rpicam-hello --list-cameras

# 10. GStreamer plugins
gst-inspect-1.0 libcamerasrc
gst-inspect-1.0 v4l2src
gst-inspect-1.0 bayer2rgb
gst-inspect-1.0 x264enc
```

Estado esperado:

``` text
AR0234:
    4 lanes
    RAW8
    chip id 0xa56
    libcamera OK

IMX296C:
    1 lane
    RAW10 sensor
    RGGB16 en CFE
    Probe successful
    /dev/video8
    ~60.38 fps

GStreamer:
    AR0234 -> libcamerasrc
    IMX296C -> v4l2src + bayer2rgb
```

------------------------------------------------------------------------

# 21. Nota para futuras actualizaciones

Esta guía documenta **la configuración que se validó
experimentalmente**, especialmente el workaround del overlay VC v0.6.10.

Antes de actualizar:

``` text
vc-mipi-driver-bcm2712
libcamera
rpicam-apps
kernel
```

guardar:

``` bash
cp /boot/firmware/config.txt ~/config.txt.camera-working
cp /boot/firmware/config_vc-mipi-driver-bcm2712.txt ~/config_vc-mipi-working.txt

dpkg -l | grep -E 'libcamera|rpicam|ar0234|vc-mipi|gstreamer' \
    > ~/camera-packages-working.txt
```

Y conservar también el overlay modificado:

``` bash
sudo cp \
    /boot/firmware/overlays/vc-mipi-bcm2712-cam1.dtbo \
    ~/vc-mipi-bcm2712-cam1-working.dtbo
```

Las versiones nuevas de VC pueden incorporar correcciones específicas
para CM5/Cam1, por lo que el parche manual debe reevaluarse al
actualizar.
