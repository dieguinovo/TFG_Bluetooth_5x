#include <zephyr/kernel.h>              /* Kernel de Zephyr: timers, sleeps, workqueues, uptime, etc. */
#include <zephyr/sys/byteorder.h>       /* Utilidades para conversión endianess (little-endian, etc.) */
#include <zephyr/net_buf.h>             /* Gestión de buffers usados por HCI y red */

#include <zephyr/bluetooth/bluetooth.h> /* Inicialización general del stack Bluetooth */
#include <zephyr/bluetooth/conn.h>      /* Gestión de conexiones BLE */
#include <zephyr/bluetooth/gatt.h>      /* Funciones GATT: descubrimiento, suscripción, atributos */
#include <zephyr/bluetooth/hci.h>       /* Comandos y estructuras HCI estándar */

#include <zephyr/bluetooth/hci_vs.h>    /* Comandos HCI vendor-specific de Nordic, por ejemplo potencia TX */

/* Definición del PHY a utilizar basado en la configuración del proyecto */
#if defined(CONFIG_APP_PHY_2M)
#define APP_PHY BT_GAP_LE_PHY_2M
#elif defined(CONFIG_APP_PHY_CODED)
#define APP_PHY BT_GAP_LE_PHY_CODED
#else
#define APP_PHY BT_GAP_LE_PHY_1M  /* PHY por defecto: 1 Mbps */
#endif
 
/* Potencia TX deseada en dBm */
#define APP_TX_DBM   (-40)   /* Valores típicos soportados: -40, 0, +3 */

/* UUID personalizado de la característica GATT del periférico que queremos encontrar */
#define BT_UUID_RANGE_CHAR_VAL BT_UUID_128_ENCODE(0x8e3c2a11, 0x5b41, 0x4a9a, 0x9d3f, 0x3a9f7f1b2c01)
#define BT_UUID_RANGE_CHAR      BT_UUID_DECLARE_128(BT_UUID_RANGE_CHAR_VAL)

/* -------------------- Variables globales -------------------- */

/* Puntero a la conexión BLE actual */
static struct bt_conn *conn;

/* Estructuras usadas durante el descubrimiento GATT y la suscripción a notificaciones */
static struct bt_gatt_discover_params disc_params;
static struct bt_gatt_subscribe_params sub_params;

/* Handles GATT del periférico remoto */
static uint16_t value_handle;  /* Handle del valor de la característica encontrada */
static uint16_t ccc_handle;    /* Handle del descriptor CCC para habilitar notificaciones */

/* Variables para control de secuencia y estadísticas */
static uint32_t last_seq;        /* Último número de secuencia recibido */
static bool have_seq;            /* Indica si ya se recibió al menos un primer paquete válido */
static uint32_t ok_cnt;          /* Número total de paquetes recibidos */
static uint32_t lost_cnt;        /* Número total de paquetes perdidos detectados por salto de secuencia */
static int64_t last_stats_ms;    /* Instante de la última ventana estadística */
static uint32_t last_ok_cnt;     /* Valor de ok_cnt al comienzo de la ventana */

/* -------------------- RSSI asíncrono -------------------- */

/* RSSI actual leído de la conexión.
 * 127 suele usarse como valor "inválido" o "sin dato" para debug.
 */
static int8_t current_rssi = 127;

/* Prototipo del worker que leerá RSSI fuera del callback Bluetooth */
static void rssi_worker_handler(struct k_work *work);

/* Work item: se ejecuta en el contexto de workqueue, no en el hilo RX de Bluetooth */
K_WORK_DEFINE(rssi_work, rssi_worker_handler);

/* Callback del timer periódico:
 * no lee directamente el RSSI, solo lanza el work item.
 */
static void rssi_timer_handler(struct k_timer *dummy)
{
    k_work_submit(&rssi_work);
}

/* Timer que dispara la lectura de RSSI */
K_TIMER_DEFINE(rssi_timer, rssi_timer_handler, NULL);

