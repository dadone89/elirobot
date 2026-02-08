![Development Status](https://img.shields.io/badge/Status-Stable-brightgreen)
![Version](https://img.shields.io/badge/Version-1.1.0-blue)
![Build](https://img.shields.io/badge/Build-Stable-brightgreen)

# elirobot

**elirobot** is an educational robot designed by and for children, based on Arduino. Children can give commands directly using the integrated keyboard on the top of the robot. Commands include basic movements and special functions designed to stimulate creativity and coding learning.

## Key Features

- Simple and intuitive interface via physical keyboard
- Playback of pre-recorded sounds to guide actions
- Very cute eyes!
- Modes:
   - Sequence: execution of movement sequences programmable by the child
   - Dance: performs repeated movements to imitate a dance
   - Remote Control: movements directly activated by directional arrows
   - Light Following: thanks to 2 photoresistors on the front, it will follow light, for example from a flashlight
- Modular and expandable open-source project

## Features in Development

- Other special functions (e.g., small logic games, etc.)
- Aesthetics (various lights)

## Prerequisites

- Compatible Arduino board (specifically, I used a 30-pin ESP32DevModule board, based on ESP32)
- Electronic components (continuous rotation servo motors, buttons, LiPo batteries, LEDs, buzzer, connection cables, etc.)
- 3D printer for key creation (but you can also make them in other simpler ways)
- Arduino IDE 2.x.x and a PC for programming

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/dadone89/elirobot.git
   cd elirobot
   ```

2. Open the main file (`elirobot.ino`) using the Arduino IDE.

3. Connect the components according to the schematics in the `docs/hardware` folder (or follow the instructions in the `SCHEMA.md` file if available).

4. Upload the sketch to the board. If you are using ESP32, make sure you have first installed board support from the board manager; in my case, I used ESP32 by Espressif V3.2.0.

## Usage

1. Turn on/power elirobot.
2. Use the keypad on the top to give the desired commands.
3. Experiment with movement functions and special functions.
4. You can modify the code or add new functionalities to customize the robot.

## Contributing

Contributions and ideas are welcome! You can:

- Report bugs or suggest new features via issues
- Submit pull requests for hardware/software improvements
- Propose alternative implementations or educational materials

## License

This project is distributed under a Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0) license. Commercial use of the project or its derivatives is not permitted. For more details, consult the [`LICENSE`](LICENSE) file or visit [https://creativecommons.org/licenses/by-nc/4.0/deed.it](https://creativecommons.org/licenses/by-nc/4.0/deed.it).

## Authors

- [dadone89](https://github.com/dadone89)
[danielealberti.it](https://www.danielealberti.it)

---

Have fun building, programming, and playing with elirobot!
