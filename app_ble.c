/**
 *          THE BeeInformed Team
 *  @file app_ble.c
 *  @brief beeinformed edge sensor connection application 
 */

#include "beeinformed_gateway.h"

/** default adapter name */
#define BEEINFO_BLE_DEF_ADAPTER         "hci0"
/** scan timeout value */
#define BEEINFO_BLE_DEF_TIMEOUT         10 


/** define the sleep period in seconds */
#define BEEINFO_BLE_SCAN_SLEEP_TIME     (1000 * 1000 * 10)
#define BEEINFO_BLE_ACQ_PERIOD          (1000 * 1000 * 60)

/** characteristics handle */
#define BLE_TX_HANDLE                   0x0010
#define BLE_RX_HANDLE                   0x0012
#define BLE_NOTI_HANDLE                 0x0013

/** ALPWISE BLE data max payload (bytes) */
#define BLE_DATA_SVC_MAX_SIZE           20


/** connected device manager structure */
struct ble_conn_handle {
    pthread_t ble_device_thread;
    gatt_connection_t *conn_handle;
    gattlib_primary_service_t* services;
    gattlib_characteristic_t* characteristics;
    bin_sema_t rx_sema;
    acqui_st_t data_env;
    uint8_t ble_rx_buf[2*sizeof(ble_data_t)];
    uint8_t rxpos;
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
static pthread_mutex_t ble_mutex = PTHREAD_MUTEX_INITIALIZER;
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
        fread ( buf, 1, strlen(h->bd_addr), fp );
        /* compare if the entry already exist */
        if(!strcmp(h->bd_addr, buf)) {
            printf("%s:----------------DEVICE FOUND IT ACQUISITION WILL BE RESTORED ------------\n\r", __func__);
            found = true;
            break;
        } 
    }

    /* if no such device, add it as a new one */
    if(!found) {
       printf("%s:----------------NEW BEEHIVE SENSOR ADDING IT ON KNOWNS LIST ------------\n\r", __func__); 
       fwrite ( h->bd_addr, 1, strlen(h->bd_addr), fp );
       ret = true;
    }


    pthread_mutex_unlock(&cfg_mutex);
    return(ret);
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
        h->rxpos = 0;
        /* complete packet arrived, signal the waiting device manager */
        bin_sem_post(&h->rx_sema);
        printf("%s: complete ble packet arrived! \n\r", __func__);
    }
}

/**
 *  @fn ble_rx_handler()
 *  @brief sends commands in overBLE format 
 *  @param
 *  @return
 */
static int  ble_send_packet(ble_data_t *b, struct ble_conn_handle *h)
{
    int ret;
    assert(b != NULL);
    assert(h != NULL);
    uint8_t len = sizeof(ble_data_t);
    uint8_t txpos = 0;

    pthread_mutex_lock(&ble_mutex);

    /* cuts the packet in 20 byte slices and send using the characteristic
     * write
     */
    while(len) {
        uint8_t size = (len < BLE_DATA_SVC_MAX_SIZE )? len: BLE_DATA_SVC_MAX_SIZE;
        ret = gattlib_write_char_by_handle(h->conn_handle, BLE_TX_HANDLE, b+txpos , size);
        if(ret) {
            ret = -1;
            goto cleanup;
        }

        txpos += size;
        len =  (len < BLE_DATA_SVC_MAX_SIZE ) ? (len - BLE_DATA_SVC_MAX_SIZE) : 0; 
    }
    ret = 0;
cleanup:
    pthread_mutex_unlock(&ble_mutex);
    return(ret);    
}

/**
 *  @fn ble_receive_packet()
 *  @brief waits for a incoming overBLE packet  
 *  @param
 *  @return
 */
static int  ble_receive_packet(ble_data_t *b, struct ble_conn_handle *h)
{
    int ret = 0;
    assert(b != NULL);
    assert(h != NULL);

    bin_sem_wait(&h->rx_sema);
    memcpy(b, &h->ble_rx_buf, sizeof(ble_data_t));

    return(ret);
}


