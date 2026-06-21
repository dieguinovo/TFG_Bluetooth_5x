#include <zephyr/kernel.h>              /* Kernel de Zephyr: sleep, workqueue, delays, etc. */
#include <zephyr/sys/byteorder.h>       /* Conversión de enteros a little-endian */
#include <zephyr/bluetooth/bluetooth.h> /* Inicialización general del stack Bluetooth */
#include <zephyr/bluetooth/gatt.h>      /* Servicios, características y notificaciones GATT */
#include <zephyr/bluetooth/conn.h>      /* Gestión de conexiones BLE */
#include <zephyr/bluetooth/uuid.h>      /* Manejo de UUID Bluetooth */

#include <zephyr/bluetooth/hci_vs.h>    /* Comandos HCI vendor-specific de Nordic */
#include <zephyr/net_buf.h>             /* Buffers usados por comandos HCI */
#include <zephyr/sys/byteorder.h>       /* Conversión endianess */

/* ---------- Variables globales ---------- */

/* Puntero al advertising set extendido */
static struct bt_le_ext_adv *adv;

/* Puntero a la conexión actual con el cliente central */
static struct bt_conn *current_conn;

/* Indica si el central ha habilitado notificaciones escribiendo en el CCC */
static bool notify_enabled;

/* Contador de secuencia que se enviará al central */
static uint32_t seq;

/* ---------- Constantes de configuración ---------- */

/* Tiempo entre notificaciones sucesivas */
#define NOTIFY_PERIOD_MS       500
//#define NOTIFY_PERIOD_MS       1000
// Acordarse de cambiar el texto de stats_tick() en central si se cambia periodo

/* Tiempo de espera cuando falla bt_gatt_notify por falta de buffers */
#define ENOMEM_BACKOFF_MS      100

/* Tiempo entre revisiones cuando no hay conexión o no están habilitadas notificaciones */
#define IDLE_RECHECK_MS        500

/* Potencia TX deseada */
#define APP_TX_DBM   		   (-40)  /* Valores típicos: -40, 0, +3 */

/* ---------- UUIDs personalizados ---------- */

/* UUID del servicio GATT de rango */
#define BT_UUID_RANGE_SVC_VAL  BT_UUID_128_ENCODE(0x8e3c2a10, 0x5b41, 0x4a9a, 0x9d3f, 0x3a9f7f1b2c01)

/* UUID de la característica GATT que notificará el contador */
#define BT_UUID_RANGE_CHAR_VAL BT_UUID_128_ENCODE(0x8e3c2a11, 0x5b41, 0x4a9a, 0x9d3f, 0x3a9f7f1b2c01)

/* Objetos UUID listos para usar por la API */
#define BT_UUID_RANGE_SVC  BT_UUID_DECLARE_128(BT_UUID_RANGE_SVC_VAL)
#define BT_UUID_RANGE_CHAR BT_UUID_DECLARE_128(BT_UUID_RANGE_CHAR_VAL)

/* ---------- Work item para envío periódico ---------- */

/* Prototipo del trabajo que enviará notificaciones */
static void send_notify_work(struct k_work *work);

/* Work retrasable: permite programar el envío con retardo */
K_WORK_DELAYABLE_DEFINE(tx_work, send_notify_work);

/* Programa el siguiente envío dentro de ms milisegundos */
static void schedule_tx(uint32_t ms)
{
	k_work_schedule(&tx_work, K_MSEC(ms));
}

/* Cancela cualquier envío pendiente */
static void stop_tx(void)
{
	k_work_cancel_delayable(&tx_work);
}

/* ---------- Reinicio robusto de advertising tras desconexión ---------- */

static void adv_restart_work(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(adv_restart, adv_restart_work);

/* Work handler: intenta reiniciar el advertising. Si falla por falta de memoria,
   reprograma otro intento un poco más tarde. */
static void adv_restart_work(struct k_work *work)
{
	int err;

	/* Reinicia advertising para permitir reconexión del central */
	err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	printk("PERI: adv restarted (err=%d)\n", err);

	/* -ENOMEM (-12): el stack aún no liberó recursos; reintenta tras 500 ms */
	if (err == -ENOMEM) {
		k_work_schedule(&adv_restart, K_MSEC(500));
	}
}

/* ---------- Callback CCC (Client Characteristic Configuration) ---------- */

/* Se ejecuta cuando el central escribe en el descriptor CCC para
 * activar o desactivar las notificaciones de la característica
 */
static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	/* Solo consideramos habilitado si el valor escrito es "notify" */
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);

	printk("PERI: notify %s\n", notify_enabled ? "ON" : "OFF");

	if (notify_enabled) {
		/* Si se activan, se programa un primer envío inmediato */
		schedule_tx(0);
	} else {
		/* Si se desactivan, se paran los envíos */
		stop_tx();
	}
}

