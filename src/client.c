#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h> 
#include "../include/file_synchronisee.h"

#define TUBE_SIZE 19

volatile sig_atomic_t sigpipe_received = 0;

/**
 * commande_handler(args) : Prend en entrée un pointeur vers une structure
 * commande_handler_args et enfile la commande contenue dans la structure puis
 * lis les tubes de sortie et d'erreur de la commande.
*/
void * commande_handler(void * x);

/**
 * sigpipe_handler(signum) : Prend en entrée un entier signum et met à jour la 
 * variable sigpipe_received si signum est SIGPIPE.
*/
void sigpipe_handler(int signum);

typedef struct commande_handler_args{
    file_synchronisee * file;
    commande cmd;
    char * tube_out_name;
    char * tube_err_name;
    char * tube_in_name;
} commande_handler_args;

typedef struct reader_args{
    int fd;
} reader_args;

int main(int argc, char ** argv){
    if(argc <= 1){
        fprintf(stderr, "usage : %s cmd1 | cmd2 | ...\n", argv[0]);
        exit(EXIT_FAILURE);
    } 
    char str[MAX_TAILLE_COMMANDES];
    int count = 0;
    int is_string = 0;

    struct sigaction action;
    action.sa_handler = sigpipe_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if(sigaction(SIGPIPE, &action, NULL) == -1){
        perror("sigaction");
        exit(EXIT_FAILURE);
    }


    for(int i = 1; i < argc && count < MAX_TAILLE_COMMANDES - 1; i++){
        for(int j = 0; (size_t)j < strlen(argv[i]); j++){
            if(argv[i][j] == ' '){
                is_string = 1;
            }
        }
        if(is_string){
            str[count] = '"';
            count++;
        }
        for(int j = 0; (size_t)j < strlen(argv[i]); j++){
            str[count] = argv[i][j];
            count++;
        }
        if(is_string){
            str[count] = '"';
            count++;
            is_string = 0;
        } 
        str[count] = ' ';   
        count++;
    }
    str[count] = '\0';

    int shm = shm_open(SHM_NAME, O_RDWR, S_IRUSR | S_IWUSR);
    if(shm == -1){
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    file_synchronisee * file = mmap(NULL, sizeof(file_synchronisee), PROT_READ 
    | PROT_WRITE, MAP_SHARED, shm, 0);

    if(file == MAP_FAILED){
        close(shm);
        perror("mmap");
        exit(EXIT_FAILURE);
    } 

    pid_t pid = getpid();
    char tube_in_name[TUBE_SIZE];
    char tube_out_name[TUBE_SIZE];
    char tube_err_name[TUBE_SIZE];
    if(snprintf(tube_in_name, TUBE_SIZE, "%s%d", PREF_IN_TUBE, pid) == -1){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        perror("snprintf");
        exit(EXIT_FAILURE);
    }
    if(snprintf(tube_out_name, TUBE_SIZE, "%s%d", PREF_OUT_TUBE, pid) == -1){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        perror("snprintf");
        exit(EXIT_FAILURE);
    }
    if(snprintf(tube_err_name, TUBE_SIZE, "%s%d", PREF_ERR_TUBE, pid) == -1){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        perror("snprintf");
        exit(EXIT_FAILURE);
    }
    if(mkfifo(tube_out_name, S_IRUSR | S_IWUSR) == -1){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    if(mkfifo(tube_err_name, S_IRUSR | S_IWUSR) == -1){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        unlink(tube_out_name);
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    if(mkfifo(tube_in_name, S_IRUSR | S_IWUSR) == -1){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        unlink(tube_out_name);
        unlink(tube_err_name);
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    pthread_t t;
    commande cmd;
    strcpy(cmd.commande, str);
    cmd.pid = pid;

    commande_handler_args args = {
        .file = file,
        .cmd = cmd,
        .tube_out_name = tube_out_name,
        .tube_err_name = tube_err_name,
        .tube_in_name = tube_in_name
    };

    if(pthread_create(&t, NULL, commande_handler, &args) != 0){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        unlink(tube_out_name);
        unlink(tube_err_name);
        unlink(tube_in_name);
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    int tube_in;
    if((tube_in = open(tube_in_name, O_WRONLY)) == -1){
        perror("open");
        exit(EXIT_FAILURE);
    }
    int c;
    while((c = getchar()) != EOF){
        if(write(tube_in, &c, 1) == -1){
            if(sigpipe_received){
                break;
            }
            perror("write");
            exit(EXIT_FAILURE);
        }
    }

    if(close(tube_in) == -1){
        perror("close");
        exit(EXIT_FAILURE);
    }

    if(unlink(tube_out_name) == -1){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        unlink(tube_err_name);
        perror("unlink");
        exit(EXIT_FAILURE);
    }

    if(unlink(tube_err_name) == -1){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        perror("unlink");
        exit(EXIT_FAILURE);
    }
    if(unlink(tube_in_name) == -1){
        munmap(file, sizeof(file_synchronisee));
        close(shm);
        perror("unlink");
        exit(EXIT_FAILURE);
    }

    if(munmap(file, sizeof(file_synchronisee)) == -1){
        close(shm);
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    if(close(shm) == -1){
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}


void * commande_handler(void * x){
    pthread_detach(pthread_self());
    commande_handler_args * args = (commande_handler_args *) x;
    file_enfiler(args->file, args->cmd);
    int tube_out;
    if((tube_out = open(args->tube_out_name, O_RDONLY)) == -1){
        perror("open");
        exit(EXIT_FAILURE);
    }
    int tube_err;
    if((tube_err = open(args->tube_err_name, O_RDONLY)) == -1){
        close(tube_out);
        perror("open");
        exit(EXIT_FAILURE);
    }

    int c;
    while(read(tube_out, &c, 1) != 0){
        putchar(c);
    }
    while(read(tube_err, &c, 1) != 0){
        if(write(STDERR_FILENO, &c, 1) == -1){
            perror("write");
            exit(EXIT_FAILURE);
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
    return NULL;
}


void sigpipe_handler(int signum){
    if(signum == SIGPIPE){
        sigpipe_received = 1;
    }
}