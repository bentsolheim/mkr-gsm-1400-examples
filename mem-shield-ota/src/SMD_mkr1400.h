#ifndef MEM_SHIELD_OTA_SMD_MKR1400_H
#define MEM_SHIELD_OTA_SMD_MKR1400_H

__attribute__ ((section(".sketch_boot")))
unsigned char sduBoot[0x4000] = {
#include "boot/mkrgsm1400.h"
};

#endif //MEM_SHIELD_OTA_SMD_MKR1400_H
