/**
 *          THE BeeInformed Team
 *  @file app_bin_sema.h
 *  @brief custom implementataion of binary semaphore for sincrhonization
 */

#ifndef __APP_BIN_SEMA_H
#define __APP_BIN_SEMA_H

/** binary semaphore structure based on condvars */
typedef struct bin_sema_s {
    pthread_mutex_t mutex;
    pthread_cond_t cvar;
    int v;
}bin_sema_t;


/**
 *  @fn bin_sem_post()
 *  @brief signals a bin semaphore
 *  @param
 *  @return
 */
void bin_sem_post(bin_sema_t *s);

/**
 *  @fn bin_sem_wait()
 *  @brief waits for a bin semaphore
 *  @param
 *  @return
 */
void bin_sem_wait(bin_sema_t *s);

#endif