/**
 *          THE BeeInformed Team
 *  @file app_bin_sema.c
 *  @brief custom implementataion of binary semaphore for sincrhonization
 */
#include "beeinformed_gateway.h"

/** public functions */

void bin_sem_post(bin_sema_t *s)
{
    pthread_mutex_lock(&s->mutex);
    if (s->v == 1)
        /* error */
    s->v += 1;
    pthread_cond_signal(&s->cvar);
    pthread_mutex_unlock(&s->mutex);
}



void bin_sem_wait(bin_sema_t *s)
{
    pthread_mutex_lock(&s->mutex);
    while (!s->v)
        pthread_cond_wait(&s->cvar, &s->mutex);
    s->v -= 1;
    pthread_mutex_unlock(&s->mutex);
}