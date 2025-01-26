# Description

Ce projet est composé de deux exécutables : 
- un client
- un lanceur

Le client écoute toutes les entrées de l'utilisateur (stdin). Pour chaque commande, le client envoie au lanceur via une file synchronisée la commande. Cette commande sera ensuite exécutée par le lanceur. Le résultat de la commande sur la sortie standard (stdout) 
et la sortie d'erreur (stderr) seront renvoyées au client qui affichera le résultat.

# But 

Ce projet est un projet d'étude destiné à manipuler les mutex, les tubes, les files synchronisés ou encore les interruptions dans le langage C.
