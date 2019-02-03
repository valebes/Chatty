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
 * @file utility.h
 * @brief file contenente funzioni per la scrittura e lettura socket
 */

#ifndef UTILITY_H_
#define UTILITY_H_

#include <sys/types.h>
#include <unistd.h>


//    Codice visto su:                             //
//    Unix Network Programming, Volume 1           //
//    http://www.unpbook.com                       //
// Socket, read e write non vanno molto d'accordo, //
// potrebbe succedere che vengano letti/scritti    //
// meno bytes di quelli richiesti.                 //

/* Abbiamo visto la stessa cosa su conn.h su assegnamento 10 e 11 durante il corso */

/**
 * @function readn
 * @brief legge fd
 *
 * @param filedes fd da leggere
 * @param buff puntatore al buffer dove scrivere quello che leggo
 * @param nbytes bytes da leggere
 * @return bytes letti, -1 in caso di errore
 */
ssize_t readn(int filedes, void *buff, size_t nbytes);

/**
 * @function writen
 * @brief scrive fd
 *
 * @param filedes fd in cui scrivere
 * @param buff puntatore al buffer contenente quello che che scrivo
 * @param nbytes bytes da scrivere
 * @return bytes scritti, -1 in caso di errore
 */
ssize_t writen(int filedes, const void *buff, size_t nbytes);

#endif /* UTILITY_H_ */
