# Validation record

## Evidence source

The verified results below were extracted from the supplied June 2022 physical-prototype demonstration video. The repository does not claim measurements that were absent from that recording.

## Observed sequence

1. The LCD displayed stable ambient gas readings of approximately 15-26 raw ADC counts.
2. A lighter was used as a controlled combustible-gas source near the MQ-5 module.
3. The displayed raw value increased to 721.
4. Local alarm indication and ventilation-fan operation were demonstrated.
5. A remote phone received the text `flame or gas detected`.

## What this proves

- The prototype was assembled and operated on physical hardware.
- The MQ-5 signal was acquired and presented on the LCD.
- A high gas reading reached the implemented alarm path.
- Local mitigation and GSM notification were demonstrated.

## What this does not prove

- Calibrated gas concentration in ppm
- Repeatability, response time, recovery time, or false-alarm rate
- Coverage area or airflow performance
- A separate flame-only validation sequence
- Operation during mains failure or cellular-network loss
- Compliance with certified gas- or fire-alarm standards

## Recommended repeatable test plan

For any future rebuild, record the sensor warm-up time, ambient baseline, alarm-on and alarm-off values, response and recovery times, SMS delivery time, modem failure behavior, relay/fan current, and at least three repeated trials for each test condition. Use safe, controlled test methods and suitable ventilation.

