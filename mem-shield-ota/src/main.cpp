#include <Arduino.h>
#include <MKRGSM.h>
#include <ArduinoHttpClient.h>
#include <SD.h>

__attribute__ ((section(".sketch_boot")))
unsigned char sduBoot[0x4000] = {
#include "boot/mkrgsm1400.h"
};

#if __has_include("secrets.h")

#include "secrets.h"

#else
#include "default_secrets.h"
#endif

GSM gsm;
GPRS gprs;
GSMSSLClient client;


const int SDchipSelect = 4;

void printDirectory(File dir, int numTabs);

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);

    Serial.begin(9600);
    while (!Serial) delay(100);

    if (SD.begin(SDchipSelect)) {
        Serial.println("SD OK");
    } else {
        Serial.println("SD Failed");
        return;
    }
    char outputFolder[] = "/test";
    File root = SD.open(outputFolder);
    printDirectory(root, 0);

    Serial.println("Network start");
    bool connected = false;
    while (!connected) {
        if ((gsm.begin(SECRET_PINNUMBER) == GSM_READY) &&
            (gprs.attachGPRS(SECRET_GPRS_APN, SECRET_GPRS_LOGIN, SECRET_GPRS_PASSWORD) == GPRS_READY)) {
            connected = true;
        } else {
            Serial.println("Not connected");
            delay(1000);
        }
    }

    digitalWrite(LED_BUILTIN, LOW);

    if (SD.exists(outputFolder)) {
        Serial.println("Output folder exists");
    } else {
        Serial.println("Creating output folder");
        if (!SD.mkdir(outputFolder)) {
            Serial.println("Unable to create folder");
        }
    }
    File textFile = SD.open("/test/test.txt", O_CREAT | O_WRITE);
    if (textFile.println(__TIME__) == 0) {
        Serial.println("Could not write to text file");
    }
    textFile.flush();
    textFile.close();

    char outputFile[20];
    sprintf(outputFile, "%s/pio.ini", outputFolder);
    File myFile = SD.open(outputFile, O_CREAT | O_WRITE | O_TRUNC);
    Serial.println("File ready");
    // https://raw.githubusercontent.com/bentsolheim/mkr-gsm-1400-examples/master/mem-shield-ota/platformio.ini
    char server[] = "raw.githubusercontent.com";
    int port = 443;
    HttpClient http(client, server, port);
    Serial.println("getting");
    http.get("/bentsolheim/mkr-gsm-1400-examples/master/mem-shield-ota/platformio.ini");
    Serial.println("got");
    http.skipResponseHeaders();

    Serial.println("reading");
    while (true) {
        if (http.available()) {
            int i = http.read();
            Serial.write(i);
            myFile.write(i);
        }
        if (!http.available() && !http.connected()) {
            break;
        }
        delayMicroseconds(10);
    }
    Serial.println("read");

    http.stop();
    myFile.flush();
    myFile.close();
    Serial.println("done.");

    root = SD.open(outputFolder);
    printDirectory(root, 0);
}

void loop() {
    Serial.print(__DATE__);
    Serial.print(" ");
    Serial.println(__TIME__);
    delay(5000);
}

void printDirectory(File dir, int numTabs) {
    while (true) {

        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }
        for (uint8_t i = 0; i < numTabs; i++) {
            Serial.print('\t');
        }
        Serial.print(entry.name());
        if (entry.isDirectory()) {
            Serial.println("/");
            printDirectory(entry, numTabs + 1);
        } else {
            // files have sizes, directories do not
            Serial.print("\t\t");
            Serial.println(entry.size(), DEC);
        }
        entry.close();
    }
}