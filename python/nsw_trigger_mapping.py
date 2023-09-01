import sys

def fatal(msg):
    sys.exit("Error: %s" % (msg))



class constants:
    npads = 104
    npfebs = 24

def generate_pad_channel_mapping(size):
    #
    # generate a map of TDS channels
    #   to pad trigger channels
    #
    mapping = {}
    for pfeb in range(constants.npfebs):
        mapping[pfeb] = ["X"]*constants.npads
        for tds_chan in range(constants.npads):
            slv_in = ["X"]*constants.npads
            slv_in[tds_chan] = 1
            slv_out = remap_pads(slv_in, pfeb, size)
            if 1 in slv_out:
                mapping[pfeb][tds_chan] = slv_out.index(1)
            else:
                mapping[pfeb][tds_chan] = "X"
    return mapping


def generate_pad_channel_unmapping(size):
    #
    # generate a map of pad trigger channels
    #   to TDS channels
    #
    mapping = {}
    for pfeb in range(constants.npfebs):
        mapping[pfeb] = ["X"]*constants.npads
        for chan in range(constants.npads):
            result = unmap_pad_channel(chan, pfeb, size)
            mapping[pfeb][chan] = result
    return mapping


def unmap_pad_channel(chan, pfeb, size):
    #
    # convert a pad trigger remapped hit
    #   into an original TDS hit
    #
    slv_out = [0]*constants.npads
    slv_out[chan] = 1
    slv_in = unmap_pads(slv_out, pfeb, size)
    if 1 not in slv_in:
        return "X"
    return slv_in.index(1)


def unmap_pads(slv_out, pfeb, size):
    #
    # convert a vector of pad trigger remapped hits
    #   into a vector of original TDS hits
    #
    slv_in   = ["X"]*104
    indices  = list(range(constants.npads))
    remapped = remap_pads(indices, pfeb, size)
    for (idx, remap_idx) in enumerate(remapped):
        if remap_idx != "X":
            slv_in[remap_idx] = slv_out[idx]
    return slv_in


