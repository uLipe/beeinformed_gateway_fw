/**
 *          THE BeeInformed Team
 *  @file beeinformed.h
 *  @brief beeinformed main header file
 */

#ifndef __BEEINFORMED_GATEWAY_H
#define __BEEINFORMED_GATEWAY_H

/**
 *  @fn func()
 *  @brief
 *  @param
 *  @return
 */

/* include standard libc needed files here */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>

/* gattlib to use Bluetooth low energy */
#include "gattlib.h"

/* include subapps here */
#include "beeinfo_include/app_ble.h"
#include "beeinfo_include/app_gps.h"
#include "beeinfo_include/app_gps.h"


#endif