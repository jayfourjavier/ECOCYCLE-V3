# ECOCYCLE V3

## Design Considerations

Safety, maintainability, and ease of use were the main design considerations for this project. The system was designed to be safe for users, with protective covers and enclosures for moving parts and electrical components. The system was also designed to be easy to maintain, with pluggable interfaces and modular components that can be easily replaced or repaired. Finally, the system was designed to be easy to use, with a user-friendly interface and clear instructions for operation.

- **Push Button Latching Circuit using Magnetic Contactor for main power supply** - this setup was chosen to ensure that the main power supply can be safely turned on and off, while also ensuring that the system stays powered off after power interruption unless turned on by an operator. An emergency stop button was also included in the design to allow for immediate power shutdown of the system in case of an emergency.
- **Pluggable Interfaces using Pinheaders and Protoboard** - the system was designed with pluggable interfaces to facilitate easier assembly, troubleshooting, and parts replacement. This design choice allows for quick and easy maintenance of the system, reducing downtime and increasing overall efficiency.
- **Cable Management using Cable Trays** - cable trays were used to organize and protect the wiring of the system. This design choice ensures that the wiring is neat and organized, reducing the risk of damage or interference with other components. AC and DC wiring were separated to prevent electromagnetic interference, as well as signal wires are separated with motor power wires.
- **Protective Covers and Enclosures** - protective covers and enclosures were included in the design to ensure that moving parts and electrical components are safely enclosed. This design choice reduces the risk of injury to users and protects the components from damage or interference.
- **Metal Control Panel Box Enclosure with Keyed Lock** - a metal control panel box enclosure with a keyed lock was included in the design to protect the electrical components from tampering or unauthorized access. This design choice ensures that only authorized personnel can access the electrical components, reducing the risk of damage or interference.
- **Modular Design** - the system was designed with modular components that can be easily replaced or repaired. This design choice allows for quick and easy maintenance of the system, reducing downtime and increasing overall efficiency.
- **Ease of Transport using Caster Wheels** - caster wheels were included in the design to allow for easy transport of the system. This design choice allows for quick and easy movement of the system, reducing the risk of injury to users and increasing overall efficiency.

## Circuit Diagram

The circuit diagram for the ECOCYCLE system is shown below. The diagram illustrates the connections between the various components of the system, including the ESP32 microcontroller, sensors, relays, and other electronic components. The diagram also shows the power supply connections and the wiring for the stepper motors and other actuators.

![Circuit Diagram](assets/circuit_diagram.png)

## 3D Model

![3D Model](assets/3d_model.png)

## Materials Used

