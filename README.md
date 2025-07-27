# NESquick

![logo](assets/logo.png)

## TODO
- Handle nametable mirroring
  - done? not sure
- Fix scrolling by handling internal regs https://www.nesdev.org/wiki/PPU_scrolling#PPU_internal_registers
  - the issue is actually the nametable not being reset on SMB1 for HUD printing. A hack has been done to force nametable to 0 for the first 3 lines.
- Fix sweep going wild and making high pitch notes
- Set background color
  - Beware the sprite 0 collision currenlty relies on the backgroudn being black
- Fix SMB title color
- be a bit more subtle on overflowing sprite with x scroll (currently rejecting overflowing sprites)
