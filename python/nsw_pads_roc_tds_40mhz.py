#!/usr/bin/env tdaq_python
"""
"""
import argparse
import array
import os
import statistics
import sys
import time
import ROOT
ROOT.gROOT.SetBatch()
# ROOT.gErrorIgnoreLevel = ROOT.kWarning

NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")
EOS = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/"
NPFEBS  = 24
NPHASES = 128
BCID_MAX = 16
PAIRS = [ (feb, feb+1) for feb in range(0, NPFEBS, 2) ]

def main():

    rootlogon("2D")

    #
    # parse CL args
    #
    ops = options()
    if not ops.i:
        fatal("Please give input file with -i")
    if not os.path.isfile(ops.i):
        fatal(f"File does not exist: {ops.i}")
    if not os.path.isdir(EOS):
        fatal(f"Cannot find {EOS}. Please run `kinit`")

    #
    # get the TTree
    #
    rfile = ROOT.TFile(ops.i)
    ttree = rfile.Get("nsw")

    #
    # parse and plot
    #
    plot(ttree, output())
    print("")
    print(f"www.cern.ch/nsw191/trigger/padtrigger/{os.path.basename(output())}")
    print("")

def options():
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-i", help="Input ROOT file",    default="")
    parser.add_argument("-o", help="Output ROOT file",   default="")
    parser.add_argument("-s", help="Sector under test",  default="")
    parser.add_argument("-l", help="Lab name, e.g. 191", default="")
    return parser.parse_args()

def output():
    ops = options()
    if not ops.o:
        dname = os.path.join(EOS, "padtrigger/")
        (lab, sector, run) = labSectorRun()
        bname = f"sTGCPadsRocTds40Mhz.{NOW}.{lab}.{sector}.{run}.pdf"
        return os.path.join(dname, bname)
    else:
        return ops.o

def plot(ttree, ofile):

    ops  = options()
    ents = ttree.GetEntries()
    (lab, sector, run) = labSectorRun()
    name = f"sTGCPadsRocTds40Mhz_{sector}_{run}"

    #
    # create histograms
    #
    ttree.GetEntry(0)
    step_size = ttree.phase_step
    print("Creating histograms...")
    xtitle = "ROC/TDS 40MHz phase"
    ytitle = "PFEB"
    ztitle = "BCID error"
    hist = {}
    hist["error"] = ROOT.TH2D(f"{name}_error", f";{xtitle};{ytitle};{ztitle}",
                              int(NPHASES/step_size), -0.5, NPHASES-0.5,
                              NPFEBS, -0.5, NPFEBS-0.5)
    xtitle = "ROC/TDS 40MHz phase (even PFEB)"
    ytitle = "ROC/TDS 40MHz phase (odd PFEB)"
    ztitle = "< BCID difference >"
    for key in ["all"] + PAIRS:
        suffix = "all" if key=="all" else f"{key[0]:02}_{key[1]:02}"
        hist[key] = ROOT.TH2D(f"{name}_{suffix}", f";{xtitle};{ytitle};{ztitle}",
                                int(NPHASES/step_size), -0.5, NPHASES-0.5,
                                int(NPHASES/step_size), -0.5, NPHASES-0.5)

    #
    # fill BCID diff histograms
    #
    bcids = getBcidPerPfebPerPhase(ttree)
    for key in PAIRS:
        (feb_0, feb_1) = key
        if not bcids[feb_0] or not bcids[feb_1]:
            continue
        for phase_0 in bcids[feb_0]:
            for phase_1 in bcids[feb_1]:
                bcid_0 = bcids[feb_0][phase_0]
                bcid_1 = bcids[feb_1][phase_1]
                bcid_diff = bcidDiff(bcid_0, bcid_1)
                hist["all"].Fill(phase_0, phase_1, bcid_diff)
                hist[ key ].Fill(phase_0, phase_1, bcid_diff)
    scaling = (len(bcids) - bcids.count(None)) / 2
    hist["all"].Scale(1 / scaling)

    #
    # fill error histogram
    #
    print(f"Entries: {ents}")
    for ent in range(ents):
        _ = ttree.GetEntry(ent)
        phase      = int(ttree.phase)
        pfeb_index = list(ttree.pfeb_index)
        pfeb_error = list(ttree.pfeb_error)
        for (index, error) in zip(pfeb_index, pfeb_error):
            hist["error"].Fill(phase, index, error)

    #
    # beauty and save
    #
    for key in ["all"] + PAIRS + ["error"]:

        #
        # metadata
        #
        tag = "All PFEBs" if key in ["all", "error"] else f"PFEB {key[0]:02} vs {key[1]:02}"
        metadata = ROOT.TLatex(0.18, 0.95, f"{sector} in {lab}, Run {run}           {tag}")
        metadata.SetTextFont(42)
        metadata.SetNDC()
        metadata.SetTextColor(ROOT.kBlack)
        metadata.SetTextSize(0.030)

        #
        # draw and save
        #
        hist[key].SetMinimum(0.0)
        hist[key].SetMaximum(1.5)
        style(hist[key])
        canv = ROOT.TCanvas(hist[key].GetName(),
                            hist[key].GetName(),
                            800, 800)
        hist[key].Draw("colzsame")
        metadata.Draw()
        if key == "all":
            canv.Print(ofile + "(", "pdf")
        elif key == "error":
            canv.Print(ofile + ")", "pdf")
        else:
            canv.Print(ofile,       "pdf")