/**
 *  @fn ble_device_handle_audio()
 *  @brief request Bee environment audio from edge device
 *  @param
 *  @return
 */
static void ble_device_handle_audio(struct ble_conn_handle *h, FILE *fp)
{

}


/**
 *  @fn ble_device_handle_acquisition()
 *  @brief handles device acquisition  
 *  @param
 *  @return
 */
static void ble_device_handle_acquisition(struct ble_conn_handle *h)
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
     tx_packet.pack_type = k_command_packet;
     tx_packet.id = k_get_temp;
     ret = ble_send_packet(&tx_packet, h);
     if(ret < 0) {
        fprintf(stderr, "Fail to get BeeHive temperature.\n");
     }else {
        printf("%s:-------------- REQUESTING BEEHIVE TEMPERATURE! ----------------\n\r", __func__);
     }

     ret = ble_receive_packet(&rx_packet, h);
     if(ret < 0) {
        fprintf(stderr, "Fail to get BeeHive temperature.\n");
     }else {
        printf("%s:-------------- GOT BEEHIVE TEMPERATURE! ----------------\n\r", __func__);
        h->data_env.temperaure = *((uint32_t *)&rx_packet.pack_data[0]);
        printf("%s:-------------- VALUE IS: %d [mDeg] ----------------\n\r", __func__, h->data_env.temperaure);
     }
    


     /* gets the humidity from the beehive environment */
     tx_packet.id = k_get_humi;
     ret = ble_send_packet(&tx_packet, h);
     if(ret < 0) {
        fprintf(stderr, "Fail to get BeeHive environment humidity.\n");
     }else {
        printf("%s:-------------- REQUESTING BEEHIVE HUMIDITY! ----------------\n\r", __func__);
     }

     ret = ble_receive_packet(&rx_packet, h);
     if(ret < 0) {
        fprintf(stderr, "Fail to get BeeHive environment humidity.\n");
     }else {
        printf("%s:-------------- GOT BEEHIVE HUMIDITY! ----------------\n\r", __func__);
        h->data_env.humidity = *((uint32_t *)&rx_packet.pack_data[0]);
        printf("%s:-------------- VALUE IS: %d [Percent] ----------------\n\r", __func__, h->data_env.temperaure);
     }


     /* gets the pressure from the beehive environment */
     tx_packet.id = k_get_press;
     ret = ble_send_packet(&tx_packet, h);
     if(ret < 0) {
        fprintf(stderr, "Fail to get BeeHive environment pressure.\n");
     }else {
        printf("%s:-------------- REQUESTING BEEHIVE PRESSURE! ----------------\n\r", __func__);
     }

     ret = ble_receive_packet(&rx_packet, h);
     if(ret < 0) {
        fprintf(stderr, "Fail to get BeeHive environment pressure.\n");
     }else {
        printf("%s:-------------- GOT BEEHIVE PRESSURE! ----------------\n\r", __func__);
        h->data_env.pressure = *((uint32_t *)&rx_packet.pack_data[0]);
        printf("%s:-------------- VALUE IS: %d [Pa] ----------------\n\r", __func__, h->data_env.pressure);
     }



     /* gets the luminosity from the beehive environment */
     tx_packet.id = k_get_lumi;
     ret = ble_send_packet(&tx_packet, h);
     if(ret < 0) {
        fprintf(stderr, "Fail to get BeeHive environment luminosity.\n");
     }else {
        printf("%s:-------------- REQUESTING BEEHIVE LUMINOSITY! ----------------\n\r", __func__);
     }

     ret = ble_receive_packet(&rx_packet, h);
     if(ret < 0) {
        fprintf(stderr, "Fail to get BeeHive environment luminosity.\n");
     }else {
        printf("%s:-------------- GOT BEEHIVE LUMINOSITY! ----------------\n\r", __func__);
        h->data_env.luminosity = *((uint32_t *)&rx_packet.pack_data[0]);
        printf("%s:-------------- VALUE IS: %d [Lux] ----------------\n\r", __func__, h->data_env.luminosity);
     }
}

