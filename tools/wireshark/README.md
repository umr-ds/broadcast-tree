This folder contains a Wireshark dissector for the Broadcast Tree Protocol. This is more for debuggung purpose, but could also be published.

## Installation
Since this uses the Lua API for writing dissectors, place the `btp.lua` file in wireshark's plugins folder, which is `~/.config/wireshark/plugins` on *nix systems.
Then, either start Wireshark or reload it with cmd-shift-l (ctrl-shift-l on linux).
