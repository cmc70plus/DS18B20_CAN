#ifndef CAN_TEMPERATURE_H
#define CAN_TEMPERATURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef enum
{
  CAN_TEMPERATURE_STATUS_OK = 0U,
  CAN_TEMPERATURE_STATUS_NO_DEVICE = 1U,
  CAN_TEMPERATURE_STATUS_CRC_ERROR = 2U,
  CAN_TEMPERATURE_STATUS_TIMEOUT = 3U
} CAN_TemperatureStatus;

HAL_StatusTypeDef CAN_Temperature_Start(void);
HAL_StatusTypeDef CAN_Temperature_Send(CAN_TemperatureStatus status,
                                       int16_t raw_temperature,
                                       int32_t temperature_milli_c);

#ifdef __cplusplus
}
#endif

#endif /* CAN_TEMPERATURE_H */
