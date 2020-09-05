#include <Client.h>
#include <SD.h>
#include "HttpToSdDownloader.h"

int HttpToSdDownloader::download(const char *host, int port, const char *path, const char *targetFileName) {
    return downloadFileToSdFile(this->client, host, port, path, targetFileName);
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

