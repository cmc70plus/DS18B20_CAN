#include "can_temperature.h"
#include "can.h"

#define CAN_TEMPERATURE_STD_ID  0x181U

static uint8_t sequence_number;

/* Install an accept-all filter. Transmission still works without RX interrupts. */
HAL_StatusTypeDef CAN_Temperature_Start(void)
{
  CAN_FilterTypeDef filter = {0};

  filter.FilterBank = 0U;
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterIdHigh = 0U;
  filter.FilterIdLow = 0U;
  filter.FilterMaskIdHigh = 0U;
  filter.FilterMaskIdLow = 0U;
  filter.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter.FilterActivation = ENABLE;
  filter.SlaveStartFilterBank = 14U;

  if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_CAN_Start(&hcan1);
}

/*
 * Frame layout (little endian):
 *   byte 0    sensor status
 *   byte 1    rolling sequence number
 *   byte 2-3  signed DS18B20 raw value (1/16 degC)
 *   byte 4-7  signed temperature in 0.001 degC
 */
HAL_StatusTypeDef CAN_Temperature_Send(CAN_TemperatureStatus status,
                                       int16_t raw_temperature,
                                       int32_t temperature_milli_c)
{
  CAN_TxHeaderTypeDef header = {0};
  uint8_t data[8];
  uint32_t mailbox;

  header.StdId = CAN_TEMPERATURE_STD_ID;
  header.ExtId = 0U;
  header.IDE = CAN_ID_STD;
  header.RTR = CAN_RTR_DATA;
  header.DLC = 8U;
  header.TransmitGlobalTime = DISABLE;

  data[0] = (uint8_t)status;
  data[1] = sequence_number++;
  data[2] = (uint8_t)((uint16_t)raw_temperature & 0xFFU);
  data[3] = (uint8_t)(((uint16_t)raw_temperature >> 8U) & 0xFFU);
  data[4] = (uint8_t)((uint32_t)temperature_milli_c & 0xFFU);
  data[5] = (uint8_t)(((uint32_t)temperature_milli_c >> 8U) & 0xFFU);
  data[6] = (uint8_t)(((uint32_t)temperature_milli_c >> 16U) & 0xFFU);
  data[7] = (uint8_t)(((uint32_t)temperature_milli_c >> 24U) & 0xFFU);

  /*
   * Put a frame into a hardware transmit mailbox.  HAL_OK only means that the
   * mailbox accepted it; a second active CAN node must ACK it on the bus.
   * Set a breakpoint here to inspect header and data before transmission.
   */
  return HAL_CAN_AddTxMessage(&hcan1, &header, data, &mailbox);
}