/**
 *  @fn ble_discover_service_and_enable_listening()
 *  @brief discover device characteristics and enable notification
 *  @param
 *  @return
 */
static void ble_discover_service_and_enable_listening(struct ble_conn_handle *h);
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
		gattlib_uuid_to_string(&h->services[i].uuid, h->uuid_str, sizeof(uuid_str));

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
		gattlib_uuid_to_string(&h->characteristics[i].uuid, h->uuid_str, sizeof(uuid_str));

		printf("%s: characteristic[%d] properties:%02x value_handle:%04x uuid:%s\n", __func__, i,
				h->characteristics[i].properties, h->characteristics[i].value_handle,
				h->uuid_str);
	}


    /* enable listening by setting nofitication and read characteristic
     * bitmask
     */
    uint16_t enable_notification = 0x0001;
	gattlib_write_char_by_handle(h->ble_conn_handle, BLE_NOTI_HANDLE, &enable_notification, sizeof(enable_notification));
    gattlib_register_notification(h->ble_conn_handle, ble_rx_handler, h);

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
    handle->conn_handle = gattlib_connect(NULL, handle->bd_addr, BDADDR_LE_PUBLIC, BT_SEC_LOW, 0, 0);
	if (handle->conn_handle == NULL) {
		handle->conn_handle = gattlib_connect(NULL, handle->bd_addr, BDADDR_LE_RANDOM, BT_SEC_LOW, 0, 0);
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

    /* connection estabilished, now just manages the device
     * until connection closes
     */
     while(handle->conn_handle != NULL) {
        printf("%s:-------------- TIME TO ACQUIRE DATA ----------------\n\r", __func__);
        ble_device_handle_acquisition(handle);
        ble_device_handle_audio(handle, fp_audio);
        acq_file_append_val(&handle->data_env, fp_audio);
        printf("%s:-------------- END OF ACQUISITION PROCESS ----------------\n\r", __func__);
        usleep(BEEINFO_BLE_ACQ_PERIOD);
     }

cleanup:
    printf("%s:-------------- EDGE DEVICE THREAD TERMINATING! ----------------\n\r", __func__);
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

        /* creates and starts the device thread */
        ret = pthread_create(&handle->ble_device_thread, NULL,ble_device_manager_thread, handle);
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
    while(ble_conn_should_run) {
        /* now we start the discovery and create connections thread 
         * to each new device 
         */
        printf("%s:-----------------STARTING NEW BLE SCAN CICLE! -----------------------\n\r", __func__);
        ret = gattlib_adapter_scan_enable(hci_adapter, ble_discovered_device, 
                        BEEINFO_BLE_DEF_TIMEOUT);
    
        if(ret) {
		    fprintf(stderr, "ERROR: Failed to scan.\n");
        }
        gattlib_adapter_scan_disable(hci_adapter);
        printf("%s:------------------SCANNING COMPLETE SLEEPING!-------------------------\n\r", __func__);
        usleep(BEEINFO_BLE_SCAN_SLEEP_TIME);
    }

    /* signal to disconnect received, so request disconnection to all devices */
   	while (ble_connections.lh_first != NULL) {
		struct ble_conn_handle* connection = ble_connections.lh_first;
		
        /* request a disconnection  waits thread terminates
         * free memory of all active connections before to 
         * kill the application 
         */
        gattlib_disconnect(connection->conn_handle);
        pthread_join(connection->ble_device_thread, NULL);
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
    memset(&data_env, 0, sizeof(data_env));

    /* perform some basic initialization and open the bt adapter */
	LIST_INIT(&ble_connections);
    ret = gattlib_adapter_open(BEEINFO_BLE_DEF_ADAPTER, &hci_adapter);
	if (ret) {
		fprintf(stderr, "ERROR: Failed to open adapter.\n");
        goto cleanup;
	}

    /* creates and starts the connman thread */
    ret = pthread_create(&ble_conn_thread, NULL,ble_connection_manager_thread, NULL);
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
