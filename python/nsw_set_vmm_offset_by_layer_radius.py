#!/usr/bin/env tdaq_python
"""
A script for setting the offset and of a sector,
  where FEBs at each layer and/or radius are given a different offset.
  NB: this assumes geographical IDs are used in the OpcNodeId.

Run like:
python nsw_set_vmm_offset_by_layer_radius.py -i /path/to/mmg_A10_blah.json --initialOffset 5 --bcPerLayer 8
"""
import argparse
import collections
import json
import os
import sys

FEBID = "OpcNodeId"
MAXOFFSET = pow(2, 12)
MAXRADIUS = 16
MAXLAYER = 8
NVMM = {
    "MMFE8": 8,
    "SFEB8": 8,
    "SFEB6": 6,
    "PFEB":  3,
}
FIRSTVMM = {
    "MMFE8": 0,
    "SFEB8": 0,
    "SFEB6": 2,
    "PFEB":  0,
}

def main():

    # check the CL args
    ops = options()
    checkOptions()
    output = getOutput()

    # announce i/o
    print("")
    print(f"Input: {ops.i}")
    print(f"Output: {output}")
    print("")

    # load & update json
    newjson = None
    with open(ops.i) as jfile:
        oldjson = json.load(jfile, object_pairs_hook=collections.OrderedDict)
        newjson = setOffset(oldjson)

    # write to disk
    with open(output, "w") as jfile:
        json.dump(newjson, jfile, indent=4)

    # fin
    print("")
    print("Done! ^.^")

def options():
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-i", help="Input JSON file", default=None)
    parser.add_argument("-o", help="Output JSON file", default=None)
    parser.add_argument("--bcPerRadius", help="Number of BC to increase offset for each radius", default=0)
    parser.add_argument("--bcPerLayer", help="Number of BC to increase offset for each layer", default=0)
    parser.add_argument("--initialOffset", help="Initial offset, i.e. the offset for L0/R0", required=True)
    parser.add_argument("-f", help="Force overwrite of output file if it already exists", action="store_true")
    return parser.parse_args()

def getOutput():
    ops = options()
    if ops.o:
        return ops.o
    tag = f"offset{ops.bcPerLayer}bcPerLayer{ops.bcPerRadius}bcPerRadius"
    return ops.i.replace(".json", f".{tag}.json")

def checkOptions():
    ops = options()
    if not ops.i:
        fatal("Please provide an input file with -i")
    if not os.path.isfile(ops.i):
        fatal(f"Input json file doesnt exist: {ops.i}")
    if not ops.i.endswith(".json"):
        fatal(f"Input json file path doesnt end with `.json`. Wat? {ops.i}")
    if os.path.isfile(getOutput()) and not ops.f:
        fatal(f"Output file {getOutput()} already exists. Use -f to force overwrite")

def setOffset(thejson):
    ops = options()
    bcPerRadius = int(ops.bcPerRadius)
    bcPerLayer = int(ops.bcPerLayer)
    initialOffset = int(ops.initialOffset)
    for key in thejson:
        feb = thejson[key]
        if not isFeb(feb):
            continue
        febid = feb[FEBID]
        radius = getRadius(febid)
        layer = getLayer(febid)
        nvmm = getNvmm(febid)
        firstvmm = getFirstVmm(febid)
        offset = initialOffset + layer * bcPerLayer + radius * bcPerRadius
        print(f"Found feb: {febid}, layer {layer}, radius {radius}, N(vmm) = {nvmm}, First(VMM) = vmm{firstvmm} => offset = {offset}")
        if offset > MAXOFFSET-1:
            fatal(f"Cant have offset this large: {offset}. Max offset: {MAXOFFSET-1}")
        for ivmm in range(firstvmm, nvmm):
            vmmkey = f"vmm{ivmm}"
            if vmmkey not in feb:
                feb[vmmkey] = {}
            feb[vmmkey]["offset"]   = offset
    return thejson

def isFeb(obj):
    if not isinstance(obj, dict):
        return False
    if not FEBID in obj:
        return False
    objid = obj[FEBID]
    return any([
        objid.startswith("MM-A/V0/SCA/Strip"),
        objid.startswith("MM-C/V0/SCA/Strip"),
        objid.startswith("sTGC-A/V0/SCA/Strip"),
        objid.startswith("sTGC-C/V0/SCA/Strip"),
        objid.startswith("sTGC-A/V0/SCA/Pad"),
        objid.startswith("sTGC-C/V0/SCA/Pad"),
    ])

def getLayer(objid):
    for layer in range(MAXLAYER):
        if f"/L{layer}" in objid:
            return layer
    fatal(f"Couldnt get layer from {objid}")

def getRadius(objid):
    for radius in range(MAXRADIUS, -1, -1):
        if f"/R{radius}" in objid:
            return radius
    fatal(f"Couldnt get radius from {objid}")

def getNvmm(objid):
    if objid.startswith("MM-"):
        return NVMM["MMFE8"]
    elif objid.startswith("sTGC-"):
        if "/Pad" in objid:
            return NVMM["PFEB"]
        elif "/Strip" in objid:
            # TODO this SFEB8/SFEB6 distinction is wrong
            if "/R0" in objid:
                return NVMM["SFEB8"]
            else:
                return NVMM["SFEB6"]
    fatal(f"Couldnt get N(VMM) from {objid}")
    
def getFirstVmm(objid):
    if objid.startswith("MM-"):
        return FIRSTVMM["MMFE8"]
    elif objid.startswith("sTGC-"):
        if "/Pad" in objid:
            return FIRSTVMM["PFEB"]
        elif "/Strip" in objid:
            # TODO this SFEB8/SFEB6 distinction is wrong
            if "/R0" in objid:
                return FIRSTVMM["SFEB8"]
            else:
                return FIRSTVMM["SFEB6"]
    fatal(f"Couldnt get First(VMM) from {objid}")

def fatal(msg):
    sys.exit("Fatal error: %s" % (msg))


if __name__ == "__main__":
    main()

