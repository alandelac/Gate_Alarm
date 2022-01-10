// Librerias
#include <ESP8266Webhook.h> // Libreria para activar el webhook con IFTTT
#include <ESP8266WiFi.h>    // Para hacer la conexion a Internet
#include <PubSubClient.h>   // Para conectarse al broker de smartnest
#include <WiFiClient.h>     // Otra cosa para la conexion a internet

#define SSID_NAME "casadelamoto3"                         // Your Wifi Network name
#define SSID_PASSWORD "alte0208"                          // Your Wifi network password
#define MQTT_BROKER "smartnest.cz"                        // Broker host
#define MQTT_PORT 1883                                    // Broker port
#define MQTT_USERNAME "ALDOELCECH"                        // Username from Smartnest
#define MQTT_PASSWORD "4l4nDete@smart"                    // Password from Smartnest (or API key)
#define MQTT_CLIENT "61cbae9e8e4ca73cdc76d846"            // Device Id from smartnest
#define FIRMWARE_VERSION "Reja-1.0.0"                     // Custom name for this program
#define KEY "mVmRHYWyx9CLK9cs14ZLc-ispw_GU9wvD8IbbBDnfrr" // Webhooks Key
#define EVENT "gate_notification"                         // Webhooks Event Name

Webhook webhook(KEY, EVENT);    // Objeto para manipular el webhook
WiFiClient espClient;           // Objeto para hacer al esp como tipo cliente
PubSubClient client(espClient); // Objeto para manipular el broker

// Para saber el estado de la reja. No se inicializa pues no se sabe el estado de la reja al iniciar el programa
String estadoReja="";

//para medir la alarma constante
unsigned long startAlarm; 
unsigned long currentAlarm;

//para tener error de lectura
unsigned long startChance;
unsigned long currentChance;

int repe;

// controlar los reportes para evitar baneo
bool repLock = false;
bool repState = false;

bool avisoAlarma=false; //comprobar si ya se envio la notificacion por el IFTTT de la alarma

// El tiempo para que la alarma se active. Se define en segundos
unsigned int tiempoConstante;

#define pinSensor 3 // aqui va el magneitico. Se puede cambiar

// Los metodos para el programa
void startWifi();
void startMqtt();
void checkMqtt();
int splitTopic(char* topic, char* tokens[], int tokensNumber);
void callback(char* topic, byte* payload, unsigned int length);
void sendToBroker(char* topic, char* message);
void checarEstadoReja();
void checarTiempo();
void resetearAlarma();
void resetearChance();
void turnOff();

// para definir los pines y otras cosas
void setup() {

  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  
  //declaracion de los GPIOS
  pinMode(pinSensor, INPUT);
  pinMode(LED_BUILTIN, INPUT); // re-assign GPIO1 as input 

  // la alarma sonara a los 10 minutos (ahorita para las pruebas sera de 20 segundos)
  tiempoConstante=180;

  // iniciamos el internet y el broker
  startWifi();
  startMqtt();

  // TEMPORAL solo para saber que si se conecto al internet
  startAlarm = millis();
  startChance=millis();


     webhook.trigger("el_programa_arranco_bien");
}

// loop que se repetira constantemente
void loop() {
  checarTiempo();     //actualizar los tiempos de currentChance y currentAlarma

  checarEstadoReja(); //checar el estado de la reja y reaccionar depende de esta y los tiempos

  // checar que la conexion al mqtt siga estable
  checkMqtt();

  // checar la conexion wifi
  if(WiFi.status() != WL_CONNECTED){
    startWifi();
  }

  // mantener el mqtt actualizado
  client.loop();
  
}

// checar el tiempo de todas las variables de tiempo
void checarTiempo()
{
  /*  El modulo cuenta con un numero finito para millis, cuando este numero
  se supera millis empieza otras vez desde 0. Por esta razon si millis es
  menor a cualquiera de mis variables estas deberan de reiniciarse.
  El uncio detalle es que si la reja lleva 8 min y esto sucede se cambiaria a 0.
  Pero la probabilidad de que eso pase es muy baja
  */
  if(millis()<startAlarm || millis()<startChance)
  {
    resetearAlarma();
    resetearChance();
  }
  
  // los tiempos actuales de la chance y la alarma en segundos
  currentAlarm = millis() - startAlarm;
  currentAlarm /= 1000;
  currentChance = millis() -startChance;
  currentChance/=1000;
}

// poner la alarma al mismo tiempo que el reloj del esp
void resetearAlarma()
{
  startAlarm = millis();
}

// poner la chance al mismo tiempo que el reloj del esp
void resetearChance()
{
  startChance=millis();
}

