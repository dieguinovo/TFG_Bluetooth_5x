/*
 *  nRF5340 DK · CENTRAL BLE · Cliente Nordic UART Service (NUS)
 *  Escanea el nombre "Dongle", se conecta y envía el mensaje
 *  "Hola Dongle".  Muestra por consola lo recibido.
 */

 #include <zephyr/kernel.h>
 #include <zephyr/bluetooth/bluetooth.h>
 #include <bluetooth/scan.h>
 #include <bluetooth/gatt_dm.h>
 #include <bluetooth/services/nus.h>
 #include <bluetooth/services/nus_client.h>
 #include <zephyr/logging/log.h>
 
 LOG_MODULE_REGISTER(central_uart, LOG_LEVEL_INF);
 
// ---------- Cliente Nordic UART Service (NUS) ----------
 
 static struct bt_nus_client nus;
 
// ---------- Callback: recepción de datos ----------
 static void nus_rx_cb(struct bt_nus_client *inst, const uint8_t *data, uint16_t len)
 {
         LOG_INF("Dongle: %.*s", len, data); // Mensaje recibido desde el dongle
 }
 
// ---------- Inicialización del cliente NUS ----------
 static const struct bt_nus_client_cb nus_cbs = {
         .received = nus_rx_cb, // Asigna funcion de recepción
 };
 
 static const struct bt_nus_client_init_param nus_init = {
         .cb = nus_cbs, // Parametros de inicialización del cliente
 };
 
// ---------- Descubrimiento GATT ----------
 static void discovery_completed(struct bt_gatt_dm *dm, void *ctx)
 {
         int err = bt_nus_handles_assign(dm, &nus);     // Asigna handles NUS tras disovery
         bt_gatt_dm_data_release(dm);   // Libera datos del discovery
 
         if (err) {
                 LOG_ERR("Asignar handles (%d)", err);
                 return;
         }
 
         err = bt_nus_subscribe_receive(&nus);  // Activa notificaciones RX
         if (err) {
                 LOG_ERR("Suscripción RX (%d)", err);
                 return;
         }
         // Envío de mensaje al Dongle
         const char msg[] = "Hola Dongle";
         err = bt_nus_client_send(&nus, msg, strlen(msg));
         LOG_INF("TX \"%s\" (%d)", msg, err);
 }
 
 // ---------- Callbacks de error y no encontrado ----------
 static void discovery_not_found(struct bt_conn *c, void *ctx)
 {
         LOG_ERR("Servicio NUS no encontrado");
 }
 
 static void discovery_error(struct bt_conn *c, int err, void *ctx)
 {
         LOG_ERR("Error discovery (%d)", err);
 }
 
 static const struct bt_gatt_dm_cb dm_cb = {
         .completed         = discovery_completed,
         .service_not_found = discovery_not_found,
         .error_found       = discovery_error,
 };
 
 // ---------- Gestión de conexiones BLE ----------
 static struct bt_conn *default_conn;
 
 static void connected(struct bt_conn *c, uint8_t err)
 {
         if (err) {
                 LOG_ERR("Conexión fallida (%u)", err);
                 return;
         }
 
         default_conn = bt_conn_ref(c);
         bt_gatt_dm_start(c, BT_UUID_NUS_SERVICE, &dm_cb, NULL);
 }
 
 static void disconnected(struct bt_conn *c, uint8_t reason)
 {
         LOG_INF("Desconectado (0x%02x)", reason);
 
         if (default_conn) {
                 bt_conn_unref(default_conn);
                 default_conn = NULL;
         }

         bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);       // Reinicia escaneo tras desconexión
 }
 
 BT_CONN_CB_DEFINE(conn_cb) = {
         .connected    = connected,
         .disconnected = disconnected,
 };
 
// ---------- Escaneo BLE con filtro por nombre ----------
 static void scan_match(struct bt_scan_device_info *info, struct bt_scan_filter_match *m, bool conn)
 {
         LOG_INF("Dispositivo filtrado encontrado → conectando…");
 }
 
 BT_SCAN_CB_INIT(scan_cb, scan_match, NULL, NULL, NULL);
 
 // ---------- Inicialización del escaneo ----------
 static void scan_init(void)
 {
         struct bt_scan_init_param p = {
                 .connect_if_match = 1, // Conexión automática al detectar coincidencia
         };
 
         bt_scan_init(&p);
         bt_scan_cb_register(&scan_cb);
 
         // Filtro por nombre: conecta solo si el nombre completo es "Dongle"
         bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, "Dongle");
         bt_scan_filter_enable(BT_SCAN_NAME_FILTER, true);
 }

 // ---------- MAIN ----------
 int main(void)
 {
         if (bt_enable(NULL)) {
                 LOG_ERR("bt_enable() falló");  // Error al inicializar BT stack
                 return 0;
         }
 
         bt_nus_client_init(&nus, &nus_init);   // Inicializa cliente NUS
 
         scan_init();   // Configura escaneo con filtros
         bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);       // Comienza escaneo activo
 
         LOG_INF("Central listo, escaneando…");
         return 0;
 }
 
