# Instalar Axelera Metis en otra CM5 con Raspberry Pi OS Trixie

Configuración comprobada en la CM5 de referencia:

| Componente | Versión |
| --- | --- |
| Voyager SDK | `1.9.0` |
| Commit de `voyager-sdk` | `6c5a29a3d56d1c3fbe0ae748d626cf67da16fdca` |
| Driver `metis-dkms` | `1.7.16` |

La prueba de referencia con `yolov5s-v7-coco` y `traffic1_480p.mp4` alcanzó **212,9 FPS**. Los FPS pueden variar según el vídeo y la configuración.

## 1. Comprobar que la CM5 detecta la Metis

```bash
sudo apt update
sudo apt install -y git pciutils
lspci -nn -d 1f9d:1100
```

Debe aparecer un dispositivo Axelera. Si no aparece, revisa la conexión y alimentación. Si el PCIe externo no está habilitado, añade `dtparam=pciex1` a `/boot/firmware/config.txt`, reinicia y repite `lspci`. No hace falta configurar PCIe Gen 3 para verificar la instalación.

## 2. Instalar el mismo SDK

```bash
git clone https://github.com/axelera-ai-hub/voyager-sdk.git ~/voyager-sdk
cd ~/voyager-sdk
git checkout 6c5a29a3d56d1c3fbe0ae748d626cf67da16fdca
cat VERSION.TXT
```

`VERSION.TXT` debe mostrar `1.9.0`. En Trixie, la compilación anterior necesitó la cabecera `<algorithm>`; añádela antes de iniciar la instalación:

```bash
h=operators/axstreamer/include/AxDataInterface.h
grep -q '^#include <algorithm>' "$h" || sed -i '1i#include <algorithm>' "$h"
./quick-start.sh
source venv_1.9.0/bin/activate
axdevice driver --install
make operators
```

Si falla la instalación del driver por falta de cabeceras del kernel, comprueba `test -d "/lib/modules/$(uname -r)/build"` e instala las cabeceras correspondientes al kernel de esa CM5 antes de repetir `axdevice driver --install`.

## 3. Comprobar la instalación

```bash
dpkg-query -W metis-dkms
dkms status | grep metis
lspci -nnk -d 1f9d:1100
axdevice -v
test -f operators/lib/libgstaxstreamer.so && echo 'Complemento GStreamer compilado'
```

Comprueba que `metis-dkms` sea `1.7.16`, que DKMS indique `installed` para el kernel activo y que `axdevice -v` encuentre la Metis. Es normal que `axdevice` cargue el firmware de ejecución `1.9.0` cuando detecta una versión anterior en la tarjeta.

## 4. Hacer la prueba completa de inferencia

```bash
export GST_PLUGIN_PATH="$PWD/operators/lib"
axdownloadmedia h264/traffic1_480p.mp4 --output media
./inference.py yolov5s-v7-coco media/h264/traffic1_480p.mp4 --no-display
```

La primera ejecución puede tardar varios minutos en preparar el modelo. La instalación funciona cuando aparecen temperatura y FPS mayores que cero. Los avisos sobre GPU y OpenCL también aparecieron en la CM5 de referencia y no impidieron la inferencia.

En cada terminal nuevo, ejecuta:

```bash
cd ~/voyager-sdk
source venv_1.9.0/bin/activate
export GST_PLUGIN_PATH="$PWD/operators/lib"
```

No instales el paquete antiguo `metis-dkms_1.0.2_all.deb`; no forma parte de esta configuración.

## Referencias

- [Instalación de Voyager SDK](https://docs.axelera.ai/sdk/user-guides/sdk-install/)
- [Compatibilidad de versiones](https://docs.axelera.ai/sdk/release-compatibility-matrix/)
- [Comprobación de la instalación](https://docs.axelera.ai/sdk/getting-started/verify-setup/)
- [Activación del PCIe externo en Raspberry Pi](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#enable-pcie)