/* -------------------- Funciones auxiliares -------------------- */

/* Convierte el identificador del PHY a texto para mostrarlo por consola */
static const char *phy_name(uint8_t phy)
{
    switch (phy) {
    case BT_GAP_LE_PHY_1M:    return "1M";
    case BT_GAP_LE_PHY_2M:    return "2M";
    case BT_GAP_LE_PHY_CODED: return "CODED";
    default:                  return "?";
    }
}

/* Inicia un escaneo BLE activo.
 * Al ser activo, además de escuchar anuncios, el central puede pedir scan response.
 */
static void start_scan(void)
{
    struct bt_le_scan_param scan_param = {
        .type = BT_HCI_LE_SCAN_ACTIVE,          /* Escaneo activo */
        .options = 0,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,  /* Intervalo rápido */
        .window = BT_GAP_SCAN_FAST_WINDOW,      /* Ventana rápida */
    };

    int err = bt_le_scan_start(&scan_param, NULL);
    printk("CENT: scan_start err=%d\n", err);
}

/* Resetea el estado interno relacionado con una conexión.
 * Se usa al arrancar y tras desconexiones.
 */
static void reset_link_state(void)
{
    have_seq = false;
    ok_cnt = 0;
    lost_cnt = 0;
    last_seq = 0;

    value_handle = 0;
    ccc_handle = 0;
    current_rssi = 127;
    last_stats_ms = 0;
    last_ok_cnt = 0;

    /* Limpia las estructuras de descubrimiento y suscripción */
    memset(&disc_params, 0, sizeof(disc_params));
    memset(&sub_params, 0, sizeof(sub_params));
}

/* -------------------- Lectura de RSSI -------------------- */

/* Lee el RSSI de la conexión actual mediante comando HCI estándar READ_RSSI */
static int8_t read_rssi(struct bt_conn *c)
{
    uint16_t handle;
    struct net_buf *buf, *rsp = NULL;
    int err;

    /* Obtiene el handle HCI asociado a la conexión BLE */
    err = bt_hci_get_conn_handle(c, &handle);
    if (err) return 127;

    /* Crea un buffer para el comando HCI READ_RSSI */
    buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(struct bt_hci_cp_read_rssi));
    if (!buf) return 127;

    /* Añade al buffer la estructura de parámetros del comando */
    struct bt_hci_cp_read_rssi *cp = net_buf_add(buf, sizeof(*cp));
    cp->handle = sys_cpu_to_le16(handle);  /* El handle debe enviarse en little-endian */

    /* Envía el comando y espera la respuesta de forma síncrona */
    err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
    if (err || !rsp) return 127;

    /* Interpreta la respuesta como la estructura correspondiente */
    struct bt_hci_rp_read_rssi *rp = (void *)rsp->data;
    int8_t rssi = rp->rssi;

    /* Libera la respuesta */
    net_buf_unref(rsp);

    return rssi;
}

/* Worker que se ejecuta periódicamente y lee el RSSI de forma segura */
static void rssi_worker_handler(struct k_work *work)
{
    if (conn) {
        current_rssi = read_rssi(conn);

        /* Se imprime aquí en vez de dentro del callback de notificación
         * para no sobrecargar el hilo RX de Bluetooth.
         */
        printk("CENT: RSSI=%d dBm  seq=%u  ok=%u  lost=%u\n",
               current_rssi, last_seq, ok_cnt, lost_cnt);
    }
}

/* -------------------- Manejo de notificaciones -------------------- */

