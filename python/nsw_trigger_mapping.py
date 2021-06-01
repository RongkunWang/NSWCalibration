import sys

NVMM_PER_FEB       = 8
NFEB_PER_LAYER     = 16
NVMM_PER_LAYER     = NVMM_PER_FEB*NFEB_PER_LAYER
NVMM_PER_HALFLAYER = NVMM_PER_FEB*NFEB_PER_LAYER/2
NFIBERS_PER_TP     = 32

def fatal(msg):
    sys.exit("Error: %s" % (msg))

def addc2layer(addc):
    if "ADDC_L1P6_IPR" in addc: return 0
    if "ADDC_L1P3_IPL" in addc: return 0
    if "ADDC_L1P6_IPL" in addc: return 1
    if "ADDC_L1P3_IPR" in addc: return 1
    if "ADDC_L4P6_IPR" in addc: return 2
    if "ADDC_L4P3_IPL" in addc: return 2
    if "ADDC_L4P6_IPL" in addc: return 3
    if "ADDC_L4P3_IPR" in addc: return 3
    if "ADDC_L4P6_HOR" in addc: return 4
    if "ADDC_L4P3_HOL" in addc: return 4
    if "ADDC_L4P6_HOL" in addc: return 5
    if "ADDC_L4P3_HOR" in addc: return 5
    if "ADDC_L1P6_HOR" in addc: return 6
    if "ADDC_L1P3_HOL" in addc: return 6
    if "ADDC_L1P6_HOL" in addc: return 7
    if "ADDC_L1P3_HOR" in addc: return 7

def artchannel2localvmm(channel):
    # from:
    # abcdefghijklmnopqrstuvwxyz012345
    # to:
    # abcdefgh________ijklmnop________qrstuvwx________yz012345
    return (channel % NVMM_PER_FEB) + NVMM_PER_FEB*int(channel / NVMM_PER_FEB)*2

def artchannel2localvmm_febreversed(channel):
    # from:
    # abcdefghijklmnopqrstuvwxyz012345
    # to:
    # hgfedcba________ponmlkji________xwvutsrq________54321zy
    return (NVMM_PER_FEB - 1 - (channel % NVMM_PER_FEB)) + NVMM_PER_FEB*int(channel / NVMM_PER_FEB)*2

def artchannel2globalvmm(addc, art, channel):
    addc    = str(addc)
    art     = str(art)
    channel = int(channel)
    if any([name in addc for name in ["ADDC_L1P6_IPR", "ADDC_L4P6_HOR",
                                      "ADDC_L4P6_IPL", "ADDC_L1P6_HOL"]]):
        if "0" in art:
            return NVMM_PER_FEB + artchannel2localvmm(channel)
        else:
            return artchannel2globalvmm(addc, "art0", channel) + NVMM_PER_HALFLAYER
    if any([name in addc for name in ["ADDC_L1P3_IPL", "ADDC_L4P3_HOL",
                                      "ADDC_L4P3_IPR", "ADDC_L1P3_HOR"]]):
        if "0" in art:
            return NVMM_PER_LAYER - 1 - NVMM_PER_FEB - artchannel2localvmm(channel)
        else:
            return artchannel2globalvmm(addc, "art0", channel) - NVMM_PER_HALFLAYER
    if any([name in addc for name in ["ADDC_L1P6_IPL", "ADDC_L4P6_HOL",
                                      "ADDC_L4P6_IPR", "ADDC_L1P6_HOR"]]):
        if "0" in art:
            return NVMM_PER_LAYER - 1 - artchannel2localvmm_febreversed(channel)
        else:
            return artchannel2globalvmm(addc, "art0", channel) - NVMM_PER_HALFLAYER
    if any([name in addc for name in ["ADDC_L1P3_IPR", "ADDC_L4P3_HOR",
                                      "ADDC_L4P3_IPL", "ADDC_L1P3_HOL"]]):
        if "0" in art:
            return artchannel2localvmm_febreversed(channel)
        else:
            return artchannel2globalvmm(addc, "art0", channel) + NVMM_PER_HALFLAYER
    fatal("Cannot convert %s, %s, %s" % (addc, art, channel))

def art2tp(addc, art):
    for fiber in range(NFIBERS_PER_TP):
        if tp2art(fiber) == (addc, art):
            return fiber
    fatal("Cannot convert %s, %s to TP fiber" % (addc, art))

def tp2art(tpfiber):
    if tpfiber ==  0: return ("ADDC_L1P6_IPR", "art1")
    if tpfiber ==  1: return ("ADDC_L1P6_IPR", "art0")
    if tpfiber ==  2: return ("ADDC_L1P3_IPL", "art1")
    if tpfiber ==  3: return ("ADDC_L1P3_IPL", "art0")
    if tpfiber ==  4: return ("ADDC_L1P3_IPR", "art1")
    if tpfiber ==  5: return ("ADDC_L1P3_IPR", "art0")
    if tpfiber ==  6: return ("ADDC_L1P6_IPL", "art1")
    if tpfiber ==  7: return ("ADDC_L1P6_IPL", "art0")
    if tpfiber ==  8: return ("ADDC_L4P6_IPR", "art1")
    if tpfiber ==  9: return ("ADDC_L4P6_IPR", "art0")
    if tpfiber == 10: return ("ADDC_L4P3_IPL", "art1")
    if tpfiber == 11: return ("ADDC_L4P3_IPL", "art0")
    if tpfiber == 12: return ("ADDC_L4P3_IPR", "art1")
    if tpfiber == 13: return ("ADDC_L4P3_IPR", "art0")
    if tpfiber == 14: return ("ADDC_L4P6_IPL", "art1")
    if tpfiber == 15: return ("ADDC_L4P6_IPL", "art0")
    if tpfiber == 16: return ("ADDC_L4P6_HOR", "art1")
    if tpfiber == 17: return ("ADDC_L4P6_HOR", "art0")
    if tpfiber == 18: return ("ADDC_L4P3_HOL", "art1")
    if tpfiber == 19: return ("ADDC_L4P3_HOL", "art0")
    if tpfiber == 20: return ("ADDC_L4P3_HOR", "art1")
    if tpfiber == 21: return ("ADDC_L4P3_HOR", "art0")
    if tpfiber == 22: return ("ADDC_L4P6_HOL", "art1")
    if tpfiber == 23: return ("ADDC_L4P6_HOL", "art0")
    if tpfiber == 24: return ("ADDC_L1P6_HOR", "art1")
    if tpfiber == 25: return ("ADDC_L1P6_HOR", "art0")
    if tpfiber == 26: return ("ADDC_L1P3_HOL", "art1")
    if tpfiber == 27: return ("ADDC_L1P3_HOL", "art0")
    if tpfiber == 28: return ("ADDC_L1P3_HOR", "art1")
    if tpfiber == 29: return ("ADDC_L1P3_HOR", "art0")
    if tpfiber == 30: return ("ADDC_L1P6_HOL", "art1")
    if tpfiber == 31: return ("ADDC_L1P6_HOL", "art0")
    fatal("Cannot convert tpfiber %s to addc,art" % (tpfiber))

def tp2globalvmm(tpfiber, vmm):
    """
    NB:
     - Fiber from 0 to 31
     - VMM   from 0 to 31
     - Global VMM from 0 to 127
    """
    (addc, art) = tp2art(tpfiber)
    return artchannel2globalvmm(addc, art, vmm)

def tp2layer(tpfiber):
    """
    NB:
     - Fiber from 0 to 31
     - Layer from 0 to 7
    """
    return int(tpfiber/4)
