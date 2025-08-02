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
- check SBC already handles ovflow
- ajdust PPU VRAM mapping
- fix bg using 0x3F10
- sprite priority
- smb1 coin in the HUD is glitching


## SMB1 HUD flicker
Issue is:
- Nametable is set to 1 just ~ 30 iterations after being set to 0 during vblank. Normally it should happen later after sprite0 collision
- it is due to a call to WriteBufferToScreen during UpdateScreen
- because 8EE2 LDA ($00),y is evaluated not to 0, while it should
- ($00), y is correctly always evaluated to $0301, but this addr is sometime not 0
- It should normally be resetted previously at PC 80D8 whith STA VRAM_Buffer1,x, (with A being 0) but x happens to be not 0
- X was evaluated at 80D0 by LDX VRAM_Buffer_Offset,y, which would end to 0 if y is 0, which is not the case
- Y is set to 0 on 80C6 but a INY cause Y to go to 1. This INY is normally skipped because of a BNE at 80CD.
- But the BNE does not happen because LDX VRAM_Buffer_AddrCtrl (PC 80C8) loads sometime #$06, which matches the following CPX #$06, while it normally supposed to be 0 
- There is yet to understand why it noes not get resetted to 0

-> SOLUTION : implement register T https://www.nesdev.org/wiki/PPU_scrolling#Register_controls
It was normal! The natable is actually going all over the place, but it is reset not by a write to PPUCTRL but by a sideeffect of PPUADDR writes.
Actually the flicker also happens with mesen (can be done by scoping the positoni of the nametable durign vblank, it goes all over the place util being reset by three consecutive STA PPU_ADDRESS (with A = #$00))
