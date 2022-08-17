#!/usr/bin/env tdaq_python
"""
A script for plotting pad trigger TDS channels versus event number
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
NPFEBS = 24
NCHANS = 104

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
    ttree = rfile.Get("decodedData")

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
        bname = f"pads_vmm_tds_channels.{NOW}.{lab}.{sector}.{run}.pdf"
        return os.path.join(dname, bname)
    else:
        return ops.o
    
def plot(ttree, ofile):

    ops  = options()
    ents = ttree.GetEntries()
    (lab, sector, run) = lab_and_sector_and_run()
    name = os.path.basename(ofile)

    #
    # create histograms
    #
    print("Creating histograms...")
    hists = {}
    for pfeb in [ -1 ] + list(range(NPFEBS)):
        hists[pfeb] = ROOT.TH2D(f"{name}_{pfeb}", ";Event number;TDS channel;Hits",
                                int(ents/10), -0.5, ents-0.5,
                                int(NCHANS),  -0.5, NCHANS-0.5)

    #
    # fill histogram
    #
    print(f"Entries: {ents}")
    for ent in range(ents):
        _ = ttree.GetEntry(ent)

        #
        # TTree interface
        #
        pfebs   = list(ttree.v_hit_pfeb)
        tds_chs = list(ttree.v_hit_tds_ch)

        #
        # fill
        #
        for (pfeb, tds_ch) in zip(pfebs, tds_chs):
            hists[pfeb].Fill(ent, tds_ch)
            hists[-1]  .Fill(ent, tds_ch)

    #
    # metadata
    #
    metadata = ROOT.TLatex(0.19, 0.95, f"{sector} in {lab}, Run {run}")
    metadata.SetTextFont(42)
    metadata.SetNDC()
    metadata.SetTextColor(ROOT.kBlack)
    metadata.SetTextSize(0.030)

    #
    # draw and save
    #
    rootlogon("2D")
    for pfeb in sorted(hists):
        first = pfeb == -1
        last  = pfeb == range(NPFEBS)[-1]
        pfebdata = ROOT.TLatex(0.72, 0.95, "All PFEBs" if pfeb == -1 else f"PFEB {pfeb:02g}")
        pfebdata.SetTextFont(42)
        pfebdata.SetNDC()
        pfebdata.SetTextColor(ROOT.kBlack)
        pfebdata.SetTextSize(0.030)
        style(hists[pfeb])
        canv = ROOT.TCanvas(f"{name}_{pfeb}", f"{name}_{pfeb}", 800, 800)
        hists[pfeb].Draw("colzsame")
        metadata.Draw()
        pfebdata.Draw()
        suffix = "(" if first else ")" if last else ""
        canv.SaveAs(f"{ofile}{suffix}", "pdf")

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
    ROOT.gStyle.SetPadRightMargin(0.04 if opt == "1D" else 0.15)
    ROOT.gStyle.SetPadBottomMargin(0.14)
    ROOT.gStyle.SetPadLeftMargin(0.15)

def style(hist):
    size = 0.045
    # size = 0.050
    hist.SetLineWidth(2)
    hist.GetXaxis().SetTitleSize(size)
    hist.GetXaxis().SetLabelSize(size)
    hist.GetYaxis().SetTitleSize(size)
    hist.GetYaxis().SetLabelSize(size)
    hist.GetZaxis().SetTitleSize(size)
    hist.GetZaxis().SetLabelSize(size)
    hist.GetXaxis().SetTitleOffset(1.2 if isinstance(hist, ROOT.TH2) else 1.1)
    hist.GetYaxis().SetTitleOffset(1.4 if isinstance(hist, ROOT.TH2) else 0.7)
    hist.GetZaxis().SetTitleOffset(1.0)
    hist.GetZaxis().SetLabelOffset(0.006)
    hist.GetXaxis().SetNdivisions(505)

if __name__ == "__main__":
    main()
