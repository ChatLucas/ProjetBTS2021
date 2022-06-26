#include <stdio.h>
#include <mosquitto.h>

#define MOSQUITTO struct mosquitto
#define MOSQUITTO_MESSAGE const struct mosquitto_message

#define PORT_SERVEUR_MQTT 1883
#define ADRESSE_IP_SERVEUR_MQTT "172.16.17.1"
#define KEEPALIVE 0

#define USER_MQTT "admin"
#define PASSWORD_MQTT "bonjour"

void my_message_callback(MOSQUITTO *mosq, void *userdata, MOSQUITTO_MESSAGE *message);

int main(int argc, char *argv[]) {
	struct       mosquitto *clientMosquitto;

	// Initialiser la librairie
	mosquitto_lib_init();

	// Créer un client mosquitto
	clientMosquitto = mosquitto_new(NULL, true, NULL);
	if (clientMosquitto == NULL) {
		perror("mosquitto_new()");return -1;
	}

	// Définir le callback de réception des messages
	mosquitto_message_callback_set(clientMosquitto, my_message_callback);

	// Définir l'utilisateur et le mot de passe
	mosquitto_username_pw_set(clientMosquitto, USER_MQTT, PASSWORD_MQTT);

	// Effectuer la connexion
	if (mosquitto_connect(clientMosquitto, ADRESSE_IP_SERVEUR_MQTT, PORT_SERVEUR_MQTT, KEEPALIVE) != MOSQ_ERR_SUCCESS) {
		perror("mosquitto_new()");return -1;
	}

	// S'abonner au topic
	mosquitto_subscribe(clientMosquitto, NULL, "#", 0);

	// Boucle infinie
	mosquitto_loop_forever(clientMosquitto, -1, 1);

	mosquitto_destroy(clientMosquitto);
	mosquitto_lib_cleanup();

	return 0;
}

void my_message_callback(MOSQUITTO *mosq, void *userdata, MOSQUITTO_MESSAGE *message) {
	if(message->payloadlen){
		printf("(%s) %s\n", message->topic, message->payload);
	} else {
		printf("%s (null)\n", message->topic);
	}
	fflush(stdout);
}