def getBcidPerPfebPerPhase(ttree):
    bcids = []
    for index in range(NPFEBS):
        bcids.append(dict())

    # ttree loop
    for ent in range(ttree.GetEntries()):
        _ = ttree.GetEntry(ent)
        phase      = int(ttree.phase)
        pfeb_index = list(ttree.pfeb_index)
        pfeb_bcid  = list(ttree.pfeb_bcid)
        for (index, bcid) in zip(pfeb_index, pfeb_bcid):
            bcids[index][phase] = bcid

    # check for disconnected PFEBs
    for index in range(NPFEBS):
        if all([bcids[index][phase] == bcids[index][0] \
                for phase in bcids[index]]):
            bcids[index] = None

    return bcids

def bcidDiff(val_0, val_1):
    # NB: 4 bits of BCID
    # Therefore BCID rolls over 15 -> 0
    simple_diff = abs(val_0 - val_1)
    return min(simple_diff, abs(simple_diff - BCID_MAX))

def labSectorRun():
    ops = options()
    lab, sector = "", ""
    if ops.l:
        lab = ops.l
    elif labGuess():
        lab = labGuess()
        print(f"Lab guess: {lab}")
    if ops.s:
        sector = ops.s
    elif sectorGuess():
        sector = sectorGuess()
        print(f"Sector guess: {sector}")
    run = runGuess()
    if not lab:
        fatal("Couldnt guess lab")
    if not sector:
        fatal("Couldnt guess sector")
    if not run:
        fatal("Couldnt guess run")
    return (lab, sector, run)

def labGuess():
    ops = options()
    fname = os.path.basename(ops.i)
    labs = ["BB5", "VS", "191A", "191C", "P1"]
    for lab in labs:
        if lab in fname:
            if lab in ["191A", "191C"]:
                return "191"
            else:
                return lab
    print(f"Cannot guess lab based on {fname}")
    print("Please use `-l` to specify in which lab the sector was tested")
    return ""

def sectorGuess():
    ops = options()
    fname = os.path.basename(ops.i)
    sides = ["A", "C"]
    sects = ["%02i" % (sect) for sect in range(1, 17)]
    for side in sides:
        for sect in sects:
            sector = side+sect
            if sector in fname:
                return sector
    print(f"Cannot guess sector based on file = {ops.i}")
    print("Please use `-s` to specify which sector is under test")
    return ""

def runGuess():
    ops = options()
    fname = os.path.basename(ops.i)
    try:
        run = int(fname.split(".")[1])
    except:
        return 99999999
        # fatal(f"Cannot extract run number from {ops.i}")
    return run

def fatal(msg):
    sys.exit(f"Error: {msg}")

def rootlogon(opt):
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetPadTickX(1)
    ROOT.gStyle.SetPadTickY(1)
    ROOT.gStyle.SetPaintTextFormat(".2f")
    ROOT.gStyle.SetTextFont(42)
    ROOT.gStyle.SetFillColor(10)
    ROOT.gStyle.SetPadTopMargin(0.06)
    ROOT.gStyle.SetPadRightMargin(0.04 if opt == "1D" else 0.19)
    ROOT.gStyle.SetPadBottomMargin(0.12)
    ROOT.gStyle.SetPadLeftMargin(0.15)
    # https://coolors.co/
    colors = [0xffffff, 0xfff056, 0xf08a4b, 0xc81d25]
    makePalette(colors)

def makePalette(colors):
    ncontours = 30
    stops = array.array("d", [val/(len(colors)-1) for val in list(range(len(colors)))])
    red   = array.array("d", [(color >> 16 & 0xFF)/0xFF for color in colors])
    green = array.array("d", [(color >>  8 & 0xFF)/0xFF for color in colors])
    blue  = array.array("d", [(color >>  0 & 0xFF)/0xFF for color in colors])
    ROOT.TColor.CreateGradientColorTable(len(stops), stops, red, green, blue, ncontours)
    ROOT.gStyle.SetNumberContours(ncontours)

def style(hist):
    # size = 0.045
    size = 0.050
    hist.SetLineWidth(2)
    hist.GetXaxis().SetTitleSize(size)
    hist.GetXaxis().SetLabelSize(size)
    hist.GetYaxis().SetTitleSize(size)
    hist.GetYaxis().SetLabelSize(size)
    hist.GetZaxis().SetTitleSize(size)
    hist.GetZaxis().SetLabelSize(size)
    hist.GetXaxis().SetTitleOffset(1.2 if isinstance(hist, ROOT.TH2) else 1.1)
    hist.GetYaxis().SetTitleOffset(1.3 if isinstance(hist, ROOT.TH2) else 0.7)
    hist.GetZaxis().SetTitleOffset(1.3)
    hist.GetZaxis().SetLabelOffset(0.003)
    if not isinstance(hist, ROOT.TH2):
        hist.GetXaxis().SetNdivisions(505)

if __name__ == "__main__":
    main()
