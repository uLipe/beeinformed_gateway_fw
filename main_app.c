/**
 *          THE BeeInformed Team
 *  @file main_app.c
 *  @brief beeinformed main application file
 */

#include "beeinformed_gateway.h"


#define  MAIN_LOOP_SLEEP_PERIOD     (useconds_t)(1000 * 1000 * 100)

/** static variables */
static FILE *cfg_fp = NULL;

/**
 *  @fn app_exit()
 *  @brief handler called when a SIGINT signal is received to correctly terminate the app
 *  @param
 *  @return
 */
static void app_exit(int arg)
{
    printf("--------------------%s: BeeInformed application was interrupted, exiting! --------------------------- \n\r", __func__);
    beeinformed_ble_app_finish();
    beeinformed_gps_app_finish();
    beeinformed_cli_app_finis();
    fclose(cfg_fp);
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
    int err = mkdir("beeinformed",0777);
    if(err < 0 ) {
        printf("-----------------Restoring the BeeInformedApplication!------------------------------ \n\r");
    }else {
        printf("-----------------Creating the BeeHives monitoring environment!-----------------------\n\r");        
    }

    cfg_fp = fopen("beeinformed/beeinformed.cfg", "+awr");

    /* registers exit signal */
    signal(SIGINT, app_exit);


    /* with config file, passes the control to ble manager */
    printf("----------------------------Starting the beeinformed subtasks!-----------------------\n\r");
    beeinformed_ble_app_start(cfg_fp);
    beeinformed_gps_app_start();
    beeinformed_cli_app_start(cfg_fp);

    for(;;) {
        usleep();
    }
}



