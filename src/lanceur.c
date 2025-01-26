#include "../include/file_synchronisee.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/wait.h>

#define TUBE_SIZE 19

/**
 * commande_handler(x) : Prend en entrée un pointeur vers une commande et 
 * exécute la commande.
*/
void * commande_handler(void * x);
/**
 * commande_waiter(x) : Prend en entrée un pointeur vers une structure de type
 * commande_waiter_args, créer un thread inactivity_handler et attend qu'une 
 * commande soit ajoutée à la file. Si une commande est ajouté à la file, la 
 * fonction créer un thread commande_handler et lui passe la commande en 
 * paramètre puis recommence à attendre une commande.
*/
void * commande_waiter(void * x);
/**
 * inactivity_handler(x) : Prend en entrée un pointeur vers une structure de 
 * type inactivity_handler_args et attend que le sémaphore s soit posté. Si le 
 * sémaphore est posté, la fonction attend delay secondes puis vérifie si le 
 * sémaphore est toujours à 0. Si le sémaphore est à 0, libère les ressources 
 * partagées et termine le processus.
*/
void * inactivity_handler(void * x);

typedef struct inactivity_handler_args{
    sem_t * s;
    int shm;
    file_synchronisee * file;
    int delay;
    pthread_t commande_waiter_thread;
} inactivity_handler_args;

typedef struct commande_waiter_args{
    int shm;
    file_synchronisee * file;
    pthread_t * inactivity_thread;
    sem_t * s;
    int delay;
} commande_waiter_args;

