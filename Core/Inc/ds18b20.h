#ifndef DS18B20_H
#define DS18B20_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef enum
{
  DS18B20_OK = 0,
  DS18B20_ERROR_NO_DEVICE = -1,
  DS18B20_ERROR_CRC = -2,
  DS18B20_ERROR_TIMEOUT = -3
} DS18B20_Status;

void DS18B20_Init(void);
DS18B20_Status DS18B20_ReadTemperature(int16_t *raw_temperature,
                                       int32_t *temperature_milli_c);

#ifdef __cplusplus
}
#endif

#endif /* DS18B20_H */