/* ---------- Definición del servicio GATT ---------- */

/* Servicio GATT definido de forma estática con:
 * - servicio primario
 * - característica con propiedad NOTIFY
 * - descriptor CCC para que el central active/desactive las notificaciones
 */
BT_GATT_SERVICE_DEFINE(range_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_RANGE_SVC),  /* Servicio primario con UUID propio */

    BT_GATT_CHARACTERISTIC(BT_UUID_RANGE_CHAR,   /* UUID de la característica */
                           BT_GATT_CHRC_NOTIFY,  /* Solo permite notificaciones */
                           BT_GATT_PERM_NONE,    /* No se permite leer ni escribir directamente */
                           NULL, NULL, NULL),    /* No hay callbacks de lectura ni escritura */

    BT_GATT_CCC(ccc_cfg_changed,                 /* Callback al cambiar el CCC */
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE) /* El central puede leer/escribir CCC */
);

/* ---------- Función de trabajo principal ---------- */

/* Trabajo periódico que envía notificaciones al central */
static void send_notify_work(struct k_work *work)
{
	/* Solo se puede notificar si:
	 * 1. existe conexión activa
	 * 2. el central ha habilitado el CCC
	 */
	if (!current_conn || !notify_enabled) {
		/* Si todavía no se cumplen las condiciones,
		 * se vuelve a revisar más adelante
		 */
		schedule_tx(IDLE_RECHECK_MS);
		return;
	}

	/* Convierte el contador de secuencia a little-endian antes de enviarlo */
	uint32_t le = sys_cpu_to_le32(seq++);

	/* range_svc.attrs[2] corresponde al valor de la característica:
	 * attrs[0] -> servicio primario
	 * attrs[1] -> declaración de característica
	 * attrs[2] -> valor de la característica
	 * attrs[3] -> descriptor CCC
	 */
	int err = bt_gatt_notify(current_conn, &range_svc.attrs[2], &le, sizeof(le));

	/* Si no hay memoria/buffers para transmitir, se hace backoff corto */
	if (err == -ENOMEM) {
		schedule_tx(ENOMEM_BACKOFF_MS);
		return;
	}

	/* Otro error cualquiera: se informa y se reintenta un poco después */
	if (err) {
		printk("PERI: notify err %d\n", err);
		schedule_tx(200);
		return;
	}

	/* Si fue bien, se programa el siguiente envío normal */
	schedule_tx(NOTIFY_PERIOD_MS);
}

/* ---------- Función para actualizar potencia ---------- */

/* Escribe la potencia TX deseada usando un comando HCI vendor-specific */
static int set_tx_power(uint8_t handle_type, uint16_t handle, int8_t desired_dbm, int8_t *selected_dbm)
{
	struct net_buf *buf, *rsp = NULL;
	int err;

	/* Crea el buffer del comando HCI */
	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
				sizeof(struct bt_hci_cp_vs_write_tx_power_level));
	if (!buf) {
		return -ENOMEM;
	}

	/* Añade los parámetros del comando */
	struct bt_hci_cp_vs_write_tx_power_level *cp = net_buf_add(buf, sizeof(*cp));
	cp->handle_type = handle_type;              /* Tipo de handle: advertising o conexión */
	cp->handle = sys_cpu_to_le16(handle);       /* Handle en little-endian */
	cp->tx_power_level = desired_dbm;           /* Potencia solicitada */

	/* Envía el comando y espera respuesta */
	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
	if (err) {
		return err;
	}

	/* Interpreta la respuesta */
	struct bt_hci_rp_vs_write_tx_power_level *rp = (void *)rsp->data;
	if (rp->status) {
		err = -EIO;
	} else if (selected_dbm) {
		/* Devuelve la potencia realmente seleccionada por el controlador */
		*selected_dbm = rp->selected_tx_power;
	}

	net_buf_unref(rsp);
	return err;
}

/* ---------- Función para comprobar potencia ---------- */

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

/* ---------- Callbacks de conexión ---------- */

