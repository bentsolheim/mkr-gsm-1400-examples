#include <Arduino.h>
#include <MKRGSM.h>
#include <SD.h>

__attribute__ ((section(".sketch_boot")))
unsigned char sduBoot[0x4000] = {
#include "boot/mkrgsm1400.h"
};

#include "HttpToSdDownloader.h"

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

    client.setTimeout(1000);
    if (!client.connect("raw.githubusercontent.com", 443)) {
        //
    }
    HttpToSdDownloader downloader(&client);

    int status = downloader.download("raw.githubusercontent.com", 443,
                                     "/bentsolheim/mkr-gsm-1400-examples/master/mem-shield-ota/platformio.ini",
                                     "/test/pio.ini");

    Serial.println();
    if (status != DOWNLOAD_OK) {
        Serial.print("Download failed. Error code: ");
        Serial.println(status);
    } else {
        Serial.println("Download success");
    }

    status = downloader.download("raw.githubusercontent.com", 443,
                                     "/bentsolheim/mkr-gsm-1400-examples/master/mem-shield-ota/diagram.png",
                                     "/test/diagram.png");

    Serial.println();
    if (status != DOWNLOAD_OK) {
        Serial.print("Download failed. Error code: ");
        Serial.println(status);
    } else {
        Serial.println("Download success");
    }

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