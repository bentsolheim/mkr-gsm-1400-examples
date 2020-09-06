#include <Client.h>
#include <SD.h>
#include "HttpToSdDownloader.h"

int getFileNameLength(const char *targetFileName);

int findLastIndex(const char *str, char x);

int downloadToSdFile(Client *client, const char *targetFileName);

int HttpToSdDownloader::download(const char *host, int port, const char *path, const char *targetFileName) {
    int result;
    if (client->connected()) {
        result = downloadReuseConnection(host, path, targetFileName);
    } else {
        result = downloadReconnect(host, port, path, targetFileName);
    }
    return result;
}

int HttpToSdDownloader::downloadReuseConnection(const char *host, const char *path, const char *targetFileName) {
    if (!prepareGetRequest(host, path)) {
        return ERROR_HTTP_WRITE_ERROR;
    }
    client->println("Connection: keep-alive");
    client->println();

    return downloadToSdFile(client, targetFileName);
}

int HttpToSdDownloader::downloadReconnect(const char *host, int port, const char *path, const char *targetFileName) {
    if (!client->connect(host, port)) {
        return ERROR_HTTP_CONNECTION_FAILED;
    }

    if (!prepareGetRequest(host, path)) {
        return ERROR_HTTP_WRITE_ERROR;
    }
    client->println("Connection: close");
    client->println();

    int status = downloadToSdFile(client, targetFileName);

    client->stop();

    return status;
}

int HttpToSdDownloader::prepareGetRequest(const char *host, const char *path) {
    char requestLine[max(sizeof(host), strlen(path) + 14)];
    snprintf(requestLine, sizeof(requestLine), "GET %s HTTP/1.1", path);
    if (client->println(requestLine) == 0) {
        return 0;
    }
    snprintf(requestLine, sizeof(requestLine), "Host: %s", host);
    if (client->println(requestLine) == 0) {
        return 0;
    }
    return 1;
}

int HttpToSdDownloader::download(const char *targetFileName) {
    return downloadToSdFile(client, targetFileName);
}

int downloadToSdFile(Client *client, const char *targetFileName) {
    if (getFileNameLength(targetFileName) > 8) {
        return ERROR_FILE_NAME_TOO_LONG;
    }
    File targetFile = SD.open(targetFileName, O_CREAT | O_WRITE | O_TRUNC);
    if (!targetFile) {
        return ERROR_FILE_OPEN;
    }

    return downloadToStream(client, &targetFile);
}

int downloadToStream(Client *client, Stream *target) {

    int contentLength, status;
    contentLength = status = readHeaderSection(client);
    if (status == -1) {
        return ERROR_HTTP_READ_ERROR;
    } else if (status == 0) {
        return ERROR_CONTENT_LENGTH_UNKNOWN;
    }

    int bytesCopied = copyStream(client, target);
    if (bytesCopied == -1) {
        return ERROR_FILE_WRITE;
    }

    target->flush();
    if (status < -1) {
        return status;
    }
    if (bytesCopied != contentLength) {
        return ERROR_CONTENT_LENGTH_MISMATCH;
    }

    return DOWNLOAD_OK;
}

int getStatusCode(char httpStatusLine[]) {
    // HTTP/1.1 404 Not Found
    char *statusCode = httpStatusLine;
    while (statusCode[0] != ' ') statusCode++;

    return strtol(statusCode, nullptr, 10);
}

/**
 * Reads the HTTP header section from the client and stops right before the response body starts. Will return the
 * value of the Content-Type header if it has been set. 0 otherwise. Returns -1 on any errors reading the http stream,
 * and in case of non 200 HTTP status code the status code multiplied by -1 is returned.
 * The method assumes that no bytes have been previously read from the response.
 * @param client
 * @return
 */
int readHeaderSection(Client *client) {

    if (!client->connected()) {
        return -1;
    }
    int contentLength = 0;
    int statusCode = 0;

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
            statusCode = getStatusCode(line);
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
    if (statusCode != 200) {
        return statusCode * -1;
    }
    return contentLength;
}

int getFileNameLength(const char *targetFileName) {
    int fileNameStartIndex = findLastIndex(targetFileName, '/') + 1;
    int fileNameEndIndex = findLastIndex(targetFileName, '.') - 1;
    if (fileNameEndIndex < 0) {
        fileNameEndIndex = strlen(targetFileName) - 1;
    }
    int fileNameLength = fileNameEndIndex - fileNameStartIndex + 1;
    return fileNameLength;
}

int findLastIndex(const char *str, char x) {
    for (int i = strlen(str); i >= 0; i--) {
        if (str[i] == x) {
            return i;
        }
    }
    return -1;
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
