/**
 *          THE BeeInformed Team
 *  @file app_ble.c
 *  @brief beeinformed edge sensor connection application 
 */

#include "beeinformed_gateway.h"

/** default adapter name */
#define BEEINFO_BLE_DEF_ADAPTER "hci0"
/** scan timeout value */
#define BEEINFO_BLE_DEF_TIMEOUT  10 

/** define the sleep period in seconds */
#define BEEINFO_BLE_SCAN_SLEEP_TIME (1000 * 1000 * 10)


/** conection manager structure */
struct ble_conn_handle {
    pthread_t ble_device_thread;
    gatt_connection_t *conn_handle;
    char device_name[MAX_NAME_SIZE];
    char bd_addr[MAX_NAME_SIZE];
    bool new_device;
    LIST_ENTRY(ble_conn_handle) entries;
};


/** static variables */
static pthread_t ble_conn_thread;
static pthread_mutex_t ble_mutex = PTHREAD_MUTEX_INITIALIZER;
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
    return(ret);
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
    strcat(acq_path,"/");
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
			puts("Succeeded to connect to the bluetooth device with random address.");
		}
	} else {
		puts("Succeeded to connect to the bluetooth device.");
	}

    /* connection estabilished, now just manages the device
     * until connection closes
     */
     while(handle->conn_handle != NULL) {
        usleep(BEEINFO_BLE_SCAN_SLEEP_TIME);
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
        pthread_mutex_lock(&ble_mutex);
        ret = gattlib_adapter_scan_enable(hci_adapter, ble_discovered_device, 
                        BEEINFO_BLE_DEF_TIMEOUT);
    
        if(ret) {
		    fprintf(stderr, "ERROR: Failed to scan.\n");
        }
        gattlib_adapter_scan_disable(hci_adapter);
        pthread_mutex_unlock(&ble_mutex);
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
