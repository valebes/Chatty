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
 * @file user.c
 * @brief file contenente implementazioni funzioni per operare sulle
          strutture per la gestione degli utenti
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <uthash.h>
#include <utlist.h>
#include <config.h>
#include <message.h>
#include <user.h>
#include <parser.h>
#include <stats.h>
#include <connections.h>

userdata *new_udata(conf *cf, struct statistics *chattyStats) {
        userdata *ud = (userdata *)malloc(sizeof(userdata));
        ud->mtx = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(ud->mtx, NULL);
        ud->users = NULL;
        ud->connessi = 0;
        ud->cf = cf;
        ud->groups = NULL;
        ud->stats = chattyStats;
        ud->stats_mtx = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(ud->stats_mtx, NULL );
        ud->his_mtx = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(ud->his_mtx, NULL );
        return ud;
}

void del_udata(userdata *ud) {
        printf("Elimino UData\n");
        user *curr_user, *tmp;
        group *curr_group, *gtmp;
        printf("Elimino utenti\n");
        HASH_ITER(hh, ud->users, curr_user, tmp) {
                del_user(ud, curr_user->nick);
        }
        HASH_ITER(hh, ud->groups, curr_group, gtmp) {
                del_group(curr_group);
                HASH_DEL(ud->groups, curr_group);
                free(curr_group);
        }
        pthread_mutex_destroy(ud->mtx);
        pthread_mutex_destroy(ud->stats_mtx);
        free(ud->mtx);
        free(ud->stats_mtx);
        pthread_mutex_destroy(ud->his_mtx);
        free(ud->his_mtx);
        printf("Utenti eliminati\n");
        free(ud);
        printf("UData Eliminato!\n");
}


int reg_user(userdata *ud, char *nick) {
        group *tmp = find_group(ud, nick);
        if(tmp != NULL) return -1;
        pthread_mutex_lock(ud->mtx);
        user *new_user = NULL;
        HASH_FIND_STR(ud->users, nick, new_user);
        if(new_user == NULL) {
                new_user = (user *)malloc(sizeof(user));
                strncpy(new_user->nick, nick, MAX_NAME_LENGTH + 1);
                new_user->history = NULL;
                new_user->fd = -1;
                new_user->hc = 0;
                HASH_ADD_STR(ud->users, nick, new_user);
        }
        else {
                pthread_mutex_unlock(ud->mtx);
                return -1;
        }
        pthread_mutex_unlock(ud->mtx);
        return 0;
}


int con_user(userdata *ud, char *nick, unsigned long fd) {

        pthread_mutex_lock(ud->mtx);
        user *curr_user;
        HASH_FIND_STR(ud->users, nick, curr_user);
        if(curr_user != NULL) {
                if(curr_user->fd == -1)
                        ud->connessi++; // Se e' gia connesso lo ignoro
                curr_user->fd = fd;
        }
        else {
                pthread_mutex_unlock(ud->mtx);
                return -1;
        }
        pthread_mutex_unlock(ud->mtx);
        return 0;
}

int del_user(userdata *ud, char *nick) {

        pthread_mutex_lock(ud->mtx);
        user *curr_user;
        int retfd = 0;
        HASH_FIND_STR(ud->users, nick, curr_user);
        if(curr_user != NULL) {
                del_user_group(ud->groups, curr_user);
                retfd = curr_user->fd;
                ud->connessi--;
                del_history(ud, &curr_user->history);
                HASH_DEL(ud->users, curr_user);
                curr_user->history = NULL;
                free(curr_user);
        }
        else{
                pthread_mutex_unlock(ud->mtx);
                return -1;
        }
        pthread_mutex_unlock(ud->mtx);
        return retfd;
}

int logout_user(userdata *ud, char *nick) {

        pthread_mutex_lock(ud->mtx);
        user *curr_user;
        int retfd = 0;
        HASH_FIND_STR(ud->users, nick, curr_user);
        if(curr_user != NULL) {
                retfd = curr_user->fd;
                ud->connessi--;
                curr_user->fd = -1;
        }
        else {
                pthread_mutex_unlock(ud->mtx);
                return -1;
        }
        pthread_mutex_unlock(ud->mtx);
        return retfd;
}