/* Callback que se ejecuta cada vez que llega una notificación GATT del periférico */
static uint8_t notify_func(struct bt_conn *c, struct bt_gatt_subscribe_params *params,
               const void *data, uint16_t length)
{
    /* Si no hay datos o llegan menos de 4 bytes, no se procesa */
    if (!data || length < 4) {
        return BT_GATT_ITER_CONTINUE;
    }

    /* El periférico envía un uint32_t little-endian con el número de secuencia */
    uint32_t seq = sys_get_le32(data);

    /* Contabiliza este paquete como recibido */
    ok_cnt++;

    /* Detección de pérdidas usando el contador de secuencia */
    if (!have_seq) {
        /* Primer paquete válido recibido */
        have_seq = true;
        last_seq = seq;
    } else if (seq > last_seq + 1) {
        /* Ha habido salto: faltan paquetes intermedios */
        lost_cnt += (seq - last_seq - 1);
        last_seq = seq;
    } else if (seq > last_seq) {
        /* Secuencia correcta y creciente */
        last_seq = seq;
    }

    /* No se lee el RSSI aquí para no bloquear el hilo RX de Bluetooth */

    return BT_GATT_ITER_CONTINUE;
}

/* -------------------- Descubrimiento GATT y suscripción -------------------- */

/* Callback usado durante el descubrimiento de atributos GATT */
static uint8_t discover_func(struct bt_conn *c, const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    /* attr == NULL significa que el descubrimiento terminó */
    if (!attr) {
        printk("CENT: discovery done (type=%u)\n", params->type);

        /* Si buscábamos descriptores y no apareció el CCC, se informa */
        if (params->type == BT_GATT_DISCOVER_DESCRIPTOR && ccc_handle == 0) {
            printk("CENT: CCC not found!\n");
        }

        return BT_GATT_ITER_STOP;
    }

    /* -------- Descubrimiento de características -------- */
    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)attr->user_data;

        /* Comprueba si el UUID coincide con la característica deseada */
        if (bt_uuid_cmp(chrc->uuid, BT_UUID_RANGE_CHAR) != 0) {
            return BT_GATT_ITER_CONTINUE;
        }

        /* Guardamos el handle del valor de la característica */
        value_handle = chrc->value_handle;
        printk("CENT: char found! (value_handle=%u). Discovering DESCRIPTORS...\n", value_handle);

        /* Reinicia el handle CCC antes de buscar descriptores */
        ccc_handle = 0;

        /* Reconfigura disc_params para descubrir descriptores a partir del value_handle */
        disc_params.uuid = NULL;                  /* Buscar todos los descriptores */
        disc_params.start_handle = value_handle + 1;
        disc_params.end_handle = 0xffff;
        disc_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

        int err = bt_gatt_discover(c, &disc_params);
        printk("CENT: discover DESCRIPTORS err=%d\n", err);

        /* Paramos el descubrimiento de características porque ya encontramos la nuestra */
        return BT_GATT_ITER_STOP;
    }

    /* -------- Descubrimiento de descriptores -------- */
    if (params->type == BT_GATT_DISCOVER_DESCRIPTOR) {

        /* Información de depuración: muestra el UUID del descriptor encontrado */
        if (attr->uuid->type == BT_UUID_TYPE_16) {
            const struct bt_uuid_16 *u16 = (const struct bt_uuid_16 *)attr->uuid;
            printk("CENT: descriptor handle=%u uuid16=0x%04x\n", attr->handle, u16->val);
        } else {
            printk("CENT: descriptor handle=%u uuid=128-bit\n", attr->handle);
        }

        /* Solo nos interesa el CCC */
        if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC) != 0) {
            return BT_GATT_ITER_CONTINUE;
        }

        /* Se encontró el CCC */
        ccc_handle = attr->handle;
        printk("CENT: CCC found! (ccc_handle=%u). Subscribing...\n", ccc_handle);

        /* Configuración de la suscripción a notificaciones */
        sub_params.notify = notify_func;              /* Callback al llegar cada notificación */
        sub_params.value = BT_GATT_CCC_NOTIFY;        /* Activa notificaciones */
        sub_params.value_handle = value_handle;       /* Handle del valor */
        sub_params.ccc_handle = ccc_handle;           /* Handle del descriptor CCC */

        int err = bt_gatt_subscribe(c, &sub_params);
        printk("CENT: bt_gatt_subscribe err=%d\n", err);

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_STOP;
}

