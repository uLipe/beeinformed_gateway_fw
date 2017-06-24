/**
 *          THE BeeInformed Team
 *  @file app_ble.c
 *  @brief beeinformed edge sensor connection application 
 */

#include "beeinformed_gateway.h"

/** default adapter name */
#define BEEINFO_BLE_DEF_ADAPTER         "hci0"
/** scan timeout value */
#define BEEINFO_BLE_DEF_TIMEOUT         60


/** define the sleep period in seconds */
#define BEEINFO_BLE_SCAN_SLEEP_TIME     (1000 * 1000 * 10)
#define BEEINFO_BLE_ACQ_PERIOD          (1000 * 1000 * 10)

/** characteristics handle */
#define BLE_TX_HANDLE                   0x0010
#define BLE_RX_HANDLE                   0x0012
#define BLE_NOTI_HANDLE                 0x0013

/** ALPWISE BLE data max payload (bytes) */
#define BLE_DATA_SVC_MAX_SIZE           20


/** connected device manager structure */
struct ble_conn_handle {
    pthread_t ble_device_thread;
    pthread_attr_t ble_dev_att;
    gatt_connection_t *conn_handle;
    gattlib_primary_service_t* services;
    gattlib_characteristic_t* characteristics;
    bin_sema_t rx_sema;
    acqui_st_t data_env;
    uint8_t ble_rx_buf[2*sizeof(ble_data_t)];
    uint8_t rxpos;
    int timestamp;
    bool should_run;
    int services_count; 
    int characteristics_count;
    char device_name[MAX_NAME_SIZE];
    char bd_addr[MAX_NAME_SIZE];
    char uuid_str[2*MAX_NAME_SIZE];
    bool new_device;
    LIST_ENTRY(ble_conn_handle) entries;
};


/** static variables */
static pthread_t ble_conn_thread;
static pthread_attr_t ble_conn_att;

static pthread_mutex_t ble_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tx_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cfg_mutex = PTHREAD_MUTEX_INITIALIZER;

LIST_HEAD(listhead,ble_conn_handle) ble_connections;
static bool ble_conn_should_run = true;
static void* hci_adapter = NULL;
static FILE *cfg;



/** static funcions */


/**
 *  @fn ble_add_device_to_list()
 *  @brief handles device discovering 
 *  @param
 *  @return
 */
static bool ble_add_device_to_list(FILE *fp, struct ble_conn_handle *h)
{
    bool ret = true;
    uint8_t buf[MAX_NAME_SIZE]={0};
    bool found = false;

    pthread_mutex_lock(&cfg_mutex);
    int saved = ftell(fp);

    /* this should never happen */
    assert(h != NULL);
    assert(fp != NULL);

    /* gets the file current EOF  */
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    while(ftell(fp) < size) {
        /* reads one entry of the config file */
        int b_read = fread ( buf, 1, strlen(h->bd_addr), fp );
        if(b_read != strlen(h->bd_addr)) {
            printf("%s: Warn: files seems to be corrupted! \n\r", __func__);
            break;
        }

        /* compare if the entry already exist */
        if(!strcmp(h->bd_addr, buf)) {
            printf("%s:----------------DEVICE FOUND IT ACQUISITION WILL BE RESTORED ------------\n\r", __func__);
            found = false;
            break;
        } 
    }

    /* if no such device, add it as a new one */
    if(!found) {
       printf("%s:----------------NEW BEEHIVE SENSOR ADDING IT ON KNOWNS LIST ------------\n\r", __func__); 
       fwrite ( h->bd_addr, 1, strlen(h->bd_addr), fp );
       ret = true;
    }

    fseek(fp, 0, saved);
    pthread_mutex_unlock(&cfg_mutex);
    return(ret);
}


/**
 *  @fn ble_cmd_handler()
 *  @brief handles device receiving data 
 *  @param
 *  @return
 */
static void ble_cmd_handler(ble_data_t *b, struct ble_conn_handle *h)
{
	edge_cmds_t cmd = (edge_cmds_t)b->id;

	switch(cmd) {
	case k_get_temp:
        memcpy(&h->data_env.temperature, b->pack_data, b->payload_size);    
		break;
	case k_get_humi:
        memcpy(&h->data_env.humidity, b->pack_data, b->payload_size);    

		break;
	case k_get_press:
        memcpy(&h->data_env.pressure, b->pack_data, b->payload_size);    
		break;
	case k_get_audio:
		break;
	case k_get_lumi:
        memcpy(&h->data_env.luminosity, b->pack_data, b->payload_size);    
        break;
	default:
		printf("%s: Unknown command, packet will not processed! \n\r", __func__);
        break;
	}
}

