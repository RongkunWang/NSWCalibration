"""
setupATLAS && lsetup 'views LCG_98python3 x86_64-centos7-gcc8-opt'
"""
import argparse
import collections
import json
import os
import sys
if sys.version_info[0] < 3:
    sys.exit("Error: python3 is required! Consider running: %s if you have access to cvmfs" % (__doc__))

REGISTER = "sdt_dac"

def main():
    ops = options()
    check_options()
    announce()
    with open(ops.i) as json_file:
        newconf = json.load(json_file, object_pairs_hook=collections.OrderedDict)
    # update vmm_common_config
    for common in newconf:
        if common == "vmm_common_config":
            update(newconf[common], ops.n)
            break
    # update the FEBs
    for feb in newconf:
        for vmm in newconf[feb]:
            if vmm.startswith("vmm"):
                update(newconf[feb][vmm], ops.n)
    with open(output(), 'w') as json_file:
        json.dump(newconf, json_file, indent=4)
    mention_diff()

def options():
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-i", help="Input JSON file",                   default=None)
    parser.add_argument("-o", help="Output JSON file",                  default=None)
    parser.add_argument("-n", help="Increase threshold by this number", default=None)
    parser.add_argument("-f", help="Force overwrite of output file if it already exists", action="store_true")
    return parser.parse_args()

def update(obj, n):
    for reg in obj:
        if reg == REGISTER:
            obj[reg] += int(n)
            return

def check_options():
    ops = options()
    if not ops.i:
        fatal("Please provide an input file with -i")
    if not os.path.isfile(ops.i):
        fatal("Input json file doesnt exist: %s" % (ops.i))
    if not ops.i.endswith(".json"):
        fatal("Input json file path doesnt end with `.json`. Wtf? %s" % (ops.i))
    if not ops.n:
        fatal("Please provide a number to increase the threshold with -n")
    try:
        _ = int(ops.n)
    except:
        fatal("Cannot convert -n to an integer: %s" % (ops.n))
    if os.path.isfile(output()) and not ops.f:
        fatal("Output file (%s) already exists. Use -f to force overwrite" % (ops.o))

def output():
    ops = options()
    if ops.o:
        return ops.o
    else:
        tag = "IncreaseThresholdBy%s" % (ops.n)
        new = ops.i.rstrip(".json")
        new = "%s_%s.json" % (new, tag)
        return new

def announce():
    ops = options()
    print("")
    print(" Input JSON file:       %s" % (ops.i))
    print(" Output JSON file:      %s" % (output()))
    print(" Increase threshold by: %s" % (ops.n))
    print("")

def mention_diff():
    print("")
    print("Done! ^.^")
    print("")
    print("Use `diff -w` to diff these files without considering changes in whitespace.")
    print("")

def fatal(msg):
    sys.exit("Fatal error: %s" % (msg))

if __name__ == "__main__":
    main()
