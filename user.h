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
  * @file user.h
  * @brief file contenente definizione delle strutture per la gestione degli
           utenti e funzioni per operare su esse
  */


#ifndef USER_H_
#define USER_H_

#include <string.h>
#include <pthread.h>
#include <uthash.h>
#include <utlist.h>
#include <config.h>
#include <message.h>
#include <parser.h>
#include <stats.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/epoll.h>


/**
 * @struct _history
 * @brief lista history utente
 *
 * @var data contiene msg
 * @var next puntatore all'elemento successivo della lista
 */
typedef struct _history {
  message_t *data;
  struct _history *next;
} history;

/**
 * @struct _user_list
 * @brief lista utenti
 *
 * @var nick nome utente
 * @var fd fd relativo a utente
 * @var next puntatore all'elemento successivo della lista
 */
typedef struct _user_list {
  char nick[MAX_NAME_LENGTH + 1];
  unsigned long fd;
  struct _user_list *next;
} ulist;

/**
 * @struct _user
 * @brief struttura contenente informazioni utente
 *
 * @var nick nome utente
 * @var history lista history utente
 * @var hc numero msg presenti nella lista history
 * @var fd relativo a utente
 * @var hh richiesto da uthash
 */
typedef struct _user {
  char nick[MAX_NAME_LENGTH + 1];
  history *history;
  unsigned long hc;
  unsigned long fd;
  UT_hash_handle hh;
} user;

/**
 * @struct _user_group
 * @brief struttura contenente informazioni utente di un gruppo
 *
 * @var nick nome utente
 * @var user puntatore a struttura _user
 * @var hh richiesto da uthash
 */
typedef struct _user_group {
  char nick[MAX_NAME_LENGTH + 1];
  user *user;
  UT_hash_handle hh;
} user_group;

/**
 * @struct_group
 * @brief struttura contenente informazioni gruppo
 *
 * @var name nome gruppo
 * @var group_mtx mutex
 * @var group hashtable contenente utenti registrati al gruppo
 * @var hh richiesto da uthash
 */
typedef struct _group {
  char name[MAX_NAME_LENGTH + 1];
  pthread_mutex_t *group_mtx;
  user_group *group;
  UT_hash_handle hh;
} group;

/**
 * @struct _usersdata
 * @brief struttura contenente utenti registrati
 *
 * @var mtx mutex
 * @var users hashtable contenente utenti registrati
 * @var connessi numero utenti connessi
 * @var cf struttura contenente configurazioni
 * @var hashtable contenente i gruppi creati
 * @var stats struttura contenente statistiche
 * @var stats_mtx mutex per stats
 * @var his_mtx mutex per history
 */
typedef struct _usersdata {
  pthread_mutex_t *mtx;
  user *users;
  unsigned long connessi;
  conf *cf;
  group *groups;
  struct statistics *stats;
  pthread_mutex_t *stats_mtx;
  pthread_mutex_t *his_mtx;
} userdata;


/**
 * @function new_udata
 * @brief crea la struttura per la gestione degli utenti
 *
 * @param cf struttura contenente configurazioni
 * @param chattyStats struttura contenente statistiche
 * @return puntatore alla struttura userdata creata
 */
userdata *new_udata(conf *cf, struct statistics *chattyStats);

/**
 * @function del_udata
 * @brief elimina la struttura per la gestione degli utenti
 *
 * @param ud puntatore alla struttura userdata da eliminare
 */
void del_udata(userdata *ud);

/**
 * @function reg_user
 * @brief registra utente
 *
 * @param ud puntatore alla struttura userdata in cui aggiungere utente
 * @param nick nome dell'utente da registrare
 * @return 0 se la registrazione e' avvenuta con successo, -1 altrimenti
 */
int reg_user(userdata *ud, char *nick);

/**
 * @function con_user
 * @brief connette utente
 *
 * @param ud puntatore alla struttura userdata in cui connettere utente
 * @param nick nome dell'utente da connettere
 * @param fd fd relativo all'utente da connettere
 * @return 0 se la connessione e' avvenuta con successo, -1 altrimenti
 */
int con_user(userdata *ud, char *nick, unsigned long fd);

/**
 * @function del_user
 * @brief elimina utente
 *
 * @param ud puntatore alla struttura userdata in cui eliminare utente
 * @param nick nome dell'utente da eliminare
 * @return 0 se l'eliminazione e' avvenuta con successo, -1 altrimenti
 */
int del_user(userdata *ud, char *nick);

/**
 * @function logout_user
 * @brief disconnette utente
 *
 * @param ud puntatore alla struttura userdata in cui disconnettere utente
 * @param nick nome dell'utente da disconnettere
 * @return 0 se la disconnessione e' avvenuta con successo, -1 altrimenti
 */
