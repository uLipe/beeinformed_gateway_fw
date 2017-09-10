/**
 *          THE BeeInformed Team
 *  @file main_app.c
 *  @brief beeinformed main application file
 */

#include "beeinformed_gateway.h"


#define  MAIN_LOOP_SLEEP_PERIOD     (useconds_t)(1000 * 1000 * 60)

/** static variables */
static FILE *cfg_fp = NULL;
char cfg_path[] = "beeinformed/beeinformed.cfg";

/**
 *  @fn app_exit()
 *  @brief handler called when a SIGINT signal is received to correctly terminate the app
 *  @param
 *  @return
 */
static void app_exit(int arg)
{
    printf("--------------------%s: BeeInformed application was interrupted, exiting! --------------------------- \n\r", __func__);
    beeinformed_app_ble_finish();
    beeinformed_app_gps_finish();
    printf("-----------------------------%s: BeeInformed is safe to exit! --------------------------------------- \n\r", __func__);
    exit(0);
}


/**
 *  @fn main()
 *  @brief BeeInformed Main application entry point 
 *  @param
 *  @return
 */
int main(int argc, char **argv)
{
    /* the first task is to create the directory which will store the acquisition files */
    int err = mkdir("beeinformed",0644);
    if(err < 0 ) {
        printf("-----------------Restoring the BeeInformedApplication!------------------------------ \n\r");
    }else {
        printf("-----------------Creating the BeeHives monitoring environment!-----------------------\n\r");        
    }

    /* creates the configuration file */
    cfg_fp = fopen(cfg_path, "ab");
    fclose(cfg_fp);

    /* registers exit signal */
    signal(SIGINT, app_exit);

    /* with config file, passes the control to ble manager */
    printf("----------------------------Starting the beeinformed subtasks!-----------------------\n\r");
    beeinformed_app_ble_start(cfg_path);
    beeinformed_app_gps_start();

    for(;;) {
        usleep(MAIN_LOOP_SLEEP_PERIOD);
    }
}



