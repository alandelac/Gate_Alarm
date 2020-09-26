#include "CTBot.h"
#include "Utilities.h"
#include <EEPROM.h>

String ssid  = "casadelamoto3"; //nombreDelInternet
String pass  = "alte0208"; //clave del Internet
String token = "1087257836:AAEX5DjNPuAjQmoSKenq0n34rhPpY46aCbI"; // token del robot
bool conexionWifi = false;

CTBot myBot; // el bot de telegram
long id= -472880073; // id del grupo de telegram

String estadoReja="";

//para medir la alarma constante
long startAlarm; 
long currentAlarm;

//para medir el tiempo de los beats
long startBeat;
long currentBeat;

//para tener error de lectura
long startChance;
long currentChance;

//para definir que modificar a partir de los numeros
bool modificarBeat=false;
bool modificarConstante=false;

bool avisoAlarma=false; //comprobar si ya se envio el aviso de la alarma

//Los parametros del beat y del tiempo de la alarma. FALTAN PONERLOS COMO EEPROM
unsigned int tiempoBeat;
unsigned int tiempoConstante;

//Los pines
#define pinSensor 3 // I
#define pinLed 2 // O . Para avisar sobre el estado de la reja
#define pinLedIntegrado 1 // O. Para verificar la conexion
#define pinBocina 0 // O. Para sonar la alarma


void setup() {

  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY); //habilitar RX como GPIO
  EEPROM.begin(512);

  tiempoBeat=readUnsignedIntFromEEPROM(50);
  tiempoConstante=readUnsignedIntFromEEPROM(60);

  

  //declaracion de los GPIOS
  pinMode(pinLed, OUTPUT);
  pinMode(pinSensor, INPUT);
  pinMode(pinLedIntegrado, OUTPUT); 
  pinMode(pinBocina, OUTPUT);

  //Cosas para que jale bien el modulo
  digitalWrite (pinLed, HIGH);
  digitalWrite (pinBocina, HIGH);
  digitalWrite (pinLedIntegrado, HIGH);

  //myBot.setMaxConnectionRetries(1); //limitar a 1 intentos para conectarse a internet
  
  myBot.wifiConnect(ssid, pass); //establecer la conexion al router
  myBot.setTelegramToken(token); // establecer la conexion con el bot de telegram

  // testear la conexion
  if (myBot.testConnection())
  {
    digitalWrite(pinLedIntegrado, LOW);//encender led si la conexion es exito
    myBot.sendMessage(id, "Hola ya estoy conectado :) \n Escribe /help para ver las opciones disponibles!!");
    conexionWifi=true;
  }
  else
  {
    digitalWrite(pinLedIntegrado, HIGH);//apagar led si la conexion no es exitosa
    conexionWifi=false;
  }

  startAlarm = millis();
  startBeat=millis();
  startChance=millis();

}

void loop()
{
  // variable para almacenar los mensajes
  TBMessage msg;

  checarTiempo(); //actualizar los tiempos de chance, beat y alarma

  checarEstadoReja(); //checar el estado de la reja y reaccionar depende de esta y los tiempos

  if(conexionWifi)
  {
    checarMensajes(msg); //checar si llego un mensaje nuevo y responder a este
  }
  checarConexion();
}

void checarEstadoReja()
{
  if(digitalRead(pinSensor)==HIGH && currentChance>5)
  {
    avisoAlarma=false;
    digitalWrite(pinLed,LOW);
    resetearAlarma();
    noTone(pinBocina);
    estadoReja="cerrado";
  }
  else if(digitalRead(pinSensor)==LOW)
  {
    resetearChance();
    digitalWrite(pinLed,HIGH);
    estadoReja="abierto";

    if(currentBeat>tiempoBeat && avisoAlarma)
    {
      tone(pinBocina, 800, 1000);
      resetearBeat();
    }
    
    if(currentAlarm>tiempoConstante && avisoAlarma==false) //La alarma ya esta activa apenas se enviara el mensaje
    {
      if(conexionWifi)
      {
        myBot.sendMessage(id, "Alarma activa y porton abierto. Escriba /silenciarAlarma o cierre el porton para reiniciar la alarma!!!");
      }
      tone(pinBocina, 800);
      avisoAlarma = true;
    }
  }
}

