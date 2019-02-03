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
 * @file utility.c
 * @brief file contenente implementazioni funzioni per la scrittura e lettura socket
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <utility.h>

//    Codice visto su:                             //
//    Unix Network Programming, Volume 1           //
//    http://www.unpbook.com                       //
// Socket, read e write non vanno molto d'accordo, //
// potrebbe succedere che vengano letti/scritti    //
// meno bytes di quelli richiesti.                 //

/* Abbiamo visto la stessa cosa su conn.h su assegnamento 10 e 11 durante il corso */

ssize_t readn(int fd, void *vptr, size_t n) /* Legge n bytes da un fd */
{
        size_t nleft;
        ssize_t nread;
        char  *ptr;

        ptr = vptr;
        nleft = n;
        while (nleft > 0) {
                if ( (nread = read(fd, ptr, nleft)) < 0) {
                        if (errno == EINTR)
                                nread = 0;
                        else
                                return(-1);
                } else if (nread == 0)
                        break;

                nleft -= nread;
                ptr   += nread;
        }
        return(n - nleft);
}
/* end readn */

ssize_t writen(int fd, const void *vptr, size_t n) /*Scrive n bytes su un fd */
{
        size_t nleft;
        ssize_t nwritten;
        const char  *ptr;

        ptr = vptr;
        nleft = n;
        while (nleft > 0) {
                if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
                        if (errno == EINTR)
                                nwritten = 0;
                        else
                                return(-1);
                }

                nleft -= nwritten;
                ptr   += nwritten;
        }
        return(n);
}
/* end writen */
