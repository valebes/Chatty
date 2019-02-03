/*
 * chatterbox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
 /**
 * @author Valerio Besozzi 543685
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
/**
 * @file task.h
 * @brief file contenente funzioni per la gestione ed esecuzione dei task
 */

#ifndef TASK_H_
#define TASK_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <user.h>



typedef struct _task_manager task_manager;
typedef struct _job job;

/**
 * @struct _task_manager
 * @brief struttura per la gestione dei task
 *
 * @var mtx_poll mutex per epoll
 * @var mtx_tm mutex per task_manager
 * @var epollfd fd relativo all'istanza epoll in uso
 * @var jobs hasmap contenente jobs da completare
 * @var ud puntatore alla struttura _userdata ud
 */
struct _task_manager {
        pthread_mutex_t *mtx_poll;
        pthread_mutex_t *mtx_tm;
        int epollfd;
        job *jobs;
        userdata *ud;
};

/**
 * @struct _job
 * @brief struttura contenente informazioni lavoro da svolgere
 *
 * @var fd fd relativo al client oggetto del job
 * @var msg messaggio inviato dal client oggetto del job
 * @var puntatore alla struttura _task_manager
 * @var hh richiesto da uthash
 */
struct _job {
  unsigned long fd;
  message_t *msg;
  task_manager *tsk;
  UT_hash_handle hh;
};


/**
 * @function new_manager
 * @brief crea la struttura per la gestione dei task
 *
 * @param epollfd fd relativo all'istanza epoll in uso
 * @param ud puntatore alla struttura _userdata in uso
 * @return puntatore alla struttura task_manager creata
 */
task_manager *new_manager(int epollfd, userdata *ud);

/**
 * @function del_task
 * @brief elimina la struttura per la gestione dei task
 *
 * @param tsk puntatore alla struttura task_manager da eliminare
 */
void del_task(task_manager *tsk);

/**
 * @function new_job
 * @brief crea un nuovo job
 *
 * @param fd fd relativo al client oggetto del job
 * @param tsk puntatore alla struttura task_manager in uso
 * @return puntatore alla struttura _job creata
 */
job *new_job(int fd, task_manager *tsk);

/**
 * @function del_job
 * @brief elimina job
 *
 * @param jobd job da eliminare
 */
void del_job(job *jobd);

/**
 * @function read_task
 * @brief legge richiesta client
 *
 * @param jobr struttura _job contenente informazioni richiesta del client
 * @return 0 se la richiesta da parte del client e' stata letta e registrata,
   -1 altrimenti
 */
int read_task(job *jobr);

/**
 * @function write_task
 * @brief esegue richiesta client
 *
 * @param jobr struttura _job contenente informazioni richiesta del client
 * @return 0 se la richiesta da parte del client e' stata eseguita correttamente,
   -1 altrimenti
 */
int write_task(job *jobw);

/**
 * @function exec_task
 * @brief esegue operazioni per soddisfare richiesta del client
 *
 * @param tsk puntatore alla struttura _task_manager in uso
 * @param s_msg messaggio contenente richiesta client
 * @param fd fd del client che ha inviato la richiesta
 * @param ud puntatore alla struttura _userdata in cui operare
 * @return 0 se la richiesta da parte del client e' stata eseguita correttamente,
   -1 altrimenti
 */
int exec_task(task_manager *tsk, message_t *s_msg, int fd, userdata *ud);

#endif /* TASK_H_ */