/**
 *  @fn ble_rx_handler()
 *  @brief handles the incoming data from BLE device 
 *  @param
 *  @return
 */
static void ble_rx_handler(const uuid_t* uuid, const uint8_t* data, size_t data_length, void* user_data) 
{
    /* obtains the device that got the notification */
    struct ble_conn_handle *h = (struct ble_conn_handle *)user_data;
    assert(h != NULL);

    /* fill the packet */
    memcpy(&h->ble_rx_buf[h->rxpos], data, data_length);
    printf("%s: ble raw data arrived len: %d! \n\r", __func__, data_length);

    h->rxpos += data_length;
    if(h->rxpos >= sizeof(ble_data_t)) {
        if(((ble_data_t *)&h->ble_rx_buf)->type != k_data_packet) {
            printf("%s: Device: %d will NOT receive this ble data! \n\r", __func__, h);
            h->rxpos = 0;
        } else {
            ble_data_t rx_packet;
            h->rxpos = 0;
            printf("%s: complete ble packet arrived! \n\r", __func__);
            printf("%s: Device: %d will receive this ble data! \n\r", __func__, h);
        
            memcpy(&rx_packet, &h->ble_rx_buf, sizeof(ble_data_t));
            ble_cmd_handler(&rx_packet, h);
        }
    }
}

/**
 *  @fn ble_receive_packet()
 *  @brief waits for a incoming overBLE packet  
 *  @param
 *  @return
 */
static inline int  ble_receive_packet(ble_data_t *b, struct ble_conn_handle *h)
{
    int ret = 0;
    assert(b != NULL);
    assert(h != NULL);

    bin_sem_wait(&h->rx_sema);
    memcpy(b, &h->ble_rx_buf, sizeof(ble_data_t));

    printf("%s: rx_packet.type: 0x%X  \n\r", __func__, b->type);
    printf("%s: rx_packet.id: 0x%X  \n\r", __func__, b->id);
    printf("%s: rx_packet.payload_size: %d \n\r", __func__, b->payload_size);

    return(ret);
}


/**
 *  @fn ble_device_handle_audio()
 *  @brief request Bee environment audio from edge device
 *  @param
 *  @return
 */
static inline void ble_device_handle_audio(struct ble_conn_handle *h, FILE *fp)
{


}


/**
 *  @fn ble_device_handle_acquisition()
 *  @brief handles device acquisition  
 *  @param
 *  @return
 */
static inline void ble_device_handle_acquisition(struct ble_conn_handle *h)
{

    ble_data_t tx_packet={0};
    ble_data_t rx_packet={0};
    int ret;

    /* this should never happen */
    assert(h != NULL);


    /* once the acquisition period was reached 
     * we need to perform acquisitions from host 
     */
     /* gets the temperature from edge */
     tx_packet.type = k_command_packet;
     tx_packet.id = k_get_temp;

    /* dispatch packet */    
    ret = gattlib_write_char_by_handle(h->conn_handle, BLE_TX_HANDLE, &tx_packet , sizeof(ble_data_t));
    if(ret) {
        printf("%s: packet failed to send! \n\r", __func__);
        ret = -1;
    }    
    printf("%s:-------------- REQUESTING BEEHIVE TEMPERATURE! ----------------\n\r", __func__);

     tx_packet.id = k_get_humi;
    /* dispatch packet */
    ret = gattlib_write_char_by_handle(h->conn_handle, BLE_TX_HANDLE, &tx_packet , sizeof(ble_data_t));
    if(ret) {
        printf("%s: packet failed to send! \n\r", __func__);
        ret = -1;
    }      
    printf("%s:-------------- REQUESTING BEEHIVE HUMIDITY! ----------------\n\r", __func__);

        /* gets the pressure from the beehive environment */
        tx_packet.id = k_get_press;
        /* dispatch packet */
    ret = gattlib_write_char_by_handle(h->conn_handle, BLE_TX_HANDLE, &tx_packet , sizeof(ble_data_t));
    if(ret) {
        printf("%s: packet failed to send! \n\r", __func__);
        ret = -1;
    }
    printf("%s:-------------- REQUESTING BEEHIVE PRESSURE! ----------------\n\r", __func__);

    /* gets the luminosity from the beehive environment */
    tx_packet.id = k_get_lumi;
    /* dispatch packet */
    ret = gattlib_write_char_by_handle(h->conn_handle, BLE_TX_HANDLE, &tx_packet , sizeof(ble_data_t));
    if(ret) {
        printf("%s: packet failed to send! \n\r", __func__);
        ret = -1;
    }
    printf("%s:-------------- REQUESTING BEEHIVE LUMINOSITY! ----------------\n\r", __func__);
    
}

