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
int OPEN_TIME_ACCU;

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
    DEBUG(freeRam());
    DEBUGLN(" Bytes Free.");
}

void loop() {
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
    if (STATE == CLOSING) {
        OPEN_TIME_ACCU = 0;
    } else {
        OPEN_TIME_ACCU += time;
    }
    DEBUGLN("updating done");
}

int freeRam ()
{
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
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

    TextFinder finder(client);

    if (finder.find("GET")) {
        DEBUGLN("Got a GET request");
        while (finder.findUntil("window", "\n\r")) {
            DEBUGLN("Found window request");
            handle_window_request(client, finder);
            goto end_request;
        }
    }
    DEBUGLN("I have no idea what to do.");
    print_index_page(client);

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
        client.println("{\"action\":\"close\"}");
        update_state(CLOSING, 0);
    } else if ((amount > 0) && (amount <= MAX_OPEN_TIME)) {
        client.print("{\"action\":\"open\",\"time\":");
        client.print(amount);
        client.println("}");
        update_state(OPENING, amount);
    } else if (amount < 0) {
        // status
        client.print("{\"state\": ");
        client.print(STATE);
        client.print(", \"remaining_time\": ");
        int remaining_time = 0;
        if (STATE == OPENING) {
            remaining_time = OPEN_STOP_TIME - now();
        }
        client.print(remaining_time);
        client.print(", \"openness\": ");
        client.print(OPEN_TIME_ACCU - remaining_time);
        client.print("}");
    } else {
        client.println("{\"action\":\"error\",\"message\":\"Unknown value\"}");
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
 * @param EthernetClient &client the connection
 */
void print_header(EthernetClient &client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json; charset=UTF-8");
    client.println();
}

/**
 * This is the gzipped click.html (It's gzipped to keep the
 * heap and stack from colliding.)
 * Watch the number of bytes free between stack and heap
 * printed on the serial console on device init.
 * To regen this, do `make` and copy click.html.gz.h here
 * (#include doesn't work for some reason)
 */
unsigned char click_html_gz[] = {
  0x1f, 0x8b, 0x08, 0x08, 0xdf, 0xd2, 0xbf, 0x53, 0x00, 0x03, 0x63, 0x6c,
  0x69, 0x63, 0x6b, 0x2e, 0x68, 0x74, 0x6d, 0x6c, 0x00, 0xb5, 0x56, 0xdf,
  0x6f, 0xdb, 0x36, 0x10, 0x7e, 0xef, 0x5f, 0xc1, 0xa0, 0x09, 0x28, 0xad,
  0x35, 0x29, 0xc7, 0x0d, 0x02, 0xd8, 0xb2, 0xfb, 0xd0, 0xed, 0xa1, 0x05,
  0xb6, 0x0c, 0x58, 0x81, 0x61, 0x8f, 0x34, 0x75, 0x91, 0x98, 0xc8, 0xa4,
  0x4a, 0xd2, 0x71, 0x8d, 0xc2, 0xff, 0xfb, 0x8e, 0x92, 0xac, 0x1f, 0x4e,
  0x3c, 0x34, 0x0f, 0x03, 0x6c, 0x98, 0xd4, 0xdd, 0x7d, 0xf7, 0xdd, 0xf1,
  0x3b, 0xca, 0xe9, 0xc5, 0xaf, 0x77, 0x9f, 0xbe, 0xfe, 0xf3, 0xe7, 0x6f,
  0xa4, 0xf0, 0x9b, 0x72, 0xf5, 0x26, 0x0d, 0x3f, 0xa4, 0x14, 0x3a, 0x5f,
  0x52, 0xd0, 0x74, 0xf5, 0x86, 0x90, 0xb4, 0x00, 0x91, 0x85, 0x05, 0x2e,
  0xbd, 0xf2, 0x25, 0xac, 0xfe, 0x56, 0x3a, 0x33, 0xbb, 0x8b, 0x94, 0x37,
  0xdb, 0xc6, 0x54, 0x2a, 0xfd, 0x48, 0x2c, 0x94, 0x4b, 0xea, 0xfc, 0xbe,
  0x04, 0x57, 0x00, 0x78, 0x4a, 0x0a, 0x0b, 0xf7, 0x4b, 0xca, 0xb9, 0x06,
  0x9f, 0x69, 0xc1, 0xd6, 0xc6, 0x78, 0xe7, 0xad, 0xa8, 0x64, 0xa6, 0x99,
  0x34, 0x1b, 0xde, 0x3d, 0xe0, 0x33, 0x36, 0x65, 0x53, 0x2e, 0x9d, 0xeb,
  0x9f, 0xb1, 0x8d, 0x42, 0x2f, 0xe7, 0xe8, 0xff, 0x97, 0x62, 0xe2, 0x0b,
  0xd8, 0xc0, 0x28, 0x51, 0xca, 0x8f, 0x05, 0xa7, 0x6b, 0x93, 0xed, 0x89,
  0x35, 0x25, 0x2c, 0x69, 0x66, 0xe4, 0x76, 0x03, 0xda, 0x1f, 0xb9, 0x64,
  0xea, 0x89, 0xc8, 0x52, 0x38, 0xb7, 0xa4, 0xd2, 0x68, 0x2f, 0x94, 0x06,
  0x4b, 0x6a, 0xb0, 0x89, 0x2b, 0xcc, 0x4e, 0x0a, 0x07, 0xb4, 0x0d, 0xdd,
  0xa0, 0xb1, 0x0d, 0x1b, 0x07, 0x56, 0x22, 0x87, 0x49, 0x48, 0x06, 0xb6,
  0xb3, 0x87, 0x7e, 0x4f, 0x57, 0x5f, 0x0b, 0xe5, 0x08, 0x7e, 0x76, 0x75,
  0xa7, 0x19, 0x52, 0x9a, 0x76, 0x00, 0x1c, 0x11, 0xba, 0x4d, 0xd5, 0xc7,
  0xe1, 0x4e, 0xb4, 0xcd, 0x78, 0x4b, 0x49, 0x26, 0xbc, 0x98, 0xb4, 0xad,
  0xf9, 0xd8, 0xc0, 0x2c, 0x3f, 0xdc, 0xd2, 0x63, 0xea, 0xb5, 0xd7, 0x04,
  0xbf, 0x93, 0x32, 0xaf, 0x7f, 0x2a, 0xab, 0x36, 0xc2, 0xee, 0xe9, 0xea,
  0xae, 0x02, 0x4d, 0xa6, 0x49, 0x72, 0x95, 0x72, 0xf1, 0x1a, 0xe8, 0xd9,
  0xcd, 0x39, 0x68, 0xb7, 0x95, 0x12, 0x42, 0x6b, 0x6b, 0xe8, 0xdb, 0x9b,
  0xd7, 0x22, 0x5f, 0xcf, 0xce, 0x21, 0x2b, 0x7d, 0x6f, 0x5a, 0xd8, 0x9b,
  0x57, 0x13, 0x3e, 0xcb, 0x77, 0x27, 0xac, 0x56, 0x3a, 0xef, 0x5a, 0xf1,
  0x5a, 0xe0, 0xe4, 0x1c, 0x70, 0x86, 0x43, 0x15, 0x0e, 0xfa, 0x53, 0x69,
  0x1c, 0x0c, 0x40, 0x53, 0x5e, 0xbd, 0x28, 0x0e, 0x6b, 0x72, 0x1b, 0x1a,
  0x47, 0x6a, 0xb1, 0xa3, 0x02, 0x95, 0xab, 0x4a, 0xb1, 0x9f, 0x6b, 0xa3,
  0x61, 0x28, 0x97, 0x10, 0xa3, 0xb2, 0x61, 0xc0, 0x09, 0xc2, 0x64, 0x2d,
  0x2c, 0x19, 0x6e, 0xba, 0x33, 0x69, 0x05, 0x7a, 0xb4, 0xa1, 0xa9, 0xcb,
  0xb6, 0x53, 0x99, 0x2f, 0xe6, 0x24, 0xb9, 0x5a, 0xd0, 0x61, 0xf5, 0x32,
  0x90, 0xcf, 0xfa, 0xe4, 0x23, 0x31, 0x8e, 0x36, 0xc5, 0xac, 0x66, 0xe5,
  0xbc, 0xf0, 0x5b, 0x3c, 0x7d, 0x94, 0xf0, 0xac, 0x1d, 0x9d, 0xde, 0x2d,
  0x75, 0xd2, 0xaa, 0xca, 0x13, 0x67, 0xe5, 0x92, 0x16, 0xde, 0x57, 0x6e,
  0xce, 0xb9, 0x78, 0x10, 0xdf, 0x59, 0x6e, 0x4c, 0x5e, 0x82, 0xa8, 0x94,
  0xab, 0x67, 0x38, 0x3c, 0xe3, 0xa5, 0x5a, 0x3b, 0xfe, 0xf0, 0x6d, 0x0b,
  0x76, 0xcf, 0x71, 0x90, 0xa7, 0x2c, 0x69, 0x77, 0xf5, 0xf8, 0x3e, 0xd4,
  0x49, 0x1a, 0xc0, 0x17, 0xd0, 0x7f, 0xfe, 0x7e, 0x78, 0x38, 0xbd, 0x81,
  0xce, 0x22, 0x1f, 0x6b, 0xbd, 0x8c, 0x28, 0xc3, 0x13, 0xa6, 0x31, 0x33,
  0x3a, 0xa2, 0xb2, 0x54, 0xf2, 0x91, 0x78, 0xb3, 0x95, 0x05, 0x16, 0x6f,
  0x3d, 0x7d, 0x4f, 0xee, 0xb7, 0x5a, 0x7a, 0x85, 0x46, 0x88, 0xc9, 0x8f,
  0xae, 0x79, 0x97, 0x2c, 0x07, 0x1f, 0x5d, 0x46, 0xc0, 0xd0, 0x0d, 0x97,
  0x31, 0x13, 0xde, 0xdb, 0x88, 0x76, 0xaa, 0xa2, 0x71, 0xbc, 0x68, 0xbd,
  0x0f, 0xdd, 0xea, 0x09, 0x0f, 0x73, 0x5b, 0xa1, 0x0f, 0x90, 0x65, 0x0f,
  0xfc, 0x0c, 0xf7, 0xcb, 0x5f, 0x77, 0x7f, 0x44, 0xbd, 0x2a, 0x27, 0xd3,
  0x21, 0x8d, 0x90, 0x61, 0x18, 0x81, 0xe7, 0x6a, 0xb4, 0x43, 0x2d, 0xb0,
  0xd2, 0xe4, 0x8d, 0x75, 0x31, 0x30, 0x86, 0x94, 0x15, 0x58, 0x89, 0xd7,
  0x1f, 0xde, 0x58, 0x98, 0xf6, 0x77, 0xe1, 0x0b, 0x66, 0xcd, 0x56, 0x67,
  0x11, 0xde, 0x15, 0xbf, 0xd4, 0x11, 0xcc, 0xe0, 0xbc, 0x68, 0x54, 0x11,
  0xe1, 0xe4, 0xc3, 0x6d, 0x3c, 0x02, 0xc0, 0x06, 0xbd, 0xed, 0xe4, 0x79,
  0x2c, 0xb3, 0x16, 0x1a, 0xb2, 0x3a, 0x4a, 0x8d, 0x92, 0x77, 0xc3, 0x2c,
  0xef, 0x08, 0xbd, 0xa2, 0xcf, 0x68, 0x78, 0xf8, 0xee, 0x91, 0x00, 0x6d,
  0x74, 0x48, 0x87, 0x66, 0x75, 0x4f, 0xa2, 0x41, 0xfc, 0x8a, 0x24, 0xe3,
  0x12, 0x49, 0x17, 0x1c, 0x98, 0x8e, 0x42, 0x0f, 0x83, 0xb5, 0xdb, 0x29,
  0x2f, 0x0b, 0xd2, 0xd4, 0x14, 0xd4, 0x0b, 0xa7, 0x30, 0xe1, 0x6e, 0x27,
  0xd3, 0xf9, 0x08, 0x2e, 0xdc, 0x18, 0x0b, 0xb2, 0xb6, 0x20, 0x1e, 0x17,
  0xcf, 0x9d, 0xaf, 0xe7, 0x23, 0xe2, 0x67, 0x9c, 0x0f, 0x27, 0xc5, 0xd4,
  0x21, 0x17, 0x7d, 0xb1, 0xa7, 0x3c, 0x4e, 0xba, 0x5a, 0x09, 0x8b, 0xa5,
  0x47, 0x31, 0x0b, 0xef, 0x9f, 0x28, 0x5e, 0xfc, 0x97, 0x6f, 0x80, 0xae,
  0xf1, 0x5f, 0x70, 0x6b, 0x27, 0x36, 0x66, 0x85, 0xca, 0x60, 0x8c, 0x73,
  0x20, 0x50, 0x62, 0x39, 0x3f, 0x47, 0xe3, 0x79, 0xf8, 0x09, 0x7e, 0x4f,
  0xe2, 0x05, 0xca, 0x7d, 0x33, 0x7a, 0xe9, 0x1f, 0x8e, 0x8b, 0x46, 0xff,
  0x7d, 0x80, 0x03, 0xff, 0x59, 0x7b, 0xb0, 0x4f, 0xa2, 0x8c, 0x1a, 0xdb,
  0x7b, 0x72, 0x9d, 0x24, 0x49, 0xeb, 0x30, 0x1c, 0xde, 0x94, 0x87, 0x97,
  0x3a, 0xfe, 0xd1, 0xe1, 0xcd, 0x1f, 0x9e, 0x7f, 0x01, 0xe4, 0x8d, 0x0a,
  0x50, 0x01, 0x09, 0x00, 0x00
};
unsigned int click_html_gz_len = 845;

/**
 * Print minified version of click.html
 * @param EthernetClient &client the connection
 */
void print_index_page(EthernetClient &client)
{
    client.println("HTTP/1.1 200 THIS IS WINDOW");
    client.println("Content-Type: text/html; charset=UTF-8");
    client.println("Content-Encoding: gzip");
    client.println();
    client.write(click_html_gz, click_html_gz_len);
}

