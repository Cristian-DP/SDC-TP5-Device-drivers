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

## Requisitos de software (en la Raspberry Pi)

```bash
sudo apt update
sudo apt install -y linux-headers-$(uname -r) build-essential python3-flask
```

---

## Compilación y carga del driver

```bash
cd driver
make
sudo insmod sdec_cdd.ko
sudo chmod 666 /dev/sdec_cdd
```

Verificar que cargó correctamente:

```bash
sudo dmesg | tail
ls -l /dev/sdec_cdd
```

Para descargar el módulo:

```bash
sudo rmmod sdec_cdd
```

---

## Ejecución de la aplicación web

Con el módulo cargado:

```bash
cd app
python3 servidor.py
```

El servidor queda escuchando en el puerto 5000. Desde cualquier dispositivo en
la misma red, abrir en el navegador:
http://<IP_DE_LA_RASPBERRY>:5000

Para conocer la IP de la Raspberry: `hostname -I`

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