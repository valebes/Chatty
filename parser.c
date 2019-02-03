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
 * @file parser.c
 * @brief contiene implementazioni funzioni che permettono la lettura del
 *        file di configurazione
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

int load_conf(char *path, conf *cf) {
        char tmp[MAXBUF];
        FILE *fp;

        if((fp = fopen(path, "r")) == NULL) {
                perror("Impossibile aprire file configurazione");
                return -1;
        }

        while(fgets(tmp, MAXBUF, fp) != NULL) {
                /* Salto commenti e line vuote */
                if(tmp[0] == '#' || strlen(tmp) <= 1) {
                        continue;
                }
                char campo[MAXBUF], valore[MAXBUF];
                char *token;
                token = strtok(tmp, DIV);
                CHECK_TOK(token);
                strncpy(campo, token, MAXBUF);
                token = strtok(NULL, DIV);
                CHECK_TOK(token);
                strncpy(valore, token, MAXBUF);

                /* Inserisco valori nella struttura */

                if(strncmp(campo, "UnixPath", MAXBUF) == 0)
                        strncpy(cf->unix_path, valore, MAXBUF);
                else if(strncmp(campo, "MaxConnections", MAXBUF) == 0)
                        cf->max_con = strtoul(valore, NULL, 10);
                else if(strncmp(campo, "ThreadsInPool", MAXBUF) == 0)
                        cf->nthreads = strtoul(valore, NULL, 10);
                else if(strncmp(campo, "MaxMsgSize", MAXBUF) == 0)
                        cf->max_msg = strtoul(valore, NULL, 10);
                else if(strncmp(campo, "MaxFileSize", MAXBUF) == 0)
                        cf->max_size = strtoul(valore, NULL, 10);
                else if(strncmp(campo, "MaxHistMsgs", MAXBUF) == 0)
                        cf->max_history= strtoul(valore, NULL, 10);
                else if(strncmp(campo, "DirName", MAXBUF) == 0)
                        strncpy(cf->down_path, valore, MAXBUF);
                else if(strncmp(campo, "StatFileName", MAXBUF) == 0)
                        strncpy(cf->stats, valore, MAXBUF);

        }
        fclose(fp);
        return 1;
}
