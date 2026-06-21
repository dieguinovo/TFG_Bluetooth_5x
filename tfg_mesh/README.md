# Bluetooth Mesh

Esta carpeta contiene los proyectos utilizados para la implementación y evaluación de una red Bluetooth Mesh básica durante el TFG.

El objetivo de esta prueba fue comprobar experimentalmente el funcionamiento de una red mallada Bluetooth Mesh y validar el papel de un nodo intermedio con función de relay para restablecer la comunicación entre dos nodos que no pueden comunicarse directamente.

## Contenido de la carpeta

La carpeta incluye dos proyectos principales:

```text
tfg_mesh/
│
├── light/
└── light_switch/
```

El contenido principal es el siguiente:

* `light/`: proyecto utilizado para los nodos de tipo servidor. En la prueba se empleó tanto para el nodo `LIGHT` como para el nodo `RELAY_DK`.
* `light_switch/`: proyecto utilizado para el nodo cliente `SWITCH`.

## Hardware utilizado

Para esta prueba se utilizaron:

* Tres placas **nRF5340-DK**.
* Un dispositivo móvil con la aplicación **nRF Mesh**.
* Ordenador con nRF Connect SDK y Visual Studio Code para programar las placas.

La red estuvo formada por los siguientes nodos:

* `LIGHT`: nodo servidor.
* `SWITCH`: nodo cliente.
* `RELAY_DK`: nodo intermedio utilizado como relay.

## Software utilizado

La prueba se desarrolló utilizando:

* **nRF Connect SDK 2.9.0**.
* **Visual Studio Code** con la extensión **nRF Connect for VS Code**.
* **nRF Mesh** para el aprovisionamiento y configuración de la red.
* Terminal serie para comprobar los mensajes generados por los nodos durante la ejecución.

## Descripción de la prueba

La prueba se basó en los ejemplos oficiales de Bluetooth Mesh incluidos en nRF Connect SDK.

El nodo `LIGHT` se configuró como servidor mediante el ejemplo `light`. Este nodo recibe mensajes del modelo **Generic OnOff** y modifica el estado de su LED en función de los mensajes recibidos.

El nodo `SWITCH` se configuró como cliente mediante el ejemplo `light_switch`. Este nodo envía mensajes Generic OnOff al servidor cuando se pulsa el botón correspondiente de la placa.

El nodo `RELAY_DK` también se programó con el ejemplo `light`, pero en la prueba su función principal fue actuar como nodo intermedio dentro de la red Mesh, permitiendo la retransmisión de mensajes entre el cliente y el servidor.

## Aprovisionamiento y configuración

La red Bluetooth Mesh se creó y configuró mediante la aplicación móvil **nRF Mesh**.

El procedimiento general fue el siguiente:

1. Crear una nueva red Bluetooth Mesh.
2. Generar o añadir una Application Key.
3. Aprovisionar el nodo servidor `LIGHT`.
4. Asociar la Application Key al modelo **Generic OnOff Server**.
5. Anotar la dirección unicast asignada al nodo servidor.
6. Aprovisionar el nodo cliente `SWITCH`.
7. Asociar la Application Key al modelo **Generic OnOff Client**.
8. Configurar la publicación del cliente hacia la dirección unicast del servidor.
9. Aprovisionar el nodo intermedio `RELAY_DK`.
10. Situar el nodo relay entre el cliente y el servidor durante la prueba experimental.

## Escenarios evaluados

La prueba se dividió en tres fases:

1. **Comunicación directa a corta distancia**
   El nodo cliente y el nodo servidor se situaron próximos entre sí. Al pulsar el botón del cliente, el LED del servidor cambiaba de estado correctamente.

2. **Pérdida de comunicación directa**
   Los nodos extremos se separaron hasta una disposición en la que la comunicación directa dejaba de ser posible. En esta situación, la pulsación del botón del cliente no producía cambios en el servidor.

3. **Comunicación mediante nodo relay**
   Se introdujo el nodo `RELAY_DK` en una posición intermedia. Con esta configuración, la comunicación entre cliente y servidor volvió a funcionar, comprobándose el efecto del nodo relay dentro de la red Mesh.

## Resultado de la prueba

La prueba permitió comprobar que una red Bluetooth Mesh puede recuperar la comunicación entre dos nodos extremos mediante la incorporación de un nodo intermedio con capacidad de retransmisión.

El resultado se validó de forma funcional mediante el cambio de estado del LED del nodo servidor al pulsar el botón del nodo cliente.

## Relación con la memoria

Esta carpeta corresponde a la prueba descrita en el apartado **4.3.- Implementación y evaluación de una red Bluetooth Mesh** de la memoria del Trabajo Fin de Grado.

En la memoria se describe el fundamento de Bluetooth Mesh, el aprovisionamiento de los nodos, la configuración de la red y los escenarios utilizados para validar el funcionamiento del nodo relay.

## Observaciones

Esta prueba no está orientada a caracterizar el rendimiento completo de Bluetooth Mesh. No se midieron métricas como latencia, throughput, consumo energético o número máximo de nodos.

El objetivo fue validar de forma práctica el funcionamiento básico de una red mallada y comprobar la utilidad de un nodo relay como elemento de extensión de cobertura.
