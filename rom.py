class Rom:
    def __init__(self, emu, mems):
        """
        mems is a list of length 2 tuples (startaddress, data)
        """
        for startaddr, data in mems:
            for index, val in enumerate(data):
                emu.mem[startaddr + index] = val

def mems_from_file(filename, start_addr):
    # load program to memory
    with open(filename, "rb") as f:
        return [(start_addr, f.read()),]

from inesparser import parse_ines

class InesRom(Rom):
    def __init__(self, inesfile, emu):
        prg, chr = parse_ines(inesfile)
        # NROM : rom starts at 0x8000
        super().__init__(emu, [(0x8000, prg)])
