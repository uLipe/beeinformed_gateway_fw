/**
 *          THE BeeInformed Team
 *  @file app_ble.c
 *  @brief beeinformed edge sensor connection application 
 */

#include "beeinformed_gateway.h"

/** default adapter name */
#define BEEINFO_BLE_DEF_ADAPTER         "hci0"
/** scan timeout value */
#define BEEINFO_BLE_DEF_TIMEOUT         2

/** defines the messaging max slot size */
#define BLE_MESSAGE_SLOT_SIZE	sizeof(ble_data_t)


/** define the sleep period in seconds */
#define BEEINFO_BLE_SCAN_SLEEP_TIME     (1000 * 500)
#define BEEINFO_BLE_ACQ_PERIOD          (1000 * 1)

/** characteristics handle */
#define BLE_TX_HANDLE                   0x0010
#define BLE_RX_HANDLE                   0x0012
#define BLE_NOTI_HANDLE                 0x0013


/** connected device manager structure */


/** static variables */
static pthread_t ble_conn_thread;
static pthread_attr_t ble_conn_att;
static pthread_mutex_t cfg_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t scan_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gatt_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool ble_conn_should_run = true;
static void* hci_adapter = NULL;
char *cfg;

k_list_t ble_devices;

/** static funcions */

/**
 *  @fn ble_comm_timeout()
 *  @brief handles communication timeout * 
 *  @param
 *  @return
 */
static void ble_comm_timeout(union sigval s) {
    ble_device_handle_t *dev = (ble_device_handle_t *)s.sival_ptr;

    if(dev != NULL) {
        printf("%s : --------------- COMMUNICATION TIMEOUT ---------------\n\r\n\r", __func__);
        ble_data_t packet;
        packet.type = k_command_packet;
        packet.id   = 0xFF;
    

        mqd_t mq;
        struct mq_attr attr;
        attr.mq_flags = O_NONBLOCK;
    
        char mq_str[32] = {0};
        strcpy(mq_str, "/mq_");
        strcat(mq_str, dev->bd_addr);
    
        mq = mq_open(mq_str,O_WRONLY, 0644, &attr);
        mq_send(mq, (uint8_t *)&packet,  sizeof(packet) , 0);
        mq_close(mq);
        printf("%s : --------------- COMMUNICATION HANDLERED ---------------\n\r\n\r", __func__);
    }
}



/**
 *  @fn ble_add_device_to_list()
 *  @brief handles device discovering 
 *  @param
 *  @return
 */
