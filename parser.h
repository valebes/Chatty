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
 * @file parser.h
 * @brief contiene le funzioni che permettono la lettura del
 *        file di configurazione
 */

#ifndef PARSER_H_
#define PARSER_H_

#define MAXBUF 1024
#define DIV "= \t\r\n\v\f"
#define CHECK_TOK(token)    \
								if(token == NULL) { \
																printf("Errore nel file di configurazione\n"); \
																return -2; \
								} \

/**
 *  @struct conf
 *  @brief configurazione server
 *  @var unix_path posizione socket
 *  @var max_con numero massimo connessioni
 *  @var nthreads numero massimo threads
 *	@var max_msg massima dimensione messaggio
 *	@var max_size massima dimensione file
 * 	@var max_history numero massimo di messaggi da salvare nella history
 *	@var down_path posizione cartella dove salvare file scambiati tra gli utenti
 *	@var stats posizione dove salvare file contenente statistiche
 */
struct _conf {
								char *unix_path;
								unsigned long max_con;
								unsigned long nthreads;
								unsigned long max_msg;
								unsigned long max_size;
								unsigned long max_history;
								char *down_path;
								char *stats;
};

typedef struct _conf conf;

/**
 * @function load_conf
 * @brief legge il file di configurazione
 * @param path locazione file configurazione
 */
int load_conf(char *path, conf *cf);

#endif /* PARSER_H_ */
