/*
 * chatterbox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
/** \file connection.c
    \author Valerio Besozzi
   Si dichiara che il contenuto di questo file e' in ogni sua parte opera
   originale dell'autore
 */
 /**
  * @file  connection.c
  * @brief Contiene le implementazioni delle funzioni che implementano il protocollo
  *        tra i clients ed il server
  */

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <message.h>
#include <utility.h>
#include <connections.h>


int openConnection(char* path, unsigned int ntimes, unsigned int secs) {
        int fd_skt = 0;
        struct sockaddr_un sa;
        strncpy(sa.sun_path, path, UNIX_PATH_MAX);
        sa.sun_family = AF_UNIX;
        if((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
                perror("Errore nella creazione del socket: \n");
                return -1;
        }
        if (ntimes > MAX_RETRIES) ntimes = MAX_RETRIES;
        if (secs > MAX_SLEEPING) secs = MAX_SLEEPING;
        while ((ntimes != 0 ) && connect(fd_skt,(struct sockaddr*)&sa, sizeof(sa)) == -1 ) {
                if (errno == ENOENT) sleep(secs);
                else {
                        perror("Errore nell connessione: \n");
                        return -1;
                }
                ntimes--;
        }
        return fd_skt;
}

int readHeader(long connfd, message_hdr_t *hdr) {
        int tmp = 0;
        memset(hdr, 0, sizeof(message_hdr_t));
        tmp = readn(connfd, hdr, sizeof(message_hdr_t));
        if(tmp < 0) return -1;
        return 1;
}

int readData(long fd, message_data_t *data) {
        int tmp = 0;
        memset(data, 0, sizeof(message_data_t));
        tmp = readn(fd, &(data->hdr), sizeof(message_data_hdr_t));
        if(tmp < 0) return -1;
        int len = data->hdr.len;
        if(len == 0) data->buf = NULL;
        else {
                data->buf = (char *)malloc(len * sizeof(char));
                memset(data->buf, 0, data->hdr.len * sizeof(char));
                int tmp_r = readn(fd, data->buf, len);
                if(tmp_r < 0) {
                        free(data->buf);
                        return -1;
                }
        }
        return 1;
}

int readMsg(long fd, message_t *msg) {
        int tmp = 0;
        memset(msg, 0, sizeof(message_t));
        tmp = readHeader(fd, &(msg->hdr));
        if(tmp < 0) return -1;
        tmp = readData(fd, &(msg->data));
        if(tmp < 0) return -1;
        return 1;
}

int sendRequest(long fd, message_t *msg) {
        int tmp = 0;
        if(DEBUG == 1) {
                printf("\top: %d sender: %s\n", msg->hdr.op, msg->hdr.sender);
                if(msg->data.buf != NULL && msg->hdr.op == 21) printf("\tbuffer: %s\n", msg->data.buf);
        }
        tmp = sendHeader(fd, &(msg->hdr));
        if(tmp < 0) return -1;
        tmp = sendData(fd, &(msg->data));
        if(tmp < 0) return -1;
        return 1;
}

int sendData(long fd, message_data_t *msg) {
        int tmp = 0;
        tmp = writen(fd, &(msg->hdr), sizeof(message_data_hdr_t));
        if(tmp < 0) return -1;
        int len = msg->hdr.len;
        int tmp_w = writen(fd, msg->buf, len);
        if(tmp_w < 0) return -1;
        return 1;
}

int sendHeader(long fd, message_hdr_t *hdr) {
        int tmp = 0;
        tmp = writen(fd, hdr, sizeof(message_hdr_t));
        if(tmp < 0) return -1;
        return 1;
}
