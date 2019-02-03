/*
 * membox Progetto del corso di LSO 2017/2018
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
 * @file chatty.c
 * @brief file principale del server chatterbox
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

/* inserire gli altri include che servono */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include "parser.h"
#include "stats.h"
#include "connections.h"
#include "task.h"
#include "tpool.h"
#include "message.h"
#include "user.h"
#include "stats.h"

#define MAX_EVENTS 60 // è una stima

void signals();

void stop_server();

void get_stats();

void *print_stats(void *arg);

int stop = 1;

char *statspath;

int alert = 0;

pthread_mutex_t *stats_mtx;

/* struttura che memorizza parametri del file di configurazione
 * e' definita in parser.h.
 *
 */
struct statistics chattyStats = { 0,0,0,0,0,0,0 };

conf cf = {NULL, 0, 0, 0, 0, 0, NULL, NULL};


static void usage(const char *progname) {
        fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
        fprintf(stderr, "  %s -f conffile\n", progname);
}

int main(int argc, char *argv[]) {

        // Controllo argomenti
        if(argc < 3 || strncmp(argv[1], "-f", 2) != 0) {
                usage(argv[0]);
                return 0;
        }

        printf(" ### #  #  ##  #### #### #  #\n");
        printf("#    #### ####  ##   ##   ##\n");
        printf(" ### #  # #  #  ##   ##   #\n");
        // Parsing file di configurazione
        cf.unix_path = malloc(MAXBUF * sizeof(char));
        cf.down_path = malloc(MAXBUF * sizeof(char));
        cf.stats = malloc(MAXBUF * sizeof(char));
        int ret = load_conf(argv[2], &cf);
        if(ret == -2) {
                printf("Errore nel file di configurazione\n");
                free(cf.unix_path);
                free(cf.down_path);
                free(cf.stats);
                return -1;
        }
        else if(ret == -1) {
                printf("Impossibile aprire il file di configurazione");
                free(cf.unix_path);
                free(cf.down_path);
                free(cf.stats);
                return -1;
        }
        else {
                printf("###############################################################\n");
                printf("Unix Path: %s\n", cf.unix_path);
                printf("Max Connessioni: %li\n", cf.max_con);
                printf("Threads: %li\n", cf.nthreads);
                printf("Max MSG: %li\n", cf.max_msg);
                printf("Max Size: %li\n", cf.max_size);
                printf("Max History: %li\n", cf.max_history);
                printf("Download Path: %s\n", cf.down_path);
                printf("Stats Path: %s\n", cf.stats);
                printf("###############################################################\n");

        }

        statspath = malloc(MAXBUF * sizeof(char));
        strcpy(statspath, cf.stats);
        signals();

        if(cf.nthreads == 0) {
                printf("Errore: il numero di threads non può essere uguale a 0\n");
                free(statspath);
                free(cf.unix_path);
                free(cf.down_path);
                free(cf.stats);
                exit(EXIT_FAILURE);
        }

        int fd_skt;
        int err = 0;
        if((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
                perror("Errore nella creazione del socket: ");
                exit(EXIT_FAILURE);
        }
        struct sockaddr_un sa;
        strncpy(sa.sun_path, cf.unix_path, strlen(cf.unix_path) + 1); //SOCKNAME: path socket
        sa.sun_family = AF_UNIX;
        unlink(cf.unix_path); // Elimino precedente socket, se esiste
        if((err = bind(fd_skt, (struct sockaddr *)&sa, sizeof(sa))) == -1) {
                perror("Errore nel binding: ");
                exit(EXIT_FAILURE);

        }
        if((err = listen(fd_skt, cf.max_con)) == -1) {
                if(stop != 0) perror("Errore: ");
                exit(EXIT_FAILURE);
        }

        struct epoll_event ev, events[MAX_EVENTS];
        memset(&ev, 0, sizeof(struct epoll_event));
        memset(&events, 0, sizeof(struct epoll_event) * MAX_EVENTS);

        int conn_sock, nfds, epollfd;
        int n = 0;
        int num = 0;

        epollfd = epoll_create(cf.max_con);
        userdata *ud = new_udata(&cf, &chattyStats);
        stats_mtx = ud->stats_mtx;
        pthread_t *t_stats = (pthread_t *)malloc(sizeof(pthread_t)); // Mi serve per gestire SIGUSR1 in modo sicuro
        pthread_create(t_stats, NULL, print_stats, NULL); // il testo parla del numero di thread per gestire le connsessioni, questo non lo conto
        t_pool *tpool = tpool_create(cf.nthreads, cf.max_con);
        task_manager *tsk = new_manager(epollfd, ud);
        pthread_mutex_t *mtx_poll = tsk->mtx_poll;
        printf("Thread creati!\n");

        ev.events = EPOLLIN;
        ev.data.fd = fd_skt;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd_skt, &ev) == -1) {
                perror("epoll_ctl: listen_sock");
                exit(EXIT_FAILURE);
        }
        while(stop) {
                if((nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) >= 0) {
                        num = nfds;
                        for (n = 0; n < nfds; ++n) {
                                if (events[n].data.fd == fd_skt) {
                                        int full = 0;
                                        pthread_mutex_lock(stats_mtx);
                                        if( chattyStats.nonline >= cf.max_con ) full = 1;
                                        pthread_mutex_unlock(stats_mtx);
                                        if(full) continue; // You Shall Not Pass!!!
                                        conn_sock = accept(fd_skt, NULL, 0);
                                        if (conn_sock == -1) {
                                                perror("accept");
                                                exit(EXIT_FAILURE);
                                        }
                                        ev.events = EPOLLIN;
                                        ev.data.fd = conn_sock;
                                        pthread_mutex_lock(mtx_poll);
                                        epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev);
                                        pthread_mutex_unlock(mtx_poll);

                                } else if(events[n].events & EPOLLIN) {
                                        job *job = new_job(events[n].data.fd, tsk);
                                        pthread_mutex_lock(mtx_poll);
                                        epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, NULL);
                                        pthread_mutex_unlock(mtx_poll);
                                        tpool_add(tpool, read_task, job);

                                } else if(events[n].events & EPOLLOUT) {
                                        job *job = new_job(events[n].data.fd, tsk);
                                        pthread_mutex_lock(mtx_poll);
                                        epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, NULL);
                                        pthread_mutex_unlock(mtx_poll);
                                        tpool_add(tpool, write_task, job);

                                }
                        }
                }
        }

        printf("Chiusura fd\n");
        pthread_mutex_lock(mtx_poll);
        for (n = 0; n <= num; ++n) epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, NULL);
        close(epollfd);
        pthread_mutex_unlock(mtx_poll);

        close(fd_skt);
        printf("Eliminazione ThreadPool\n");
        tpool_delete(tpool);
        printf("Eliminazione TaskManager\n");
        del_task(tsk);
        printf("Eliminazione Stats\n");
        pthread_join(*t_stats, NULL);
        printf("Eliminazione UData\n");
        del_udata(ud);
        // PER IL PROBLEMA DEL TEST4: SAREBBE DA PASSARE LA TABELLA HASH DEI TASK QUANDO VIENE CHIUSO IL PROGRAMMA, IN MODO DA ELIMINARE I TASK PENDENTI

        // Libero Memoria
        free(statspath);
        free(cf.unix_path);
        free(cf.down_path);
        free(cf.stats);
        free(t_stats);
        printf("Chiusura chatty completata\n");
        return 0;
}

