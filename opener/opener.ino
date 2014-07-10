#include <avr/wdt.h>
#if ARDUINO > 18
#include <SPI.h>
#endif
#include <Ethernet.h>
#include <TextFinder.h>
#include <Time.h>

#if 1 /* DEBUG */
#define DEBUG(x) Serial.print(x)
#define DEBUGLN(x) Serial.println(x)
#else
#define DEBUG(x) do {} while (0)
#define DEBUGLN(x) do {} while (0)
#endif

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
    DEBUGLN("Hello.");

    //enable watchdog
    wdt_enable(WDTO_2S);

    //configuring the two relays
    DEBUGLN("Init Relay");
    pinMode(WINDOW_OPEN_RELAY, OUTPUT);
    pinMode(WINDOW_CLOSE_RELAY, OUTPUT);
    digitalWrite(WINDOW_OPEN_RELAY, HIGH);
    digitalWrite(WINDOW_CLOSE_RELAY, HIGH);

    //start up the ethernet server
    DEBUG("Init IP");
    //dhcp:
    //Ethernet.begin(mac);
    //use this for static ip:
    Ethernet.begin(mac, ip);
    DEBUG(".");

    server.begin();
    DEBUGLN(".");
    DEBUGLN("Init done");
}

//the main loop
void loop() {
    //reset watchdog
    wdt_reset();
    handle_request();
    wdt_reset();
    handle_window();
}

void update_state(int state, int time)
{
    DEBUG("updating state to ");
    DEBUGLN(state);
    DEBUGLN(time);
    STATE = state;
    OPEN_STOP_TIME = now() + time;
    DEBUGLN("updating done");
}

/**
 * Handle the HTTP request
 */
void handle_request()
{
    EthernetClient client = server.available();
    if (!client) {
        return;
    }
    TextFinder  finder(client);
    boolean found = false;

    if (finder.find("GET"))
    {
        DEBUGLN("Got a GET request");
        while(finder.findUntil("window", "\n\r"))
        {
            found = true;
            DEBUGLN("Found window request");
            handle_window_request(client, finder);
            goto end_request;
        }
    }
    if(!found) {
        print_header(client);
        DEBUGLN("I have no idea what to do.");
        print_index_page(client);
    }
end_request:
    client.flush();
    client.stop();
}

/**
 * Apply the current state to the window controls.
 */
void handle_window()
{
    if (STATE == OPENING) {
        DEBUGLN("OPENING");
        DEBUGLN(OPEN_STOP_TIME);
        DEBUGLN(now());
        if (OPEN_STOP_TIME > now()) {
            enable(WINDOW_OPEN_RELAY);
        } else {
            disable(WINDOW_OPEN_RELAY);
            update_state(IDLE, 0);
        }
    } else if (STATE == CLOSING) {
        DEBUGLN("CLOSING");
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
    } else if (amount < 0) {
        // status
        client.print("{'state': ");
        client.print(STATE);
        client.print(", 'remaining_time': ");
        if (STATE == OPENING) {
            client.print(OPEN_STOP_TIME - now());
        } else {
            client.print(0);
        }
        client.print("}");
    } else {
        client.println("{'action':'error','message':'Unknown value'}");
        update_state(IDLE, 0);
    }
    DEBUGLN("window request done");
}

/**
 * Enable the relais
 * @param int pin - the digital pin to trigger
 */
void enable(int pin)
{
    DEBUG("ENABLING pin ");
    DEBUGLN(pin);
    digitalWrite(pin, LOW);
}

/**
 * Disable the relais
 * @param int pin - the digital pin to trigger
 */
void disable(int pin)
{
    DEBUG("DISABLING pin ");
    DEBUGLN(pin);
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
    client.println("<!DOCTYPE html> <html lang='en'> <head> <title>Window!</title> <link rel='stylesheet' href='//netdna.bootstrapcdn.com/bootstrap/3.1.1/css/bootstrap.min.css'> <link rel='stylesheet' href='//netdna.bootstrapcdn.com/bootstrap/3.1.1/css/bootstrap-theme.min.css'> </head> <body role='document'> <div class='container theme-showcase' role='main'> <div class='page-header'> <h1>This is window.</h1> </div> <p> <a href='/?window=60' class='btn btn-lg btn-primary'>Open 100%</a> <a href='/?window=45' class='btn btn-lg btn-success'>Open 75%</a> <a href='/?window=30' class='btn btn-lg btn-info'>Open 50%</a> <a href='/?window=5' class='btn btn-lg btn-warning'>Open 10%</a> <a href='/?window=0' class='btn btn-lg btn-danger'>Close</a> </p> </p> <a href='/?window=-1' class='btn btn-lg btn-default'>Status</a> <p> </div> <script src='https://ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js'></script> <script src='//netdna.bootstrapcdn.com/bootstrap/3.1.1/js/bootstrap.min.js'></script> </body> </html>");
}