void checarMensajes(TBMessage Message)
{
  if (myBot.getNewMessage(Message)) //checar si hay un mensaje nuevo
  {
    if (Message.messageType == CTBotMessageText && Message.group.id < 0) //comprobar que es de tipo texto y que viene del grupo de chat
    {
      if (Message.text.equalsIgnoreCase("/help")) //Si dice help despliega los comandos disponibles
      {
        myBot.sendMessage(Message.group.id, "Escribe el comando que quieras acceder:"
                          "\n /silenciarAlarma reinica la alarma cuando esta esta sonando \n"
                          "\n /cambiarTiempoAlarma cambia el tiempo que la reja esta abierta antes de alarma constante \n"
                          "\n /cambiarTiempoBeat cambia el tiempo entre cada beat \n"
                          "\n /info despliega la informacion del sensor");
      }
      else if (Message.text.equalsIgnoreCase("/silenciarAlarma")) //reinicia el tiempo de la alarma
      {
        myBot.sendMessage(Message.group.id, "Alarma reiniciada");
        avisoAlarma=false;
        resetearAlarma();
        noTone(pinBocina);
      }
      
      else if (Message.text.equalsIgnoreCase("/cambiarTiempoAlarma")) //cambia el tiempo para que la alarma se active
      {
        myBot.sendMessage(Message.group.id, "Seleccione nuevo tiempo: "
        "\n /1 min  /3 min"
        "\n /5 min   /10 min"
        "\n /15 min  /30 min"
        "\n /60 min");
        modificarConstante=true;
      }
      else if (Message.text.equalsIgnoreCase("/cambiarTiempoBeat"))
      {
        myBot.sendMessage(Message.group.id, "Seleccione nuevo tiempo: "
        "\n /1 s   /3 s"
        "\n /5 s   /10 s"
        "\n /15 s  /30 s"
        "\n /60 s");
        modificarBeat=true;
      }
      else if (Message.text.equalsIgnoreCase("/info"))
      {
        unsigned int z=tiempoConstante/60;
        String sms = "Estado de la reja: "+estadoReja+
                     "\n Tiempo de reja abierta: " + (String)currentAlarm +"  (si es igual o menor que 5 significa que esta cerrado)"
                     "\n Tiempo de Beat: "+(String)tiempoBeat+" segundos"
                     "\n Tiempo de alarma: "+(String)z+" minutos";
        myBot.sendMessage(Message.group.id, sms);
      }
      else if (Message.text.equalsIgnoreCase("/1"))
      {
        myBot.sendMessage(Message.group.id, "Tiempo actualizado");
        actualizarDatos(1);
      }
      else if (Message.text.equalsIgnoreCase("/3"))
      {
        myBot.sendMessage(Message.group.id, "Tiempo actualizado");
        actualizarDatos(3);
      }
      else if (Message.text.equalsIgnoreCase("/5"))
      {
        myBot.sendMessage(Message.group.id, "Tiempo actualizado");
        actualizarDatos(5);
      }
      else if (Message.text.equalsIgnoreCase("/10"))
      {
        myBot.sendMessage(Message.group.id, "Tiempo actualizado");
        actualizarDatos(10);
      }
      else if (Message.text.equalsIgnoreCase("/15"))
      {
        myBot.sendMessage(Message.group.id, "Tiempo actualizado");
        actualizarDatos(15);
      }
      else if (Message.text.equalsIgnoreCase("/30"))
      {
        myBot.sendMessage(Message.group.id, "Tiempo actualizado");
        actualizarDatos(30);
      }
      else if (Message.text.equalsIgnoreCase("/60"))
      {
        myBot.sendMessage(Message.group.id, "Tiempo actualizado");
        actualizarDatos(60);
      }
      else
      {
        myBot.sendMessage(Message.group.id, "Intente con /help");
      }
    }
  }
}

void actualizarDatos(unsigned int x){
  if(modificarBeat)
  {
    writeUnsignedIntIntoEEPROM(50, x);
    EEPROM.commit();
    tiempoBeat=readUnsignedIntFromEEPROM(50);
  }
  if(modificarConstante)
  {
    x=x*60;
    writeUnsignedIntIntoEEPROM(60, x);
    EEPROM.commit();
    tiempoConstante=readUnsignedIntFromEEPROM(60);
  }
  modificarBeat=false;
  modificarConstante=false;
}

void checarConexion()
{
  if (myBot.testConnection())
  {
    digitalWrite(pinLedIntegrado, LOW);//encender led si la conexion es exito
    if(conexionWifi==false)
    {
      unsigned int y=tiempoConstante/60;
      //myBot.sendMessage(id, "Hola me desconecte por un momento pero ya regrese :). Prueba /help para ver las opciones disponibles.");
      String sms = "Estado de la reja: "+estadoReja+
                     "\n Tiempo de reja abierta: " + (String)currentAlarm +"  (si es igual o menor que 5 significa que esta cerrado)"
                     "\n Tiempo de Beat: "+(String)tiempoBeat+" segundos"
                     "\n Tiempo de alarma: "+(String)y+" minutos";
      //myBot.sendMessage(id, sms);
      conexionWifi=true;
    }
  }
  else
  {
    myBot.setMaxConnectionRetries(2);
    digitalWrite(pinLedIntegrado, HIGH);//apagar led si la conexion no es exitosa
    myBot.wifiConnect(ssid, pass); //establecer la conexion al router
    conexionWifi=false;
  }
}

void checarTiempo()
{
  if(millis()<startAlarm || millis()<startBeat || millis()<startChance)
  {
    resetearAlarma();
    resetearBeat();
    resetearChance();
  }
  
  currentAlarm = millis() - startAlarm;
  currentAlarm /= 1000;
  currentBeat=millis()-startBeat;
  currentBeat/=1000;
  currentChance = millis() -startChance;
  currentChance/=1000;
}

void resetearAlarma()
{
  startAlarm = millis();
}

void resetearBeat()
{
  if (currentBeat>tiempoBeat ){
    startBeat=millis();
  }
}

void resetearChance()
{
  startChance=millis();
}

void writeUnsignedIntIntoEEPROM(int address, unsigned int number)
{ 
  EEPROM.write(address, number >> 8);
  EEPROM.write(address + 1, number & 0xFF);
}

unsigned int readUnsignedIntFromEEPROM(int address)
{
  return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
}
