# Evaluación del alcance BLE

Esta carpeta contiene los proyectos utilizados para la prueba de evaluación del alcance de una comunicación Bluetooth Low Energy en modo conexión.

El objetivo de la prueba fue comparar el comportamiento de distintos PHY de Bluetooth 5.x y analizar cómo influyen la capa física y la potencia de transmisión sobre la estabilidad y la distancia máxima del enlace.

## Contenido de la carpeta

La carpeta incluye los proyectos correspondientes al dispositivo central y al dispositivo periférico utilizados durante la prueba de alcance.

El firmware del periférico se encarga de anunciarse mediante advertising conectable y, una vez establecida la conexión, enviar notificaciones periódicas al central. El firmware del central realiza el escaneo, establece la conexión, configura el PHY utilizado, recibe las notificaciones y registra por consola la información necesaria para evaluar el enlace.

Se conservan también carpetas de compilación, como `build_1m`, `build_2m` o `build_coded`, correspondientes a las distintas configuraciones empleadas durante la campaña experimental.

## Hardware utilizado

Para esta prueba se utilizaron:

* Dos placas **nRF5340-DK**.
* Un ordenador portátil para programar y monitorizar el nodo central.
* Una batería externa para alimentar el nodo periférico durante las medidas de campo.

## Software utilizado

La prueba se desarrolló con:

* **nRF Connect SDK 2.9.0**.
* **Visual Studio Code** con la extensión **nRF Connect for VS Code**.
* Terminal serie para visualizar los registros generados por el dispositivo central y el periférico.

## Descripción de la prueba

La comunicación se realizó en modo conexión entre dos placas nRF5340-DK. Una de ellas actuó como dispositivo central y la otra como dispositivo periférico.

El periférico transmite una notificación cada 500 ms mediante una característica GATT de tipo Notify. La carga útil de cada notificación contiene un contador de secuencia de 32 bits, utilizado para comprobar la continuidad de la comunicación y detectar posibles pérdidas de mensajes.

El central recibe las notificaciones, contabiliza cuántas llegan correctamente en ventanas de 10 segundos y lee periódicamente el RSSI de la conexión mediante un comando HCI. Además, el central solicita la actualización del PHY correspondiente a cada configuración de prueba.

## Configuraciones evaluadas

Durante la prueba se evaluaron los siguientes PHY:

* **LE 1M**.
* **LE 2M**.
* **LE Coded S=8**.

También se utilizaron dos niveles de potencia de transmisión:

* **+3 dBm**, para evaluar el comportamiento con un margen radio elevado.
* **-40 dBm**, para reducir el alcance y poder observar el punto de fallo dentro de una distancia menor.

En cada ensayo se configuró el mismo valor de potencia tanto en el central como en el periférico.

## Métricas registradas

Durante las medidas se registraron principalmente:

* PHY utilizado.
* Potencia de transmisión configurada.
* RSSI de la conexión.
* Número de notificaciones recibidas en cada ventana de 10 segundos.
* Pérdidas de notificaciones detectadas mediante el contador de secuencia.
* Eventos de desconexión y reconexión.

El parámetro `OK/10s` indica el número de notificaciones recibidas correctamente en una ventana de 10 segundos. Dado que el periférico envía una notificación cada 500 ms, el valor esperado en condiciones ideales es de aproximadamente 20 notificaciones por ventana.

## Relación con la memoria

Esta carpeta corresponde a la prueba descrita en el apartado **4.1.- Evaluación del alcance máximo** de la memoria del Trabajo Fin de Grado.

En dicha prueba se comparó el alcance observado para los PHY LE 1M, LE 2M y LE Coded S=8, utilizando potencias de transmisión de +3 dBm y -40 dBm.

## Observaciones

Las carpetas de compilación se conservan para documentar las configuraciones utilizadas durante el desarrollo y la realización de las pruebas. No obstante, los proyectos pueden recompilarse de nuevo desde nRF Connect for VS Code.

Los resultados obtenidos dependen del entorno de propagación, la orientación de las placas, la altura de colocación, la potencia configurada y las condiciones radioeléctricas presentes durante la medida.