/* Work item retrasable para iniciar el descubrimiento GATT poco después de conectar */
static void start_discovery_work(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(start_disc, start_discovery_work);

static void start_discovery_work(struct k_work *work)
{
    if (!conn) {
        printk("CENT: discovery skipped (no conn)\n");
        return;
    }

    /* Descubre características en toda la tabla de atributos del periférico */
    disc_params.func = discover_func;
    disc_params.uuid = NULL;               /* Busca todas las características */
    disc_params.start_handle = 1;
    disc_params.end_handle = 0xffff;
    disc_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

    int err = bt_gatt_discover(conn, &disc_params);
    printk("CENT: bt_gatt_discover(err=%d)\n", err);
}

/* -------------------- Configuración de PHY -------------------- */

/* Solicita al controlador cambiar el PHY de la conexión al definido en APP_PHY */
static void request_phy(struct bt_conn *c)
{
    struct bt_conn_le_phy_param phy = {
        .options = BT_CONN_LE_PHY_OPT_NONE,
        .pref_tx_phy = APP_PHY,
        .pref_rx_phy = APP_PHY,
    };

#if (APP_PHY == BT_GAP_LE_PHY_CODED)
    /* Si se usa PHY CODED, se selecciona S8 para priorizar alcance */
    phy.options = BT_CONN_LE_PHY_OPT_CODED_S8;
#endif

    int err = bt_conn_le_phy_update(c, &phy);
    printk("CENT: request PHY=%s err=%d\n", phy_name(APP_PHY), err);
}

/* -------------------- Configuración de potencia TX -------------------- */

/* Escribe la potencia TX deseada usando comando HCI vendor-specific de Nordic */
static int set_tx_power(uint8_t handle_type, uint16_t handle, int8_t desired_dbm, int8_t *selected_dbm)
{
    struct net_buf *buf, *rsp = NULL;
    int err;

    buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
                sizeof(struct bt_hci_cp_vs_write_tx_power_level));
    if (!buf) {
        return -ENOMEM;
    }

    struct bt_hci_cp_vs_write_tx_power_level *cp = net_buf_add(buf, sizeof(*cp));
    cp->handle_type = handle_type;
    cp->handle = sys_cpu_to_le16(handle);
    cp->tx_power_level = desired_dbm;

    err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
    if (err) {
        return err;
    }

    struct bt_hci_rp_vs_write_tx_power_level *rp = (void *)rsp->data;
    if (rp->status) {
        err = -EIO;
    } else if (selected_dbm) {
        *selected_dbm = rp->selected_tx_power; /* Potencia realmente aceptada por el controlador */
    }

    net_buf_unref(rsp);
    return err;
}

/* Lee la potencia TX actual usando comando vendor-specific */
static int get_tx_power(uint8_t handle_type, uint16_t handle, int8_t *cur_dbm)
{
    struct net_buf *buf, *rsp = NULL;
    int err;

    buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_TX_POWER_LEVEL,
                            sizeof(struct bt_hci_cp_vs_read_tx_power_level));
    if (!buf) return -ENOMEM;

    struct bt_hci_cp_vs_read_tx_power_level *cp = net_buf_add(buf, sizeof(*cp));
    cp->handle_type = handle_type;
    cp->handle = sys_cpu_to_le16(handle);

    err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_TX_POWER_LEVEL, buf, &rsp);
    if (err) return err;

    struct bt_hci_rp_vs_read_tx_power_level *rp = (void *)rsp->data;
    if (rp->status) err = -EIO;
    else if (cur_dbm) *cur_dbm = rp->tx_power_level;

    net_buf_unref(rsp);
    return err;
}

/* -------------------- Escaneo y conexión -------------------- */