int brutal_logout(userdata *ud, int fd, int epollfd, pthread_mutex_t *mtx) {
        pthread_mutex_lock(ud->mtx);
        user *curr_user, *tmp;
        int trovato = 0;
        HASH_ITER(hh, ud->users, curr_user, tmp) {
                if(fd == curr_user->fd) {
                        ud->connessi--;
                        curr_user->fd = -1;
                        trovato = 1;
                }
        }
        pthread_mutex_lock(mtx);
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
        pthread_mutex_unlock(mtx);
        pthread_mutex_unlock(ud->mtx);
        if(!trovato) return -1;
        return 0;
}
void all_user(userdata *ud, ulist **list) {

        user *curr_user, *tmp;
        ulist *new_user;
        pthread_mutex_lock(ud->mtx);
        HASH_ITER(hh, ud->users, curr_user, tmp) {
                new_user = (ulist *)malloc(sizeof(ulist));
                strncpy(new_user->nick, curr_user->nick, MAX_NAME_LENGTH + 1);
                new_user->fd = curr_user->fd;
                LL_APPEND(*list, new_user);
        }
        pthread_mutex_unlock(ud->mtx);
}

void del_list(ulist **list) {
        ulist *curr_user, *tmp;
        LL_FOREACH_SAFE(*list, curr_user,tmp) {
                LL_DELETE(*list, curr_user);
                free(curr_user);
        }
}

int save_msg(userdata *ud, char *nick, message_t *msg) {
        pthread_mutex_lock(ud->mtx);
        pthread_mutex_lock(ud->his_mtx);
        user *curr_user;
        conf *cf = ud->cf;
        HASH_FIND_STR(ud->users, nick, curr_user);
        if(curr_user != NULL && (curr_user->hc <  cf->max_history)) {
                history *new_h = (history *)malloc(sizeof(history));
                new_h->data = new_msg(msg);
                LL_PREPEND(curr_user->history, new_h);
                curr_user->hc++;
        }
        else if(curr_user != NULL && (curr_user->hc >= cf->max_history)) {
                history *del = curr_user->history;
                while(del->next != NULL) {
                        del = del->next;
                }
                message_t *tmp_msg = del->data;
                if(tmp_msg != NULL) {
                        free(tmp_msg->data.buf);
                        free(tmp_msg);
                }
                LL_DELETE(curr_user->history, del);
                free(del);
                history *new_h = (history *)malloc(sizeof(history));
                new_h->data = new_msg(msg);
                LL_PREPEND(curr_user->history, new_h);
        }
        else {
                pthread_mutex_unlock(ud->his_mtx);
                pthread_mutex_unlock(ud->mtx);
                return -1;
        }
        printf("Messaggio per %s salvato, dim history: %ld\n",
               nick, curr_user->hc);
        pthread_mutex_unlock(ud->his_mtx);
        pthread_mutex_unlock(ud->mtx);
        return 0;
}

void del_history(userdata *ud, history **list) {
        history *curr_msg, *tmp;
        pthread_mutex_lock(ud->his_mtx);

        LL_FOREACH_SAFE(*list, curr_msg,tmp) {
                message_t *tmp_msg = curr_msg->data;
                if(tmp_msg != NULL) {
                        free(tmp_msg->data.buf);
                        free(tmp_msg);
                }
                LL_DELETE(*list, curr_msg);
                free(curr_msg);
        }
        *list = NULL;
        pthread_mutex_unlock(ud->his_mtx);
}

user *find_user(userdata *ud, char *nick) {

        user *ret_user = NULL;
        pthread_mutex_lock(ud->mtx);
        HASH_FIND_STR(ud->users, nick, ret_user);
        pthread_mutex_unlock(ud->mtx);
        return ret_user;
}

history *find_history(userdata *ud, char*nick) {

        user *ret_user = NULL;
        history *ret_history = NULL;
        pthread_mutex_lock(ud->mtx);
        pthread_mutex_lock(ud->his_mtx);
        HASH_FIND_STR(ud->users, nick, ret_user);
        if(ret_user != NULL && ret_user->history != NULL)
                ret_history = ret_user->history;
        pthread_mutex_unlock(ud->his_mtx);
        pthread_mutex_unlock(ud->mtx);
        return ret_history;
}

