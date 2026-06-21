/*
 *  nRF52840 Dongle · PERIFÉRICO BLE · Servicio Nordic UART
 *  Anuncia UUID NUS, acepta la conexión y responde "Welcome DK"
 *  al primer mensaje.  Enciende el LED mientras hay conexión.
 */

 #include <zephyr/kernel.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/gpio.h>
 #include <zephyr/bluetooth/bluetooth.h>
 #include <zephyr/bluetooth/hci.h>
 #include <bluetooth/services/nus.h>
 #include <zephyr/logging/log.h>
 
 LOG_MODULE_REGISTER(dongle_main, LOG_LEVEL_INF);
 
 // ---------- LED: configuración ----------
 #define LED_DONGLE_NODE DT_ALIAS(led0)
 static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_DONGLE_NODE, gpios);
 
 // ---------- Variables globales ---------- 
 static struct bt_conn *current_conn;
 static bool            nus_ready;
 
 // ---------- Advertising BLE con UUID NUS ----------
 static const struct bt_data ad[] = {
         BT_DATA_BYTES(BT_DATA_FLAGS,
                       BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR), // Flags generales
         BT_DATA_BYTES(BT_DATA_UUID128_ALL,
                       0x6E, 0x40, 0x01, 0x00,
                       0xB5, 0xA3, 0xF3, 0x93,
                       0xE0, 0xA9, 0x00, 0xE5, 
                       0x0E, 0xC9, 0xA2, 0x9E) // UUID de Nordic UART Service (NUS)
 };
 
 // ---------- Callback de recepción de datos BLE ----------
 static void nus_rx_cb(struct bt_conn *conn, const uint8_t *data, uint16_t len)
 {
         LOG_INF("RX: %.*s", len, data);
        
         // Solo responde al primer mensaje recibido
         if (!nus_ready) {
                 nus_ready = true;
                 const char *msg = "Welcome Central!";
                 int err = bt_nus_send(conn, msg, strlen(msg));
                 LOG_INF("Welcome %s (err %d)", err ? "FAIL" : "OK", err);
         }
 }
 
 static struct bt_nus_cb nus_cbs = {
         .received = nus_rx_cb,
 };
 
 // ---------- Callbacks de conexión / desconexión ----------
 static void connected(struct bt_conn *conn, uint8_t err)
 {
         if (err) {
                 LOG_ERR("Failed to connect (%u)", err);
                 return;
         }
 
         current_conn = bt_conn_ref(conn);      // Guardar referencia a conexión
         gpio_pin_set_dt(&led, 1);      // Enciende LED al conectar
         LOG_INF("Central connected; waiting RX…");
 }
 
 static void disconnected(struct bt_conn *conn, uint8_t reason)
 {
         LOG_INF("Disconnected (0x%02X)", reason);
         gpio_pin_set_dt(&led, 0);      // Apaga LED al desconectar
 
         if (current_conn) {
                 bt_conn_unref(current_conn);
                 current_conn = NULL;
         }
         nus_ready = false;
 }
 
 static struct bt_conn_cb conn_cbs = {
         .connected    = connected,
         .disconnected = disconnected,
 };
 
 // ---------- MAIN ----------
 void main(void)
 {
         int err;
 
         LOG_INF("Dongle boot");
 
         // LED: inicialización
         if (!device_is_ready(led.port)) {
                 LOG_ERR("LED not ready");
                 return;
         }
         gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
 
         // Bluetooth Stack
         err = bt_enable(NULL);
         if (err) {
                 LOG_ERR("Bluetooth init failed (%d)", err);
                 return;
         }
         bt_conn_cb_register(&conn_cbs);
         
         // Inicializa NUS
         err = bt_nus_init(&nus_cbs);
         if (err) {
                 LOG_ERR("NUS init failed (%d)", err);
                 return;
         }
 
         // Advertising
         err = bt_le_adv_start(BT_LE_ADV_CONN_NAME,
                               ad, ARRAY_SIZE(ad), NULL, 0);
         LOG_INF("Advertising %s (%d)",
                 err ? "FAIL" : "started", err);
 }
 