void signals() {
        struct sigaction sigpipe;
        memset(&sigpipe, 0, sizeof(sigpipe)); // Cosi valgrind non dovrebbe dare problemi
        sigpipe.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &sigpipe, NULL); // Gestisco SIGPIPE, errore dovuto alla scrittura su un socket gia' chiuso

        struct sigaction exith;
        memset(&exith, 0, sizeof(exith));
        exith.sa_handler = stop_server;
        sigaction(SIGQUIT, &exith, NULL); // Gestisco SIGQUIT
        sigaction(SIGTERM, &exith, NULL); // Gestisco SIGTERM
        sigaction(SIGINT, &exith, NULL); // Gestisco SIGINT

        struct sigaction statsh;
        memset(&statsh, 0, sizeof(statsh));
        statsh.sa_handler = get_stats;
        sigaction(SIGUSR1, &statsh, NULL); // Gestisco SIGQUIT
}

void stop_server() {
        stop = 0;
}

void get_stats() { // In modo che l'operazione di stampa di stats sia signal safe
        alert = 1;
}

void *print_stats(void *arg) {
        for(;;) {
                if(stop == 0) pthread_exit(NULL);
                if(alert == 1) {
                        FILE *fd =  fopen(statspath, "a");// Append
                        if (fd != NULL) {
                                pthread_mutex_lock(stats_mtx);
                                printStats(fd);
                                pthread_mutex_unlock(stats_mtx);
                        }
                        fclose(fd);
                        alert = 0;
                }
        }
}
