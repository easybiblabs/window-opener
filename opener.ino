#include <SPI.h>
#include <Ethernet.h>
#include <TextFinder.h>

//Digital port of the "open relay"
#define WINDOW_OPEN_RELAY  2
//Digital port of the "close relay"
#define WINDOW_CLOSE_RELAY 3
//Maximum time in seconds the open button can be triggered
#define MAX_OPEN_TIME      15
//IP address
byte ip[]  = { 192, 168, 178, 178 };
byte mac[] = { 0x5A, 0xA2, 0xDA, 0x0D, 0x12, 0x34 }; 

EthernetServer server(80);

//setup routine
void setup() {
  Serial.begin(9600);
  Serial.println("Hello.");
  
  //configuring the two relays
  Serial.println("Init Relay");
  pinMode(WINDOW_OPEN_RELAY, OUTPUT);
  pinMode(WINDOW_CLOSE_RELAY, OUTPUT);
  digitalWrite(WINDOW_OPEN_RELAY, HIGH);
  digitalWrite(WINDOW_CLOSE_RELAY, HIGH);
  
  //start up the ethernet server
  Serial.println("Init IP");
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.println("Init done");
}

//the main loop
void loop() {
  EthernetClient client = server.available();
  if(client) {  
    TextFinder  finder(client);
    boolean found = false;
    
    if(finder.find("GET"))
    {
      Serial.println("Got a GET request");
      while(finder.findUntil("window", "\n\r"))
      {
        found = true;
        Serial.println("Found window request");
        handle_window(client, finder);
      }
    }
    if(!found) {
        print_header(client);
        Serial.println("I have no idea what to do.");
        client.println("Hello. This is the window opener.");
      }
    delay(1);
    client.stop();
  }
}

/**
 * the main routine 
 * 
 * @param EthernetClient &client the connection
 * @param TextFinder &finder the request text finder
 *
 * this is far from perfect: it just fetches the next int value 
 * from the request strings and acts on it. Since we already forwarded
 * the "pointer" in the main routine to the string window, it works for us.
 * if the integer value is 0, it triggers the close button,
 * if it is >0 and <MAX_OPEN_TIME, it triggers the open button,
 * and errors otherwise
 */
void handle_window(EthernetClient &client, TextFinder  &finder) 
{
  print_header(client);
  int amount = finder.getValue();
  if(amount == 0) {
    client.println("{'action':'close'}");
    trigger(WINDOW_CLOSE_RELAY, 0);
  } else if((amount > 0) && (amount < MAX_OPEN_TIME)) {
    client.print("{'action':'open','time':");
    client.print(amount);
    client.println("}");
    trigger(WINDOW_OPEN_RELAY, amount);
  } else {
    client.println("{'action':'error','message':'Unknown value'}");
    Serial.println("error: unknown value: " + amount);
  }
}

/**
 * Triggers the relais.
 *
 * @param int pin - the digital pin to trigger
 * @param int amount - duration to trigger in seconds. 0 will result in 500ms triggering.
 */
void trigger(int pin, int amount)  
{
  if(amount == 0) {
    //dont wait a full second for close
    amount = 500;
  } else {
    amount = amount * 1000;
  }
  Serial.print("Triggering pin ");
  Serial.print(pin);
  Serial.print(" for ");
  Serial.print(amount);
  Serial.println("ms");
  digitalWrite(pin, LOW);
  delay(amount);
  digitalWrite(pin, HIGH);
  Serial.println("Triggered.");
}
 
/**
 * Prints HTTP header
 * 
 * @param EthernetClient &client the connection
 */
void print_header(EthernetClient &client)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
}

