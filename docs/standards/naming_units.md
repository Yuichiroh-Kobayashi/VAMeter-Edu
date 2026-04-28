# Naming and Units Policy

## Variable names

Include physical quantity and unit in names where possible.

Good:

```cpp
float measuredVoltageV;
float measuredCurrentA;
float displayedCurrentA;
float csvSampleIntervalMs;
float currentLimitA;
```

Bad:

```cpp
float value;
float current;
float data;
float limit;
```

## Units

Use SI units or explicit classroom units:

- voltage: `V` or `mV`
- current: `A` or `mA`
- power: `W` or `mW`
- resistance: `Ohm`
- time: `s` or `ms`
- energy: `Wh`
- capacity: `Ah` or `mAh`
- frequency: `Hz`
- duty / ratio: `Percent`
- count: `Count`

## Display vs internal values

Do not derive internal calculation from localized display strings.
Display formatting is not measurement logic.
