#include <stdio.h>
#include <time.h>
#include <stdlib.h>


#define MAX_SIZE 80
#define DEBUT_MAIL  "swaks --to \"pi@maisonconnectee.lan\" \
 --from \"alice@maisonconnectee.lan\" --server mail.maisonconnectee.lan \
  --auth LOGIN --auth-user \"alice\" --auth-password \"bonjour\" -tls"

int main( int argc, char * argv[] )
{

    time_t timestamp = time( NULL );
    struct tm * pTime = localtime( & timestamp );

    char date[ MAX_SIZE ];
    strftime( date, MAX_SIZE, "%c", pTime );
    printf( "date et l heure : %s\n",date );


    char mail [500];
    sprintf(mail," %s --body \"intrusion detecte a %s\"",DEBUT_MAIL,date);
    printf("commande :%s\n",mail);
    system (mail);

    return 0;

}
