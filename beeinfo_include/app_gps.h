/**
 *          THE BeeInformed Team
 *  @file app_gps.h
 *  @brief beeinformed gps data manager app
 */

 #ifndef __APP_GPS_H
 #define __APP_GPS_H

/** app gps acquisition data type*/
typedef struct {
    uint32_t lati;
    uint32_t longi;
    uint32_t pdop;
    uint32_t hdop;
    uint32_t utc_timestamp;
}gps_data_t;


/**
 *  @fn beeinformed_app_gps_start()
 *  @brief starts the beeinformed gps manager 
 *  @param
 *  @return
 */
void beeinformed_app_gps_start(void);

/**
 *  @fn beeinformed_app_gps_finish()
 *  @brief terminates the beeinformed gps manager
 *  @param
 *  @return
 */
void beeinformed_app_gps_finish(void);

/**
 *  @fn beeinformed_app_gps_get_data()
 *  @brief gets asynchronously the current gps data
 *  @param
 *  @return
 */
int beeinformed_app_gps_get_data(gps_data_t *g);

 #endif