int logout_user(userdata *ud, char *nick);

/**
 * @function brutal_user
 * @brief disconnette utente dato il suo fd
 *
 * @param ud puntatore alla struttura userdata in cui disconnettere utente
 * @param fd descrittore della connessione relativo all'utente da disconnettere
 * @param epollfd fd relativo all'istanza epoll in uso
 * @param mtx mutex
 * @return 0 se la disconnessione e' avvenuta con successo, -1 altrimenti
 */
int brutal_logout(userdata *ud, int fd, int epollfd, pthread_mutex_t *mtx);

/**
 * @function all_user
 * @brief restituisce lista utenti connessi
 *
 * @param ud puntatore alla struttura userdata dove cercare utente
 * @param list lista dove salvare utenti
 */
void all_user(userdata *ud, ulist **list);

/**
 * @function del_list
 * @brief elimina lista utenti connessi
 *
 * @param list lista da eliminare
 */
void del_list(ulist **list);

/**
 * @function save_msg
 * @brief salva msg nella history di un utente non connesso
 *
 * @param ud puntatore alla struttura userdata in cui e' presente utente
 * @param nick nome utente del destinatario del msg
 * @param msg msg da salvare
 * @return 0 se la disconnessione e' avvenuta con successo, -1 altrimenti
 */
int save_msg(userdata *ud, char *nick, message_t *msg);

/**
 * @function del_history
 * @brief elimina history utente
 *
 * @param ud puntatore alla struttura userdata in cui e' presente utente
 * @param list lista da eliminare
 */
void del_history(userdata *ud, history **list);

/**
 * @function find_user
 * @brief cerca un utente
 *
 * @param ud puntatore alla struttura userdata in cui cercare utente
 * @param nick nome utente da cercare
 * @return user se l'utente e' stato trovato, null altrimenti
 */
user *find_user(userdata *ud, char *nick);

/**
 * @function find_history
 * @brief cerca history utente
 *
 * @param ud puntatore alla struttura userdata in cui cercare history utente
 * @param nick nome utente da cercare
 * @return history se la history utente e' stata trovata, null altrimenti
 */
history *find_history(userdata *ud, char*nick);

void print_list(ulist *list); //Serve solo per debug

void print_users(userdata *ud); //Serve solo per debug

/**
 * @function stats
 * @brief aggiorna statistiche
 *
 * @param ud puntatore alla struttura userdata in cui e' presente la struttura
          stats da aggiornare
 */
void stats(int reg, int con, int msg_delivered, int msg_waiting,
   int file_delivered, int file_waiting, int err, userdata *ud);

   /**
    * @function new_group
    * @brief crea una struttura di tipo _group
    *
    * @param name il nome del gruppo da creare
    * @return puntatore alla struttura _group appena creata
    */
   group *new_group(char *name);

   /**
    * @function del_group
    * @brief elimina una struttura di tipo _group
    *
    * @param gdata il gruppo da eliminare
    */
   void del_group(group *gdata);

   /**
    * @function reg_user_group
    * @brief aggiunge un utente ad un gruppo
    *
    * @param gdata il gruppo in cui aggiungere l'utente
    * @param new utente da aggiungere al gruppo
    * @return 0 se utente e' stato aggiunto correttamente, -1 in caso di errore
    */
   int reg_user_group(group *gdata, user *new);

   /**
    * @function del_user_group
    * @brief elimina un utente da un gruppo
    *
    * @param gdata il gruppo in cui c'e' l'utente da rimuovere
    * @param del utente da rimuovere dal gruppo
    * @return 0 se utente e' stato eliminato correttamente, -1 in caso di errore
    */
   int del_user_group(group *gdata, user *del);

   /**
    * @function find_user_group
    * @brief verifica la presenza di un utente in un gruppo
    *
    * @param gdata il gruppo in cui cercare l'utente
    * @param nick nickname dell'utente da cercare
    * @return 0 se utente e' stato trovato, -1 in caso di errore
    */
   int find_user_group(group *gdata, char *nick);

   /**
    * @function find_group
    * @brief cerca un gruppo tra i gruppi creati
    *
    * @param ud struttura _userdata dove e' presente hashmap con i gruppi
    * @param name nome del gruppo da cercare
    * @return puntatore alla struttura _group relativo al gruppo cercato, NULL
      altrimenti
    */
   group *find_group(userdata *ud, char *name);

   /**
    * @function add_group
    * @brief crea e registra un gruppo in ud
    *
    * @param ud struttura _userdata dove e' presente hashmap con i gruppi
    * @param name nome del gruppo da creare e registrare
    * @return 0 se il gruppo e' stato creato e registrato correttamente, -1
      altrimenti
    */
   int add_group(userdata *ud, char *name);

#endif /* USER_H_ */
