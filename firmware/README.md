# Firmware setup

Open `gsm_gas_flame_alarm/gsm_gas_flame_alarm.ino` in the Arduino IDE and select **Arduino Uno** as the target board.

## Required Arduino libraries

- `Wire` - included with the Arduino AVR core
- `SoftwareSerial` - included with the Arduino AVR core
- `LiquidCrystal_I2C` - install a version compatible with `lcd.init()` and `lcd.backlight()`

## Configure before uploading

Review these constants near the top of the sketch:

```cpp
constexpr uint8_t LCD_ADDRESS = 0x27;
constexpr bool FLAME_ACTIVE_LOW = true;
constexpr bool RELAY_ACTIVE_LOW = true;
constexpr uint16_t GAS_ALARM_ON_THRESHOLD = 300;
constexpr uint16_t GAS_ALARM_OFF_THRESHOLD = 250;
constexpr long GSM_BAUD = 9600;
const char ALERT_PHONE_NUMBER[] = "+000000000000";
```

The threshold values are functional placeholders, not measured ppm limits. Determine safe thresholds using controlled calibration with the actual sensor, enclosure, airflow, and target gas.

## Electrical checks

1. Confirm that the GSM carrier board accepts the Arduino UART logic level.
2. Power the GSM module from a supply capable of its transient current demand; do not assume the Uno 5 V pin is sufficient.
3. Use a relay module or driver stage appropriate for the 12 V fan current.
4. Join signal grounds where required while maintaining the correct 5 V and 12 V power domains.
5. Confirm relay polarity before connecting the fan.

## Safety

This code is intended for prototype evaluation. It has not been tested or certified against fire-alarm, gas-detection, EMC, electrical-safety, or functional-safety standards.