/**
 *  @fn ble_discover_service_and_enable_listening()
 *  @brief discover device characteristics and enable notification
 *  @param
 *  @return
 */
static void ble_discover_service_and_enable_listening(struct ble_conn_handle *h)
{
    int ret;

    /* this should never happen */
    assert(h != NULL);
    

    /* discover device characteristic and services */
    ret = gattlib_discover_primary(h->conn_handle, &h->services, &h->services_count);
	if (ret != 0) {
		fprintf(stderr, "Fail to discover primary services.\n");
		goto cleanup;
	}

	for (int i = 0; i < h->services_count; i++) {
		gattlib_uuid_to_string(&h->services[i].uuid, h->uuid_str, sizeof(h->uuid_str));

		printf("%s: service[%d] start_handle:%02x end_handle:%02x uuid:%s\n",__func__, i,
				h->services[i].attr_handle_start, h->services[i].attr_handle_end,
				h->uuid_str);
	}

	ret = gattlib_discover_char(h->conn_handle, &h->characteristics, &h->characteristics_count);
	if (ret != 0) {
		fprintf(stderr, "Fail to discover characteristics.\n");
		goto cleanup;
	}
	for (int i = 0; i < h->characteristics_count; i++) {
		gattlib_uuid_to_string(&h->characteristics[i].uuid, h->uuid_str, sizeof(h->uuid_str));

		printf("%s: characteristic[%d] properties:%02x value_handle:%04x uuid:%s\n", __func__, i,
				h->characteristics[i].properties, h->characteristics[i].value_handle,
				h->uuid_str);
	}


    /* enable listening by setting nofitication and read characteristic
     * bitmask
     */
    uint16_t char_prop = GATTLIB_CHARACTERISTIC_WRITE_WITHOUT_RESP;

	ret = gattlib_write_char_by_handle(h->conn_handle, BLE_TX_HANDLE+1, &char_prop, sizeof(char_prop));
    if(ret) {
		fprintf(stderr, "failed set tx characteristic properties.\n");        
    }

    char_prop = 0x0001;
	ret = gattlib_write_char_by_handle(h->conn_handle, BLE_NOTI_HANDLE, &char_prop, sizeof(char_prop));
    if(ret) {
		fprintf(stderr, "failed set noti characteristic properties.\n");        
    }
    gattlib_register_notification(h->conn_handle, ble_rx_handler, h);


cleanup:
    return;

}

/**
 *  @fn ble_device_manager_thread()
 *  @brief handles device discovering 
 *  @param
 *  @return
 */