// checar si la reja esta cerrada o abierta y actuar sobre esto
void checarEstadoReja()
{

  // si el sensor esta conectado (reja cerrada) actualiza la informacion
  if(digitalRead(pinSensor)==HIGH && currentChance>5)
  {
    repe = 0;
    if(repLock == false){
      sendToBroker("report/lockedState", "true"); // se hace el reporte a smartnest
      sendToBroker("report/powerState", "OFF"); // se hace el reporte a smartnest
      repLock = true; // ya esta locked la reja 
      if(repState == true){
        webhook.trigger("La_reja_ya_se_cerro"); // se avisa por telegram
      }
      repState = false; // si esta cerrada se reinicia la alarma
    }
    
    resetearAlarma(); // se resetea constantemente pues la reja esta cerrada

  }
  // si el sensor se desconecta (reja abierta) actualiza la informacion
  else if(digitalRead(pinSensor)==LOW)
  {
    resetearChance(); // la chance porque no le se a la electronica jijijija

    // si antes estaba cerrado se actualiza el cambio
    if(repLock == true){
      sendToBroker("report/lockedState", "false"); // se actualiza el cambio de manera en linea
      repLock = false; // se actuliza el cambio de manera local
    }

    // si la alarma lleva mas tiempo abierto que la constante activa la alarma
    if(currentAlarm > tiempoConstante)
    {
      repe = repe +1;

      if(repState==false){
        sendToBroker("report/powerState", "ON"); // se avisa al broker
      }
      webhook.trigger("La_reja_lleva_abierta", String(3*repe), "minutos"); // se avisa por telegram
      repState = true; // se hace el trigger para no activar la alarma nuevamente
      resetearAlarma(); // se resetea, si se cumple el tiempo se vuelve a llamar
    }
  }
}

// cuando se recibe un mensaje aqui se analiza
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Topic:");
	Serial.println(topic);
  // splits the topic and gets the message
  int tokensNumber = 10;
  char* tokens[tokensNumber];
  char message[length + 1];
  splitTopic(topic, tokens, tokensNumber);
  sprintf(message, "%c", (char)payload[0]);

  for (int i = 1; i < length; i++) {
    sprintf(message, "%s%c", message, (char)payload[i]);
  }
  Serial.print("Message:");
	Serial.println(message);

  //------------------ACTIONS HERE---------------------------------
  ///Actions here
	if (strcmp(tokens[1], "directive") == 0 && strcmp(tokens[2], "powerState") == 0) {
  }
}

// para reconectarse al wifi
void startWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_NAME, SSID_PASSWORD);
  Serial.println("Connecting ...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    attempts++;
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, LOW);
  }

  if(WiFi.status() == WL_CONNECTED){
    webhook.trigger("conectado_al_wifi");
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println('\n');
		Serial.print("Connected to ");
		Serial.println(WiFi.SSID());
		Serial.print("IP address:\t");
		Serial.println(WiFi.localIP());
  }
  else{
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println('\n');
		Serial.println('I could not connect to the wifi network after 10 attempts \n');
  }

  delay(500);
}

// conectarse al broker
void startMqtt() {
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect(MQTT_CLIENT, MQTT_USERNAME, MQTT_PASSWORD)) {
			Serial.println("connected");
      webhook.trigger("SI_me_pude_conectar_al_broker");
		} else {
      webhook.trigger("no_me_pude_conectar_al_broker");
			if (client.state() == 5) {
        
				Serial.println("Connection not allowed by broker, possible reasons:");
				Serial.println("- Device is already online. Wait some seconds until it appears offline for the broker");
				Serial.println("- Wrong Username or password. Check credentials");
				Serial.println("- Client Id does not belong to this username, verify ClientId");

			} else {
				Serial.println("Not possible to connect to Broker Error code:");
				Serial.print(client.state());
			}
      delay(0x7530);
    
  }}

  char subscibeTopic[100];
  sprintf(subscibeTopic, "%s/#", MQTT_CLIENT);
  client.subscribe(subscibeTopic);  //Subscribes to all messages send to the device

  sendToBroker("report/online", "true");  // Reports that the device is online
  delay(100);
  sendToBroker("report/firmware", FIRMWARE_VERSION);  // Reports the firmware version
  delay(100);
  sendToBroker("report/ip", (char*)WiFi.localIP().toString().c_str());  // Reports the ip
  delay(100);
  sendToBroker("report/network", (char*)WiFi.SSID().c_str());  // Reports the network name
  delay(100);

  char signal[5];
  sprintf(signal, "%d", WiFi.RSSI());
  sendToBroker("report/signal", signal);  // Reports the signal strength
  delay(100);
}

int splitTopic(char* topic, char* tokens[], int tokensNumber) {
  const char s[2] = "/";
  int pos = 0;
  tokens[0] = strtok(topic, s);

  while (pos < tokensNumber - 1 && tokens[pos] != NULL) {
    pos++;
    tokens[pos] = strtok(NULL, s);
  }

  return pos;
}

void checkMqtt() {
  if (!client.connected()) {
    startMqtt();
  }
}

void sendToBroker(char* topic, char* message) {
  if (client.connected()) {
    char topicArr[100];
    sprintf(topicArr, "%s/%s", MQTT_CLIENT, topic);
    client.publish(topicArr, message);
  }
}

