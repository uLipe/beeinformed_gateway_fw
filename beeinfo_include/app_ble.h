/**
 *          THE BeeInformed Team
 *  @file app_ble.h
 *  @brief beeinformed edge sensor connection application 
 */

 #ifndef __APP_BLE_H
 #define __APP_BLE_H

/** maximum payload in bytes */
#define PACKET_MAX_PAYLOAD 		16

/** timeout to wait for ble communcation */
#define BLE_COMM_TIMEOUT        10


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
	k_sequence_packet,
}pack_type_t;

/** command list */
typedef enum {
	k_get_sensors = 1,
	k_get_audio,
	k_get_status,
	k_reboot,
}edge_cmds_t;


/** packet structure */
typedef struct {
	uint8_t type;
	uint8_t id;
	uint8_t pack_amount;
	uint8_t payload_size;
	uint8_t pack_data[PACKET_MAX_PAYLOAD];
}ble_data_t;


typedef struct device_timer_spec_s {
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec trigger;
}device_timer_spec_t;


/* device context structure */
typedef struct  ble_device_handle_s{
    pthread_t ble_device_thread;
    pthread_attr_t ble_dev_att;
    gatt_connection_t *conn_handle;
    gattlib_primary_service_t* services;
    gattlib_characteristic_t* characteristics;
    acqui_st_t data_env;
	mqd_t mq;
    struct mq_attr attr;
    struct timespec mq_rcv_tout;
    device_timer_spec_t timer;
	int timestamp;
    bool should_run;
    int services_count; 
    int characteristics_count;
    char device_name[MAX_NAME_SIZE];
    char bd_addr[MAX_NAME_SIZE];
    char uuid_str[2*MAX_NAME_SIZE];
    bool new_device;
    k_list_t link;
} ble_device_handle_t;



/**
 *  @fn beeinformed_app_ble_start()
 *  @brief starts the beeinformed devices connection manager 
 *  @param
 *  @return
 */
void beeinformed_app_ble_start(char *path);

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