#include "../include/file_synchronisee.h"
#include <signal.h>

void file_init(file_synchronisee *file){
    file->last = 0;
    if(sem_init(&file->mutex, 1, 1) == -1){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&file->vide, 1, MAX_COMMANDES) == -1){
        sem_destroy(&file->mutex);
        perror("sem_init");
        exit(EXIT_FAILURE);

    }
    if(sem_init(&file->plein, 1, 0) == -1){
        sem_destroy(&file->mutex);
        sem_destroy(&file->vide);
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
}

void file_destroy(file_synchronisee *file){
    sem_wait(&file->mutex);
    if(sem_destroy(&file->mutex) == -1){
        perror("sem_destroy");
        exit(EXIT_FAILURE);
    }
    if(sem_destroy(&file->vide) == -1){
        perror("sem_destroy");
        exit(EXIT_FAILURE);
    }
    if(sem_destroy(&file->plein) == -1){
        perror("sem_destroy");
        exit(EXIT_FAILURE);
    }
}

void file_enfiler(file_synchronisee *file, commande c){
    sem_wait(&file->vide);
    sem_wait(&file->mutex);

    int i;
    if(sem_getvalue(&file->plein, &i) == -1){
        perror("sem_getvalue");
        exit(EXIT_FAILURE);
    }

    strcpy(file->commande[(i + file->last) % MAX_COMMANDES], c.commande);
    file->pid[(i + file->last) % MAX_COMMANDES] = c.pid;

    sem_post(&file->mutex);
    sem_post(&file->plein);
}

commande * file_defiler(file_synchronisee *file){
    sem_wait(&file->plein);
    sem_wait(&file->mutex);

    int i;
    if(sem_getvalue(&file->plein, &i) == -1){
        perror("sem_getvalue");
        exit(EXIT_FAILURE);
    }
    commande *c = malloc(sizeof(commande));
    strcpy(c->commande, file->commande[file->last]);
    c->pid = file->pid[file->last];
    file->last = (file->last + 1) % MAX_COMMANDES;

    sem_post(&file->mutex);
    sem_post(&file->vide);
    return c;
}

void file_kill_all(file_synchronisee *file){
    sem_wait(&file->mutex);
    int i;
    if(sem_getvalue(&file->plein, &i) == -1){
        perror("sem_getvalue");
        exit(EXIT_FAILURE);
    }
    for(int j = 0; j < i; j++){
        kill(file->pid[(j + file->last) % MAX_COMMANDES], SIGKILL);
    }
    sem_post(&file->mutex);
}

