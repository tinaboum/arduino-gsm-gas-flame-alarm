# Hardware and reconstructed pin mapping

## Verified hardware

| Function | Component |
| --- | --- |
| Main controller | Arduino Uno |
| Combustible-gas input | MQ-5 analog sensor module |
| Flame input | Digital IR flame-sensor module |
| Local display | 16x2 LCD with I2C backpack |
| Cellular communication | Neoway M660 GSM/GPRS module |
| Audible indication | Active buzzer |
| Visual indication | Red and green LEDs |
| Ventilation switching | Relay module and 12 V DC fan |
| Controller/modem supply | 5 V domain shown in the project material |
| Fan supply | Separate 12 V domain |

## Firmware pin mapping

The following mapping was reconstructed from the available project material and must be checked against the physical wiring before energizing the system.

| Arduino signal | Pin | Connected function |
| --- | ---: | --- |
| `GAS_SENSOR_PIN` | A2 | MQ-5 analog output |
| `FLAME_SENSOR_PIN` | D5 | Flame-module digital output |
| `BUZZER_PIN` | D2 | Buzzer control |
| `GREEN_LED_PIN` | D7 | Normal-state indicator |
| `RED_LED_PIN` | D8 | Alarm-state indicator |
| `FAN_RELAY_PIN` | D10 | Ventilation relay input |
| `GSM_RX_PIN` | D9 | Arduino RX from modem TX |
| `GSM_TX_PIN` | D12 | Arduino TX to modem RX |
| I2C SDA | A4 | LCD backpack SDA |
| I2C SCL | A5 | LCD backpack SCL |

## Power-domain notes

The project demonstration shows the fan on a 12 V supply and the controller/GSM electronics on a separate 5 V supply. The exact carrier board and regulator design are not fully documented. Verify:

- GSM supply voltage and peak-current capability
- UART logic-level compatibility
- Common signal reference between interconnected boards
- Relay coil voltage and active polarity
- Flyback and switching protection provided by the relay/driver module
- Fan running and startup current

