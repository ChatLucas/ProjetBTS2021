#include <sys/types.h>	// pour open()
#include <sys/stat.h>	// pour open()
#include <fcntl.h>		// pour open()
#include <unistd.h>		// pour close(), read()
#include <stdio.h>
#include <stdlib.h>
#include <mosquitto.h>
#include <string.h>
#include <mysql.h>
#include <time.h>

#define MOSQUITTO struct mosquitto
#define MOSQUITTO_MESSAGE const struct mosquitto_message

#define PORT_SERVEUR_MQTT 1883
#define ADRESSE_IP_SERVEUR_MQTT "172.16.17.1"
#define KEEPALIVE 0

#define USER_MQTT "admin"
#define PASSWORD_MQTT "bonjour"

#define HOTE "localhost"
#define NOM_BASE "domotique"
#define USER "adminBaseMaison"
#define PASSWORD "bonjour"

struct modules {
	char nom[20];
	int valeur;
	char type[20];
	char zone[20];
};

void my_message_callback(MOSQUITTO *mosq, void *userdata, MOSQUITTO_MESSAGE *message);
int mettreAJourEvenement(MYSQL *pMysql, struct modules module, MOSQUITTO *clientMosquitto);
int insererRequetteMySQL(MYSQL *pMysql, struct modules module);
int modifierValeurMySQL(MYSQL *pMysql, struct modules module);
int moduleDejaPresent(MYSQL *pMysql, struct modules module);
int gererSecurite(MYSQL *pMysql, struct modules module, MOSQUITTO *clientMosquitto);

