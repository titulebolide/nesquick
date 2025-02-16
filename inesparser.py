import matplotlib.pyplot as plt
import numpy as np

def parse_header(header):
    prg_len = header[4] * 16384
    chr_len = header[5] * 8192
    return prg_len, chr_len

def parse_ines(filename):
    """
    https://www.nesdev.org/wiki/INES
    """

    with open(filename, "rb") as f:
        data = f.read()

    if data[0:4] != b"NES\x1a":
        print(data[0:4])
        raise Exception("Bad file header")
    
    header = data[0:16]
    prg_len, chr_len = parse_header(header)

    if len(data) != chr_len + prg_len + 16:
        print(len(data), chr_len + prg_len + 16)
        raise Exception("Unsupported file format")

    prg = data[16:prg_len+16]
    chr = data[prg_len+16:chr_len+prg_len+16]
    return prg, chr    

def ines_show_chr(chr):
    tile = np.zeros((32*8, 16*8))
    # x is left to right
    # y is up to down
    # but for imshow x is up to down, y is left to right
    for tile_y in range(32):
        for tile_x in range(16):
            tile_no = tile_x + tile_y * 16
            plane0_addr = (tile_no) << 4
            print(plane0_addr, plane0_addr+16)
            plane0 = chr[plane0_addr:plane0_addr+8]
            plane1 = chr[plane0_addr+8:plane0_addr+16]
            # print(bin(plane0), bin(plane1))
            for i in range(8):
                for j in range(8):
                    color0 = (plane0[j] >> i) & 1
                    color1 = (plane1[j] >> i) & 1
                    color = (color1 << 1) + color0
                    
                    # increase color value to separate from the bg
                    if color != 0:
                        color += 4
                    # if transparent (color 0, change to 1 every two to have a checkerboard)
                    elif (tile_y + tile_x) %2 == 0 :
                        color = 1 

                    tile[tile_y*8 + j, tile_x*8 + (7-i)] = color
    plt.subplot(1,2,1)
    plt.imshow(tile[:16*8])
    plt.subplot(1,2,2)
    plt.imshow(tile[16*8:])
    plt.show()

if __name__ == "__main__":
    prg,chr = parse_ines("rom/donkeykong.nes")
    print(chr)
    ines_show_chr(chr)
