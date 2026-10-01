#include "ds18b20.h"

#define DS18B20_CONVERT_TIMEOUT_MS  800U

/* Busy-wait delay based on the Cortex-M4 cycle counter (168 cycles/us). */
static void DS18B20_DelayUs(uint32_t microseconds)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = microseconds * (SystemCoreClock / 1000000U);

  while ((uint32_t)(DWT->CYCCNT - start) < ticks)
  {
  }
}

/* Drive the open-drain PC13 output low to start a 1-Wire time slot. */
static void DS18B20_DriveLow(void)
{
  DS18B20_GPIO_Port->BSRR = ((uint32_t)DS18B20_Pin << 16U);
}

/* Write high to an open-drain output: the external 4.7 kOhm resistor pulls DQ up. */
static void DS18B20_Release(void)
{
  DS18B20_GPIO_Port->BSRR = DS18B20_Pin;
}

static uint8_t DS18B20_ReadLine(void)
{
  return ((DS18B20_GPIO_Port->IDR & DS18B20_Pin) != 0U) ? 1U : 0U;
}

static uint32_t DS18B20_EnterCritical(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void DS18B20_ExitCritical(uint32_t primask)
{
  if (primask == 0U)
  {
    __enable_irq();
  }
}

/* Reset the 1-Wire bus and sample the DS18B20 presence pulse. */
static uint8_t DS18B20_Reset(void)
{
  uint8_t present;
  uint32_t primask = DS18B20_EnterCritical();

  DS18B20_DriveLow();
  DS18B20_DelayUs(480U);
  DS18B20_Release();
  DS18B20_DelayUs(70U);
  present = (DS18B20_ReadLine() == 0U) ? 1U : 0U;
  DS18B20_DelayUs(410U);

  DS18B20_ExitCritical(primask);
  return present;
}

static void DS18B20_WriteBit(uint8_t bit_value)
{
  uint32_t primask = DS18B20_EnterCritical();

  DS18B20_DriveLow();
  if (bit_value != 0U)
  {
    DS18B20_DelayUs(6U);
    DS18B20_Release();
    DS18B20_DelayUs(64U);
  }
  else
  {
    DS18B20_DelayUs(60U);
    DS18B20_Release();
    DS18B20_DelayUs(10U);
  }

  DS18B20_ExitCritical(primask);
}

static uint8_t DS18B20_ReadBit(void)
{
  uint8_t bit_value;
  uint32_t primask = DS18B20_EnterCritical();

  DS18B20_DriveLow();
  DS18B20_DelayUs(3U);
  DS18B20_Release();
  DS18B20_DelayUs(10U);
  bit_value = DS18B20_ReadLine();
  DS18B20_DelayUs(53U);

  DS18B20_ExitCritical(primask);
  return bit_value;
}

static void DS18B20_WriteByte(uint8_t data)
{
  uint8_t bit_index;

  for (bit_index = 0U; bit_index < 8U; ++bit_index)
  {
    DS18B20_WriteBit(data & 0x01U);
    data >>= 1U;
  }
}

static uint8_t DS18B20_ReadByte(void)
{
  uint8_t data = 0U;
  uint8_t bit_index;

  for (bit_index = 0U; bit_index < 8U; ++bit_index)
  {
    data |= (uint8_t)(DS18B20_ReadBit() << bit_index);
  }

  return data;
}

/* Dallas/Maxim CRC-8, reflected polynomial 0x8C (normal form 0x31). */
static uint8_t DS18B20_Crc8(const uint8_t *data, uint8_t length)
{
  uint8_t crc = 0U;
  uint8_t byte_index;

  for (byte_index = 0U; byte_index < length; ++byte_index)
  {
    uint8_t in_byte = data[byte_index];
    uint8_t bit_index;

    for (bit_index = 0U; bit_index < 8U; ++bit_index)
    {
      uint8_t mix = (uint8_t)((crc ^ in_byte) & 0x01U);
      crc >>= 1U;
      if (mix != 0U)
      {
        crc ^= 0x8CU;
      }
      in_byte >>= 1U;
    }
  }

  return crc;
}

void DS18B20_Init(void)
{
  /* DWT is disabled after reset; enable trace and its cycle counter first. */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DS18B20_Release();
}

DS18B20_Status DS18B20_ReadTemperature(int16_t *raw_temperature,
                                       int32_t *temperature_milli_c)
{
  uint8_t scratchpad[9];
  uint8_t index;
  uint32_t start_tick;
  int16_t raw;

  if ((raw_temperature == NULL) || (temperature_milli_c == NULL))
  {
    return DS18B20_ERROR_NO_DEVICE;
  }

  /* A missing presence pulse normally means wiring, power or pull-up trouble. */
  if (DS18B20_Reset() == 0U)
  {
    return DS18B20_ERROR_NO_DEVICE;
  }

  DS18B20_WriteByte(0xCCU); /* Skip ROM: one sensor on the bus. */
  DS18B20_WriteByte(0x44U); /* Convert T. */

  /* The device returns 0 while converting and 1 when the result is ready. */
  start_tick = HAL_GetTick();
  while (DS18B20_ReadBit() == 0U)
  {
    if ((HAL_GetTick() - start_tick) >= DS18B20_CONVERT_TIMEOUT_MS)
    {
      return DS18B20_ERROR_TIMEOUT;
    }
    HAL_Delay(1U);
  }

  if (DS18B20_Reset() == 0U)
  {
    return DS18B20_ERROR_NO_DEVICE;
  }

  DS18B20_WriteByte(0xCCU); /* Skip ROM. */
  DS18B20_WriteByte(0xBEU); /* Read Scratchpad. */

  for (index = 0U; index < 9U; ++index)
  {
    scratchpad[index] = DS18B20_ReadByte();
  }

  /* Reject corrupted scratchpad data before converting it to a temperature. */
  if (DS18B20_Crc8(scratchpad, 8U) != scratchpad[8])
  {
    return DS18B20_ERROR_CRC;
  }

  /* Signed 16-bit raw value uses a scale of 1/16 degree Celsius per LSB. */
  raw = (int16_t)(((uint16_t)scratchpad[1] << 8U) | scratchpad[0]);
  *raw_temperature = raw;
  *temperature_milli_c = ((int32_t)raw * 1000) / 16;

  return DS18B20_OK;
}
