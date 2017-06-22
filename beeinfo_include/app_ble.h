/**
 *          THE BeeInformed Team
 *  @file app_ble.h
 *  @brief beeinformed edge sensor connection application 
 */

 #ifndef __APP_BLE_H
 #define __APP_BLE_H

/** app ble data type tags */
typedef enum {
    k_ble_gps_tag = 1,
    k_ble_user_data_tag,
    k_ble_node_requisition_tag
}app_ble_data_tag_t;


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