def parseHeader(header):
    prg_len = header[4] * 16384
    chr_len = header[5] * 8192
    return prg_len, chr_len

def parseInes(filename):
    """
    https://www.nesdev.org/wiki/INES
    """

    with open(filename, "rb") as f:
        data = f.read()

    if data[0:4] != b"NES\x1a":
        print(data[0:4])
        raise Exception("Bad file header")
    
    header = data[0:16]
    prg_len, chr_len = parseHeader(header)

    if len(data) != chr_len + prg_len + 16:
        print(len(data), chr_len + prg_len + 16)
        raise Exception("Unsupported file format")

    prg = data[16:prg_len+16]
    chr = data[prg_len+16:chr_len+prg_len+16]
    return prg, chr

parseInes("starter.nes")
    

    