static bool ble_add_device_to_list(char * path , ble_device_handle_t *h)
{
    bool ret = true;
    ble_device_handle_t dev;
    bool found = false;
    int bread= 0;
    FILE *fp = fopen(path, "rb");

    printf("%s:Config file: %s \n\r", __func__, path); 
    
    pthread_mutex_lock(&cfg_mutex);
    /* this should never happen */
    assert(h != NULL);
    assert(fp != NULL);

    for(;;){
        /* reads one entry of the config file */
        bread = fread( &dev, 1, sizeof(ble_device_handle_t), fp );

        if(!bread) {
            printf("%s: reached on end of file exiting  \n\r", __func__);                       
            break;
        } else {
            printf("%s: Current device found: %s  \n\r", __func__, dev.bd_addr);                        
            /* compare if the entry already exist */
            if(!strcmp(h->bd_addr, dev.bd_addr)) {
                printf("%s:----------------DEVICE FOUND IT ACQUISITION WILL BE RESTORED ------------\n\r", __func__);
                found = true;
                break;
            }             
        }

    }

    fclose(fp);
    
    /* if no such device, add it as a new one */
    if(!found) {
       printf("%s:----------------NEW BEEHIVE SENSOR ADDING IT ON KNOWNS LIST ------------\n\r", __func__); 
       fp = fopen(path, "ab");  
       fwrite (h, 1, sizeof(ble_device_handle_t), fp );
       fclose(fp);
       ret = true;
    } else {
        ret = false;
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
    ble_device_handle_t *dev = (ble_device_handle_t *)user_data;
    ble_data_t dump;
    assert(dev != NULL);

    memcpy(&dump, data, data_length);

    //printf("%s: device: %s \n\r", __func__, dev->bd_addr );
    //printf("%s: type: %d!! \n\r", __func__, dump.type);
    //printf("%s: id: %d!! \n\r", __func__, dump.id);
    //printf("%s: pack_amount: %d!! \n\r", __func__, dump.pack_amount);
    //printf("%s: payload_size: %d!! \n\r", __func__, dump.payload_size);

    /* data received, store on queue for furthre processing */
    mqd_t mq;
    struct mq_attr attr;
    attr.mq_flags = O_NONBLOCK;

    char mq_str[32] = {0};
    strcpy(mq_str, "/mq_");
    strcat(mq_str, dev->bd_addr);

    mq = mq_open(mq_str,O_WRONLY, 0644, &attr);
    if(mq_send(mq, (uint8_t *)&dump,  sizeof(dump) , 0) < 0) {
        printf("%s: queue seems to be full! \n\r", __func__);    
    }
    mq_close(mq);
}


/**
 *  @fn ble_device_handle_acquisition()
 *  @brief handles device acquisition  
 *  @param
 *  @return
 */
static inline void ble_device_handle_acquisition(ble_device_handle_t *h)
{
    /* this should never happen */
    assert(h != NULL);
    ble_data_t packet = {0};
    uint8_t mq_data[sizeof(ble_data_t) * 2];    
    ble_data_t *rx_packet = (ble_data_t *)&mq_data;
    
    int ret;

    packet.type = k_command_packet;
    packet.id   = k_get_sensors;

    /* send the command to the current sensor node */
    printf("%s: sending command to sensor node\n\r", __func__);            
    ret = gattlib_write_char_by_handle(h->conn_handle, BLE_TX_HANDLE, &packet, sizeof(packet));
    if(ret) {
        fprintf(stderr, "failed to send command to device .\n"); 
        h->should_run = false;        
        goto cleanup;       
    } else {

        h->timer.trigger.it_value.tv_sec = BLE_COMM_TIMEOUT;                
        timer_settime(h->timer.timerid, 0, &h->timer.trigger, NULL);        
        printf("%s: packet sent to device, waiting response\n\r", __func__);        
    }


    if(mq_receive(h->mq, mq_data, sizeof(mq_data), NULL) < sizeof(rx_packet)) {
        printf("%s: corrupt packet arrived, discarding!! \n\r", __func__);
        printf("%s: type: %d!! \n\r", __func__, rx_packet->type);
        printf("%s: id: %d!! \n\r", __func__, rx_packet->id);
        printf("%s: pack_amount: %d!! \n\r", __func__, rx_packet->pack_amount);
        
        /* some error occurred, so destroy the thread and waits a reconnection */
        h->should_run = false;
    } else {

        if(rx_packet->type == k_command_packet) {
            /* a command packet here, indicate fault with communication, exit */
            h->should_run = false;
            goto cleanup;
        }

        /* stops timer until packet processing */
        h->timer.trigger.it_value.tv_sec = 0;        
        timer_settime(h->timer.timerid, 0, &h->timer.trigger, NULL);        
        


        uint32_t packet_cnt = rx_packet->pack_amount - 1;
        uint8_t *ptr = (uint8_t *)&h->data_env;
        bool error = false;

        memcpy(ptr, &rx_packet->pack_data, rx_packet->payload_size);
        ptr += rx_packet->payload_size;

        while (packet_cnt) {

            /* rearm timer to avoid deadlock */
            h->timer.trigger.it_value.tv_sec = BLE_COMM_TIMEOUT;        
            timer_settime(h->timer.timerid, 0, &h->timer.trigger, NULL);        

            if(mq_receive(h->mq, mq_data, sizeof(mq_data), NULL) < sizeof(rx_packet)) {


               /* stops timer until packet processing */
               h->timer.trigger.it_value.tv_sec = 0;        
               timer_settime(h->timer.timerid, 0, &h->timer.trigger, NULL);        


                if(rx_packet->type == k_command_packet) {
                    /* a command packet here, indicate fault with communication, exit */
                    h->should_run = false;
                    goto cleanup;
                }
        
                printf("%s: corrupt packet arrived, discarding!! \n\r", __func__);
                h->should_run = false;
                goto cleanup;                
            } 

            /* stops timer until packet processing */
            h->timer.trigger.it_value.tv_sec = 0;        
            timer_settime(h->timer.timerid, 0, &h->timer.trigger, NULL);        


            if(!error) {
                memcpy(ptr, &rx_packet->pack_data, rx_packet->payload_size);
                ptr += rx_packet->payload_size;        
            }
            packet_cnt--;
            
        }
    }


    /* prints the data */
    printf("%s: data sent by sensor_id: %s are:  \n\r", __func__, h->bd_addr); 
    printf("Temperature: %u [mdeg] \n\r", h->data_env.temperature);  
    printf("Humidity: %u  [percent]\n\r", h->data_env.humidity);            
    printf("Pressure: %u  [Pa]\n\r",  h->data_env.pressure);
    printf("Luminance: %u [mLux] \n\r", h->data_env.luminosity);

    
cleanup:
    return;
}

/**
 *  @fn ble_discover_service_and_enable_listening()
 *  @brief discover device characteristics and enable notification
 *  @param
 *  @return
 */
static void ble_discover_service_and_enable_listening(ble_device_handle_t *h)
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
    uint16_t char_prop = 0x000C;

	ret = gattlib_write_char_by_handle(h->conn_handle, BLE_TX_HANDLE+1, &char_prop, sizeof(char_prop));
    if(ret) {
		fprintf(stderr, "failed set tx characteristic properties.\n");        
    }

    char_prop = 0x0003;
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
    ble_device_handle_t *handle = args;
 
    FILE *fp_acq;
    FILE *fp_audio;
    char root_path[MAX_NAME_SIZE]={0};
    char aud_path[MAX_NAME_SIZE]={0};
    char acq_path[MAX_NAME_SIZE]={0};
    
    printf("%s:-------------- NEW DEVICE PROCESS STARTED! ----------------\n\r", __func__);
    strcat(root_path, "beeinformed/");
    strcat(root_path, handle->bd_addr);
    strcat(aud_path, root_path);
    strcat(aud_path,"/beeaudio.dat");
    
    strcat(acq_path, root_path);
    strcat(acq_path,"/beedata.dat");

    printf("%s:-------------- SETTING BEE DEVICE ENVIRONMENT ----------------\n\r", __func__);
    printf("%s: Audio File: %s \n\r", __func__, aud_path);
    printf("%s: Hive environment File: %s \n\r", __func__, acq_path);
    printf("%s:---------------------------------------------------------------\n\r", __func__);
    
    if(handle->new_device) {
        mkdir(root_path,0644);
    }
    /* obtains the acquisition file of the device */

    fp_acq =fopen(acq_path, "ab");
    fp_audio =fopen(aud_path, "ab");
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

    /* creates the timeout channel */
    memset(&handle->timer.sev, 0, sizeof(struct sigevent));
    memset(&handle->timer.trigger, 0, sizeof(struct itimerspec));
    handle->timer.sev.sigev_notify = SIGEV_THREAD;
    handle->timer.sev.sigev_notify_function = &ble_comm_timeout;
    handle->timer.sev.sigev_value.sival_ptr = handle;
    handle->timer.trigger.it_value.tv_sec = BLE_COMM_TIMEOUT;
    if(timer_create(CLOCK_REALTIME, &handle->timer.sev, &handle->timer.timerid) < 0) {
        /* failed to create timer, exit */
        fprintf(stderr, "ERROR: Failed to create ble device timer.\n");        
        goto cleanup;
    }


    /* creates a messaging system to store messages */
    handle->attr.mq_flags = 0;
    handle->attr.mq_maxmsg = 128;
    handle->attr.mq_msgsize = BLE_MESSAGE_SLOT_SIZE;
    handle->attr.mq_curmsgs = 0;
    
    char mq_str[32] = {0};
    strcpy(mq_str, "/mq_");
    strcat(mq_str, handle->bd_addr);
    printf("%s: mqueue name: %s \n\r", __func__, mq_str);
    
    /* close the mqueue before to use it, this will flushes the queue */
    mq_unlink(mq_str);
    handle->mq = mq_open(mq_str, O_CREAT | O_RDWR, 0644, &handle->attr);

    if(handle->mq < 0) {
        fprintf(stderr, "ERROR: Failed to create ble device managerqueue.\n");
        goto cleanup;            
    }

    /* connection estabilished, now just manages the device
     * until connection closes
     */
     while(handle->should_run && (handle->conn_handle != NULL)) {
        ble_device_handle_acquisition(handle);
        usleep(BEEINFO_BLE_ACQ_PERIOD);
     }

cleanup:
    printf("%s:-------------- EDGE DEVICE THREAD TERMINATING! ----------------\n\r", __func__);
    fclose(fp_audio);
    fclose(fp_acq);
    timer_delete(handle->timer.timerid);
    mq_close(handle->mq);

    mq_unlink(mq_str); 
    gattlib_disconnect(handle->conn_handle);
    free(handle);
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

    printf("%s:------------------ DEVICE DISCOVERED! ------------------\n\r", __func__);
    printf("%s: NAME: %s \n\r", __func__, name);
    printf("%s: BD_ADDRESS: %s \n\r", __func__, addr);
    printf("%s:--------------------------------------------------------\n\r", __func__);

    
    if(!strcmp(name, "beeinformed_edge")) {
        

        ble_device_handle_t *handle = malloc(sizeof(ble_device_handle_t));
        memset(handle, 0, sizeof(ble_device_handle_t));
        assert(handle != NULL);

        /* gets the device information */
        strcpy(&handle->bd_addr[0], addr);
        strcpy(&handle->device_name[0], name);
        handle->new_device = ble_add_device_to_list(cfg, handle);
        handle->should_run = true;

        /* add device on connected devices list */
        sys_dlist_init(&handle->link);        
        sys_dlist_append(&ble_devices, &handle->link);

        /* creates and starts the device thread */
        ret = pthread_create(&handle->ble_device_thread, &handle->ble_dev_att,ble_device_manager_thread, handle);
        if(ret) {
            fprintf(stderr, "ERROR: Failed to start ble device manager.\n");
        }
        
    }
cleanup:    
    return;    
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
        printf("%s:-----------------SCANNING BLE DEVICES! -----------------------\n\r", __func__);        
        pthread_mutex_lock(&scan_mutex);
        /* perform some basic initialization and open the bt adapter */
        ret = gattlib_adapter_open(BEEINFO_BLE_DEF_ADAPTER, &hci_adapter);
        if (ret) {
            fprintf(stderr, "ERROR: Failed to open adapter.\n");
            pthread_mutex_unlock(&scan_mutex);              
            continue;
        }

        ret = gattlib_adapter_scan_enable(hci_adapter, ble_discovered_device,BEEINFO_BLE_DEF_TIMEOUT);
        if(ret) 
            fprintf(stderr, "ERROR: Failed to scan.\n");
        gattlib_adapter_scan_disable(hci_adapter);
        gattlib_adapter_close(hci_adapter);        
        pthread_mutex_unlock(&scan_mutex);  
        printf("%s:-----------------END OF SCANNING BLE DEVICES! -----------------------\n\r", __func__);               
        usleep(BEEINFO_BLE_SCAN_SLEEP_TIME);
    }


    /* if application was terminated, disconnects all the devices */
    ble_device_handle_t *dev;

    SYS_DLIST_FOR_EACH_CONTAINER(&ble_devices,dev , link) {
        /* disconnects and free the memory */
        dev->should_run = false;
        pthread_join(dev->ble_device_thread, NULL);        
    } 

    return(NULL);
}


/** public functions */
void beeinformed_app_ble_start(char *path)        

{
    int ret = 0;

    cfg = path;

    sys_dlist_init(&ble_devices);

    /* creates and starts the connman thread */
    ret = pthread_create(&ble_conn_thread, &ble_conn_att,ble_connection_manager_thread, NULL);
    if(ret) {
		fprintf(stderr, "ERROR: Failed to start ble conn manager.\n");
        goto cleanup;        
    }

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
