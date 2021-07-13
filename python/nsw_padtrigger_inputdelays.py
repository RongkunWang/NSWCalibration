#!/usr/bin/env tdaq_python
"""
A script for plotting PFEB BCID versus pad trigger input phase/delay.
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
NPHASES = 16
NBCIDS  = 16

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
    # make output
    #
    print(f"Creating output file ({output()})...")
    ofile = ROOT.TFile.Open(output(), "recreate")

    #
    # parse and plot
    #
    plot(ttree, ofile)

    #
    # convert to website
    #
    ofile.Close()
    if ops.r:
        root2html(output())

def options():
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-i", help="Input ROOT file",       default="")
    parser.add_argument("-o", help="Output ROOT file",      default="")
    parser.add_argument("-s", help="Sector under test",     default="")
    parser.add_argument("-l", help="Lab name, e.g. 191",    default="")
    parser.add_argument("-r", help="Make plots w/root2html", action="store_true")
    return parser.parse_args()

def output():
    ops = options()
    if not ops.o:
        dname = os.path.join(EOS, "padtrigger/")
        bname = f"inputdelays.{NOW}.canv.root"
        return os.path.join(dname, bname)
    else:
        return ops.o
    
def plot(ttree, ofile):

    rootlogon("1D")
    ops  = options()
    ents = ttree.GetEntries()
    hist = {}

    #
    # create histograms
    #
    print("Creating histograms...")
    for feb in range(NPFEBS):
        hist[feb] = ROOT.TH2D(
            f"pfeb_{feb:02d}_phase_vs_bcid",
            f";Input phase [240 MHz];BCID;Reads",
            int(NPHASES), -0.5, NPHASES-0.5,
            int(NBCIDS),  -0.5, NBCIDS-0.5,
        )

    #
    # fill histograms
    #
    print(f"Entries: {ents}")
    for ent in range(ents):
        _ = ttree.GetEntry(ent)

        if ent % 100 == 0:
            print(f"Entry {ent}")

        #
        # TTree interface
        #
        delay      = convert(ttree.delay)
        pfeb_index = list(ttree.pfeb_index)
        pfeb_bcid  = list(ttree.pfeb_bcid)

        #
        # fill
        #
        for (index, bcid) in zip(pfeb_index, pfeb_bcid):
            hist[index].Fill(delay, bcid)

    #
    # create plots
    #
    for feb in hist:
        style(hist[feb])
    print("Creating plots...")
    ofile.cd()

    #
    # metadata
    #
    (lab, sector, run) = lab_and_sector_and_run()
    metadata = ROOT.TLatex(0.15, 0.95, f"{sector} in {lab}, Run {run}")
    metadata.SetTextFont(42)
    metadata.SetNDC()
    metadata.SetTextColor(ROOT.kBlack)
    metadata.SetTextSize(0.030)

    #
    # draw and save
    #
    ofile.cd()
    rootlogon("2D")
    for feb in hist:
        name    = f"inputdelay_pfeb_{feb:02d}_{NOW}"
        canv    = ROOT.TCanvas(name, name, 800, 800)
        febname = ROOT.TLatex(0.65, 0.95, f"PFEB {feb:02d}")
        febname.SetNDC()
        febname.SetTextSize(0.030)
        hist[feb].Draw("colzsame")
        canv.SetLogy(False)
        canv.SetLogz(False)
        metadata.Draw()
        febname.Draw()
        canv.Write()


def convert(delay):
    #
    # 0xAAAAAAAA => 0xA
    #
    return delay & 0b1111

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

def root2html(fname):
    webpath = "https://www.cern.ch/nsw191/trigger/padtrigger"
    print("Converting TCanvas into html with root2html...")
    if not os.path.isfile(fname):
        fatal(f"Bad ROOT file for root2html: {fname}")
    script = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/root2html.py"
    os.system(f"{script} {fname}")
    print("Plots:")
    print(f"  {webpath}/{os.path.basename(fname.rstrip('.canv.root'))}")
    print("")

def rootlogon(opt):
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetPadTickX(1)
    ROOT.gStyle.SetPadTickY(1)
    ROOT.gStyle.SetPaintTextFormat(".2f")
    ROOT.gStyle.SetTextFont(42)
    ROOT.gStyle.SetFillColor(10)
    ROOT.gStyle.SetPalette(ROOT.kBlackBody)
    ROOT.TColor.InvertPalette()
    ROOT.gStyle.SetPadTopMargin(0.06)
    ROOT.gStyle.SetPadRightMargin(0.04 if opt == "1D" else 0.19)
    ROOT.gStyle.SetPadBottomMargin(0.14)
    ROOT.gStyle.SetPadLeftMargin(0.15)

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
    hist.GetYaxis().SetTitleOffset(1.2 if isinstance(hist, ROOT.TH2) else 0.7)
    hist.GetZaxis().SetTitleOffset(1.2)
    hist.GetZaxis().SetLabelOffset(0.003)
    if not isinstance(hist, ROOT.TH2):
        hist.GetXaxis().SetNdivisions(505)

if __name__ == "__main__":
    main()
