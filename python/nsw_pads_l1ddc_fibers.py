#!/usr/bin/env tdaq_python
"""
A script for plotting PFEB BCID difference observed in the pad trigger
  as a function of ROC phase.
"""
import argparse
import array
import os
import sys
import time
import ROOT
ROOT.gROOT.SetBatch()

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
        bname = f"sTGCPadsL1DDCFibers.{NOW}.{lab}.{sector}.{run}.pdf"
        return os.path.join(dname, bname)
    else:
        return ops.o

def plot(ttree, ofile):

    ops  = options()
    ents = ttree.GetEntries()
    (lab, sector, run) = labSectorRun()
    name = f"sTGCPadsL1DDCFibers_{sector}_{run}"

    #
    # create histograms
    #
    ttree.GetEntry(0)
    step_size = ttree.phase_step
    print("Creating histograms...")
    xtitle = "ROC 40MHz TDS ePLL phase (L)"
    ytitle = "ROC 40MHz TDS ePLL phase (R)"
    ztitle  = "< L #minus R BCID difference >"
    hist = {}
    for key in ["all"] + PAIRS:
        suffix = "all" if key=="all" else f"{key[0]:02}_{key[1]:02}"
        hist[key] = ROOT.TH2D(f"{name}_{suffix}", f";{xtitle};{ytitle};{ztitle}",
                                int(NPHASES/step_size), -0.5, NPHASES-0.5,
                                int(NPHASES/step_size), -0.5, NPHASES-0.5)

    #
    # fill histogram
    #
    print(f"Entries: {ents}")
    for ent in range(ents):
        _ = ttree.GetEntry(ent)

        #
        # TTree interface
        #
        nreads     = int(ttree.nreads)
        phase_L    = int(ttree.phase_L)
        phase_R    = int(ttree.phase_R)
        pfeb_bcid  = list(ttree.pfeb_bcid)
        pfeb_mask  = list(ttree.pfeb_mask)
        pfeb_index = list(ttree.pfeb_index)
        pfeb_left  = list(ttree.pfeb_left)

        #
        # fill
        #
        for key in PAIRS:
            (feb_0, feb_1) = key
            bcid_0, bcid_1 = pfeb_bcid[feb_0],  pfeb_bcid[feb_1]
            mask_0, mask_1 = pfeb_mask[feb_0],  pfeb_mask[feb_1]
            left_0, left_1 = pfeb_left[feb_0],  pfeb_left[feb_1]
            indx_0, indx_1 = pfeb_index[feb_0], pfeb_index[feb_1]
            if not mask_0 or not mask_1:
                continue
            if (left_0 and left_1) or (not left_0 and not left_1):
                fatal("Im confused - cant have two adjacent LL/RR PFEBs!")
            bcid_LR_diff = bcidDiff(bcid_0, bcid_1)
            hist["all"].Fill(phase_L, phase_R, bcid_LR_diff)
            hist[ key ].Fill(phase_L, phase_R, bcid_LR_diff)

    #
    # beauty and save
    #
    for key in ["all"] + PAIRS:

        #
        # metadata
        #
        tag = "All PFEBs" if key=="all" else f"PFEB {key[0]:02} vs {key[1]:02}"
        metadata = ROOT.TLatex(0.18, 0.95, f"{sector} in {lab}, Run {run}           {tag}")
        metadata.SetTextFont(42)
        metadata.SetNDC()
        metadata.SetTextColor(ROOT.kBlack)
        metadata.SetTextSize(0.030)

        #
        # scale by the number of register reads,
        #  remembering that some PFEBs may not be connected
        # NB: "2" because difference of pairs
        #
        scaling = (sum(pfeb_mask) / 2) if key=="all" else 1
        hist[key].Scale(1 / (nreads * scaling))

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
        if key=="all":
            canv.Print(ofile+"(", "pdf")
        elif key==PAIRS[-1]:
            canv.Print(ofile+")", "pdf")
        else:
            canv.Print(ofile,     "pdf")

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
    labs = ["BB5", "VS", "191A", "191C"]
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