/* Comprueba si un advertising packet pertenece al periférico que nos interesa */
static bool is_our_adv(struct net_buf_simple *ad)
{
    struct net_buf_simple buf = *ad;

    /* Recorre los campos AD en formato length-type-value */
    while (buf.len > 1) {
        uint8_t len = net_buf_simple_pull_u8(&buf);
        if (len == 0 || len > buf.len) return false;

        uint8_t type = net_buf_simple_pull_u8(&buf);
        len -= 1;

        /* Busca manufacturer data con la firma propia */
        if (type == BT_DATA_MANUFACTURER_DATA && len >= 4) {
            const uint8_t *d = buf.data;

            /* Patrón definido para identificar el periférico:
             * 0xFFFF + 'L' + 'R'
             */
            if (d[0] == 0xFF && d[1] == 0xFF && d[2] == 'L' && d[3] == 'R') {
                return true;
            }
        }

        /* Avanza al siguiente campo AD */
        net_buf_simple_pull(&buf, len);
    }

    return false;
}

/* Callback invocado por Zephyr cuando se recibe un advertising */
static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad)
{
    /* Ignora anuncios que no sean del periférico objetivo */
    if (!is_our_adv(ad)) return;

    /* Si ya hay una conexión activa, ignoramos nuevos anuncios */
    if (conn) {
        return;
    }

    printk("CENT: found device, connecting... scanRSSI=%d\n", info->rssi);

    /* Detiene el escaneo antes de iniciar la conexión */
    bt_le_scan_stop();

    /* Parámetros de creación de conexión */
    struct bt_conn_le_create_param create_param = BT_CONN_LE_CREATE_PARAM_INIT(
        BT_CONN_LE_OPT_NONE,
        BT_GAP_SCAN_FAST_INTERVAL,
        BT_GAP_SCAN_FAST_INTERVAL
    );

    /* Parámetros de conexión:
     * intervalo mínimo y máximo = 24 unidades de 1.25 ms => 30 ms
     * intervalo mínimo y máximo = 96 unidades de 1.25 ms => 120 ms
     * latency = 0
     * timeout = 800 unidades de 10 ms => 8 s
     * timeout = 2000 unidades de 10 ms => 20 s
     */
    struct bt_le_conn_param *conn_param = BT_LE_CONN_PARAM(24, 24, 0, 800);
    //struct bt_le_conn_param *conn_param = BT_LE_CONN_PARAM(96, 96, 0, 2000); // Para prueba máxima

    /* Crea la conexión y guarda la referencia en conn */
    int err = bt_conn_le_create(info->addr, &create_param, conn_param, &conn);
    printk("CENT: bt_conn_le_create err=%d\n", err);

    if (err) {
        /* Si falla, libera referencia y vuelve a escanear */
        if (conn) {
            bt_conn_unref(conn);
            conn = NULL;
        }
        k_sleep(K_MSEC(200));
        start_scan();
    }
}

/* Estructura de callbacks de escaneo */
static struct bt_le_scan_cb scan_cb = {
    .recv = scan_recv,
};

/* -------------------- Callbacks de conexión -------------------- */

/* Callback al terminar el procedimiento de conexión */
static void connected(struct bt_conn *c, uint8_t err)
{
    if (err) {
        printk("CENT: connect failed (err %u)\n", err);

        /* Limpia la conexión fallida y reanuda escaneo */
        if (conn) {
            bt_conn_unref(conn);
            conn = NULL;
        }
        k_sleep(K_MSEC(200));
        start_scan();
        return;
    }

    printk("CENT: connected (target PHY=%s)\n", phy_name(APP_PHY));

    /* Ajuste de potencia TX de la conexión */
    uint16_t handle;
    int8_t sel = 0;
    int8_t cur = 127;

    if (bt_hci_get_conn_handle(c, &handle) == 0) {

        int e = set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, handle, APP_TX_DBM, &sel);
        printk("CENT: TXPWR CONN desired=%d selected=%d (err=%d)\n",
            APP_TX_DBM, sel, e);

        /* Si la escritura fue bien, se hace lectura de comprobación */
        if (e == 0) {
            int r = get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, handle, &cur);
            printk("CENT: TXPWR CONN readback=%d (err=%d)\n", cur, r);
        }
    }

    /* Solicita el PHY deseado */
    request_phy(c);

    /* Lanza el descubrimiento GATT con un pequeño retardo
     * para dar tiempo a estabilizar la conexión
     */
    k_work_schedule(&start_disc, K_MSEC(200));

    /* Arranca timer periódico para lectura de RSSI cada segundo */
    k_timer_start(&rssi_timer, K_SECONDS(1), K_SECONDS(1));
}

