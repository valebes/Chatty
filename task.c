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
 * @file task.c
 * @brief file contenente implementazioni funzioni per la gestione ed esecuzione dei task
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <string.h>
 #include <signal.h>
 #include <fcntl.h>
 #include <pthread.h>
 #include <sys/time.h>
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <sys/mman.h>
 #include <sys/stat.h>
 #include <sys/epoll.h>
 #include <stats.h>
 #include <message.h>
 #include <connections.h>
 #include <user.h>
 #include <parser.h>
 #include <task.h>

task_manager *new_manager(int epollfd, userdata *ud) {
        task_manager *tsk = (task_manager *)malloc(sizeof(task_manager));
        tsk->epollfd = epollfd;
        tsk->jobs = NULL;
        tsk->ud = ud;
        tsk->mtx_poll = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(tsk->mtx_poll, NULL);
        tsk->mtx_tm = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(tsk->mtx_tm, NULL);
        return tsk;
}

void del_task(task_manager *tsk) {
        pthread_mutex_destroy(tsk->mtx_poll);
        free(tsk->mtx_poll);
        pthread_mutex_destroy(tsk->mtx_tm);
        free(tsk->mtx_tm);
        job *curr_job, *tmp;
        HASH_ITER(hh, tsk->jobs, curr_job, tmp) {
                        HASH_DEL(tsk->jobs, curr_job);
                        del_job(curr_job);
        }
        free(tsk);
}

job *new_job(int fd, task_manager *tsk) {
        job *new = (job *)malloc(sizeof(job));
        new->fd = fd;
        new->msg = NULL;
        new->tsk = tsk;
        return new;
}

void del_job(job *jobd) {
        free(jobd); // Controllare, possibile memory leak
}

int read_task(job *jobr) {
        int fd = jobr->fd;
        task_manager *tsk = jobr->tsk;

        struct epoll_event ev;
        memset(&ev, 0, sizeof(struct epoll_event));
        int epollfd = tsk->epollfd;

        pthread_mutex_t *mtx_poll = tsk->mtx_poll;
        pthread_mutex_t *mtx_tm = tsk->mtx_tm;

        userdata *ud = tsk->ud;

        jobr->msg = malloc(sizeof(message_t));
        message_t *msg = jobr->msg;

        if(fd == -1) {
           free(msg);
           del_job(jobr);
           return -1;
         }
        printf("\t Dati ricevuti da [fd:%d]\n", fd);
        if(readHeader(fd, &(msg->hdr)) > 0 && readData(fd, &(msg->data)) > 0) {
                /* leggo fd e inserisco task nella coda */

                pthread_mutex_lock(mtx_tm);
                job *tmp = NULL;
                HASH_FIND_INT(tsk->jobs, &fd, tmp);
                if(tmp == NULL) HASH_ADD_INT(tsk->jobs, fd, jobr);
                else {
                        printf("Errore: il server ha ancora una richiesta in sospeso da: %d\n", fd);
                        free(msg->data.buf);
                        free(msg);
                        del_job(jobr);
                }
                pthread_mutex_unlock(mtx_tm);

                /* fd pronto a scrittura */
                ev.data.fd = fd;
                ev.events = EPOLLOUT;
                pthread_mutex_lock(mtx_poll);
                epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
                pthread_mutex_unlock(mtx_poll);
        }
        else{
                free(msg);
                del_job(jobr);
                if(brutal_logout(ud, fd, epollfd, mtx_poll) == 0)
                        stats(0, -1, 0, 0, 0, 0, 0, ud);
                return -1;
        }
        return 0;
}

