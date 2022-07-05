#!/usr/bin/env tdaq_python
"""
A script for plotting pad trigger coincidences versus ROC TDS TTC control phase
"""
import argparse
import array
import os
import sys
import time
import ROOT
ROOT.gROOT.SetBatch()
ROOT.gErrorIgnoreLevel = ROOT.kWarning

NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")
EOS = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/"
NPFEBS  = 24
NPHASES = 8

def main():

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
    parser.add_argument("-i", help="Input ROOT file",       default="")
    parser.add_argument("-o", help="Output ROOT file",      default="")
    parser.add_argument("-s", help="Sector under test",     default="")
    parser.add_argument("-l", help="Lab name, e.g. 191",    default="")
    return parser.parse_args()

def output():
    ops = options()
    if not ops.o:
        dname = os.path.join(EOS, "padtrigger/")
        (lab, sector, run) = lab_and_sector_and_run()
        bname = f"pads_control_phase.{NOW}.{lab}.{sector}.{run}.pdf"
        return os.path.join(dname, bname)
    else:
        return ops.o
    
def plot(ttree, ofile):

    ops  = options()
    ents = ttree.GetEntries()
    (lab, sector, run) = lab_and_sector_and_run()
    name = f"pads_control_phase_{sector}_{run}"

    #
    # create histogram
    #
    print("Creating histogram...")
    hist = ROOT.TH2D(name, ";ROC/TDS TTC control phase;;PFEB hit rate",
                     int(NPHASES), -0.5, NPHASES-0.5,
                     int(NPFEBS),  -0.5, NPFEBS-0.5,
                     )

    #
    # fill histogram
    #
    print(f"Entries: {ents}")
    for ent in range(ents):
        _ = ttree.GetEntry(ent)

        #
        # TTree interface
        #
        delay     = int(ttree.delay)
        pfeb_addr = int(ttree.pfeb_addr)
        pfeb_rate = int(ttree.pfeb_rate)

        #
        # fill
        #
        xbin = hist.GetXaxis().FindBin(delay)
        ybin = hist.GetYaxis().FindBin(pfeb_addr)
        hist.SetBinContent(xbin, ybin, pfeb_rate)

    #
    # bin labels
    #
    hist.GetYaxis().SetLabelFont(82)
    hist.GetYaxis().SetLabelSize(0.04)
    pfeb_names = ordered_pfebs()
    for pfeb_addr in range(NPFEBS):
        pfeb_name = pfeb_names[pfeb_addr]
        label = f"{pfeb_name}({pfeb_addr:02g})"
        ybin = hist.GetYaxis().FindBin(pfeb_addr)
        hist.GetYaxis().SetBinLabel(ybin, label)

    #
    # metadata
    #
    metadata = ROOT.TLatex(0.35, 0.95, f"{sector} in {lab}, Run {run}")
    metadata.SetTextFont(42)
    metadata.SetNDC()
    metadata.SetTextColor(ROOT.kBlack)
    metadata.SetTextSize(0.030)

    #
    # draw and save
    #
    hist.SetMinimum(0.2)
    style(hist)
    rootlogon("2D")
    canv = ROOT.TCanvas(name, name, 800, 800)
    hist.Draw("colzsame")
    canv.SetLogy(False)
    canv.SetLogz(True)
    metadata.Draw()
    canv.SaveAs(ofile)

def ordered_pfebs():
    return [
      "PFEB_L1Q1_IP", "PFEB_L2Q1_IP", "PFEB_L3Q1_IP", "PFEB_L4Q1_IP",
      "PFEB_L1Q1_HO", "PFEB_L2Q1_HO", "PFEB_L3Q1_HO", "PFEB_L4Q1_HO",
      "PFEB_L1Q2_IP", "PFEB_L2Q2_IP", "PFEB_L3Q2_IP", "PFEB_L4Q2_IP",
      "PFEB_L1Q2_HO", "PFEB_L2Q2_HO", "PFEB_L3Q2_HO", "PFEB_L4Q2_HO",
      "PFEB_L1Q3_IP", "PFEB_L2Q3_IP", "PFEB_L3Q3_IP", "PFEB_L4Q3_IP",
      "PFEB_L1Q3_HO", "PFEB_L2Q3_HO", "PFEB_L3Q3_HO", "PFEB_L4Q3_HO",
    ]

def lab_and_sector_and_run():
    ops = options()
    lab, sector = "", ""
    if ops.l:
        lab = ops.l
    elif lab_guess():
        lab = lab_guess()
        print(f"Lab guess: {lab}")
    if ops.s:
        sector = ops.s
    elif sector_guess():
        sector = sector_guess()
        print(f"Sector guess: {sector}")
    run = run_guess()
    if not lab:
        fatal("Couldnt guess lab")
    if not sector:
        fatal("Couldnt guess sector")
    if not run:
        fatal("Couldnt guess run")
    return (lab, sector, run)

def lab_guess():
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

def sector_guess():
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

def run_guess():
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
    ROOT.gStyle.SetPalette(ROOT.kCherry)
    ROOT.TColor.InvertPalette()
    ROOT.gStyle.SetPadTopMargin(0.06)
    ROOT.gStyle.SetPadRightMargin(0.04 if opt == "1D" else 0.19)
    ROOT.gStyle.SetPadBottomMargin(0.14)
    ROOT.gStyle.SetPadLeftMargin(0.25)

def style(hist):
    # size = 0.045
    size = 0.050
    hist.SetLineWidth(2)
    hist.GetXaxis().SetTitleSize(size)
    hist.GetXaxis().SetLabelSize(size)
    hist.GetYaxis().SetTitleSize(size)
    # hist.GetYaxis().SetLabelSize(size)
    hist.GetZaxis().SetTitleSize(size)
    hist.GetZaxis().SetLabelSize(size)
    hist.GetXaxis().SetTitleOffset(1.2 if isinstance(hist, ROOT.TH2) else 1.1)
    hist.GetYaxis().SetTitleOffset(1.2 if isinstance(hist, ROOT.TH2) else 0.7)
    hist.GetZaxis().SetTitleOffset(1.4)
    hist.GetZaxis().SetLabelOffset(0.003)
    if not isinstance(hist, ROOT.TH2):
        hist.GetXaxis().SetNdivisions(505)

if __name__ == "__main__":
    main()
