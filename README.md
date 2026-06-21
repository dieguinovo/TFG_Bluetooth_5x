# Evaluación de prestaciones de Bluetooth 5.x

Este repositorio contiene el código fuente y los archivos auxiliares utilizados en el TFG **“Evaluación de prestaciones de Bluetooth 5.x”**.

El trabajo se centra en el estudio y validación experimental de distintas funcionalidades de Bluetooth Low Energy y Bluetooth 5.x utilizando placas de desarrollo de Nordic Semiconductor. Las pruebas se han desarrollado principalmente con placas **nRF5340-DK**, empleando también un **nRF52840-Dongle** en las pruebas iniciales de programación y comunicación básica.

## Contenido del repositorio

La estructura principal del repositorio es la siguiente:

```text
TFG_Bluetooth_5x/
│
├── blinky/
├── bluetooth/
├── tfg_ble_range/
├── tfg_direction_finding/
└── tfg_mesh/
```

Cada carpeta corresponde a una parte del trabajo:

* `blinky/`: pruebas básicas de programación mediante el ejemplo Blinky.
* `bluetooth/`: primera comunicación Bluetooth Low Energy basada en Nordic UART Service.
* `tfg_ble_range/`: prueba de evaluación del alcance BLE con distintos PHY y potencias de transmisión.
* `tfg_direction_finding/`: prueba de Bluetooth Direction Finding y análisis de muestras I/Q.
* `tfg_mesh/`: implementación y evaluación de una red Bluetooth Mesh básica.

Cada una de estas carpetas incluye su propio archivo `README.md`, donde se describe con más detalle el objetivo de la prueba, el contenido incluido y las observaciones necesarias para su uso.

## Hardware utilizado

Durante el desarrollo del trabajo se utilizaron los siguientes dispositivos:

* Placas **nRF5340-DK**.
* Dispositivo **nRF52840-Dongle**.
* Ordenador con entorno de desarrollo nRF Connect.
* Batería externa para alimentar una de las placas durante las pruebas de alcance.

## Software utilizado

Las pruebas se desarrollaron utilizando principalmente:

* **nRF Connect SDK 2.9.0**.
* **Visual Studio Code** con la extensión **nRF Connect for VS Code**.
* **nRF Connect for Desktop**.
* **nRF Mesh** para la configuración de la red Bluetooth Mesh.
* **MATLAB** para el análisis de las muestras I/Q capturadas en la prueba de Direction Finding.

## Relación con la memoria

El repositorio acompaña a la memoria del Trabajo Fin de Grado. Las carpetas se corresponden con las pruebas descritas en los capítulos de desarrollo:

* La carpeta `blinky/` está relacionada con las pruebas básicas de programación del Capítulo 3.
* La carpeta `bluetooth/` está relacionada con la primera prueba BLE del Capítulo 3.
* La carpeta `tfg_ble_range/` corresponde a la prueba de alcance descrita en el apartado 4.1.
* La carpeta `tfg_direction_finding/` corresponde a la prueba de Direction Finding descrita en el apartado 4.2.
* La carpeta `tfg_mesh/` corresponde a la prueba Bluetooth Mesh descrita en el apartado 4.3.

## Carpetas de compilación

En algunas pruebas se conservan carpetas de compilación generadas durante el desarrollo, como `build`, `build_1m`, `build_2m` o `build_coded`. Estas carpetas se mantienen para documentar la configuración empleada durante las pruebas realizadas.

No obstante, los proyectos pueden recompilarse desde nRF Connect for VS Code o mediante las herramientas de línea de comandos de nRF Connect SDK.

## Observaciones

Este repositorio no pretende ser una librería genérica de Bluetooth Low Energy, sino una recopilación organizada del código utilizado en las pruebas experimentales del TFG.

Los proyectos incluidos pueden requerir ajustes menores en función de la versión del SDK, la placa seleccionada o la configuración concreta del entorno de desarrollo.