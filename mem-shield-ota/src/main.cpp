#include <Arduino.h>
#include <MKRGSM.h>
#include "secrets.h"

GSM gsm;
GPRS gprs;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);

    gsm.begin(SECRET_PINNUMBER);
    gprs.attachGPRS(SECRET_GPRS_APN, SECRET_GPRS_LOGIN, SECRET_GPRS_PASSWORD);

    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
}

void loop() {
    yield();
}