int main(int argc, char ** argv){
    int delay_inactivity;
    if(argc == 1){
        delay_inactivity = 0;
    } else if(argc == 3){
        if(strcmp(argv[1], "-d") != 0){
            fprintf(stderr, "usage : %s -d delay\n", argv[0]);
            return EXIT_FAILURE;
        }
        if(strtol(argv[2], NULL, 10) == 0){
            fprintf(stderr, "delay must be a strict positive integer\n");
            fprintf(stderr, "usage : %s -d delay\n", argv[0]);
            return EXIT_FAILURE;
        }
        delay_inactivity = atoi(argv[2]);
        if(delay_inactivity <= 0){
            fprintf(stderr, "delay must be a strict positive integer\n");
            fprintf(stderr, "usage : %s -d delay\n", argv[0]);
            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "usage : %s -d delay\n", argv[0]);
        return EXIT_FAILURE;
    }


    int shm = shm_open(SHM_NAME, O_RDWR | O_CREAT,
        S_IRUSR | S_IWUSR);
    if(shm == -1){
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    
    if(ftruncate(shm, sizeof(file_synchronisee)) == -1){
        close(shm);
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }
    file_synchronisee * file = mmap(NULL, sizeof(file_synchronisee), 
        PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);

    if(file == MAP_FAILED){
        close(shm);
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    file_init(file);

    pthread_t commande_waiter_thread;
    pthread_t inactivity_handler_thread;
    sem_t inactivity;

    if(sem_init(&inactivity, 0, 0) == -1){
        goto shm_unlink_shm_name;
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    commande_waiter_args c_args ={
        .shm = shm,
        .file = file,
        .inactivity_thread = &inactivity_handler_thread,
        .delay = delay_inactivity,
        .s = &inactivity,
    };

    if(pthread_create(&commande_waiter_thread, NULL, commande_waiter, (void *)&c_args) == -1){
        goto sem_destroy_inactivity;
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    int c;
    printf("Entrez q pour quitter\n");
    while((c = getchar()) != EOF && c != 'q'){}

    if(pthread_cancel(commande_waiter_thread) == -1){
        perror("pthread_cancel");
        if(delay_inactivity != 0){
            goto pthread_cancel_inactivity_handler_thread;
        }
    }
    if(delay_inactivity != 0 
        && pthread_cancel(inactivity_handler_thread) == -1){
        perror("pthread_cancel");
        goto sem_destroy_inactivity;
    }
    if(sem_destroy(&inactivity) == -1){
        perror("sem_destroy");
        goto shm_unlink_shm_name;
    }
    if(shm_unlink(SHM_NAME) == -1){
        perror("shm_unlink");
        goto close_shm;
    }
    if(close(shm) == -1){
        perror("close");
        goto file_kill_all;
    }
    file_kill_all(file);
    file_destroy(file);
    if(munmap(file, sizeof(file_synchronisee)) == -1){
        perror("munmap");
        goto exit;
    }

    return EXIT_SUCCESS;

    pthread_cancel_inactivity_handler_thread:
        pthread_cancel(inactivity_handler_thread);
    sem_destroy_inactivity:
        sem_destroy(&inactivity);
    shm_unlink_shm_name:
        shm_unlink(SHM_NAME);
    close_shm:
        close(shm);
    file_kill_all:
        file_kill_all(file);
    file_destroy(file);
    munmap(file, sizeof(file_synchronisee));
    exit : 
        return EXIT_FAILURE;
}

void * commande_handler(void * x){
    pthread_detach(pthread_self());

    commande * c = (commande *)x;
    char tube_out_name[TUBE_SIZE];
    char tube_err_name[TUBE_SIZE];
    char tube_in_name[TUBE_SIZE];

    if(snprintf(tube_out_name, TUBE_SIZE, "%s%d", PREF_OUT_TUBE, c->pid) == -1){
        perror("snprintf");
        exit(EXIT_FAILURE);
    }
    if(snprintf(tube_err_name, TUBE_SIZE, "%s%d", PREF_ERR_TUBE, c->pid) == -1){
        perror("snprintf");
        exit(EXIT_FAILURE);
    }
    if(snprintf(tube_in_name, TUBE_SIZE, "%s%d", PREF_IN_TUBE, c->pid) == -1){
        perror("snprintf");
        exit(EXIT_FAILURE);
    }

    int tube_out = open(tube_out_name, O_WRONLY);
    if(tube_out == -1){
        perror("open");
        exit(EXIT_FAILURE);
    }
    int tube_err;
    if((tube_err = open(tube_err_name, O_WRONLY)) == -1){
        close(tube_out);
        perror("open");
        exit(EXIT_FAILURE);
    }
    int tube_in;
    if((tube_in = open(tube_in_name, O_RDONLY)) == -1){
        close(tube_out);
        close(tube_err);
        perror("open");
        exit(EXIT_FAILURE);
    }
    char spaces [MAX_TAILLE_COMMANDES][MAX_TAILLE_COMMANDES];
    char actualChar = c->commande[0];
    int space_count = 0;
    int i = 0;
    int j = 0;
    while(actualChar != '\0'){
        while(actualChar == ' '){
            i++;
            actualChar = c->commande[i];
        }
        if(actualChar != '\0' && actualChar != '"'){
            while(actualChar != ' ' && actualChar != '\0'){
                spaces[space_count][j] = actualChar;
                j++;
                i++;
                actualChar = c->commande[i];
            }
            spaces[space_count][j] = '\0';
            space_count++;
            j = 0;
        }
        if(actualChar != '\0' && actualChar == '"'){
            i++;
            actualChar = c->commande[i];
            while(actualChar != '"' && actualChar != '\0'){
                spaces[space_count][j] = actualChar;
                j++;
                i++;
                actualChar = c->commande[i];
            }
            spaces[space_count][j] = '\0';
            space_count++;
            j = 0;
            if(actualChar != '\0'){
                i++;
                actualChar = c->commande[i];
            }
        }
    }
    j = 0;
    char args[space_count - 1][MAX_TAILLE_COMMANDES];
    int arg_count = 0;
    int pipes[space_count - 1][2];
    int pipe_count = 0;
    char exec[MAX_TAILLE_COMMANDES];

    for(int i = 0; i < space_count; i++){
        if(i == j){
            if(pipe(pipes[pipe_count]) == -1){
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            pipe_count++;
            strcpy(exec, spaces[i]);
        } 
        
        if ((i != j && (spaces[i][0] == '|' || i == space_count - 1)) ||
            (i == j && (i == space_count -1))){
            if(i == space_count - 1 && i != j){
                strcpy(args[arg_count], spaces[i]);
                arg_count++;
            }
            j = i + 1;

            switch(fork()){
                case -1 :
                    perror("fork");
                    exit(EXIT_FAILURE);
                case 0 :
                    if(dup2(tube_err, STDERR_FILENO) == -1){
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    if(i != space_count - 1){
                        if(pipe_count != 1){
                            if(dup2(pipes[pipe_count - 2][0], STDIN_FILENO) == -1){
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }
                            if(dup2(pipes[pipe_count - 1][1], STDOUT_FILENO) == -1){
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }
                        } else {
                            if(dup2(pipes[pipe_count - 1][1], STDOUT_FILENO) == -1){
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }
                            if(dup2(tube_in, STDIN_FILENO) == -1){
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }
                        }
                    } else {
                        if(pipe_count != 1){
                            if(dup2(pipes[pipe_count - 2][0], STDIN_FILENO) == -1){
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }
                            if(dup2(tube_out, STDOUT_FILENO) == -1){
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }
                        } else {
                            if(dup2(tube_out, STDOUT_FILENO) == -1){
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }
                            if(dup2(tube_in, STDIN_FILENO) == -1){
                                perror("dup2");
                                exit(EXIT_FAILURE);
                            }
                        }
                    }
                    if(arg_count != 0){
                        execlp(exec, exec, args, NULL);
                    } else {
                        execlp(exec, exec, NULL);
                    }
                    perror("execlp");
                    exit(EXIT_FAILURE);
                default :
                    if(wait(NULL) == -1){
                        perror("wait");
                        exit(EXIT_FAILURE);
                    }
                    if(i != space_count - 1){
                        if(pipe_count != 1){
                            if(close(pipes[pipe_count - 2][0]) == -1){
                                perror("close");
                                exit(EXIT_FAILURE);
                            }
                            if(close(pipes[pipe_count - 1][1]) == -1){
                                perror("close");
                                exit(EXIT_FAILURE);
                            }
                        } else {
                            if(close(pipes[pipe_count - 1][1]) == -1){
                                perror("close");
                                exit(EXIT_FAILURE);
                            }
                        }
                    } else {
                        if(pipe_count != 1){
                            if(close(pipes[pipe_count - 2][0]) == -1){
                                perror("close");
                                exit(EXIT_FAILURE);
                            }
                        }
                    }
                    break;
            }
            arg_count = 0;
        } else if(i > j){
            strcpy(args[arg_count], spaces[i]);
            arg_count++;
        }
    }

    if(close(tube_out) == -1){
        close(tube_err);
        perror("close");
        exit(EXIT_FAILURE);
    } 
    if(close(tube_err) == -1){
        perror("close");
        exit(EXIT_FAILURE);
    }
    if(close(tube_in) == -1){
        perror("close");
        exit(EXIT_FAILURE);
    }
    free(c);
    return NULL;
}

void * inactivity_handler(void * x){
    pthread_detach(pthread_self());
    inactivity_handler_args * s = (inactivity_handler_args *)x;
    int i = 1;
    while(i != 0){
        sem_wait(s->s);
        sleep((unsigned int)s->delay);
        if(sem_getvalue(s->s, &i) == -1){
            perror("sem_getvalue");
            exit(EXIT_FAILURE);
        }
    }

    if(shm_unlink(SHM_NAME) == -1){
        perror("shm_unlink");
        exit(EXIT_FAILURE);
    }

    if(close(s->shm) == -1){
        perror("close");
        exit(EXIT_FAILURE);
    }

    file_kill_all(s->file);
    file_destroy(s->file);

    if(munmap(s->file, sizeof(file_synchronisee)) == -1){
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
    return NULL;
}

void * commande_waiter(void * x){
    pthread_detach(pthread_self());
    commande_waiter_args * cw = (commande_waiter_args *)x;
    inactivity_handler_args i_args = {
        .s = cw->s,
        .shm = cw->shm,
        .file = cw->file,
        .delay = cw->delay,
        .commande_waiter_thread = pthread_self()
    };
    if(cw->delay != 0){
        if(pthread_create(cw->inactivity_thread, NULL, inactivity_handler, (void *)&i_args) == -1){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    while(1){
        int i;
        if(sem_getvalue(cw->s, &i) == -1){
            perror("sem_getvalue");
            exit(EXIT_FAILURE);
        }
        if(i == 0){
            sem_post(cw->s);
        }
        commande *c = file_defiler(cw->file);
        if(c == NULL){
            perror("file_defiler");
            exit(EXIT_FAILURE);
        }
        if(sem_getvalue(cw->s, &i) == -1){
            perror("sem_getvalue");
            exit(EXIT_FAILURE);
        }
        if(i == 0){
            sem_post(cw->s);
        }
        pthread_t t;
        if(pthread_create(&t, NULL, commande_handler, (void *)c) == -1){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
}