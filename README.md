# CPE 301 Swamp Cooler Project

By Justin Juera <jjuera@unr.edu>

This repository contains the code and documentation for the CPE 301 Swamp
Cooler Project.

If you are a student looking to study or reuse parts of this code, please read
the License section at the bottom of this document.

## Components Used

- **Microcontroller**: [Arduino Atmega2560](https://www.arduino.cc/en/Main/ArduinoBoardMega2560)
  - This is the main controller for the swamp cooler system.
- **Temperature and Humidity Sensor**: [DHT11](https://docs.cirkitdesigner.com/component/a76bb123-3d3c-417d-b1f3-c8417efb7bc7)
  - Used to monitor both temperature and humidity levels.
- **Analog Water Level Detection Sensor**
  - Monitors the water level in the reservoir. If the water level is too low,
    the system will enter a non-operational state until the user refills the
    reservoir and presses the reset button.
- **LCD Display**: [LCD1602](https://docs.cirkitdesigner.com/component/e58c1528-e30a-4ff6-91ac-11313be5fedc/lcd1602)
  - Displays current temperature, humidity, and system status.
- **Stepper Motor**: [28BYJ-48](https://www.mouser.com/datasheet/2/758/stepd-01-data-sheet-1143075.pdf)
  - Used for the vents to control airflow direction.
- **Stepper Motor Driver**: [ULN2003](https://www.seeedstudio.com/document/pdf/ULN2003%20Datasheet.pdf)
  - Drives the stepper motor for vent control.
- **Real-Time Clock Module**: [DS1307](https://datasheets.maximintegrated.com/en/ds/DS1307.pdf)
  - Used to keep track of real-time for logging.
- **3-6V Motor with Propeller Fan Blade**
  - Provides airflow for the swamp cooler.
- **Motor Driver**: [L293D](https://docs.cirkitdesigner.com/component/f47b3186-2f6b-4ec1-9978-c189b4938930)
  - Used by the microcontroller to control the fan motor speed.
- **Power Supply Module**
  - Delivers the necessary power to the motors.
  - Note that the rest of the system is powered via USB from the microcontroller.
- **Push Buttons**
  - Used for user input to control vent direction, system reset, and system
    disable/enable.
- **LEDs**
  - Used for status indication.
- **Breadboard, jumper wires, and resistors**
  - Used for connecting components and building the circuit.

## System Overview

The swamp cooler system is designed to monitor environmental conditions and
adjust its operation accordingly. It uses a DHT11 sensor to measure temperature
and humidity, and an analog water level sensor to ensure there is sufficient
water in the reservoir. If the water level is too low, the system will not
operate until the user refills the reservoir and presses the reset button.

Note that the motors (fan and stepper motor for vents) are powered separately
from the microcontroller using a separate power supply module. There are two
breadboards used in this project: one for the microcontroller and sensors, and
another for the motors and their drivers. The motor breadboard and the
microcontroller breadboard are not electrically connected but share a common
ground. This separation is to prevent cases where the power supply for the
motors turns off while the microcontroller is still powered, which will cause
the motors to draw power from the microcontroller and potentially damage it.

### System States

The system has the following states:

- **Idle**: The system is powered on but not actively cooling.
  - Transitions to **Idle** state only during initial power-on. This is an
    implementation-specific detail where the state machine is designed so that
    each state has enter logic that runs once upon entering the state. When the
    system is powered on, it starts in the Idle state by default, but has to
    enter the Idle state again to run the enter logic.
  - Transitions to **Running** state if temperature exceeds threshold and water
    level is sufficient.
  - Transitions to **Error** state if water level is too low.
  - Transitions to **Disabled** state if user presses the enable/disable toggle
    button.
- **Running**: The system is actively cooling based on sensor readings.
  - Transitions to **Idle** state if temperature drops below threshold.
  - Transitions to **Error** state if water level is too low.
  - Transitions to **Disabled** state if user presses the enable/disable toggle
    button.
- **Error**: The system has detected an error condition (low water level) and is
  not operational. No cooling will occur until the user refills the reservoir and
  presses the reset button.
  - Transitions to **Idle** state if the user presses the reset button.
  - Transitions to **Disabled** state if user presses the enable/disable toggle
    button.
- **Disabled**: The user has manually disabled the system using the disable
  button. No cooling will occur until the user re-enables the system.
  - Transitions to **Idle** state if user presses the enable/disable toggle
    button.

## Circuit Image

![Circuit Image](./images/circuit.jpg)

## Schematic Diagram

![Schematic Diagram](./images/schematic.png)

Components used are described in the Components Used section above.

## System Demonstration

Link will be provided soon.

Addendum: the video demonstrates logging with real-time clock timestamps.
However, it did not include the year/month/day. A new commit was added to
include year/month/day in the log output. Here is an updated screenshot of the
log output with the full date and time:

![Updated Log Output](./images/addendum-datetime.png)

# License (read me if you are a student)

This project is licensed under the GPLv3 (or later) license. You are encouraged
to study and learn from this code.

If you copy, modify, or distribute any part of this project, you are required
to comply with the terms of the GPLv3 license. This includes preserving the
original copyright notice and providing access to the source code of any
derivative works.

This is to encourage open sharing of knowledge while discouraging any bad-faith
copying of the code. Because this license requires that derivative works
preserve the copyright notice, it will be clear if this code has been copied as
an attempt of plagiarism; otherwise, failure to preserve the copyright notice
would constitute a violation of the license. This still allows for modification
and learning, as independent adaptations and reimplementations from studying
the code are permitted by both the license and academic integrity policies.

You are free to modify and adapt this code (whether in part or in whole) as you
see fit, provided that any distributed derivative works remain licensed under
the GPLv3+ and include the appropriate attribution and license text.

An example of proper attribution of part of this code, such as reusing the
print integer function (`U0putint(int)` in the source) in your own code, would
be:

```c
/*
 * U0putint function
 * 
 * Originally written by Justin Juera
 * Source: https://github.com/jjuera-unr/cpe301-swamp-cooler
 * Licensed under GPLv3 or later
 *
 * This function has been reused here with attribution in accordance
 * with the original license.
 */
void U0putint(int, int = 0);
void U0putint(int value, int leadingZeros) {
    // render negative numbers correctly by prepending negative sign and printing
    // as positive
    if (value < 0) {
        U0putchar('-');
        value = -value;
    }

    const char digits[] = "0123456789";
    int temp = value;
    int count = 0;

    // count number of digits for leading zeros
    if (temp == 0) {
        count = 1;
    } else {
        while (temp > 0) {
            temp /= 10;
            count++;
        }
    }

    while (leadingZeros > count) {
        U0putchar('0');
        leadingZeros--;
    }

    if (value >= 10) {
        U0putint(value / 10, 0);
    }

    U0putchar(digits[value % 10]);
}
```
