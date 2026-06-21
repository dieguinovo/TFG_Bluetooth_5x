# Bluetooth Direction Finding

Esta carpeta contiene los archivos utilizados en la prueba de Bluetooth Direction Finding realizada en el TFG.

El objetivo de esta prueba fue validar la generación y recepción de paquetes Bluetooth Low Energy con **Constant Tone Extension (CTE)**, así como capturar y analizar las muestras **I/Q** obtenidas durante dicho intervalo.

La prueba se realizó sin matriz de antenas externa. Por este motivo, no se obtiene una estimación angular real, sino que se valida la parte inicial de la cadena necesaria para una futura implementación de **Angle of Arrival (AoA)**.

## Contenido de la carpeta

La carpeta incluye los proyectos de transmisión y recepción, los scripts de análisis en MATLAB y varios archivos auxiliares relacionados con una futura ampliación mediante matriz de antenas.

```text
tfg_direction_finding/
│
├── direction_finding_connectionless_tx/
├── direction_finding_connectionless_rx/
├── analisis_iq_cte.m
├── estimacion_aoa_array.m
├── log.txt
├── overlay_cpuapp_template.overlay
└── overlay_cpunet_template.overlay
```

El contenido principal es el siguiente:

* `direction_finding_connectionless_tx/`: proyecto utilizado en la placa transmisora.
* `direction_finding_connectionless_rx/`: proyecto utilizado en la placa receptora.
* `analisis_iq_cte.m`: script de MATLAB utilizado para analizar las muestras I/Q capturadas durante el CTE.
* `estimacion_aoa_array.m`: plantilla de MATLAB preparada para una futura estimación angular con matriz de antenas.
* `log.txt`: archivo de ejemplo con los bloques CTE y las muestras I/Q capturadas por consola.
* `overlay_cpuapp_template.overlay`: plantilla de overlay para una futura configuración de GPIO en el núcleo de aplicación.
* `overlay_cpunet_template.overlay`: plantilla de overlay para una futura configuración asociada al núcleo de red.

## Hardware utilizado

Para esta prueba se utilizaron:

* Dos placas **nRF5340-DK**.
* Un ordenador para programar las placas y visualizar la salida por consola.
* MATLAB para el análisis posterior de las muestras capturadas.

No se utilizó una matriz de antenas externa ni un conmutador RF real. Las muestras I/Q proceden únicamente de la antena integrada de la placa receptora.

## Software utilizado

La prueba se desarrolló utilizando:

* **nRF Connect SDK 2.9.0**.
* **Visual Studio Code** con la extensión **nRF Connect for VS Code**.
* Terminal serie para capturar los bloques CTE y las muestras I/Q.
* **MATLAB** para el procesado posterior de los datos.

## Descripción de la prueba

La prueba se basó en los ejemplos oficiales de Direction Finding sin conexión incluidos en nRF Connect SDK.

La placa transmisora ejecuta el ejemplo `direction_finding_connectionless_tx`, actuando como beacon de advertising periódico con CTE. La placa receptora ejecuta el ejemplo `direction_finding_connectionless_rx`, actuando como locator y sincronizándose con el advertising periódico para capturar las muestras I/Q durante el CTE.

El código del receptor se modificó para imprimir por consola los bloques CTE recibidos. Cada bloque incluye una cabecera con información del informe recibido y una tabla con las muestras I/Q.

Durante la prueba se capturaron varios bloques CTE consecutivos, obteniéndose muestras válidas en todos ellos.

## Análisis de muestras I/Q

El archivo `log.txt` contiene un ejemplo de los datos capturados por consola durante la prueba. Este archivo fue utilizado como entrada para el script `analisis_iq_cte.m`.

El script de MATLAB permite:

1. Leer el archivo de log.
2. Extraer los bloques CTE completos.
3. Reconstruir la señal compleja a partir de las componentes I y Q.
4. Representar las muestras en el plano I/Q.
5. Calcular la fase instantánea.
6. Aplicar `unwrap` a la fase.
7. Calcular la amplitud de la señal.
8. Analizar la evolución del RSSI y de la estabilidad de fase entre bloques.

Este análisis permitió comprobar que las muestras capturadas durante el CTE presentan un comportamiento coherente con la recepción de una señal de tono constante.

## Limitación de la prueba

Aunque la salida del receptor indique `type=AOA`, esta prueba no calcula un ángulo de llegada real. Para obtener una estimación angular sería necesario disponer de una matriz de antenas y de un conmutador RF que permitiera asociar las muestras I/Q a distintas antenas.

En esta prueba, todas las muestras proceden de la antena integrada de la placa receptora. Por tanto, no existe información espacial suficiente para calcular diferencias de fase entre antenas.

## Preparación para futura ampliación

Se incluyen varios archivos preparados para una posible ampliación con matriz de antenas:

* `overlay_cpuapp_template.overlay`
* `overlay_cpunet_template.overlay`
* `estimacion_aoa_array.m`

Los archivos `.overlay` son plantillas para documentar cómo podrían configurarse los GPIO necesarios para controlar un conmutador RF externo. Los GPIO incluidos en estas plantillas son solo ejemplos y deberán sustituirse por los pines reales de la matriz utilizada.

De forma orientativa, el overlay del núcleo de aplicación podría incorporarse al proyecto del receptor como un overlay de placa, por ejemplo:

```text
direction_finding_connectionless_rx/boards/nrf5340dk_nrf5340_cpuapp.overlay
```

El overlay asociado al núcleo de red dependerá de la estructura final del proyecto y de cómo se genere la imagen del núcleo de red en la versión del SDK utilizada. Por ese motivo, `overlay_cpunet_template.overlay` se conserva únicamente como plantilla de referencia.

Cuando se disponga de una matriz de antenas real, será necesario:

1. Revisar el esquemático de la matriz.
2. Identificar las líneas de control del conmutador RF.
3. Conectar dichas líneas a GPIO disponibles de la nRF5340-DK.
4. Sustituir los GPIO de ejemplo por los GPIO reales.
5. Adaptar el vector `ant_patterns[]` al orden real de conmutación de antenas.
6. Repetir la captura de CTE.
7. Asociar las muestras I/Q a las distintas antenas.
8. Calcular las diferencias de fase necesarias para estimar el ángulo.

## Relación con la memoria

Esta carpeta corresponde a la prueba descrita en el apartado **4.2.- Evaluación de Direction Finding: ángulo de llegada (AoA) y ángulo de salida (AoD)** de la memoria del Trabajo Fin de Grado.

En la memoria se presentan los fundamentos de Direction Finding, la configuración experimental sin matriz de antenas, el análisis de las muestras I/Q y la preparación para una futura ampliación con matriz de antenas.

## Observaciones

El archivo `log.txt` se incluye como registro de ejemplo de la prueba realizada y como entrada para el análisis en MATLAB.

Las carpetas de compilación se conservan para documentar la configuración empleada durante el desarrollo. No obstante, los proyectos pueden recompilarse desde nRF Connect for VS Code.

La prueba valida la transmisión del CTE, la sincronización del receptor y la captura de muestras I/Q, pero la estimación angular real queda pendiente de una ampliación hardware con matriz de antenas.
