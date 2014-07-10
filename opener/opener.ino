#include <avr/wdt.h>
#if ARDUINO > 18
#include <SPI.h>
#endif
#include <Ethernet.h>
#include <TextFinder.h>
#include <Time.h>

//Digital port of the "open relay"
#define WINDOW_OPEN_RELAY  2
//Digital port of the "close relay"
#define WINDOW_CLOSE_RELAY 3
//Maximum time in seconds the open button can be triggered
#define MAX_OPEN_TIME      47
//IP address
byte ip[]  = { 192, 168, 178, 178 };
byte mac[] = { 0x5A, 0xA2, 0xDA, 0x0D, 0x12, 0x34 }; 

#define IDLE 0
#define OPENING 1
#define CLOSING 2
int STATE;
int OPEN_STOP_TIME;

EthernetServer server(80);

//setup routine
void setup() {
    Serial.begin(9600);
    Serial.println("Hello.");

    //enable watchdog
    wdt_enable(WDTO_2S);

    //configuring the two relays
    Serial.println("Init Relay");
    pinMode(WINDOW_OPEN_RELAY, OUTPUT);
    pinMode(WINDOW_CLOSE_RELAY, OUTPUT);
    digitalWrite(WINDOW_OPEN_RELAY, HIGH);
    digitalWrite(WINDOW_CLOSE_RELAY, HIGH);

    //start up the ethernet server
    Serial.print("Init IP");
    //dhcp:
    //Ethernet.begin(mac);
    //use this for static ip:
    Ethernet.begin(mac, ip);
    Serial.print(".");

    server.begin();
    Serial.println(".");
    Serial.println("Init done");
}

//the main loop
void loop() {
    //reset watchdog
    wdt_reset();
    handle_request();
    Serial.println("handle request done");
    wdt_reset();
    handle_window();
    Serial.println("handle window done");
}

void update_state(int state, int time)
{
    Serial.print("updating state to ");
    Serial.println(state);
    Serial.println(time);
    STATE = state;
    OPEN_STOP_TIME = now() + time;
    Serial.println("updating done");
}

/**
 * Handle the HTTP request
 */
void handle_request()
{
    EthernetClient client = server.available();
    if (client) {
        TextFinder  finder(client);
        boolean found = false;

        if (finder.find("GET"))
        {
            Serial.println("Got a GET request");
            while(finder.findUntil("window", "\n\r"))
            {
                found = true;
                Serial.println("Found window request");
                handle_window_request(client, finder);
            }
        }
        if(!found) {
            print_header(client);
            Serial.println("I have no idea what to do.");
            print_index_page(client);
        }
        client.flush();
        client.stop();
    }
}

/**
 * Apply the current state to the window controls.
 */
void handle_window()
{
    if (STATE == OPENING) {
        Serial.println("OPENING");
        Serial.println(OPEN_STOP_TIME);
        Serial.println(now());
        if (OPEN_STOP_TIME > now()) {
            enable(WINDOW_OPEN_RELAY);
        } else {
            disable(WINDOW_OPEN_RELAY);
            update_state(IDLE, 0);
        }
    } else if (STATE == CLOSING) {
        Serial.println("CLOSING");
        disable(WINDOW_OPEN_RELAY);
        enable(WINDOW_CLOSE_RELAY);
        delay(100);
        disable(WINDOW_CLOSE_RELAY);
        update_state(IDLE, 0);
    }
}

/**
 * Handle the window request; update the state.
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
void handle_window_request(EthernetClient &client, TextFinder  &finder) 
{
    print_header(client);
    int amount = finder.getValue();
    if (amount == 0) {
        client.println("{'action':'close'}");
        update_state(CLOSING, 0);
    } else if ((amount > 0) && (amount <= MAX_OPEN_TIME)) {
        client.print("{'action':'open','time':");
        client.print(amount);
        client.println("}");
        update_state(OPENING, amount);
    } else {
        client.println("{'action':'error','message':'Unknown value'}");
        Serial.println("error: unknown value: " + amount);
        update_state(IDLE, 0);
    }
    Serial.println("window request done");
}

/**
 * Enable the relais
 * @param int pin - the digital pin to trigger
 */
void enable(int pin)
{
    Serial.print("ENABLING pin ");
    Serial.print(pin);
    digitalWrite(pin, LOW);
}

/**
 * Disable the relais
 * @param int pin - the digital pin to trigger
 */
void disable(int pin)
{
    Serial.print("DISABLING pin ");
    Serial.print(pin);
    digitalWrite(pin, HIGH);
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


/**
 * Print minified version of click.html
 *
 * @param EthernetClient &client the connection
 */
void print_index_page(EthernetClient &client)
{
    client.println("<!DOCTYPE html><html lang='en'><head><title>Window!</title><link rel='stylesheet' href='//netdna.bootstrapcdn.com/bootstrap/3.1.1/css/bootstrap.min.css'><link rel='stylesheet' href='//netdna.bootstrapcdn.com/bootstrap/3.1.1/css/bootstrap-theme.min.css'></head><body role='document'><div class='container theme-showcase' role='main'><div class='page-header'><h1>This is window.</h1></div><p><a href='/?window=47' class='btn btn-lg btn-primary'>Open 100%</a><a href='/?window=35' class='btn btn-lg btn-success'>Open 75%</a><a href='/?window=23' class='btn btn-lg btn-info'>Open 50%</a><a href='/?window=5' class='btn btn-lg btn-warning'>Open 10%</a><a href='/?window=0' class='btn btn-lg btn-danger'>Close</a></p></div><script src='https://ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js'></script><script src='//netdna.bootstrapcdn.com/bootstrap/3.1.1/js/bootstrap.min.js'></script></body></html>");
}

