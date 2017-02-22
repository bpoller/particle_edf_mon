/*
Trame recu par la teleinfo

ADCO 040422040644 5	        (N° d'identification du compteur : ADCO (12 caractères))
OPTARIF HC.. <	                (Option tarifaire (type d'abonnement) : OPTARIF (4 car.))
ISOUSC 45 ?	                (Intensité souscrite : ISOUSC ( 2 car. unité = ampères))
HCHC 077089461 0	        (Index heures creuses si option = heures creuses : HCHC ( 9 car. unité = Wh))
HCHP 096066754 >	        (Index heures pleines si option = heures creuses : HCHP ( 9 car. unité = Wh))
PTEC HP..  	                (Période tarifaire en cours : PTEC ( 4 car.))
IINST 002 Y	                (Intensité instantanée : IINST ( 3 car. unité = ampères))
IMAX 044 G	                (Intensité maximale : IMAX ( 3 car. unité = ampères))
PAPP 00460 +	                (Puissance apparente : PAPP ( 5 car. unité = Volt.ampères))
HHPHC E 0	                (Groupe horaire si option = heures creuses ou tempo : HHPHC (1 car.))
MOTDETAT 000000 B	        (Mot d'état (autocontrôle) : MOTDETAT (6 car.))
*/

#include <HttpClient.h>
#include "application.h"
    

HttpClient http;

// Headers currently need to be set at init, useful for API keys etc.
http_header_t headers[] = {
    { "Content-Type", "application/json" },
    { "Accept" , "*/*"},
    {"Authorization","Basic authCodeHere"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};

http_request_t request;
http_response_t response;

long Index_HC=0;
long Index_HP=0;
byte I_A=0;
int P_W=0;
char PERIODE=' ';

long p_Index_HC=0;
long p_Index_HP=0;

int syncTimeCounter = 0;

unsigned long temps_d_acquisition = 0;

void setup()
{
  Serial.begin(9600);
  Serial1.begin(1200);
  
  Particle.syncTime();

  temps_d_acquisition=millis();
  Recupere_la_Teleinfo();


  request.hostname = "51.15.141.206";
  request.port = 9200;
  request.path = "/edf/measure";
  
  Particle.publish("bpo_edf_meter_status", "System started");
}

void loop() {
    waitForMS(60000);
    
    syncTimeCounter++;
    temps_d_acquisition=millis();
    
    Recupere_la_Teleinfo();
    sendInfo();
    
    syncTimeEvery(60);
}

void waitForMS(unsigned long ms) {
    while (millis() - temps_d_acquisition < ms) {
        Particle.process();
    }
}

void syncTimeEvery(int interval){
    if(syncTimeCounter % interval == 0) {
        Particle.syncTime();      
    }
}

void sendInfo(){
   
   String data;
   String tariff;
    
    if(PERIODE=='C'){
        data = String(Index_HC-p_Index_HC);
        tariff = "OFF_PEAK";
    }
    else{
        data = String(Index_HP-p_Index_HP);
        tariff = "PEAK";
    }
    
    request.body = "{\"time\":" + String(Time.now()) + "000, \"measurement\":"+data+", \"tariff\":\""+tariff+"\"}";

    // Get request
    http.post(request, response, headers);
    
    Particle.publish("bpo_edf_meter_sent",  request.body);
}

void Recupere_la_Teleinfo(){
    char charIn_Trame_Teleinfo = 0;
    String Etiquette; 
    String Valeur;
    String Checksum;
    
    
    //copy current into previous values.
    p_Index_HC = Index_HC;
    p_Index_HP = Index_HP;
    
    Index_HC=Index_HP=I_A=P_W=0;
    PERIODE= ' ';
    
    // on recommence tant que l'on a pas recu tous les élément voulus
    while ( Index_HC==0 || Index_HP==0 || I_A==0 || P_W==0 || PERIODE==' ' ){ 
    
        // reste dans cette boucle tant qu'on ne recoit pas le Charactere de début de ligne 0x0A
        
        while (charIn_Trame_Teleinfo != 0x0A){
            if ((millis()-temps_d_acquisition)>1500 ) {
                Particle.publish("bpo_edf_meter_status", "Teleinfo inaccessible");
                Particle.process();
                temps_d_acquisition = millis();
            } 
            if (Serial1.available()){
                charIn_Trame_Teleinfo = Serial1.read() & 0x7F;
            }
        }
        Etiquette = readField(0x20);
        Valeur =   readField(0x20);
        Checksum = readField(0x0D);
        
        if (Etiquette.substring(1,5).equals("HCHC")) {Index_HC = Valeur.toInt();}
        if (Etiquette.substring(1,5).equals("HCHP")) {Index_HP = Valeur.toInt();}
        if (Etiquette.substring(1,5).equals("PTEC")) {PERIODE  = Valeur[1];     }
        if (Etiquette.substring(1,5).equals("IINS")) {I_A      = Valeur.toInt();}
        if (Etiquette.substring(1,5).equals("PAPP")) {P_W      = Valeur.toInt();}  
    }
}

 String readField(char endOfField){
    char character = 0;
    String field;
    while (character != endOfField){
        if (Serial1.available()){
            character = Serial1.read() & 0x7F; 
            if (character != endOfField){
                field += character;
            }
        }         
    }
    return field;
 }

 boolean checksumOk(String label, String value, String checksum) {
    char controle=0;
    String frame = label + " " + value;
    for (byte i=0;i<(frame.length());i++){
              controle += frame[i];
    }  
    controle = (controle & 0x3F) + 0x20;
    return controle == checksum.charAt(0);
}
