#include <Arduino.h>
#include <MKRGSM.h>
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


static const int DOWNLOAD_OK = 0;
static const int ERROR_HTTP_CONNECTION_FAILED = -1;
static const int ERROR_HTTP_READ_ERROR = -4;
static const int ERROR_FILE_WRITE = -2;
static const int ERROR_FILE_OPEN = -3;
static const int ERROR_CONTENT_LENGTH_MISMATCH = -5;
static const int ERROR_CONTENT_LENGTH_UNKNOWN = -6;

int downloadFileToSdFile(Client *client, const char *host, int port, const char *path, const char *targetFileName);

int downloadFileToStream(Client *client, const char *host, int port, const char *path, Stream *target);

int downloadToStream(Client *client, Stream *target);

int readHeaderSection(Client *client);

int copyStream(Stream *source, Stream *target);

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
    int status = downloadFileToSdFile(&client, "raw.githubusercontent.com", 443,
                                      "/bentsolheim/mkr-gsm-1400-examples/master/mem-shield-ota/platformio.ini",
                                      "/test/pio.ini");
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


int downloadFileToSdFile(Client *client, const char *host, int port, const char *path, const char *targetFileName) {

    File sdFile = SD.open(targetFileName, O_CREAT | O_WRITE | O_TRUNC);
    if (!sdFile) {
        return ERROR_FILE_OPEN;
    }
    return downloadFileToStream(client, host, port, path, &sdFile);
}

int downloadFileToStream(Client *client, const char *host, int port, const char *path, Stream *target) {

    if (!client->connect(host, port)) {
        return ERROR_HTTP_CONNECTION_FAILED;
    }

    char requestLine[max(sizeof(host), strlen(path) + 14)];
    snprintf(requestLine, sizeof(requestLine), "GET %s HTTP/1.1", path);
    Serial.println(requestLine);
    client->println(requestLine);
    snprintf(requestLine, sizeof(requestLine), "Host: %s", host);
    client->println(requestLine);
    client->println("Connection: close");
    client->println();

    return downloadToStream(client, target);
}

/**
 * Reads the HTTP header section from the client and stops right before the response body starts. Will return the
 * value of the Content-Type header if it has been set. 0 otherwise. Returns -1 on any errors reading the http stream.
 * The method assumes that no bytes have been previously read from the response.
 * @param client
 * @return
 */
int readHeaderSection(Client *client) {

    if (!client->connected()) {
        return -1;
    }
    int contentLength = 0;
    char line[1000]; // Have not figured out how to make the line length arbitrary
    while (true) {
        size_t readBytes = client->readBytesUntil('\n', line, sizeof(line));
        if (readBytes == 0) {
            return -1;
        }
        if (readBytes == 1 && line[0] == '\r') {
            // We have encountered a line with only \r\n, which indicates end of http header section
            break;
        }
        line[readBytes - 1] = '\0'; // This will overwrite the \r preceding the \n char - we don't need that

        int headerNameEndIndex = 0;
        while (headerNameEndIndex < readBytes && line[headerNameEndIndex] != ':') headerNameEndIndex++;

        if (readBytes == headerNameEndIndex) {
            // This line does not contain a ':', which means this is the http status line.
            // Should probably read the status code here.
            continue;
        }

        // Get the http header name from the line
        char headerName[headerNameEndIndex + 1];
        for (int i = 0; i < headerNameEndIndex; i++) {
            headerName[i] = tolower(line[i]);
        }
        headerName[headerNameEndIndex] = '\0';

        // Get the http header value from the line
        char value[readBytes - headerNameEndIndex];
        int valueStartIndex = headerNameEndIndex + 1; // The header name ends at ':'. Start right after that.
        while (line[valueStartIndex] == ' ') valueStartIndex++; // Skip any white spaces after ':'
        for (int i = valueStartIndex; i < readBytes; i++) {
            value[i - valueStartIndex] = line[i];
        }
        value[readBytes - valueStartIndex] = '\0';

        if (strcmp(headerName, "content-length") == 0) {
            contentLength = atoi(value);
        }
    }
    return contentLength;
}

int downloadToStream(Client *client, Stream *target) {

    int contentLength = readHeaderSection(client);
    if (contentLength == -1) {
        return ERROR_HTTP_READ_ERROR;
    } else if (contentLength == 0) {
        return ERROR_CONTENT_LENGTH_UNKNOWN;
    }

    int bytesCopied = copyStream(client, target);
    if (bytesCopied == -1) {
        return ERROR_FILE_WRITE;
    }

    if (bytesCopied != contentLength) {
        return ERROR_CONTENT_LENGTH_MISMATCH;
    }

    target->flush();
    client->stop();

    return DOWNLOAD_OK;
}

int copyStream(Stream *source, Stream *target) {
    char c;
    int bytesCopied = 0;
    while (source->available()) {
        c = source->read();
        if (target->print(c) == 0) {
            return -1;
        }
        bytesCopied++;
    }
    return bytesCopied;
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