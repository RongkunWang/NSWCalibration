#!/usr/bin/env tdaq_python
"""
A script for adding a pad trigger object to one or many jsons.

Run like:
python nsw_add_pad_trigger_in_json.py -i "/path/to/STGC_P1_C*.json"
"""
import argparse
import collections
import glob
import json
import os
import sys
import time

TEMPLATE = "/detwork/nsw/json/trigger/stg/cosmics/padtriggerfpga_common_config.json"
COMMON = "padtriggerfpga_common_config"
OBJECT = "PadTriggerSCA_00"
SIDES = ["A", "C"]
NUMBERS = range(1, 17)
NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")

def main():

    # check the CL args
    ops = options()
    checkOptions()

    # make output dir
    outdir = getOutputDir()
    os.makedirs(outdir)
    print("")
    print(f"Output directory:\n {outdir}")

    # load the template file
    print("")
    print(f"Template pad trigger:\n {ops.t}")
    with open(ops.t) as jfile:
        padtrigger = json.load(jfile, object_pairs_hook=collections.OrderedDict)

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
            newjson = merge(oldjson, padtrigger, sector)

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
    parser.add_argument("-i", help="Input JSON file",                default=None)
    parser.add_argument("-o", help="Output directory",               default=None)
    parser.add_argument("-t", help="Template pad trigger JSON file", default=TEMPLATE)
    parser.add_argument("--nameCommon", help="Name of the pad trigger common config", default=COMMON)
    parser.add_argument("--nameObject", help="Name of the pad trigger object config", default=OBJECT)
    parser.add_argument("-f", help="Force overwrite of output file if it already exists", action="store_true")
    return parser.parse_args()


def getInputs():
    ops = options()
    if not ops.i:
        fatal("Please provide an input file with -i")
    fnames = []
    for fpath in ops.i.split(","):
        fnames.extend(glob.glob(fpath))
    return fnames


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
        fatal("Please provide an input file with -i")
    for fname in inputs:
        if not os.path.isfile(fname):
            fatal(f"Input json file doesnt exist: {fname}")
        if not fname.endswith(".json"):
            fatal(f"Input json file path doesnt end with `.json`. Wat? {fname}")
    if not ops.t:
        fatal("Please provide a pad trigger template file with -t")
    if not os.path.isfile(ops.t):
        fatal(f"Input json file doesnt exist: {ops.t}")
    if not ops.t.endswith(".json"):
        fatal(f"Template json file path doesnt end with `.json`. Wat? {ops.t}")


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
    newjson[ops.nameCommon] = template[ops.nameCommon]
    newjson[ops.nameObject] = template[ops.nameObject]
    for key in newjson[ops.nameObject]:
        if key == "OpcServerIp":
            host, port = getHost(sector), getPort(sector)
            newjson[ops.nameObject][key] = f"{host}:{port}"
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

