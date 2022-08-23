#!/usr/bin/env tdaq_python
"""
A script for adding pad trigger objects to the official jsons.

Run like:
python nsw_add_pad_trigger_in_json.py
"""
import argparse
import collections
import glob
import json
import os
import sys
import time

JSONDIR  = "/det/nsw/json/stgc/readout/"
TEMPLATE = "/detwork/nsw/json/trigger/stg/rim/PadTriggers.json"
COMMON = "padtriggerfpga_common_config"
OBJECT = "PadTrigger"
OLDOBJECT = "PadTriggerSCA_00"
RIML1DDC = "RimL1DDC"
SIDES = ["A", "C"]
NUMBERS = range(1, 17)
NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")

def main():

    # check the CL args
    ops = options()
    checkOptions()
    print("")
    print(f"Input directory:\n {ops.i}")

    # make output dir
    outdir = getOutputDir()
    os.makedirs(outdir)
    print("")
    print(f"Output directory:\n {outdir}")

    # load the pad triggers file
    print("")
    print(f"Pad triggers:\n {ops.j}")
    with open(ops.j) as jfile:
        padtriggers = json.load(jfile, object_pairs_hook=collections.OrderedDict)

    # announce inputs
    print("")
    print(f"Input files:")
    inputs = getInputs()
    for inp in inputs:
        print(f" {inp}")

    # loop over input files
    print("")
    print(f"Writing to:")
    for inp in inputs:

        # guess sector from filename
        sector = getSector(inp)

        # load & update json
        with open(inp) as jfile:
            oldjson = json.load(jfile, object_pairs_hook=collections.OrderedDict)
            newjson = merge(oldjson, padtriggers, sector)

        # write to disk
        output = os.path.join(outdir, os.path.basename(inp))
        print(f" {output}")
        if os.path.isfile(output) and not ops.f:
            fatal(f"Output file {output} already exists. Use -f to force overwrite")
        with open(output, "w") as jfile:
            json.dump(newjson, jfile, indent=4)

    # fin
    print("")
    print("Done! ^.^")


def options():
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-i", help="Input JSON directory",           default=JSONDIR)
    parser.add_argument("-j", help="JSON file of pad triggers",      default=TEMPLATE)
    parser.add_argument("-o", help="Output directory",               default=None)
    parser.add_argument("--nameCommon", help="Name of the pad trigger common config", default=COMMON)
    parser.add_argument("--nameObject", help="Name of the pad trigger object config", default=OBJECT)
    parser.add_argument("--nameRimL1DDC", help="Name of the RimL1DDC object config", default=RIML1DDC)
    parser.add_argument("-f", help="Force overwrite of output dir if it already exists", action="store_true")
    return parser.parse_args()


def getInputs():
    ops = options()
    if not ops.i:
        fatal("Please provide an input json dir with -i")
    inputs = sorted(glob.glob(os.path.join(ops.i, "*")))
    inputs = list(filter(lambda inp: inp.endswith(".json"), inputs))
    return inputs


def getOutputDir():
    ops = options()
    if ops.o:
        return ops.o
    return os.path.abspath(NOW)


def checkOptions():
    ops = options()
    inputs = getInputs()
    outdir = getOutputDir()
    if not inputs:
        fatal("Please provide an input json dir with -i")
    for fname in inputs:
        if not os.path.isfile(fname):
            fatal(f"Input json file doesnt exist: {fname}")
        if not fname.endswith(".json"):
            fatal(f"Input json file path doesnt end with `.json`. Wat? {fname}")
    if not ops.j:
        fatal("Please provide a pad trigger file with -j")
    if not os.path.isfile(ops.j):
        fatal(f"Input json file doesnt exist: {ops.j}")
    if not ops.j.endswith(".json"):
        fatal(f"Pad trigger json file path doesnt end with `.json`. Wat? {ops.j}")


def getSector(fname):
    bname = os.path.basename(fname)
    for side in SIDES:
        for num in NUMBERS:
            sector = f"{side}{num:02}"
            if sector in bname:
                return sector
    fatal(f"Cannot find sector in {fname}")


def merge(oldjson, template, sector):
    ops = options()
    newjson = oldjson
    newjson[ops.nameRimL1DDC] = template[ops.nameRimL1DDC]
    newjson[ops.nameObject]   = template[f"{ops.nameObject}_{sector}"]
    newjson[ops.nameCommon]   = template[ops.nameCommon]
    if OLDOBJECT in newjson:
        newjson.pop(OLDOBJECT)
    return newjson


def getHost(sector):
    num = int(sector.lstrip("A").lstrip("C"))
    sectorPerHost = 2
    firstHost = 8
    theHost = firstHost + ((num - 1) // sectorPerHost)
    return f"pc-tdq-flx-nsw-stgc-{theHost:02}.cern.ch"


def getPort(sector):
    num = int(sector.lstrip("A").lstrip("C"))
    isLarge = num % 2 == 1
    return "48020" if isLarge else "48021"


def fatal(msg):
    sys.exit("Fatal error: %s" % (msg))


if __name__ == "__main__":
    main()

