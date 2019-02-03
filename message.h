/*
 * chatterbox Progetto del corso di LSO 2017/2018
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
  * @file message.h
  * @brief file contenente definizioni base, strutture e funzioni
    per gestione messaggi
  */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <config.h>
#include <ops.h>

/**
 *  @struct header
 *  @brief header del messaggio
 *
 *  @var op tipo di operazione richiesta al server
 *  @var sender nickname del mittente
 */
typedef struct {
        op_t op;
        char sender[MAX_NAME_LENGTH+1];
} message_hdr_t;

/**
 *  @struct header
 *  @brief header della parte dati
 *
 *  @var receiver nickname del ricevente
 *  @var len lunghezza del buffer dati
 */
typedef struct {
        char receiver[MAX_NAME_LENGTH+1];
        unsigned int len;
} message_data_hdr_t;

/**
 *  @struct data
 *  @brief body del messaggio
 *
 *  @var hdr header della parte dati
 *  @var buf buffer dati
 */
typedef struct {
        message_data_hdr_t hdr;
        char               *buf;
} message_data_t;

/**
 *  @struct messaggio
 *  @brief tipo del messaggio
 *
 *  @var hdr header
 *  @var data dati
 */
typedef struct {
        message_hdr_t hdr;
        message_data_t data;
} message_t;

/* ------ funzioni di utilità ------- */

/**
 * @function setheader
 * @brief scrive l'header del messaggio
 *
 * @param hdr puntatore all'header
 * @param op tipo di operazione da eseguire
 * @param sender mittente del messaggio
 */
static inline void setHeader(message_hdr_t *hdr, op_t op, char *sender) {
#if defined(MAKE_VALGRIND_HAPPY)
        memset((char*)hdr, 0, sizeof(message_hdr_t));
#endif
        hdr->op  = op;
        strncpy(hdr->sender, sender, strlen(sender)+1);
}
/**
 * @function setData
 * @brief scrive la parte dati del messaggio
 *
 * @param msg puntatore al body del messaggio
 * @param rcv nickname o groupname del destinatario
 * @param buf puntatore al buffer
 * @param len lunghezza del buffer
 */
static inline void setData(message_data_t *data, char *rcv, const char *buf, unsigned int len) {
#if defined(MAKE_VALGRIND_HAPPY)
        memset((char*)&(data->hdr), 0, sizeof(message_data_hdr_t));
#endif

        strncpy(data->hdr.receiver, rcv, strlen(rcv)+1);
        data->hdr.len  = len;
        data->buf      = (char *)buf;
}

/**
 * @function new_msg
 * @brief crea una copia di un msg
 *
 * @param msg messaggio da copiare
 * @return puntatore alla copia del msg appena creata
 */
static inline message_t *new_msg(message_t *msg) {
        message_t *new = (message_t *)malloc(sizeof(message_t));
   #if defined(MAKE_VALGRIND_HAPPY)
        memset((char *)new, 0, sizeof(message_t));
   #endif
        new->hdr.op = msg->hdr.op;
        strncpy(new->hdr.sender, msg->hdr.sender, MAX_NAME_LENGTH + 1);

        new->data.hdr.len = msg->data.hdr.len;
        strncpy(new->data.hdr.receiver, msg->data.hdr.receiver, MAX_NAME_LENGTH + 1);

        new->data.buf = (char *)malloc(new->data.hdr.len * sizeof(char));
 #if defined(MAKE_VALGRIND_HAPPY)
        memset(new->data.buf, 0, new->data.hdr.len * sizeof(char));
 #endif
        for (int i = 0; i < new->data.hdr.len; i++)
                new->data.buf[i] = msg->data.buf[i];
        return new;
}

#endif /* MESSAGE_H_ */