int main(int argc, char *argv[]) {
	struct modules module;
	MOSQUITTO *clientMosquitto;
	MYSQL *pMysql;
	char messageFin[60];
	char message[60];
	char trame[30];
	char topique[20];
	char characterLu;
	int tailleTrame = 0;
	int tailleTrameMax = 100;
	int ttyS0;
	int boucle;
	int appareilID;
	int offset;
	int premiereTrame = 1;
	int nombreDecharacterLu;
	
	/* Initialiser la structure MySQL */
	if ((pMysql = mysql_init(NULL)) == NULL) {
		fprintf(stderr, "mysql_init(): %s\n", mysql_error(pMysql));
		return -1;
	}
	
	/* Se connecter a base de donnees */
	if (mysql_real_connect(pMysql, HOTE, USER, PASSWORD, NOM_BASE, 0, NULL, 0) == NULL) {
		fprintf(stderr, "mysql_real_connect(): %s\n", mysql_error(pMysql));
		mysql_close(pMysql);
		return -1;
	}
	
	// Initialiser la librairie
	mosquitto_lib_init();
	
	// Créer un client mosquitto
	clientMosquitto = mosquitto_new(NULL, true, NULL);
	if (clientMosquitto == NULL) {
		perror("mosquitto_new()");
		return -1;
	}
	
	// Définir le callback de réception des messages
	mosquitto_message_callback_set(clientMosquitto, my_message_callback);
	
	// Définir l'utilisateur et le mot de passe
	mosquitto_username_pw_set(clientMosquitto, USER_MQTT, PASSWORD_MQTT);
	
	// Effectuer la connexion
	if (mosquitto_connect(clientMosquitto, ADRESSE_IP_SERVEUR_MQTT, PORT_SERVEUR_MQTT, KEEPALIVE) != MOSQ_ERR_SUCCESS) {
		perror("mosquitto_new()");
		return -1;
	}
	
	ttyS0 = open("/dev/ttyS0", O_RDONLY);
	
	if (ttyS0 == -1) {
		perror("open");
		return -1;
	}
	
	while (1) {
		read(ttyS0, &characterLu, sizeof(characterLu));
		if (tailleTrame == 5) {
			tailleTrameMax = 7 + ((trame[1]<<8) + trame[2]) + trame[3];
		}
		premiereTrame = 0;
		trame[tailleTrame] = characterLu;
		tailleTrame++;
		if (tailleTrame >= tailleTrameMax) {
			offset = ((trame[1]<<8) + trame[2]) + 1;
			strcpy(message, "");
			appareilID = (trame[offset] << 24) + (trame[offset+1] << 16) + (trame[offset+2] << 8) + trame[offset+3];
			sprintf(message, "ID : %08X, Ty : %02X, ", appareilID, trame[6]);
			sprintf(module.nom, "%08X", appareilID);
			switch (trame[6]) {
				case 0xA5 : 
					//si learn
					if ((trame[10] & 0x08) == 0x00) {
						sprintf(messageFin, "Le : 1, Fo : %d, Ty : %d", (trame[7] >> 2), (((trame[7] & 0x03) << 5) | trame[8]) >> 3);
						if ((trame[7] >> 2) == 2) {
							sprintf(module.type, "Temperature");
						} else {
							sprintf(module.type, "Presence");
						}
					} else {
						sprintf(messageFin, "Le : 0, Da : %08X", (trame[7]<<24) + (trame[8]<<16) + (trame[9]<<8) + trame[10]);
						module.valeur = (trame[7]<<24) + (trame[8]<<16) + (trame[9]<<8) + trame[10];
					}
					strcat(message, messageFin);
					break;
				case 0xD5 : 
					sprintf(messageFin, "Le : %d, Co : %d", !((trame[7]&0x08) >> 3), trame[7] & 0x01);
					strcat(message, messageFin);
					if ((trame[7]&0x08) >> 3) {
						module.valeur = trame[7] & 0x01;
						sprintf(module.type, "Ouverture");
					}
					break;
				case 0xF6 : 
					if ((trame[7]&0x10) != 0x10) {
						sprintf(messageFin, "Et : O");
					} else {
						sprintf(messageFin, "Et : F, Ca : %d, Bt : %d", ((trame[7]&0xC0) >> 6), ((trame[7] & 0x20) >> 5));
					}
					strcat(message, messageFin);
					sprintf(module.type, "Bouton");
					module.valeur = (trame[7]&0xE0) >> 5;
					break;
			}
			sprintf(module.zone, "Zone 1");
			
			if (message[24] == 'L' && message[29] == '1') {
				if (!moduleDejaPresent(pMysql, module)) {
					//insererRequetteMySQL(pMysql, module);
					// Publier sur un topic
					mosquitto_publish(clientMosquitto, NULL, "domotique/learn", strlen(message), message, 0, false);
				}
			}
			if (message[24] == 'L' && message[29] == '0') {
				if (moduleDejaPresent(pMysql, module)) {
					modifierValeurMySQL(pMysql, module);
					if (((module.valeur>>15) & 0x01) == 1) {
						mettreAJourEvenement(pMysql, module, clientMosquitto);
					}
					if (message[32] == 'C' && message[37] == '0') {
						mettreAJourEvenement(pMysql, module, clientMosquitto);
					}
				}
			}
			if (message[29] == 'F') {
				if (!moduleDejaPresent(pMysql, module)) {
					//insererRequetteMySQL(pMysql, module);
					// Publier sur un topic
					mosquitto_publish(clientMosquitto, NULL, "domotique/learn", strlen(message), message, 0, false);
				}
				else {
					gererSecurite(pMysql, module, clientMosquitto);
				}
			}
			/*
			if (moduleDejaPresent(pMysql, module)) {
				if (message[24] == 'F') {
					modifierValeurMySQL(pMysql, module);
				}
				else if (message[24] == 'L' && message[29] == '0') {
					modifierValeurMySQL(pMysql, module);
				}
			}
			else {
				if (message[24] == 'L' && message[29] == '1') {
					//insererRequetteMySQL(pMysql, module);
					// Publier sur un topic
					mosquitto_publish(clientMosquitto, NULL, "domotique/learn", strlen(message), message, 0, true);
				}
				else if (message[24] == 'F') {
					//insererRequetteMySQL(pMysql, module);
					// Publier sur un topic
					mosquitto_publish(clientMosquitto, NULL, "domotique/learn", strlen(message), message, 0, true);
				}
			}
			*/
			printf("%s\n", message);
			tailleTrame = 0;
			tailleTrameMax = 100;
		}
	}
	
	/* Fermer la connexion a la base */
	mysql_close(pMysql);
	
	mosquitto_destroy(clientMosquitto);
	mosquitto_lib_cleanup();
	
	close(ttyS0);
	
	return 0;
}

void my_message_callback(MOSQUITTO *mosq, void *userdata, MOSQUITTO_MESSAGE *message) {
	if(message->payloadlen) {
		printf("%s %s\n", message->topic, message->payload);
	} else {
		printf("%s (null)\n", message->topic);
	}
	fflush(stdout);
}
/*
int insererRequetteMySQL(MYSQL *pMysql, struct modules module) {
	char requeteSql[256];
	sprintf(requeteSql, "INSERT INTO modules (mod_hex, mod_val, mod_typId, mod_zonId)\nSELECT \'%s\', 0, typ_id, zon_id\nFROM types, zones\nWHERE typ_nom=\'%s\' AND zon_nom=\'%s\'", module.nom, module.type, module.zone);
	//printf("%s\n", requeteSql);
	// Executer la requete : inserer un element //
	if (mysql_real_query(pMysql, requeteSql, strlen(requeteSql)) != 0) {
		fprintf(stderr, "mysql_real_query(): %s\n", mysql_error(pMysql));
		mysql_close(pMysql);
		return -1;
	}
}
*/
int modifierValeurMySQL(MYSQL *pMysql, struct modules module) {
	MYSQL_RES *resultat;
	MYSQL_ROW row;
	char requeteSql[256];
	int type;
	float valeur;
	sprintf(requeteSql, "SELECT mod_typId FROM modules\nWHERE mod_hex=\'%s\'", module.nom);
	//printf("%s\n", requeteSql);
	if (mysql_real_query(pMysql, requeteSql, strlen(requeteSql)) != 0) {
		fprintf(stderr, "mysql_real_query(): %s\n", mysql_error(pMysql));
		mysql_close(pMysql);
		return -1;
	}
	resultat = mysql_store_result(pMysql);
	row = mysql_fetch_row(resultat);
	sscanf(row[0], "%d", &type);
	mysql_free_result(resultat);
	if (type == 2) {
		valeur = ((255 - (module.valeur>>8)) / 255.0) * 40;
	} else if (type == 3) {
		valeur = ((0 + (module.valeur>>24)) / 50.0);
		if (valeur < 0) {
			valeur += 5;
		}
	} else if (type == 4) {
		valeur = module.valeur;
	}
	strcpy(requeteSql, "");
	sprintf(requeteSql, "UPDATE modules\nSET mod_val=\'%f\'\nWHERE mod_hex=\'%s\'", valeur, module.nom);
	//printf("%s\n", requeteSql);
	if (mysql_real_query(pMysql, requeteSql, strlen(requeteSql)) != 0) {
		fprintf(stderr, "mysql_real_query(): %s\n", mysql_error(pMysql));
		mysql_close(pMysql);
		return -1;
	}
}

