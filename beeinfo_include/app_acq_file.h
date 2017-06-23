/**
 *          THE BeeInformed Team
 *  @file app_acq_file.h
 *  @brief beeinformed acquisition file definitions 
 */

#ifndef __APP_ACQ_FILE_H
#define __APP_ACQ_FILE_H

/* acquisition file fields */

/** app data  acq type tags */
typedef enum {
    k_tag_acq_time =1
    k_tag_temperaure,
    k_tag_humidity,
    k_tag_pressure,
    k_tag_luminosity,
}acq_data_tag_t;


/* ASCII version */
typedef struct {
    char acq_time[MAX_NAME_SIZE];
    char temperaure[MAX_NAME_SIZE];
    char humidity[MAX_NAME_SIZE];
    char pressure[MAX_NAME_SIZE];
    char luminosity[MAX_NAME_SIZE];
}acqui_file_payload_t;

/* binary version */
typedef struct {
    uin32_t     acq_time;
    uint32_t    temperaure;
    uint32_t    humidity;
    uint32_t    pressure;
    uint32_t    luminosity;
}acqui_st_t;

/* define the acquisition file Text Header */
#define ACQUISITION_FILE_HEADER_STR {       \
    "BEEINFORMED ACQUISITION FILE: \n\r"     \
    "TIMESTAMP[s],Temperature[C],Relative Humi[percent],Pressure[Pa],Luminosity[Lux]\n\r"   \
}


/**
 *  @fn acq_file_append_val()
 *  @brief append a new line to acquisition file 
 *  @param
 *  @return
 */
int acq_file_append_val(acqui_st_t *aq, FILE *f);

/**
 *  @fn acq_file_get_data()
 *  @brief get a specific data field from acquisition file
 *  @param
 *  @return
 */
int acq_file_get_data(void *data, size_t size, uint32_t timestamp ,acq_data_tag_t tag);
 
#endif