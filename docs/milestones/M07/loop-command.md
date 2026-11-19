# M07-05: the music loop command (0x90500) is never sent by the game (review 1, finding 5)

Question: the native sound driver supports a looping music stream (`vcs_sound.cpp`: `case 0x90500: m_loop = first`;
at the file's end the stream wraps when `m_loop != 0`). The 8-minute radio drive (`hardware/radio-handheld-*`,
`VCS_MUSIC_LOG=1`) logged 168 music commands and no 0x90500. Does the game use looping at all?

## Static check of SLUS_215.90 (USA v1.00), 2026-11-19

Method: a scan of the ELF's loaded segments (a short Python script over the program headers; no tools beyond the
standard library) for (a) every `lui rX, 8|9` followed within 10 instructions by `ori`/`addiu rX, rX, imm`, (b) every
`jal 0x0010DD50` (the EE-side music command sender; `a0` = command, `a1` = argument) with the `a0` set before it or in
its delay slot, (c) any 32-bit data word equal to 0x00090500.

| Call site of 0x10DD50 | Command (a0) |
|---|---|
| 0x10DC8C | 0x4A0000 |
| 0x10E630 | 0x90200 (preload, cutscenes) |
| 0x10E658 | 0x90300 (start preloaded) |
| 0x10E698 | 0x80200 (open at a position) |
| 0x10E6C8, 0x10E8E0 | 0x80E00 (state poll) |
| 0x10E6DC | 0x80700 (stop) |
| 0x10E704 | 0x90100 (position poll) |
| 0x10E858 | 0xC00 (volume now) |
| 0x10E868 | 0xB00 (volume at next step) |
| 0x10E88C | 0x90000 (stream length) |
| 0x10E904 | 0x90400 (mono) |

Result: no instruction pair builds 0x90500 and no loaded word equals 0x00090500. The game never asks the driver to
loop a music stream; `m_loop` stays 0 (also reset by every preload). A station or score that should continue is
re-opened at a computed position (0x802xx with the position in ms): the radio drive shows 12 opens across 5 files plus one cutscene
preload (0x90236, file 54), e.g. file 4 opened at 986,803 / 998,187 / 1,073,451 / 1,177,387 / 1,191,723 / 1,227,563 /
1,380,779 ms, each ended by a stop (0x80700).

So M07-05's "seeking / looping" is, for this game: seeking = the open-at-position path (exercised on hardware); looping
= not used by the game (the driver's loop path is unreachable from it). Limitation: a static scan; a command assembled
by arithmetic other than `lui`+`ori`/`addiu` (none seen at the 12 call sites) would escape it.
