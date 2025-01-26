import rom
import emu6502
import lstdebug

l = lstdebug.LstDebugger("rom/hello-world/build/starter.lst")

emu = emu6502.Emu6502(l)

r = rom.InesRom("rom/hello-world/build/starter.nes", emu)

emu.reset()

emu.run()