/* Se ejecuta cuando un central se conecta al periférico */
static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("PERI: connect failed (err %u)\n", err);
		return;
	}
	
	/* Guarda una referencia a la conexión para poder usarla más tarde */
	current_conn = bt_conn_ref(conn);

	uint16_t handle;
	int8_t sel = 0;

	/* Obtiene el handle HCI de la conexión */
	if (bt_hci_get_conn_handle(conn, &handle) == 0) {
		/* Solicita la potencia TX para esta conexión */
		int e = set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, handle, APP_TX_DBM, &sel);
		printk("PERI: TXPWR CONN desired=%d selected=%d (err=%d)\n", APP_TX_DBM, sel, e);

		/* Si fue bien, hace lectura de comprobación */
		if (e == 0) {
			int8_t cur = 127;
			int r = get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, handle, &cur);
			printk("PERI: TXPWR CONN readback=%d (err=%d)\n", cur, r);
		}
	}

	printk("PERI: connected\n");

	/* Importante:
	 * aquí todavía NO se empieza a transmitir.
	 * Primero el central debe suscribirse a la característica escribiendo en el CCC.
	 */
}

/* Se ejecuta cuando se pierde la conexión */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("PERI: disconnected (reason 0x%02x)\n", reason);

	/* Detiene cualquier transmisión pendiente */
	stop_tx();

	/* Libera la referencia a la conexión */
	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	/* Se resetea el estado de notificaciones */
	notify_enabled = false;

	/* Reiniciar advertising con un pequeño delay (evita -ENOMEM justo al desconectar) */
	k_work_schedule(&adv_restart, K_MSEC(300));
}

/* Se ejecuta cuando cambia el PHY de la conexión */
static void le_phy_updated(struct bt_conn *conn, struct bt_conn_le_phy_info *param)
{
	printk("PERI: PHY updated tx=%u rx=%u\n", param->tx_phy, param->rx_phy);
}

/* Registro de callbacks de conexión */
static struct bt_conn_cb conn_cb = {
	.connected = connected,
	.disconnected = disconnected,
	.le_phy_updated = le_phy_updated,
};

/* ---------- Datos de advertising ---------- */

/* Datos de fabricante personalizados para que el central nos identifique.
 * Formato:
 * - 0xFF 0xFF -> identificador ficticio
 * - 'L' 'R'   -> firma usada por el proyecto
 * - 0x01 0x00 -> bytes extra
 */
static uint8_t mfg[6] = { 0xFF, 0xFF, 'L', 'R', 0x01, 0x00 };

/* Payload de advertising:
 * - flags BLE generales
 * - manufacturer data con nuestra firma
 */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg, sizeof(mfg)),
};

/* ---------- Función principal ---------- */

int main(void)
{
	int err;

	printk("PERI: init\n");

	/* Inicializa el stack Bluetooth */
	err = bt_enable(NULL);
	if (err) {
		printk("PERI: bt_enable failed (err %d)\n", err);
		return 0;
	}

	/* Registra callbacks de conexión */
	bt_conn_cb_register(&conn_cb);

	/* Configura advertising extendido:
	 * - conectable
	 * - extended advertising
	 * - intervalo 100 ms
	 */
	struct bt_le_adv_param param = BT_LE_ADV_PARAM_INIT(
		BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_EXT_ADV,
		0x00A0, 0x00A0, NULL
	);

	/* Crea el advertising set */
	err = bt_le_ext_adv_create(&param, NULL, &adv);
	if (err) {
		printk("PERI: adv_create failed (err %d)\n", err);
		return 0;
	}

	/* Carga los datos de advertising */
	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("PERI: set_data failed (err %d)\n", err);
		return 0;
	}

	/* Inicia el advertising */
	err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("PERI: adv_start failed (err %d)\n", err);
		return 0;
	}

	/* ---------- Ajuste de potencia del advertising ---------- */

	int8_t sel = 0;

	/* En extended advertising, el handle suele ser el índice del advertising set */
	uint16_t adv_handle = 0;

#if defined(CONFIG_BT_EXT_ADV)
	adv_handle = bt_le_ext_adv_get_index(adv);
#endif

	err = set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, adv_handle, APP_TX_DBM, &sel);
	printk("PERI: TXPWR ADV desired=%d selected=%d (err=%d, adv_handle=%u)\n",
		APP_TX_DBM, sel, err, adv_handle);

	if (err == 0) {
		int8_t cur = 127;
		int r = get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, adv_handle, &cur);
		printk("PERI: TXPWR ADV readback=%d (err=%d)\n", cur, r);
	}

	printk("PERI: advertising\n");

	/* No se inicia tx_work aquí.
	 * Las notificaciones solo empiezan cuando:
	 * 1. el central se conecta
	 * 2. el central habilita el CCC
	 */
	while (1) {
		k_sleep(K_SECONDS(1));
	}
}