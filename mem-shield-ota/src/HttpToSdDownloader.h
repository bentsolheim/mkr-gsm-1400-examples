#ifndef MEM_SHIELD_OTA_HTTPTOSDDOWNLOADER_H
#define MEM_SHIELD_OTA_HTTPTOSDDOWNLOADER_H

#include <Client.h>


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


class HttpToSdDownloader {
public:
    explicit HttpToSdDownloader(Client *pClient) {
        this->client = pClient;
    }

    int download(const char *host, int port, const char *path, const char *targetFileName);

private:
    Client *client;
};

#endif //MEM_SHIELD_OTA_HTTPTOSDDOWNLOADER_H
