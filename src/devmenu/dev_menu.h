// The Switch port's developer menu (M08): L3 + R3 opens and closes it from anywhere in the game (the chord never
// reaches the game: src/devmenu/chord.h). Controller-only navigation through Dear ImGui: the D-pad or left stick moves,
// A (the right button) chooses, B (the bottom one) closes; the touch screen works too. From it: upstream's settings menu (the
// settings registry: internal resolution, timing, audio, controller, diagnostics), the trainer, the free camera, the
// performance overlay, the frame-time recording and the port's diagnostics.
//
// The desktop's Back + Start / RB / LB chords are off on the Switch (Back is the game's Select). The menu is drawn in
// upstream's UI hook before the settings and trainer menus (vcs::launcher::setHostUi) and blocks the game's input like
// them while open and until the buttons that closed it are let go.
#pragma once

#include <cstdint>
#include <string>

namespace devmenu
{
    // Main thread, after vcs::launcher::registerSettings and before settings::load: the port's own settings (Advanced >
    // Diagnostics) and the rows the Switch has nothing for hidden (the Keyboard page, the desktop window, the
    // background behaviour).
    void registerSettings();
    // Main thread, before vcs::launcher::setUiCallbacks: the host UI, the chords turned off, where the recordings and
    // the trainer note go (`userdata`, the run's own folder).
    void install(const std::string &userdata);
    // Main thread, after installHooks: the control-channel command `devmenu [open|close|settings|trainer]` (state, and
    // the menu opened for scripted checks; the physical chord is the player's way). `rdram`: the EE RAM, read for the
    // game's own free-aim flag (logged on every change: R1-then-L3 reaching the game, M08-14).
    void installCommands(const uint8_t *rdram);

    bool open();
    // One line of the menus' and the chord's state for the run's report.
    std::string reportLine();
}
