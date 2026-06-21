# Pruebas básicas Blinky

Esta carpeta contiene las primeras pruebas de programación realizadas con placas Nordic Semiconductor utilizando el ejemplo básico **Blinky** de nRF Connect SDK.

El objetivo de estas pruebas fue comprobar que el entorno de desarrollo estaba correctamente configurado, que las placas podían compilarse y programarse sin errores, y que el firmware se ejecutaba correctamente tras ser cargado en cada dispositivo.

## Contenido de la carpeta

La carpeta incluye dos proyectos:

```text
blinky/
│
├── nrf52_blinky/
└── nrf53_blinky/
```

* `nrf52_blinky/`: proyecto Blinky utilizado para comprobar la programación básica del nRF52840-Dongle.
* `nrf53_blinky/`: proyecto Blinky utilizado para comprobar la programación básica de la nRF5340-DK.

## Hardware utilizado

Para estas pruebas se utilizaron los siguientes dispositivos:

* **nRF5340-DK**.
* **nRF52840-Dongle**.
* Ordenador con nRF Connect SDK y Visual Studio Code.

## Software utilizado

Las pruebas se desarrollaron con:

* **nRF Connect SDK 2.9.0**.
* **Visual Studio Code** con la extensión **nRF Connect for VS Code**.
* **nRF Connect for Desktop**, especialmente para la programación del nRF52840-Dongle mediante la aplicación Programmer.

## Descripción de la prueba

El ejemplo Blinky configura un LED de la placa como salida digital y alterna periódicamente su estado. Aunque se trata de una aplicación sencilla, permite comprobar el flujo completo de trabajo:

1. Crear o abrir el proyecto en Visual Studio Code.
2. Seleccionar la placa objetivo correspondiente.
3. Compilar el proyecto.
4. Programar el dispositivo.
5. Verificar visualmente el parpadeo del LED.

En el caso de la **nRF5340-DK**, la programación se realizó directamente mediante el depurador/programador integrado SEGGER J-Link OB. En el caso del **nRF52840-Dongle**, la carga del firmware se realizó mediante el modo DFU y la aplicación Programmer de nRF Connect for Desktop.

## Relación con la memoria

Esta carpeta está relacionada con el apartado **3.3.- Programación básica** de la memoria del Trabajo Fin de Grado.

## Observaciones

Estas pruebas no forman parte de la evaluación avanzada de prestaciones de Bluetooth 5.x. Su finalidad fue validar el entorno de trabajo y familiarizarse con el proceso de compilación y carga de firmware antes de desarrollar las pruebas Bluetooth posteriores.
