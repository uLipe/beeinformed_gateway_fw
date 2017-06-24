/**
 *          THE BeeInformed Team
 *  @file app_ble.h
 *  @brief beeinformed edge sensor connection application 
 */

 #ifndef __APP_BLE_H
 #define __APP_BLE_H

/** maximum payload in bytes */
#define PACKET_MAX_PAYLOAD 	15



/** app ble data type tags */
typedef enum {
    k_ble_gps_tag = 1,
    k_ble_user_data_tag,
    k_ble_node_requisition_tag
}app_ble_data_tag_t;

/** over BLE protocol data and relevant commands */

/** packet types */
typedef enum {
	k_command_packet = 0,
	k_data_packet,
}pack_type_t;

/** command list */
typedef enum {
	k_get_temp = 1,
	k_get_humi,
	k_get_press,
	k_get_audio,
	k_get_lumi,
	k_get_status,
	k_reboot,
	k_set_alarm_period,
}edge_cmds_t;


/** packet structure */
typedef struct {
	uint8_t type;
	uint8_t id;
	uint8_t pack_amount;
	uint8_t pack_nbr;
	uint8_t payload_size;
	uint8_t pack_data[PACKET_MAX_PAYLOAD];
}ble_data_t;

/**
 *  @fn beeinformed_app_ble_start()
 *  @brief starts the beeinformed devices connection manager 
 *  @param
 *  @return
 */
void beeinformed_app_ble_start(FILE *fp);

/**
 *  @fn beeinformed_app_ble_finish()
 *  @brief terminates the beeinformed device connection manager
 *  @param
 *  @return
 */
void beeinformed_app_ble_finish(void);

/**
 *  @fn beeinformed_app_ble_send_data()
 *  @brief custom personalized data channel to ble connection manager
 *  @param
 *  @return
 */
int  beeinformed_app_ble_send_data(void *data, size_t size, app_ble_data_tag_t tag);

 #endif