int write_task(job *jobw) {
        int fd = jobw->fd;
        task_manager *tsk = jobw->tsk;

        struct epoll_event ev;
        memset(&ev, 0, sizeof(struct epoll_event));
        int epollfd = tsk->epollfd;
        pthread_mutex_t *mtx_poll = tsk->mtx_poll;
        pthread_mutex_t *mtx_tm = tsk->mtx_tm;
        userdata *ud = tsk->ud;

        /* leggo task in sospeso di fd */
        pthread_mutex_lock(mtx_tm);
        job *tmp = NULL;
        HASH_FIND_INT(tsk->jobs, &fd, tmp);
        if(tmp == NULL) {
                printf("Errore: il server non ha una richiesta in sospeso da: %d\n", fd);
                del_job(jobw);
                pthread_mutex_unlock(mtx_tm);
                return -1;
        }
        HASH_DEL(tsk->jobs, tmp);
        pthread_mutex_unlock(mtx_tm);

        message_t *msg = tmp->msg;
        int ret = exec_task(tsk, msg, fd, ud);

        del_job(jobw);
        if(ret == 0) {
                printf("\t Dati inviati a [fd:%d]\n", fd);
                free(msg->data.buf);
                msg->data.buf = NULL;
                free(msg);
                del_job(tmp);
                ev.data.fd = fd;
                ev.events = EPOLLIN;
                pthread_mutex_lock(mtx_poll);
                epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
                pthread_mutex_unlock(mtx_poll);
        }
        else{
                printf("\t Task di {nick: %s }{fd: %d }{op: %d } fallita\n", msg->hdr.sender, fd, msg->hdr.op); // Devo anche disconnettere utente e aggiornare stats
                free(msg->data.buf);
                msg->data.buf = NULL;
                free(msg);
                del_job(tmp);
                if(brutal_logout(ud, fd, epollfd, mtx_poll) == 0) stats(0, -1, 0, 0, 0, 0, 0, ud);
                return -1;
        }
        return 0;
}

