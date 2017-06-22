/**
 *          THE BeeInformed Team
 *  @file app_cli.h
 *  @brief beeinformed cli interface to IHM application
 */

 #ifndef __APP_CLI_H
 #define __APP_CLI_H

/**
 *  @fn beeinformed_app_cli_start()
 *  @brief starts the ihm bridge manager
 *  @param
 *  @return
 */
void beeinformed_app_cli_start(FILE *fp);

/**
 *  @fn beeinformed_app_cli_finish()
 *  @brief terminates the ihm bridge manager
 *  @param
 *  @return
 */
void beeinformed_app_cli_finish(void);


 #endif