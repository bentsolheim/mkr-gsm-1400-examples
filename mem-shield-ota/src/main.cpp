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

char outputFolder[] = "/test";

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
}

void loop() {
    Serial.print(__DATE__);
    Serial.print(" ");
    Serial.println(__TIME__);

    int status;
    unsigned long start;

    start = millis();
    if (!client.connect("raw.githubusercontent.com", 443)) {
        Serial.println("Unable to connect");
    } else {
        Serial.print("Connected in ");
        Serial.print(millis() - start);
        Serial.println(" millis");
    }
    HttpToSdDownloader downloader(&client);

    struct p {
        const char *path;
        const char *targetFileName;
    };

    p filesToDownload[] = {
            p{
                    .path = "/bentsolheim/mkr-gsm-1400-examples/master/mem-shield-ota/platformio2.ini",
                    .targetFileName = "/test/pio2.ini"
            },
            p{
                    .path = "/bentsolheim/mkr-gsm-1400-examples/master/mem-shield-ota/platformio.ini",
                    .targetFileName = "/test/pio.ini"
            },
            p{
                    .path = "/bentsolheim/mkr-gsm-1400-examples/master/mem-shield-ota/diagram.png",
                    .targetFileName = "/test/diagram.png"
            },
    };
    
    for (auto & i : filesToDownload) {

        const char *path = i.path;
        const char *targetFileName = i.targetFileName;
        start = millis();
        status = downloader.downloadReuseConnection("raw.githubusercontent.com", path, targetFileName);

        if (status != DOWNLOAD_OK) {
            Serial.print("Download of ");
            Serial.print(path);
            Serial.print(" failed. Error code: ");
            Serial.println(status);
        } else {
            Serial.print("Downloaded ");
            Serial.print(path);
            Serial.print(" to ");
            Serial.print(targetFileName);
            Serial.print(" in ");
            Serial.print(millis() - start);
            Serial.println(" millis");
        }
    }

    client.stop();

    File root = SD.open(outputFolder);
    printDirectory(root, 0);

    delay(15000);
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