def remap_pads(slv_in, pfeb, size):
    #
    # remap a vector of pad hits from TDS channel
    #   to pad trigger channel
    # NB: this function is kept as close as possible
    #     to the actual firmware
    #
    if pfeb not in range(constants.npfebs):
        fatal(f"Bad pfeb: {pfeb}")
    if size not in ["L", "S"]:
        fatal(f"Bad size: {size}")

    slv_out = [0]*104

    if size == "L":

        if pfeb == 0:
            for i in range(102, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 102):
                slv_out[i] = slv_in[103-i]
            return slv_out

        if pfeb == 1:
            for i in range(96, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 96):
                slv_out[i] = slv_in[8+i]
            return slv_out

        if pfeb == 2:
            for i in range(96, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 96):
                slv_out[i] = slv_in[8+i]
            return slv_out

        if pfeb == 3:
            for i in range(0, constants.npads):
                slv_out[i] = slv_in[103-i]
            return slv_out

        if pfeb == 4:
            for i in range(96, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 96):
                slv_out[i] = slv_in[103-i]
            return slv_out

        if pfeb == 5:
            for i in range(96, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 96):
                slv_out[i] = slv_in[8+i]
            return slv_out

        if pfeb == 6:
            for i in range(91, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 91):
                slv_out[i] = slv_in[13+i]
            return slv_out

        if pfeb == 7:
            for i in range(96, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 96):
                slv_out[i] = slv_in[98-i]
            return slv_out

        if pfeb == 8:
            for i in range(56, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 56):
                slv_out[i] = slv_in[93-i]
            return slv_out

        if pfeb == 9:
            for i in range(56, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 56):
                slv_out[i] = slv_in[34+i]
            return slv_out

        if pfeb == 10:
            for i in range(75, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 75):
                slv_out[i] = slv_in[24+i]
            return slv_out

        if pfeb == 11:
            for i in range(75, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 75):
                slv_out[i] = slv_in[103-i]
            return slv_out

        if pfeb == 12:
            for i in range(56, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 56):
                slv_out[i] = slv_in[93-i]
            return slv_out

        if pfeb == 13:
            for i in range(56, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 56):
                slv_out[i] = slv_in[34+i]
            return slv_out

        if pfeb == 14:
            for i in range(56, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 56):
                slv_out[i] = slv_in[34+i]
            return slv_out

        if pfeb == 15:
            for i in range(56, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 56):
                slv_out[i] = slv_in[93-i]
            return slv_out

        if pfeb == 16:
            for i in range(60, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 60):
                slv_out[i] = slv_in[85-i]
            return slv_out

        if pfeb == 17:
            for i in range(60, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 60):
                slv_out[i] = slv_in[44+i]
            return slv_out

        if pfeb == 18:
            for i in range(70, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 70):
                slv_out[i] = slv_in[34+i]
            return slv_out

        if pfeb == 19:
            for i in range(70, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 70):
                slv_out[i] = slv_in[95-i]
            return slv_out

        if pfeb == 20:
            for i in range(52, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 52):
                slv_out[i] = slv_in[77-i]
            return slv_out

        if pfeb == 21:
            for i in range(52, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 52):
                slv_out[i] = slv_in[52+i]
            return slv_out

        if pfeb == 22:
            for i in range(56, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 56):
                slv_out[i] = slv_in[48+i]
            return slv_out

        if pfeb == 23:
            for i in range(56, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 56):
                slv_out[i] = slv_in[81-i]
            return slv_out

    else:

        if pfeb == 0: # - QS1C4 (gas gap name in row 5 of the excel file)
            for i in range(68, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 67+1):
                slv_out[i] = slv_in[103-i]
            return slv_out

        if pfeb == 1: # - QS1C3 (gas gap name in row 5 of the excel file)
            for i in range(68, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 67+1):
                slv_out[i] = slv_in[24+i]
            return slv_out

        if pfeb == 2: # - QS1C2 (gas gap name in row 5 of the excel file)
            for i in range(72, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 71+1):
                slv_out[i] = slv_in[24+i]
            return slv_out

        if pfeb == 3: # - QS1C1 (gas gap name in row 5 of the excel file)
            for i in range(72, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 71+1):
                slv_out[i] = slv_in[103-i]
            return slv_out

        if pfeb == 4: # - QS1P1 (gas gap name in row 5 of the excel file)
            for i in range(68, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 67+1):
                slv_out[i] = slv_in[103-i]
            return slv_out

        if pfeb == 5: # - QS1P2 (gas gap name in row 5 of the excel file)
            for i in range(68, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 67+1):
                slv_out[i] = slv_in[24+i]
            return slv_out

        if pfeb == 6: # - QS1P3 (gas gap name in row 5 of the excel file)
            for i in range(51, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 50+1):
                slv_out[i] = slv_in[24+i]
            return slv_out

        if pfeb == 7: # - QS1C1 (gas gap name in row 5 of the excel file)
            for i in range(51, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 50+1):
                slv_out[i] = slv_in[103-i]
            return slv_out

        if pfeb == 8: # - QS2C4 (gas gap name in row 5 of the excel file)
            for i in range(48, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 47+1):
                slv_out[i] = slv_in[87-i]
            return slv_out

        if pfeb == 9: # - QS2C3 (gas gap name in row 5 of the excel file)
            for i in range(48, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 47+1):
                slv_out[i] = slv_in[40+i]
            return slv_out

        if pfeb == 10: # - QS2C2 (gas gap name in row 5 of the excel file)
            for i in range(45, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 44+1):
                slv_out[i] = slv_in[42+i]
            return slv_out

        if pfeb == 11: # - QS2C1 (gas gap name in row 5 of the excel file)
            for i in range(45, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 44+1):
                slv_out[i] = slv_in[85-i]
            return slv_out

        if pfeb == 12: # - QS2P1 (gas gap name in row 5 of the excel file)
            for i in range(30, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 29+1):
                slv_out[i] = slv_in[78-i]
            return slv_out

        if pfeb == 13: # - QS2P2 (gas gap name in row 5 of the excel file)
            for i in range(30, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 29+1):
                slv_out[i] = slv_in[49+i]
            return slv_out

        if pfeb == 14: # - QS2P3 (gas gap name in row 5 of the excel file)
            for i in range(45, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 44+1):
                slv_out[i] = slv_in[42+i]
            return slv_out

        if pfeb == 15: # - QL2P4 (gas gap name in row 5 of the excel file)
            for i in range(45, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 44+1):
                slv_out[i] = slv_in[85-i]
            return slv_out

        if pfeb == 16: # - QS3C4 (gas gap name in row 5 of the excel file)
            for i in range(39, constants.npads):
                slv_out[i] = "X"
            slv_out[0] = "X"
            for i in range(1, 38+1):
                slv_out[i] = slv_in[104-i]
            return slv_out

        if pfeb == 17: # - QS3C3 (gas gap name in row 5 of the excel file)
            for i in range(39, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 38+1):
                slv_out[i] = slv_in[1+i]
            return slv_out

        if pfeb == 18: # - QS3C2 (gas gap name in row 5 of the excel file)
            for i in range(42, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 41+1):
                slv_out[i] = slv_in[0+i]
            return slv_out

        if pfeb == 19: # - QS3C1 (gas gap name in row 5 of the excel file)
            for i in range(42, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 2):
                slv_out[i] = "X"
            for i in range(2, 41+1):
                slv_out[i] = slv_in[105-i]
            return slv_out

        if pfeb == 20: # - QS3P1 (gas gap name in row 5 of the excel file)
            for i in range(24, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 23+1):
                slv_out[i] = slv_in[96-i]
            return slv_out

        if pfeb == 21: # - QS3P2 (gas gap name in row 5 of the excel file)
            for i in range(24, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 23+1):
                slv_out[i] = slv_in[73+i]
            return slv_out

        if pfeb == 22: # - QS3P3 (gas gap name in row 5 of the excel file)
            for i in range(38, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 37+1):
                slv_out[i] = slv_in[66+i]
            return slv_out

        if pfeb == 23: # - QS3P4 (gas gap name in row 5 of the excel file)
            for i in range(39, constants.npads):
                slv_out[i] = "X"
            for i in range(0, 38+1):
                slv_out[i] = slv_in[103-i]
            return slv_out