static void *ble_device_manager_thread(void *args)
{
    struct ble_conn_handle *handle = args;
 
    size_t stacksize = 0;
    pthread_attr_getstacksize(&handle->ble_dev_att, &stacksize);
    stacksize *= 10;
    pthread_attr_setstacksize(&handle->ble_dev_att, &stacksize);

    FILE *fp_acq;
    FILE *fp_audio;
    char root_path[MAX_NAME_SIZE]={0};
    char aud_path[MAX_NAME_SIZE]={0};
    char acq_path[MAX_NAME_SIZE]={0};

    
    printf("%s:-------------- NEW DEVICE PROCESS STARTED! ----------------\n\r", __func__);
    strcat(root_path, "beeinformed/");
    strcat(root_path, handle->bd_addr);
    strcat(aud_path, root_path);
    strcat(aud_path,"/audio.dat");
    
    strcat(acq_path, root_path);
    strcat(acq_path,"/beedata.csv");

    printf("%s:-------------- SETTING BEE DEVICE ENVIRONMENT ----------------\n\r", __func__);
    printf("%s: Audio File: %s \n\r", __func__, aud_path);
    printf("%s: Hive environment File: %s \n\r", __func__, acq_path);
    printf("%s:---------------------------------------------------------------\n\r", __func__);
    
    if(handle->new_device) {
        mkdir(root_path,0777);
    }
    /* obtains the acquisition file of the device */

    fp_acq =fopen(acq_path, "ar");
    fp_audio =fopen(aud_path, "ar");
    assert(fp_audio != NULL);
    assert(fp_acq != NULL);

    /* obtains device connection handle */
    handle->conn_handle = gattlib_connect(NULL, handle->bd_addr, BDADDR_LE_PUBLIC, BT_SEC_LOW, 0, 200);
	if (handle->conn_handle == NULL) {
		handle->conn_handle = gattlib_connect(NULL, handle->bd_addr, BDADDR_LE_RANDOM, BT_SEC_LOW, 0, 200);
		if (handle->conn_handle == NULL) {
			fprintf(stderr, "Fail to connect to the bluetooth device.\n");
			goto cleanup;
		} else {
			printf("%s: Succeeded to connect to the bluetooth device with random address.\n\r", __func__);
		}
	} else {
			printf("%s: Succeeded to connect to the bluetooth device.\n\r", __func__);
	}

    /* enable the notifications and gets the service database */
    printf("%s:---------- DISCOVERING BEEINFORMED EDGE BLE DATABASE -----------\n\r", __func__);
    ble_discover_service_and_enable_listening(handle);
    printf("%s:---------- DISCOVERED BEEINFORMED EDGE BLE DATABASE -----------\n\r", __func__);
    usleep(BEEINFO_BLE_ACQ_PERIOD/10);


    /* connection estabilished, now just manages the device
     * until connection closes
     */
     while(handle->should_run) {
        ble_device_handle_acquisition(handle);
        ble_device_handle_audio(handle, fp_audio);
        acq_file_append_val(&handle->data_env, fp_audio);
        handle->timestamp += BEEINFO_BLE_ACQ_PERIOD/(BEEINFO_BLE_ACQ_PERIOD /10);

        printf("+---------------------------------------------------------------+\n\r");
        printf("|                                                               |\n\r");
        printf("|   BeeInformed - Hive - ID FC:D6:BD:10:0B:B2: Status:          |\n\r");
        printf("    TIMESTAMP: %d [s]                                            \n\r",handle->timestamp);
        printf("+------------------------------+--------------------------------+\n\r");
        printf("|         ****                 |                                |\n\r");
        printf("|        *  ***                |           |   |      -|        |\n\r");
        printf("|        *  ***                |           |   |       |        |\n\r");
        printf("|        *    *                |           |   |     --|        |\n\r");
        printf("|        *  ***                |           |   |       |        |\n\r");
        printf("|        *  ***                |           |   |    ---|        |\n\r");
        printf("|       *      *               |         \\|/  |        |        |\n\r");
        printf("|      *    *****              |             \\|/  ---- |        |\n\r");
        printf("|      * ********              |                       |        |\n\r");
        printf("|       ********               |           ____________|        |\n\r");
        printf("|        ******                |                                |\n\r");
        printf("|                              |                                |\n\r");
        printf("|                              |                                |\n\r");
        printf("   %d[mDEG]                         %d[Pa]                       \n\r", handle->data_env.temperature, handle->data_env.pressure);
        printf("|                              |                                |\n\r");
        printf("+---------------------------------------------------------------+\n\r");
        printf("|                              |                                |\n\r");
        printf("|           **                 |            ****                |\n\r");
        printf("|          ****                |          *********             |\n\r");
        printf("|          *****               |         *****    **            |\n\r");
        printf("|          ******              |         *****    **            |\n\r");
        printf("|         ********             |          ***** **              |\n\r");
        printf("|        ***********           |           *******              |\n\r");
        printf("|       **** *********         |           ******               |\n\r");
        printf("|      ***     ********        |           *   **               |\n\r");
        printf("|      ***    ********         |           *    *               |\n\r");
        printf("|       *** **********         |           ******               |\n\r");
        printf("|        ***********           |            ****                |\n\r");
        printf("|          ******              |                                |\n\r");
        printf("         %d[%%]                       %d[mLUX]                  \n\r", handle->data_env.humidity, handle->data_env.luminosity);
        printf("+------------------------------+--------------------------------+\n\r");

        printf("\n\r");
        printf("\n\r");
        usleep(BEEINFO_BLE_ACQ_PERIOD);
     }

cleanup:
    printf("%s:-------------- EDGE DEVICE THREAD TERMINATING! ----------------\n\r", __func__);
    fclose(fp_audio);
    fclose(fp_acq);
    return(NULL);
}