int moduleDejaPresent(MYSQL *pMysql, struct modules module) {
	MYSQL_RES *resultat;
	MYSQL_ROW row;
	char requeteSql[256];
	sprintf(requeteSql, "SELECT mod_id FROM modules\nWHERE mod_hex=\'%s\'", module.nom);
	//printf("%s\n", requeteSql);
	if (mysql_real_query(pMysql, requeteSql, strlen(requeteSql)) != 0) {
		fprintf(stderr, "mysql_real_query(): %s\n", mysql_error(pMysql));
		mysql_close(pMysql);
		return -1;
	}
	resultat = mysql_store_result(pMysql);
	row = mysql_fetch_row(resultat);
	mysql_free_result(resultat);
	if (row != NULL) {
		return 1;
	}
	return 0;
}

int mettreAJourEvenement(MYSQL *pMysql, struct modules module, MOSQUITTO *clientMosquitto) {
	MYSQL_RES *resultat;
	MYSQL_ROW row;
	long int seconds;
	char requeteSql[256];
	char message[60];
	time(&seconds);
	sprintf(requeteSql, "SELECT zon_act\nFROM modules\nINNER JOIN zones ON mod_zonId = zon_id\nWHERE mod_hex=\'%s\';", module.nom);
	if (mysql_real_query(pMysql, requeteSql, strlen(requeteSql)) != 0) {
		fprintf(stderr, "mysql_real_query(): %s\n", mysql_error(pMysql));
		mysql_close(pMysql);
		return -1;
	}
	resultat = mysql_store_result(pMysql);
	row = mysql_fetch_row(resultat);
	mysql_free_result(resultat);
	if (row[0][0] == '0') {
		return 0;
	}
	strcpy(requeteSql, "");
	sprintf(requeteSql, "INSERT INTO evenement (eve_date, eve_modId)\nSELECT NOW(), mod_id\nFROM modules WHERE mod_hex=\'%s\'", module.nom);
	if (mysql_real_query(pMysql, requeteSql, strlen(requeteSql)) != 0) {
		fprintf(stderr, "mysql_real_query(): %s\n", mysql_error(pMysql));
		mysql_close(pMysql);
		return -1;
	}
	sprintf(message, "module : %s, Date : %s", module.nom, ctime(&seconds));
	mosquitto_publish(clientMosquitto, NULL, "domotique/intrusion", strlen(message), message, 0, false);
}

int gererSecurite(MYSQL *pMysql, struct modules module, MOSQUITTO *clientMosquitto) {
	char requeteSql[256];
	char message[60];
	switch (module.valeur) {
	case 0 : sprintf(requeteSql, "UPDATE zones\nSET zon_act=true");
		sprintf(message, "3");
		break;
	case 1 : sprintf(requeteSql, "UPDATE zones\nSET zon_act=false");
		sprintf(message, "0");
		break;
	case 2 : sprintf(requeteSql, "UPDATE zones\nSET zon_act=true\nWHERE zon_nom=\'Zone 1\'");
		sprintf(message, "1");
		break;
	case 3 : sprintf(requeteSql, "UPDATE zones\nSET zon_act=true\nWHERE zon_nom=\'Zone 2\'");
		sprintf(message, "2");
		break;
	}
	if (mysql_real_query(pMysql, requeteSql, strlen(requeteSql)) != 0) {
		fprintf(stderr, "mysql_real_query(): %s\n", mysql_error(pMysql));
		mysql_close(pMysql);
		return -1;
	}
	mosquitto_publish(clientMosquitto, NULL, "domotique/zones", strlen(message), message, 0, false);
}