#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>

#define TAILLE_MAX_TUBE 128
#define MAX_TAILLE_COMMANDES 1024
#define MAX_COMMANDES 10
#define MAX_CLIENT 10
#define PREF_ERR_TUBE "/tmp/tubeerr"
#define PREF_OUT_TUBE "/tmp/tubeout"
#define PREF_IN_TUBE "/tmp/tubein"
#define SHM_NAME "/shm_name"


typedef struct commande{
    char commande[MAX_TAILLE_COMMANDES];
    pid_t pid;
} commande;

typedef struct {
    char commande[MAX_COMMANDES][MAX_TAILLE_COMMANDES];
    pid_t pid[MAX_COMMANDES];
    int last;
    sem_t mutex;
    sem_t vide;
    sem_t plein; 
} file_synchronisee;

/**
 * file_init(file) : Prend en entrée un pointeur vers une file synchronisée et
 * initialise les sémaphores de la file.
*/
extern void file_init(file_synchronisee *file);
/**
 * file_destroy(file) : Prend en entrée un pointeur vers une file synchronisée
 * déjà initialisée et détruit les sémaphores de la file.
*/
extern void file_destroy(file_synchronisee *file);

/**
 * file_enfiler(file, c) : Prend en entrée un pointeur vers une file 
 * synchronisée déjà initialisée et une commande. La fonction ajoute la 
 * commande à la file. Si il n'y a pas de place dans la file, la fonction 
 * attend qu'une place se libère.
*/
extern void file_enfiler(file_synchronisee *file, commande c);

/**
 * file_defiler(file) : Prend en entrée un pointeur vers une file synchronisée
 * déjà initialisée et retourne un pointeur vers la commande en tête de file. Si
 * la file est vide, la fonction attend qu'une commande soit ajoutée à la file.
*/
extern commande * file_defiler(file_synchronisee *file);

/**
 * file_kill_all(file) : Prend en entrée un pointeur vers une file synchronisée
 * déjà initialisée et tue tous les processus en attente dans la file.
*/
extern void file_kill_all(file_synchronisee *file);