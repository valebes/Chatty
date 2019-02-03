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
 * @file tpool.c
 * @brief contiene implementazioni funzioni per la creazione, eliminazione
 *        e gestione del threadpool
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <tpool.h>


typedef struct _task {
        int (*fun)(job *);
        void *arg;
} task;

struct _t_pool {
        pthread_mutex_t lock;
        pthread_cond_t cond;
        pthread_t *threads;
        task *coda; // Mediante array circolare
        int nthreads;
        int dim_coda;
        int head;
        int tail;
        int count;
        int shutdown;
        int avviati;
};

static void *tpool_worker(void *cpool);

int tpool_free(t_pool *pool);


t_pool *tpool_create(int nthreads, int dim_coda) {

        t_pool *pool;

        if(nthreads <= 0 || dim_coda <= 0) return NULL;

        if((pool = (t_pool *)malloc(sizeof(t_pool))) == NULL) {
                perror("Errore: ");
                CLEAN_UP(pool);
        }

        /* Inizializzo */
        pool->nthreads = 0;
        pool->dim_coda = dim_coda;
        pool->head = pool->tail = 0;
        pool->count = 0;
        pool->shutdown = pool->avviati = 0;

        if((pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * nthreads))
           == NULL) {
                perror("Errore: ");
                CLEAN_UP(pool);
        }
        if((pool->coda = (task *)malloc(sizeof(task) * dim_coda)) == NULL) {
                perror("Errore: ");
                CLEAN_UP(pool);
        }
        if(pthread_mutex_init(&(pool->lock), NULL) != 0) {
                CLEAN_UP(pool);
        }
        if(pthread_cond_init(&(pool->cond), NULL) != 0) {
                CLEAN_UP(pool);
        }

        int i;
        for(i = 0; i < nthreads; i++) {
                printf("Creo Thread[%d] \n", i);
                if(pthread_create(&(pool->threads[i]), NULL, tpool_worker,
                                  (void*)pool) != 0) {
                        printf("Errore nella creazione del Thread[%d] \n", i);
                        perror("Errore: ");
                        tpool_delete(pool);
                        return NULL;
                }
                pool->nthreads++;
                pool->avviati++;
        }

        return pool;

}

int tpool_add(t_pool *pool, int (*fun)(job *),void *arg) {

        if(pool == NULL || fun == NULL) return -1; // Controllo se esiste pool e se e' presente la funzione
        pthread_mutex_lock(&(pool->lock));

        /* Controllo che la coda non sia piena */

        if(pool->count == pool->dim_coda) {
                printf("Errore: la coda e' piena \n");
                return -1;
        }

        /* Inserisco task */
        pool->coda[pool->tail].fun = fun;
        pool->coda[pool->tail].arg = arg;
        pool->tail = (pool->tail + 1) % pool->dim_coda; // Scorro coda
        pool->count++;

        pthread_cond_signal(&(pool->cond));
        pthread_mutex_unlock(&(pool->lock));

        return 0;

}



int tpool_delete(t_pool *pool) {

        if(pool == NULL) return -1;
        printf("Elimino ThreadPool\n");
        pthread_mutex_lock(&(pool->lock));
        if(pool->shutdown == 1) return -1; // Controllo se sono gia' terminati

        pool->shutdown = 1;
        pthread_cond_broadcast(&(pool->cond));
        pthread_mutex_unlock(&(pool->lock));

        int i;
        for(i = 0; i < pool->nthreads; i++) {
                if(pthread_join(pool->threads[i], NULL) != 0) {
                        perror("Errore: ");
                        return -1;
                }
                printf("Elimino Thread[%d]\n", i);

        }
        tpool_free(pool);
        return 0;
}


int tpool_free(t_pool *pool) {

        free(pool->threads);
        free(pool->coda);
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->cond));
        free(pool);

        return 0;
}


static void *tpool_worker(void *cpool) {

        t_pool *pool = (t_pool *)cpool;
        task task;

        while(!pool->shutdown) {
                pthread_mutex_lock(&(pool->lock));
                while((pool->count == 0) && (!pool->shutdown)) {
                        pthread_cond_wait(&(pool->cond), &(pool->lock));
                }
                if(pool->shutdown) break;
                task.fun = pool->coda[pool->head].fun;
                task.arg = pool->coda[pool->head].arg;

                pool->head = (pool->head + 1) % pool->dim_coda; // Scorro
                pool->count--;
                pthread_mutex_unlock(&(pool->lock));
                (*(task.fun))(task.arg);
        }
        pool->avviati--;

        pthread_mutex_unlock(&(pool->lock));
        pthread_exit(NULL);
        return NULL;
}
