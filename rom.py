class Rom:
    def __init__(self, emu, mems):
        """
        mems is a list of length 2 tuples (startaddress, data)
        """
        print(mems)
        for startaddr, data in mems:
            for index, val in enumerate(data):
                emu.mem[startaddr + index] = val

def mems_from_file(filename, start_addr):
    # load program to memory
    with open(filename, "rb") as f:
        return [(start_addr, f.read()),]

