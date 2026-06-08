# SDC - TP5: Device Drivers

Character Device Driver (CDD) para Linux que sensa dos señales (temperatura y
humedad) desde un sensor DHT11 conectado a una Raspberry Pi, junto con una
aplicación web que grafica la señal seleccionada en tiempo real.

**Materia:** Sistemas de Computación (UNC)
**Integrantes:** Cristian Pereyra, Francisco Coschica, Nicolás López Casanegra
**Docente:** Ing. Javier Jorge

---

## Descripción

El proyecto consta de dos componentes:

- **`driver/sdec_cdd.c`** — Módulo de kernel (CDD) que lee el sensor DHT11 cada
  1 segundo mediante un timer del kernel. Expone el dispositivo `/dev/sdec_cdd`.
  - `read()` devuelve el valor de la señal actualmente seleccionada.
  - `write()` permite seleccionar la señal: `0` = temperatura, `1` = humedad.

- **`app/servidor.py`** — Servidor web (Flask) que corre en la Raspberry Pi.
  Lee `/dev/sdec_cdd` y sirve una interfaz web con un gráfico en tiempo real
  (`app/templates/index.html`, usando Chart.js). Permite cambiar de señal desde
  el navegador, resetea el gráfico al cambiar y ajusta las escalas a nivel de
  usuario.

---

## Hardware

- Raspberry Pi (probado en Zero 2 W con Raspberry Pi OS Lite 32-bit)
- Sensor DHT11 (módulo de 3 pines)

### Conexionado

| Pin DHT11 | Pin físico RPi | Función  |
|-----------|----------------|----------|
| S (señal) | 7              | GPIO4    |
| + (VCC)   | 1              | 3.3V     |
| − (GND)   | 9              | GND      |

> **Importante:** alimentar el DHT11 con 3.3V (pin 1), NO con 5V.

---

## Compilación del driver

El enfoque principal de este TP es la **cross-compilation**: se compila el
módulo en la PC host (x86) apuntando a la arquitectura ARM de la Raspberry, y
luego se transfiere el binario `.ko` por SSH. Se incluye además una opción de
compilación nativa como alternativa de respaldo.

---

### Opción principal — Cross-compilation (PC host x86 → RPi ARM)

**Prerequisitos en la PC host:**

```bash
# Toolchain ARM (32-bit, para RPi Zero 2W con kernel v7)
sudo apt install -y gcc-arm-linux-gnueabihf flex bison libssl-dev bc
```

**Obtener el source del kernel en la versión EXACTA de la RPi:**

La versión del kernel se obtiene con `uname -r` en la RPi (ej: `6.12.75+rpt-rpi-v7`).

```bash
git clone https://github.com/raspberrypi/linux.git rpi-linux-source
cd rpi-linux-source
git checkout rpi-6.12.y
# Buscar y hacer checkout del commit de la version exacta:
git log --oneline | grep "Linux 6.12.75"     # devuelve el hash
git checkout <hash>
```

**Configurar el source con los datos de la RPi:**

```bash
# Copiar .config y Module.symvers desde los headers de la RPi
# (se obtienen por scp desde /usr/src/linux-headers-<version> de la RPi)
cp <headers_rpi>/.config .config
cp <headers_rpi>/Module.symvers Module.symvers

# Dejar CONFIG_LOCALVERSION vacío (el sufijo se inyecta al compilar)
sed -i 's/CONFIG_LOCALVERSION=.*/CONFIG_LOCALVERSION=""/' .config

# Preparar el arbol (compila las herramientas host para x86 y fija la version)
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- LOCALVERSION=+rpt-rpi-v7 modules_prepare
```

**Compilar el módulo:**

```bash
cd driver
make -f Makefile.cross KDIR=$HOME/rpi-linux-source
```

**Verificar que el vermagic coincide con el kernel de la RPi:**

```bash
modinfo sdec_cdd.ko | grep vermagic
# Debe mostrar: 6.12.75+rpt-rpi-v7 ...
```

**Transferir y cargar en la RPi:**

```bash
scp sdec_cdd.ko pi@<IP_RPi>:~/
ssh pi@<IP_RPi>
sudo insmod sdec_cdd.ko
sudo chmod 666 /dev/sdec_cdd
```

---

### Opción alternativa — Compilación nativa (en la RPi)

Si la cross-compilation falla, se puede compilar directamente en la Raspberry.

**¿Por qué puede fallar la cross-compilation?** Es un proceso sensible a la
coincidencia exacta de versiones: requiere el source del kernel en el commit
correcto, el `.config` y `Module.symvers` de la RPi, el toolchain ARM apropiado
y un `vermagic` que coincida con el kernel en ejecución. Si cualquiera de estos
elementos no coincide (por ejemplo, una versión de kernel distinta tras un
`apt upgrade`, o un toolchain de otra versión de gcc), el módulo no compila o
no carga. La compilación nativa evita esto porque compila siempre contra el
kernel que está corriendo en ese momento.

**Pasos (en la RPi):**

```bash
sudo apt install -y linux-headers-$(uname -r) build-essential
cd driver
make                      # usa el Makefile nativo
sudo insmod sdec_cdd.ko
sudo chmod 666 /dev/sdec_cdd
```

---

### Descargar el módulo

```bash
sudo rmmod sdec_cdd
```

---

## Ejecución de la aplicación web

Con el módulo cargado, en la RPi:

```bash
cd app
python3 servidor.py
```

El servidor escucha en el puerto 5000. Desde un navegador en la misma red:
http://<IP_DE_LA_RASPBERRY>:5000

Para conocer la IP de la Raspberry: `hostname -I`

Requisito: `sudo apt install -y python3-flask`

---

## Uso

- La página muestra el gráfico de la señal seleccionada actualizándose cada segundo.
- Los botones **Temperatura** y **Humedad** cambian la señal que lee el CDD.
- Al cambiar de señal, el gráfico se resetea y reajusta sus escalas.
- Se muestran los valores actual, mínimo y máximo de la sesión.

---

## Prueba manual del driver (sin la app)

```bash
cat /dev/sdec_cdd        # lee la señal actual
echo "1" > /dev/sdec_cdd # selecciona humedad
cat /dev/sdec_cdd        # ahora lee humedad
echo "0" > /dev/sdec_cdd # vuelve a temperatura
```