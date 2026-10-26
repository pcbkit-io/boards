# Delta-S02

**Дельта-С-02**, the ZX Spectrum 48K compatible home computer made by Elara in Cheboksary in 1993,
as its factory documents draw it: «Схема электрическая принципиальная БПК Дельта-С» (Приложение 5
of its manual) for the circuit, «Плата процессора БПК Дельта-С-02» (Приложение 6) for where every
part sits on its 285 × 150 mm board. 52 Soviet chips and no ULA: the picture, the memory's
RAS/CAS timing and the frame interrupt are made by counters and gates.

Open it at **https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/retro/delta-s02** and run it:
the board runs as it is wired, chip by chip (ADR 0043): a Z80A on its pins, eight КР565РУ5В
DRAMs behind their multiplexers, the КМ573РФ8А with the standard 48K ROM, the КР1533 and К555 logic
with their datasheets' delays. A television on the RGB socket XS9 scans the board's own sync and
colour lines; the keyboard on XS2/XS3 takes your keys (operate the device, then type: `J`, `"`,
`"`, Enter is `LOAD ""`); the cassette recorder on XS7 holds The Scorched Earth (X-Project
Association, 1997, freeware): press **Play** in the laboratory's Computer section to load it as
the board does (about two minutes of the board's time, much longer in a browser: every chip runs
at every nanosecond), or **Loaded** to resume the board as that tape leaves it, saved on this
board with this ROM (a release asset beside the cassette): the game's menu at once.

| Part | What it is |
|---|---|
| DD44 | Z80A (КР1858ВМ1 class), 3.5 MHz from the 14 MHz crystal BQ1 through DD6 |
| DD41 | КМ573РФ8А, the 48K ROM |
| DD25–DD28, DD31–DD34 | КР565РУ5В, 64 K × 1 DRAM each, one per data bit |
| DD3, DD4, DD14 | the line counter: 464 dots of 7 MHz a line |
| DD20, DD21, DD7 | the frame counter (313 lines) and the FLASH divider |
| DD24, DD30, DD35, DD36, DD17 | the RAM address multiplexers: the CPU's and the picture's, row and column |
| DD38–DD40, DD51, DD52, DD50, DD49 | pixel shift registers, attribute and border registers, colour |
| DD42, DD43, DD45, DD48, DD46 | address, data and control buffers |
| DD47, DD15, DD16 | port #FE (border, beeper, tape out), keyboard and Kempston joystick |
| DA1 | К554СА3А, the tape input's comparator |
| XS5 … XS9, XP1 | power, Kempston, tape, Sinclair joystick, RGB; the system connector |

Not simulated: the RF modulator in its can (VT1, L2) and the transistor buffers of the RGB socket;
the television reads the colour lines behind them. The modulator's output goes by a coaxial cable
from the pads XS4 to the aerial socket (САТ-Г) on the case. The keyboard itself (a membrane on a steel
plate) is off the board, on the flat cables at XS2 and XS3.

The board is placed as the factory's drawing places it and routed by PCBKit (two layers, every net,
no errors). The parts are Soviet and long out of production; the board can be ordered,
and the chips' equivalents (74ALS, 74LS, 4164, 27128, Z80) fit their 2.5 mm metric sockets only
with bent legs: the metric pitch is the original's.

Sources: zx-pk.ru (the factory sheet scanned by crackown, 2022), the Omsk archive of Andrey
Pikalo (connector pinouts), research notes in PCBKit's `research/delta-s02-2026-10-26.md`.
