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
 * @file tpool.h
 * @brief contiene funzioni per la creazione, eliminazione e gestione del
      threadpool
 */

#ifndef TPOOL_H_
#define TPOOL_H_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <user.h>
#include <task.h>

#define CLEAN_UP(pool)                                    \
								if(pool) {                                \
									tpool_free(pool);                       \
								}                                         \
								return NULL;                              \

typedef struct _t_pool t_pool;

/**
 * @function tpool_create
 * @brief crea threadpool
 *
 * @param nthreads numero thread da creare
 * @param max_con numero massimo connessioni
 * @return puntatore alla struttura t_pool creata
 */
t_pool *tpool_create(int nthreads, int max_con);

/**
 * @function tpool_add
 * @brief aggiunge task
 *
 * @param pool puntatore alla struttura t_pool in cui aggiungere task
 * @param (*fun)(job *) funzione da chiamare
 * @param arg argomento per la funzione da chiamare
 * @return 0 se il task e' stato aggiunto correttamente, -1 altrimenti
 */
int tpool_add(t_pool *pool, int (*fun)(job *), void *arg);

/**
 * @function tpool_delete
 * @brief elimina threadpool
 *
 * @param pool puntatore alla struttura t_pool da eliminare
 * @return 0 se il threadpool e' stato eliminato, -1 altrimenti
 */
int tpool_delete(t_pool *pool);


#endif /* TPOOL_H_ */