/* Callback al desconectar */
static void disconnected(struct bt_conn *c, uint8_t reason)
{
    /* Para el timer de RSSI */
    k_timer_stop(&rssi_timer);

    printk("CENT: disconnected (reason 0x%02x)\n", reason);

    /* Limpia variables internas */
    reset_link_state();

    /* Libera la referencia a la conexión */
    if (conn) {
        bt_conn_unref(conn);
        conn = NULL;
    }

    /* Reanuda escaneo */
    k_sleep(K_MSEC(200));
    start_scan();
}

/* Callback cuando el controlador informa de un cambio de PHY */
static void le_phy_updated(struct bt_conn *c, struct bt_conn_le_phy_info *param)
{
    printk("CENT: PHY updated tx=%s rx=%s\n", phy_name(param->tx_phy), phy_name(param->rx_phy));

    /* Si por cualquier motivo el PHY resultante no coincide con el deseado,
     * se vuelve a solicitar.
     */
    if (param->tx_phy != APP_PHY || param->rx_phy != APP_PHY) {
        printk("CENT: unexpected PHY -> forcing back to %s\n", phy_name(APP_PHY));
        request_phy(c);
    }
}

/* Muestra cada 10 s cuántas notificaciones han llegado en esa ventana */
static void stats_tick(void)
{
    if (!conn) {
        last_stats_ms = 0;
        last_ok_cnt = 0;
        return;
    }

    int64_t now = k_uptime_get();

    /* Primera vez que entra con conexión activa: inicializa ventana */
    if (last_stats_ms == 0) {
        last_stats_ms = now;
        last_ok_cnt = ok_cnt;
        return;
    }

    /* Cada 10 s calcula el incremento de paquetes recibidos */
    if (now - last_stats_ms >= 10000) {
        uint32_t ok_delta = ok_cnt - last_ok_cnt;
        // Cambiar el print dependiendo del periodo en el peripheral
        printk("CENT: OK/10s=%u (expected ~20)\n", ok_delta); // Con 500 ms
        // printk("CENT: OK/10s=%u (expected ~10)\n", ok_delta); // Con 1000 ms
        last_ok_cnt = ok_cnt;
        last_stats_ms = now;
    }
}

/* Registro global de callbacks de conexión */
static struct bt_conn_cb conn_cb = {
    .connected = connected,
    .disconnected = disconnected,
    .le_phy_updated = le_phy_updated,
};

/* -------------------- Función principal -------------------- */

int main(void)
{
    printk("CENT: init (build target PHY=%s)\n", phy_name(APP_PHY));

    /* Inicializa la pila Bluetooth */
    int err = bt_enable(NULL);
    if (err) {
        printk("CENT: bt_enable failed (err %d)\n", err);
        return 0;
    }

    /* Registra callbacks de conexión y escaneo */
    bt_conn_cb_register(&conn_cb);
    bt_le_scan_cb_register(&scan_cb);

    /* Limpia estado inicial */
    reset_link_state();

    /* Comienza a escanear periféricos */
    start_scan();

    /* Bucle principal:
     * la lógica real va por callbacks, aquí solo se imprime la estadística periódica
     */
    while (1) {
        stats_tick();
        k_sleep(K_SECONDS(1));
    }
}