int exec_task(task_manager *tsk, message_t *s_msg, int fd, userdata *ud) {
        message_t r_msg;
        memset(&r_msg, 0, sizeof(message_t));

        op_t op = s_msg->hdr.op;

        if(s_msg->hdr.sender == NULL || strlen(s_msg->hdr.sender) == 0) {
                printf("\t Sender assente\n");
                setHeader(&(r_msg.hdr), OP_FAIL, "");
                sendHeader(fd, &(r_msg.hdr));
                return -1;
        }

        printf("Eseguo richiesta di {nick: %s } {fd: %d } {op: %d}\n", s_msg->hdr.sender, fd, op);

        switch(op) {
        case REGISTER_OP: {
                conf *conf = ud->cf;
                int i = 0;

                printf("Registazione utente {nick: %s } {fd: %d } in corso\n", s_msg->hdr.sender, fd);

                if(reg_user(ud, s_msg->hdr.sender) == 0) {

                        printf("Registazione utente {nick: %s } {fd: %d } completata!\n", s_msg->hdr.sender, fd);

                        if(ud->connessi < conf->max_con) {
                                if(con_user(ud, s_msg->hdr.sender, fd) == 0) {  // Connetto UTENTE
                                        stats(1, 1, 0, 0, 0, 0, 0, ud); // Aggiorno statistiche
                                        ulist *users = NULL, *tmp, *curr_user;
                                        all_user(ud, &users);
                                        pthread_mutex_lock(ud->mtx);
                                        char *buffer = malloc(ud->connessi * (MAX_NAME_LENGTH + 1) * sizeof(char));
                                        memset(buffer, 0, ud->connessi * (MAX_NAME_LENGTH + 1) * sizeof(char));
                                        char *ptr = buffer;
                                        LL_FOREACH_SAFE(users, curr_user,tmp) {
                                                if(curr_user->fd != -1) {
                                                        strncpy(ptr, curr_user->nick, MAX_NAME_LENGTH + 1);
                                                        ptr += MAX_NAME_LENGTH + 1;
                                                        i++;
                                                }
                                        }
                                        setHeader(&(r_msg.hdr), OP_OK, "");
                                        setData(&(r_msg.data), "", buffer, ud->connessi * (MAX_NAME_LENGTH + 1));
                                        pthread_mutex_unlock(ud->mtx);

                                        if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                                del_list(&users);
                                                free(users);
                                                free(buffer);
                                                return -1;
                                        }
                                        if(sendData(fd, &(r_msg.data)) < 0) {
                                                del_list(&users);
                                                free(users);
                                                free(buffer);
                                                return -1;
                                        }
                                        del_list(&users);
                                        free(users);
                                        free(buffer);
                                }
                                else {
                                        setHeader(&(r_msg.hdr), OP_NICK_ALREADY, "");
                                        if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                                return -1;
                                        }
                                }
                        }
                }
                else {
                        setHeader(&(r_msg.hdr), OP_NICK_ALREADY, "");
                        stats(0, 0, 0, 0, 0, 0, 1, ud);
                        if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                return -1;
                        }
                }
                break;
        }

        case CONNECT_OP: {
                int i = 0;
                conf *conf = ud->cf;

                printf("Connessione utente {nick: %s } {fd: %d }\n", s_msg->hdr.sender, fd);

                if(ud->connessi < conf->max_con) {
                        if(con_user(ud, s_msg->hdr.sender, fd) == 0) {                                 // Connetto UTENTE
                                stats(0, 1, 0, 0, 0, 0, 0, ud);  // Aggiorno statistiche
                                ulist *users = NULL, *tmp, *curr_user;
                                all_user(ud, &users);
                                pthread_mutex_lock(ud->mtx);
                                char *buffer = malloc(ud->connessi * (MAX_NAME_LENGTH + 1) * sizeof(char));
                                memset(buffer, 0, ud->connessi * (MAX_NAME_LENGTH + 1) * sizeof(char));
                                char *ptr = buffer;
                                LL_FOREACH_SAFE(users, curr_user,tmp) {
                                        if(curr_user->fd != -1) {
                                                strncpy(ptr, curr_user->nick, MAX_NAME_LENGTH + 1);
                                                ptr += MAX_NAME_LENGTH + 1;
                                                i++;
                                        }
                                }

                                setHeader(&(r_msg.hdr), OP_OK, "");
                                setData(&(r_msg.data), "", buffer, ud->connessi * (MAX_NAME_LENGTH + 1));
                                pthread_mutex_unlock(ud->mtx);

                                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                        del_list(&users);
                                        free(users);
                                        free(buffer);
                                        return -1;
                                }
                                if(sendData(fd, &(r_msg.data)) < 0) {
                                        del_list(&users);
                                        free(users);
                                        free(buffer);
                                        return -1;
                                }

                                del_list(&users);
                                free(users);
                                free(buffer);
                        }
                        else {
                                setHeader(&(r_msg.hdr), OP_NICK_UNKNOWN, "");
                                stats(0, 0, 0, 0, 0, 0, 1, ud);
                                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                        return -1;
                                }
                        }
                }
                break;
        }

        case POSTTXT_OP: {
                int len = s_msg->data.hdr.len;
                conf *conf = ud->cf;

                char *r_nick = s_msg->data.hdr.receiver;
                user *r_user = find_user(ud, r_nick);

                group *tmp = find_group(ud, r_nick);
                user_group *curr_user, *utmp;

                if(r_user == NULL && tmp == NULL) {
                        setHeader(&(r_msg.hdr), OP_NICK_UNKNOWN, ""); // Nick sconosciuto
                        stats(0, 0, 0, 0, 0, 0, 1, ud);
                }
                else if(tmp != NULL && (find_user_group(tmp, s_msg->hdr.sender) == -1)) {
                        setHeader(&(r_msg.hdr), OP_NICK_UNKNOWN, ""); // Nick sconosciuto
                        stats(0, 0, 0, 0, 0, 0, 1, ud);
                }
                else if(tmp != NULL) {
                        HASH_ITER(hh, tmp->group, curr_user, utmp) {
                                if(curr_user->user != NULL) {
                                        user *dest = curr_user->user;
                                        if(dest->fd != -1) {
                                                message_t *new = new_msg(s_msg);
                                                new->hdr.op = TXT_MESSAGE; // destinatario connesso
                                                sendRequest(dest->fd, new);
                                                stats(0, 0, 1, 0, 0, 0, 0, ud);
                                                printf("Messaggio di {nick: %s } {fd: %d } inviato\n", s_msg->hdr.sender, fd);
                                                free(new->data.buf);
                                                free(new);
                                        }
                                        else if(dest->fd == -1) { // destinatario non connesso
                                                s_msg->hdr.op = TXT_MESSAGE;
                                                save_msg(ud, dest->nick, s_msg);
                                                stats(0, 0, 0, 1, 0, 0, 0, ud);
                                                printf("Messaggio di {nick: %s } {fd: %d } salvato\n", s_msg->hdr.sender, fd);
                                        }
                                }
                                setHeader(&(r_msg.hdr), OP_OK, "");
                        }
                }
                else {
                        int r_fd = r_user->fd;

                        if(len > conf->max_msg) {
                                setHeader(&(r_msg.hdr), OP_MSG_TOOLONG, ""); // Messaggio troppo lungo
                                stats(0, 0, 0, 0, 0, 0, 1, ud);
                        }
                        else if(r_fd >= 0) { // destinatario connesso
                                stats(0, 0, 1, 0, 0, 0, 0, ud);
                                message_t *new = new_msg(s_msg);
                                new->hdr.op = TXT_MESSAGE;
                                setHeader(&(r_msg.hdr), OP_OK, "");
                                sendRequest(r_fd, new);
                                printf("Messaggio di {nick: %s } {fd: %d } inviato\n", s_msg->hdr.sender, fd);
                                free(new->data.buf);
                                free(new);
                        }
                        else if(r_fd == -1) {
                                setHeader(&(r_msg.hdr), OP_OK, "");
                                s_msg->hdr.op = TXT_MESSAGE;
                                stats(0, 0, 0, 1, 0, 0, 0, ud);
                                save_msg(ud, r_nick, s_msg);
                                printf("Messaggio di {nick: %s } {fd: %d } salvato\n", s_msg->hdr.sender, fd);
                        }
                }
                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                        return -1;
                }
                break;
        }

        case POSTTXTALL_OP: {
                int len = s_msg->data.hdr.len;
                conf *conf = ud->cf;

                if(len > conf->max_msg) {
                        setHeader(&(r_msg.hdr), OP_MSG_TOOLONG, ""); // Messaggio troppo lungo
                        stats(0, 0, 0, 0, 0, 0, 1, ud);
                }
                else {
                        ulist *users = NULL, *tmp, *curr_user;
                        all_user(ud, &users);

                        LL_FOREACH_SAFE(users, curr_user,tmp) {
                                if(curr_user->fd != -1) {
                                        message_t *new = new_msg(s_msg);
                                        new->hdr.op = TXT_MESSAGE;            // destinatario connesso
                                        sendRequest(curr_user->fd, new);
                                        stats(0, 0, 1, 0, 0, 0, 0, ud);
                                        printf("Messaggio di {nick: %s } {fd: %d } inviato\n", s_msg->hdr.sender, fd);
                                        free(new->data.buf);
                                        free(new);
                                }
                                else if(curr_user->fd == -1) {   // destinatario non connesso
                                        s_msg->hdr.op = TXT_MESSAGE;
                                        save_msg(ud, curr_user->nick, s_msg);
                                        stats(0, 0, 0, 1, 0, 0, 0, ud);
                                        printf("Messaggio di {nick: %s } {fd: %d } salvato\n", s_msg->hdr.sender, fd);
                                }
                        }
                        setHeader(&(r_msg.hdr), OP_OK, "");
                        del_list(&users);
                        free(users);
                }
                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                        return -1;
                }
                break;
        }
        case POSTFILE_OP: {
                message_data_t file;
                conf *conf = ud->cf;

                readData(fd, &file); // potrei in realta' spostare la lettura del file in read_task

                if(file.hdr.len > (conf->max_size * 1024)) {
                        stats(0, 0, 0, 0, 0, 0, 1, ud); //Stats
                        setHeader(&(r_msg.hdr), OP_MSG_TOOLONG, "");
                        free(file.buf);
                }
                else {
                        char *path = (char *)malloc((strlen(conf->down_path) + strlen(s_msg->data.buf) + 2) * sizeof(char)); // Controllare
                        memset(path, 0, (strlen(conf->down_path) + strlen(s_msg->data.buf) + 2) * sizeof(char));
                        strcpy(path, conf->down_path);
                        strcat(path, "/");
                        char *lastslash = strrchr(s_msg->data.buf, '/'); // Solo su unix, per windows sarebbe strrchr(s_msg->data.buf, '\\')
                        if (lastslash == NULL)
                                strncat(path, s_msg->data.buf, s_msg->data.hdr.len);
                        else {
                                lastslash++;
                                int len = s_msg->data.hdr.len - (lastslash - s_msg->data.buf);
                                strncat(path, lastslash, len);
                        }

                        printf("Upload: %s\n", path);

                        FILE *fdfile;
                        fdfile = fopen(path, "wb"); // Apro File
                        if(fdfile == NULL) {
                                perror("Errore: ");
                                free(file.buf);
                                free(path);
                                return -1;
                        }
                        int w = fwrite(file.buf, sizeof(char), file.hdr.len, fdfile); //scrivo
                        if(w != file.hdr.len) {
                                perror("Errore: ");
                                fclose(fdfile);
                                free(file.buf);
                                free(path);
                                return -1;
                        }
                        char *r_nick = s_msg->data.hdr.receiver;
                        user *r_user = find_user(ud, r_nick);
                        group *tmp = find_group(ud, r_nick);
                        user_group *curr_user, *utmp;

                        if(r_user == NULL && tmp == NULL) {
                                setHeader(&(r_msg.hdr), OP_NICK_UNKNOWN, ""); // Nick sconosciuto
                        }
                        else if(tmp != NULL) {
                                HASH_ITER(hh, tmp->group, curr_user, utmp) {
                                        if(curr_user->user != NULL) {
                                                user *dest = curr_user->user;
                                                if(dest->fd != -1) {
                                                        message_t *new = new_msg(s_msg);
                                                        new->hdr.op = FILE_MESSAGE; // destinatario connesso
                                                        sendRequest(dest->fd, new);
                                                        stats(0, 0, 0, 0, 1, 0, 0, ud);
                                                        free(new->data.buf);
                                                        free(new);
                                                }
                                                else if(dest->fd == -1) { // destinatario non connesso
                                                        stats(0, 0, 0, 0, 0, 1, 0, ud);
                                                        s_msg->hdr.op =  FILE_MESSAGE;
                                                        save_msg(ud, dest->nick, s_msg);
                                                }
                                        }
                                }
                                setHeader(&(r_msg.hdr), OP_OK, "");
                        }
                        else {
                                int r_fd = r_user->fd;
                                if(r_fd >= 0) { // destinatario connesso
                                        message_t *new = new_msg(s_msg);
                                        new->hdr.op =  FILE_MESSAGE;
                                        setHeader(&(r_msg.hdr), OP_OK, "");
                                        sendRequest(r_fd, new);
                                        stats(0, 0, 0, 0, 1, 0, 0, ud);
                                        free(new->data.buf);
                                        free(new);
                                }
                                else if(r_fd == -1) {
                                        setHeader(&(r_msg.hdr), OP_OK, "");
                                        stats(0, 0, 0, 0, 0, 1, 0, ud);
                                        s_msg->hdr.op = FILE_MESSAGE;
                                        save_msg(ud, r_nick, s_msg);
                                }
                        }
                        free(file.buf);
                        fclose(fdfile);
                        free(path);
                }
                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                        return -1;
                }
                break;
        }
        case GETFILE_OP: {
                conf *conf = ud->cf;
                char *path = (char *)malloc((strlen(conf->down_path) + strlen(s_msg->data.buf) + 2) * sizeof(char));
                memset(path, 0, (strlen(conf->down_path) + strlen(s_msg->data.buf) + 2) * sizeof(char));
                strcpy(path, conf->down_path);
                strcat(path, "/");
                strcat(path, s_msg->data.buf);
                printf("Richiesta file: %s\n", path);

                char  *mappedfile = NULL; // Come in client.c
                int fdfile = open(path, O_RDONLY);

                if (fdfile < 0) {
                        setHeader(&(r_msg.hdr), OP_NO_SUCH_FILE, ""); // errore nell'apertura del file
                        stats(0, 0, 0, 0, 0, 0, 1, ud);
                        if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                perror("Errore: ");
                                free(path);
                                close(fdfile);
                                return -1;
                        }
                }
                else {   // controllo che il file esista
                        struct stat st;
                        if (stat(path, &st)==-1) {
                                perror("stat");
                                setHeader(&(r_msg.hdr), OP_NO_SUCH_FILE, "");
                                stats(0, 0, 0, 0, 0, 0, 1, ud);
                                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                        perror("Errore: ");
                                        free(path);
                                        close(fdfile);
                                        return -1;
                                }
                        }
                        else if (!S_ISREG(st.st_mode)) {
                                setHeader(&(r_msg.hdr), OP_NO_SUCH_FILE, ""); // errore nell'apertura del file
                                stats(0, 0, 0, 0, 0, 0, 1, ud);
                                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                        perror("Errore: ");
                                        free(path);
                                        close(fdfile);
                                        return -1;
                                }
                        }
                        else {
                                int size = st.st_size;
                                mappedfile = mmap(NULL, size, PROT_READ,MAP_PRIVATE, fdfile, 0);
                                if (mappedfile == MAP_FAILED) { // stat
                                        perror("mmap");
                                        setHeader(&(r_msg.hdr), OP_NO_SUCH_FILE, ""); // errore nell'apertura del file
                                        stats(0, 0, 0, 0, 0, 0, 1, ud);
                                        if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                                perror("Errore: ");
                                                free(path);
                                                close(fdfile);
                                                return -1;
                                        }
                                }
                                else {
                                        setHeader(&(r_msg.hdr), OP_OK, "");
                                        stats(0, 0, 0, 0, 1, -1, 0, ud);
                                        if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                                perror("Errore: ");
                                                free(path);
                                                close(fdfile);
                                                return -1;
                                        }
                                        setData(&(r_msg.data), "", mappedfile, size);
                                        if(sendData(fd, &(r_msg.data)) < 0) {
                                                perror("Errore: ");
                                                free(path);
                                                close(fdfile);
                                                return -1;
                                        }
                                }
                        }
                }
                free(path);
                close(fdfile);
                break;
        }
        case GETPREVMSGS_OP: { // RICORDA: Devo eliminare i messaggi meno recenti quando raggiungo max history
                char *s_nick = s_msg->hdr.sender;
                history *user_h = find_history(ud, s_nick);
                user *curr_user = find_user(ud, s_nick);

                printf("{nick: %s} richiede history\n", s_nick);

                if(user_h == NULL) {
                        setHeader(&(r_msg.hdr), OP_FAIL, "");
                        printf("History di {nick: %s} e' vuota!\n", s_nick);
                        if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                                return -1;
                        }
                }
                else {
                        pthread_mutex_lock(ud->his_mtx);
                        history *curr_h, *tmp;
                        setHeader(&(r_msg.hdr), OP_OK, "");
                        setData(&(r_msg.data), "", (char *)&(curr_user->hc), sizeof(size_t)); // Invio numero messaggi nella history
                        if(sendRequest(fd, &r_msg) < 0) {
                                pthread_mutex_unlock(ud->his_mtx);
                                return -1;
                        }
                        LL_FOREACH_SAFE(user_h, curr_h, tmp) {
                                message_t *tmpmsg = new_msg(curr_h->data);
                                if(tmpmsg->hdr.op == TXT_MESSAGE) stats(0, 0, 1, -1, 0, 0, 0, ud); //Stats
                                else stats(0, 0, 0, 0, 1, -1, 0, ud);
                                if(sendRequest(fd, tmpmsg) < 0) {
                                        pthread_mutex_unlock(ud->his_mtx);
                                        free(tmpmsg->data.buf);
                                        free(tmpmsg);
                                        return -1;
                                }
                                free(tmpmsg->data.buf);
                                free(tmpmsg);
                        }
                        printf("History di {nick: %s} consegnata\n", s_nick);
                        pthread_mutex_unlock(ud->his_mtx);

                }
                break;
        }
        case USRLIST_OP: {
                ulist *users = NULL, *tmp, *curr_user;
                all_user(ud, &users);
                pthread_mutex_lock(ud->mtx);
                char *buffer = malloc(ud->connessi * (MAX_NAME_LENGTH + 1) *sizeof(char));
                memset(buffer, 0, ud->connessi * (MAX_NAME_LENGTH + 1) *sizeof(char));
                char *ptr = buffer;

                LL_FOREACH_SAFE(users, curr_user,tmp) {
                        if(curr_user->fd != -1) {
                                strncpy(ptr, curr_user->nick, MAX_NAME_LENGTH + 1);
                                ptr += MAX_NAME_LENGTH + 1;
                        }
                }

                setHeader(&(r_msg.hdr), OP_OK, "");
                setData(&(r_msg.data), "", buffer, ud->connessi  * (MAX_NAME_LENGTH + 1));
                pthread_mutex_unlock(ud->mtx);
                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                        del_list(&users);
                        free(users);
                        free(buffer);
                        return -1;
                }
                if(sendData(fd, &(r_msg.data)) < 0) {
                        del_list(&users);
                        free(users);
                        free(buffer);
                        return -1;
                }

                del_list(&users);
                free(users);
                free(buffer);
                break;
        }
        case UNREGISTER_OP: {

                int epollfd = tsk->epollfd;
                pthread_mutex_t *mtx_poll = tsk->mtx_poll;

                int delfd = 0;
                if((delfd = del_user(ud, s_msg->hdr.sender)) < 0) {
                        setHeader(&(r_msg.hdr), OP_NICK_UNKNOWN, "");
                }
                else {
                        stats(-1, -1, 0, 0, 0, 0, 0, ud); //stats
                        setHeader(&(r_msg.hdr), OP_OK, "");
                }

                if(sendHeader(fd, &(r_msg.hdr)) < 0) return -1;
                if(delfd > 0) {
                        pthread_mutex_lock(mtx_poll);
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, delfd, NULL);
                        pthread_mutex_unlock(mtx_poll);
                }
                printf("Eliminazione utente {nick: %s } {fd: %d } completata!\n", s_msg->hdr.sender, fd);
                break;
        }
        case DISCONNECT_OP: {
                int epollfd = tsk->epollfd;
                pthread_mutex_t *mtx_poll = tsk->mtx_poll;

                int delfd = 0;
                if((delfd = logout_user(ud, s_msg->hdr.sender)) < 0) {
                        setHeader(&(r_msg.hdr), OP_NICK_UNKNOWN, "");
                }
                else {
                        stats(0, -1, 0, 0, 0, 0, 0, ud); //stats
                        setHeader(&(r_msg.hdr), OP_OK, "");
                }

                if(sendHeader(fd, &(r_msg.hdr)) < 0) return -1;
                if(delfd > 0) {
                        pthread_mutex_lock(mtx_poll);
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, delfd, NULL);
                        pthread_mutex_unlock(mtx_poll);
                }
                printf("Disconnessione utente {nick: %s } {fd: %d } completata!\n", s_msg->hdr.sender, fd);
                break;
        }
        case CREATEGROUP_OP: {
                int ret = 0;
                printf("DEBUG\n");
                if(find_user(ud, s_msg->hdr.sender) == NULL) { // COntrollo se utente esiste
                        setHeader(&(r_msg.hdr), OP_NICK_UNKNOWN, "");
                        ret = -1;
                }
                else if(find_user(ud, s_msg->data.hdr.receiver) != NULL) {//controllo che non ci sia utente con nome del gruppo da creare
                        setHeader(&(r_msg.hdr), OP_NICK_ALREADY, "");
                        ret = -1;
                }
                else if(find_group(ud, s_msg->data.hdr.receiver) != NULL) {
                        setHeader(&(r_msg.hdr), OP_NICK_ALREADY, "");
                        ret = -1;
                }
                else {
                        if(add_group(ud, s_msg->data.hdr.receiver) == 0) setHeader(&(r_msg.hdr), OP_OK, "");
                        else {
                                setHeader(&(r_msg.hdr), OP_FAIL, "");
                                ret = -1;
                        }
                        printf("Nuovo gruppo creato: %s\n", s_msg->data.hdr.receiver);
                        group *tmp = find_group(ud, s_msg->data.hdr.receiver);
                        if(tmp != NULL) {
                                user *new = find_user(ud, s_msg->hdr.sender);
                                if(reg_user_group(tmp, new) != 0) {
                                        setHeader(&(r_msg.hdr), OP_FAIL, "");
                                        ret = -1;
                                }
                                else setHeader(&(r_msg.hdr), OP_OK, "");
                        }
                }
                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                        return -1;
                }
                if(ret == -1) return -1;
                break;
        }
        case ADDGROUP_OP: {
                user *utmp = NULL;
                int ret = 0;
                if((utmp = find_user(ud, s_msg->hdr.sender)) == NULL) { // COntrollo se utente esiste
                        setHeader(&(r_msg.hdr), OP_NICK_UNKNOWN, "");
                        ret = -1;
                }
                else {
                        group *tmp = find_group(ud, s_msg->data.hdr.receiver);
                        if(tmp != NULL) {
                                if(reg_user_group(tmp, utmp) != 0) {
                                        setHeader(&(r_msg.hdr), OP_FAIL, "");
                                        ret = -1;
                                }
                                else setHeader(&(r_msg.hdr), OP_OK, "");
                        }
                }
                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                        return -1;
                }
                if(ret == -1) return -1;
                break;
        }
        case DELGROUP_OP: {
                int ret = 0;
                user *utmp = NULL;
                if((utmp = find_user(ud, s_msg->hdr.sender)) == NULL) { // Controllo se utente esiste
                        setHeader(&(r_msg.hdr), OP_NICK_UNKNOWN, "");
                        ret = -1;
                }
                else {
                        group *tmp = find_group(ud, s_msg->data.hdr.receiver);
                        if(tmp != NULL) {
                                if(del_user_group(tmp, utmp) != 0) {
                                        setHeader(&(r_msg.hdr), OP_FAIL, "");
                                        ret = -1;
                                }
                                else setHeader(&(r_msg.hdr), OP_OK, "");
                        }
                }
                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                        return -1;
                }
                if(ret == -1) return -1;
                break;
        }
        default: {
                setHeader(&(r_msg.hdr), OP_FAIL, "");
                if(sendHeader(fd, &(r_msg.hdr)) < 0) {
                        return -1;
                }
        } break;
        }
        return 0;
}