/**
 *  @fn ble_discovered_device()
 *  @brief handles device discovering 
 *  @param
 *  @return
 */
static void ble_discovered_device(const char* addr, const char* name) {
    int ret;
    
    if(!strcmp(name, "beeinformed_edge")) {
        printf("%s:------------------ DEVICE DISCOVERED! ------------------\n\r", __func__);
        printf("%s: NAME: %s \n\r", __func__, name);
        printf("%s: BD_ADDRESS: %s \n\r", __func__, addr);
        printf("%s:--------------------------------------------------------\n\r", __func__);
        

        struct ble_conn_handle *handle = malloc(sizeof(struct ble_conn_handle));
        memset(handle, 0, sizeof(struct ble_conn_handle));
        assert(handle != NULL);

        /* gets the device information */
        strcpy(&handle->bd_addr[0], addr);
        strcpy(&handle->device_name[0], addr);
        handle->new_device = ble_add_device_to_list(cfg, handle);
        handle->should_run = true;

        pthread_mutex_init(&handle->rx_sema.mutex, NULL);
        pthread_cond_init(&handle->rx_sema.cvar, NULL);

        /* creates and starts the device thread */
        ret = pthread_create(&handle->ble_device_thread, &handle->ble_dev_att,ble_device_manager_thread, handle);
        if(ret) {
            fprintf(stderr, "ERROR: Failed to start ble device manager.\n");
        }
        
    }
}

/**
 *  @fn ble_connection_manager_thread()
 *  @brief connection manager background task
 *  @param
 *  @return
 */
static void *ble_connection_manager_thread(void *args)
{
    int ret;
    (void)args;
    printf("%s: starting beeinformed connection manager! \n\r", __func__);
    size_t stacksize = 0;
    pthread_attr_getstacksize(&ble_conn_att, &stacksize);
    stacksize *= 10;
    pthread_attr_setstacksize(&ble_conn_att, &stacksize);

    


    printf("%s:-----------------STARTING NEW BLE SCAN CICLE! -----------------------\n\r", __func__);
    ret = gattlib_adapter_scan_enable(hci_adapter, ble_discovered_device, 
                    BEEINFO_BLE_DEF_TIMEOUT);
    if(ret) {
        fprintf(stderr, "ERROR: Failed to scan.\n");
    }
    printf("%s:------------------SCANNING COMPLETE SLEEPING!-------------------------\n\r", __func__);
    gattlib_adapter_scan_disable(hci_adapter);


    while(ble_conn_should_run) {
        /* now we start the discovery and create connections thread 
         * to each new device 
         */    

        usleep(BEEINFO_BLE_SCAN_SLEEP_TIME);
    }

    /* signal to disconnect received, so request disconnection to all devices */
   	while (ble_connections.lh_first != NULL) {
		struct ble_conn_handle* connection = ble_connections.lh_first;
		
        /* request a disconnection  waits thread terminates
         * free memory of all active connections before to 
         * kill the application 
         */
        connection->should_run = false;
        pthread_join(connection->ble_device_thread, NULL);
        gattlib_disconnect(connection->conn_handle);
		LIST_REMOVE(ble_connections.lh_first, entries);
		free(connection->services);
		free(connection->characteristics);        
        free(connection);
	}

    gattlib_adapter_close(hci_adapter);
    return(NULL);
}


/** public functions */
void beeinformed_app_ble_start(FILE *fp)
{
    int ret = 0;
    assert(fp != NULL);

    /* perform some basic initialization and open the bt adapter */
	LIST_INIT(&ble_connections);
    ret = gattlib_adapter_open(BEEINFO_BLE_DEF_ADAPTER, &hci_adapter);
	if (ret) {
		fprintf(stderr, "ERROR: Failed to open adapter.\n");
        goto cleanup;
	}

    /* creates and starts the connman thread */
    ret = pthread_create(&ble_conn_thread, &ble_conn_att,ble_connection_manager_thread, NULL);
    if(ret) {
		fprintf(stderr, "ERROR: Failed to start ble conn manager.\n");
        goto cleanup;        
    }

    cfg = fp;

cleanup:
    return;
}

void beeinformed_app_ble_finish(void)
{
    /* request conn man to terminate */
    ble_conn_should_run = false;
    pthread_join(ble_conn_thread, NULL);
}

int  beeinformed_app_ble_send_data(void *data, size_t size, app_ble_data_tag_t tag)
{
    int ret;
    /** TODO */
    return(ret);
}
