# NESquick

![logo](assets/logo.png)

## TODO
- Handle nametable mirroring
- Handle mid-screen ppuscroll changes (SMB HUB) (maybe it already works, bubt i'm not sure as it is synchronous)
  - "Typically, this register is written to during vertical blanking to make the next frame start rendering from the desired location, but it can also be modified during rendering in order to split the screen. Changes made to the vertical scroll during rendering will only take effect on the next frame"
- in some places a static cast could be removed in favour of just chaning the type of the base variable. Takes bit more space but faster.
- use arenas
- Fix scrolling by handling internal regs https://www.nesdev.org/wiki/PPU_scrolling#PPU_internal_registers
- Fix notes fading (e.g SMB music)
- Set background color
- Fix SMB title color
- be a bit more subtle on overlowing sprite with x scroll (currently rejecting overflowing sprites)

