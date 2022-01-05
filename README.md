# Projet FabLab

Le projet que nous avons construit est composé de :
  - Un Arduino UNO
  - Un module ESP8266 WIFI
  - Un capteur IR de mouvement
  - Un capteur son
  - Un ruban 30 LEDs
  - Un lecteur de badge RFID

Ce circuit est capable d'être connecté au WIFI pour communiquer avec un broker MQTT afin de lui envoyer les informations sur son état, c'est à dire si le circuit est en mode automatique ou manuel, ou savoir si une présence a été détectée par l'arduino.
Le changement de mode de détection est fait grace au passage d'un badge authorisé devant le lecteur de badge, alors se déclanche une animation sur le bandeau LED pour indiquer le changement d'état.
Lorsque le circuit est en automatique, il détecte les mouvement grace au capteur IR et écoute si il n'y a pas de mouvement pour savoir si il y a une présence vocale. Si le circuit détecte une présence alors il envoie sur un serveur MQTT qu'une présence a été détéctée, et une fois qu'elle n'est plus détéctée alors le circuit envoie qu'il n'y a plus personne.
Lorsque le circuit est en manuel, il n'y a plus aucune détection des capteurs, le module envoie au serveur MQTT qu'il est en manuel et attend de repasser en automatique au passage d'un badge.

Si le circuit ne parvient pas à se connecter au WIFI il peut tout de même fonctionner, il n'y aura juste pas la fonctionnalité MQTT


Code écrit par l'équipe : Madoulaud Capucine, Jorda Marion, Mattias Désiré, Massacry Audran, Jacquet Léopold et Rey Florian