| Material | Image | Description | Usage |
| ---------- | ------- | ------------- | ------- |
ESP32 | ![ESP32](assets/esp32.png) | A microcontroller board based on ESP32 SoC Chip running at 240 MHz with builtin Wi-Fi capabilities, i2c, spi and adc communication | Chosen mainly for its Wi-Fi capabilities for sending data for the remote dashboard of the system, while I2C handles LCD, MCP23017 I/O expander and ToF sensors, and SPI handles MAX31865 for temperature sensing using PT100 RTD sensors, adc for analog soil moisture sensor. |
MCP23017 | ![MCP23017](assets/mcp23017.png) | A 16-bit I/O expander with I2C interface | Used to expand the number of GPIO pins available on the ESP32 for controlling multiple devices. |
MAX31865 | ![MAX31865](assets/max31865.png) | A resistance-to-digital converter for PT100 and PT1000 RTD temperature sensors | Used to interface the PT100 RTD temperature sensors with the ESP32 for accurate temperature measurements. |
RELAY MODULE 16 CHANNEL | ![RELAY MODULE 16 CHANNEL](assets/relay_module_16_channel.png) | A relay module with 16 channels for isolated switching | Used to control the 24V buzzer, 24v gear dc motor, 220v magnetic contactor for the meat grinder. |
LOAD CELL | ![LOAD CELL](assets/load_cell.png) | A transducer that converts force into an electrical signal | Used to measure the weight of ground food waste and soil added to the mixer of the system. |
| HX711 | ![HX711](assets/hx711.png) | A precision 24-bit analog-to-digital converter (ADC) designed for weigh scales and industrial control applications | Used to interface the load cell with the ESP32 for accurate weight measurements. |
| LIMIT SWITCH | ![LIMIT SWITCH](assets/limit_switch.png) | A switch that is actuated by the motion of a machine part or presence of an object | Used to detect the starting position of the sensor arm and mixer gate, as well as to detect the closing of the food waste and loam soil hopper, of the system. |
STEPPER MOTOR | ![STEPPER MOTOR](assets/stepper_motor.png) | A brushless DC electric motor that divides a full rotation into a number of equal steps | Used to control the movement of the mixer, sensor arm, and mixer gate of the system. |
TB6600 STEPPER MOTOR DRIVER | ![TB6600](assets/tb6600.png) | A stepper motor driver that can drive bipolar stepper motors, limit current, and microstepping | Used to control the stepper motors in the system. |
24V SWITCHING MODE POWER SUPPLY | ![24V SWITCHING MODE POWER SUPPLY](assets/24v_switching_mode_power_supply.png) | A power supply that converts AC to DC and provides a stable 24V output | Used to power the stepper motors, relays, and other components in the system. |
XL4015 BUCK CONVERTER MODULE | ![XL4015](assets/xl4015.png) | A DC-DC buck converter that can step down voltage from a higher level to a lower level | Used to step down the 24V supply to 5V for the ESP32 and other components. |
PC817 | ![PC817](assets/pc817.png) | An optocoupler that provides electrical isolation between input and output | Used to convert the 24V signal from the inductive proximity sensor to 3.3V. |
PROTOBOARD | ![PROTOBOARD](assets/protoboard.png) | A printed circuit board with through hole connections for prototyping electronic circuits | Together with 2.54mm Male and female pinheaders, used for building the "MOTHERBOARD" of the system with pluggable interfaces to facilitate more convenient assembly, troubleshooting, and parts replacement. |
MALE AND FEMALE PINHEADERS | ![MALE AND FEMALE PINHEADERS](assets/pinheaders.png) | Connectors used to create pluggable interfaces on the protoboard | Used to create pluggable interfaces on the protoboard for easier assembly and maintenance. |
ANALOG SOIL MOISTURE SENSOR | ![ANALOG SOIL MOISTURE SENSOR](assets/analog_soil_moisture_sensor.png) | A sensor that measures the volumetric water content in soil | Used to check the moisture level of the food waste and soil mixture in the system. |
PT100 RTD | ![PT100 RTD](assets/pt100.png) | A type of resistance temperature detector (RTD) that uses platinum as the sensing element | Used to check the temperature of the food waste and soil mixture in the system. |
CABLE TRAY | ![CABLE TRAY](assets/cable_tray.png) | A tray used to support insulated electrical cables used for power distribution and communication. | Used to organize and protect the wiring of the system. |
HEAT SHRINK TUBING | ![HEAT SHRINK TUBING](assets/heat_shrink_tubing.png) | A shrinkable plastic tube used to insulate wires and provide strain relief | Used to insulate and protect the wiring connections in the system. |
GALVANIZED IRON SHEETS | ![GALVANIZED IRON SHEETS](assets/galvanized_iron_sheets.png) | Sheets of steel coated with a layer of zinc to prevent rusting | Used in the assembly of soil hopper, mixer, panel box enclosure, and protective covers of the system |
GALVANIZED IRON TUBULAR BARS | ![GALVANIZED IRON TUBULAR](assets/tubular.png) | Tubular steel bars coated with zinc to prevent rusting | Used to provide structural support for the frame of the system. |
PILLOW BLOCKS | ![PILLOW BLOCKS](assets/pillow_block.png) | A type of bearing housing that provides support for a rotating shaft with the help of compatible bearings and various accessories | Used to support the rotating shafts of the mixer and sensor arm in the system. |
LEAD SCREW T8 AND NUT | ![LEAD SCREW T8 AND NUT](assets/lead_screw_with_tnut.png) | A mechanical linear actuator that translates rotational motion to linear motion | Used to control the vertical movement of the sensor arm, and mixer gate in the system. |
FLANGE COUPLING | ![FLANGE COUPLING](assets/flange_coupling.png) | A type of coupling that connects two shafts together at their ends for the purpose of transmitting power | Used to make a mixer blade removeable wit the mixer shaft for easier maintenance and cleaning. |
