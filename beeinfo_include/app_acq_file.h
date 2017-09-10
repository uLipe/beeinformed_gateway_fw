/**
 *          THE BeeInformed Team
 *  @file app_acq_file.h
 *  @brief beeinformed acquisition file definitions 
 */

#ifndef __APP_ACQ_FILE_H
#define __APP_ACQ_FILE_H

/* acquisition file fields */

/* binary version */
typedef struct {
	int32_t temperature;
	uint32_t pressure;
	uint32_t luminosity;
	uint32_t humidity;
}acqui_st_t;

/**
 *  @fn acq_file_append_val()
 *  @brief append a new line to acquisition file 
 *  @param
 *  @return
 */
int acq_file_append_val(acqui_st_t *aq, FILE *f, uint32_t timestamp);


/**
 *  @fn acq_file_get_data()
 *  @brief get a specific data field from acquisition file
 *  @param
 *  @return
 */
int acq_file_get_data(void *data, size_t size, uint32_t timestamp);
 
#endif