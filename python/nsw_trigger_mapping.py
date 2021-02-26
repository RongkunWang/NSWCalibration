import sys

NVMM_PER_FEB       = 8
NFEB_PER_LAYER     = 16
NVMM_PER_LAYER     = NVMM_PER_FEB*NFEB_PER_LAYER
NVMM_PER_HALFLAYER = NVMM_PER_FEB*NFEB_PER_LAYER/2

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
