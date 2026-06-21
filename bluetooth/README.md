# Primera comunicación Bluetooth Low Energy

Esta carpeta contiene la primera prueba de comunicación Bluetooth Low Energy realizada durante el TFG. La prueba se basó en una comunicación entre una placa **nRF5340-DK**, configurada como dispositivo central, y un **nRF52840-Dongle**, configurado como dispositivo periférico.

El objetivo fue validar el funcionamiento básico de una conexión BLE, el descubrimiento de servicios GATT y el intercambio inicial de datos mediante el **Nordic UART Service (NUS)**.

## Contenido de la carpeta

La carpeta incluye dos proyectos:

```text
bluetooth/
│
├── central_uart/
└── my_dongle/
```

* `central_uart/`: proyecto ejecutado en la nRF5340-DK, configurada como dispositivo central.
* `my_dongle/`: proyecto ejecutado en el nRF52840-Dongle, configurado como dispositivo periférico.

## Hardware utilizado

Para esta prueba se utilizaron los siguientes dispositivos:

* **nRF5340-DK**, actuando como central BLE.
* **nRF52840-Dongle**, actuando como periférico BLE.
* Ordenador con nRF Connect SDK y Visual Studio Code.

## Software utilizado

La prueba se desarrolló utilizando:

* **nRF Connect SDK 2.9.0**.
* **Visual Studio Code** con la extensión **nRF Connect for VS Code**.
* **nRF Connect for Desktop**, especialmente para la programación del nRF52840-Dongle mediante Programmer.
* Terminal serie para observar los mensajes generados por ambos dispositivos.

## Descripción de la prueba

En esta prueba, el dispositivo periférico anuncia su disponibilidad mediante paquetes de advertising BLE. El dispositivo central realiza un escaneo activo, detecta el periférico esperado y establece una conexión con él.

Una vez establecida la conexión, el central realiza el descubrimiento GATT, localiza el servicio Nordic UART Service y se suscribe a las notificaciones enviadas por el periférico. Posteriormente, se realiza un intercambio inicial de datos para comprobar que la comunicación bidireccional funciona correctamente.

La prueba permitió validar los siguientes aspectos:

1. Inicialización de la pila Bluetooth.
2. Advertising del dispositivo periférico.
3. Escaneo activo desde el dispositivo central.
4. Establecimiento de una conexión BLE.
5. Descubrimiento de servicios GATT.
6. Suscripción a notificaciones.
7. Intercambio básico de datos entre central y periférico.
8. Visualización de eventos mediante consola serie.

## Programación de los dispositivos

El proyecto `central_uart/` se cargó en la placa **nRF5340-DK** utilizando el programador integrado de la placa.

El proyecto `my_dongle/` se cargó en el **nRF52840-Dongle** mediante el modo DFU y la aplicación Programmer de nRF Connect for Desktop.

## Relación con la memoria

Esta carpeta está relacionada con el apartado **3.4.- Programación básica con Bluetooth** de la memoria del Trabajo Fin de Grado.

## Observaciones

Esta prueba no tuvo como objetivo medir throughput, alcance ni consumo energético. Su finalidad fue comprobar el funcionamiento básico de una comunicación BLE y servir como paso previo antes de desarrollar las pruebas avanzadas del Capítulo 4.

Los proyectos incluidos pueden requerir recompilación en función de la versión del SDK y de la configuración del entorno de desarrollo.