void print_list(ulist *list) {
        ulist *cp = list;
        while(cp != NULL) {
                printf("UTENTE: %s FD: %li\n", cp->nick, cp->fd);
                cp = cp->next;
        }
}

void print_users(userdata *ud)
{
        user *s;
        pthread_mutex_lock(ud->mtx);
        for(s=ud->users; s != NULL; s=(user*)(s->hh.next)) {
                printf("%s id: %li \n", s->nick, s->fd);
        }
        pthread_mutex_unlock(ud->mtx);
}

void stats(int reg, int con, int msg_delivered, int msg_waiting,
           int file_delivered, int file_waiting, int err, userdata *ud) {

        pthread_mutex_lock(ud->stats_mtx);
        struct statistics *tmp = ud->stats;
        tmp->nusers += reg;
        tmp->nonline += con;
        tmp->ndelivered += msg_delivered;
        tmp->nnotdelivered += msg_waiting;
        tmp->nfiledelivered += file_delivered;
        tmp->nfilenotdelivered += file_waiting;
        tmp->nerrors += err;
        pthread_mutex_unlock(ud->stats_mtx);

}

group *new_group(char *name) {
        group *gdata = (group *)malloc(sizeof(group));
        gdata->group_mtx = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(gdata->group_mtx, NULL);
        gdata->group = NULL;
        strncpy(gdata->name, name, MAX_NAME_LENGTH + 1);
        return gdata;
}

void del_group(group *gdata) {
  user_group *curr_user, *tmp;
  if(gdata != NULL) {
  pthread_mutex_lock(gdata->group_mtx);
  HASH_ITER(hh, gdata->group, curr_user, tmp) {
                  HASH_DEL(gdata->group, curr_user);
                  free(curr_user);

          pthread_mutex_unlock(gdata->group_mtx);
  }
        pthread_mutex_destroy(gdata->group_mtx);
        free(gdata->group_mtx);
        printf("Gruppo eliminato\n");
      }
}

int reg_user_group(group *gdata, user *new) {
        if(new == NULL) return -1;
        if(gdata == NULL) return -1;
        pthread_mutex_lock(gdata->group_mtx);
        user_group *tmp = NULL;
        HASH_FIND_STR(gdata->group, new->nick, tmp);
        if(tmp == NULL) {
                user_group *new_user = (user_group *)malloc(sizeof(user_group));
                strncpy(new_user->nick, new->nick, MAX_NAME_LENGTH + 1);
                new_user->user = new;
                HASH_ADD_STR(gdata->group, nick, new_user);
        }
        pthread_mutex_unlock(gdata->group_mtx);
        return 0;
}

int del_user_group(group *gdata, user *del) {
        if(del == NULL) return -1;
        if(gdata == NULL) return -1;
        pthread_mutex_lock(gdata->group_mtx);
        user_group *tmp = NULL;
        HASH_FIND_STR(gdata->group, del->nick, tmp);
        if(tmp != NULL) HASH_DEL(gdata->group, tmp);
        free(tmp);
        pthread_mutex_unlock(gdata->group_mtx);
        return 0;
}

int find_user_group(group *gdata, char *nick) {
        if(gdata == NULL) return -1;
        pthread_mutex_lock(gdata->group_mtx);
        user_group *tmp = NULL;
        HASH_FIND_STR(gdata->group, nick, tmp);
        if(tmp == NULL) {
                pthread_mutex_unlock(gdata->group_mtx);
                return -1;
        }
        pthread_mutex_unlock(gdata->group_mtx);
        return 0;
}


group *find_group(userdata *ud, char *name) {
        group *tmp = NULL;
        pthread_mutex_lock(ud->mtx);
        HASH_FIND_STR(ud->groups, name, tmp);
        pthread_mutex_unlock(ud->mtx);
        return tmp;
}

int add_group(userdata *ud, char *name) {
        group *tmp = NULL;
        pthread_mutex_lock(ud->mtx);
        HASH_FIND_STR(ud->groups, name, tmp);
        if(tmp == NULL) {
                tmp = new_group(name);
                HASH_ADD_STR(ud->groups, name, tmp);
        }
        else {
                pthread_mutex_unlock(ud->mtx);
                return -1;
        }
        pthread_mutex_unlock(ud->mtx);
        return 0;
}
