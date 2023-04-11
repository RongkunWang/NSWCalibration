"""
A short script to plot the output of the sTGCPadTdsBcidOffset calibration.

Run like:
> python nsw_pad_tds_bcid_offset.py -i myFile.root
"""

import argparse
import os
import sys

import ROOT
ROOT.gROOT.SetBatch()

HEX = 16
NBCS = 0xdeb + 1
NPFEBS = 24

def main():
    # check args
    ops = options()
    if not os.path.isfile(ops.i):
        fatal(f"Input ROOT file doesnt exist: {ops.i}")
    if not os.path.isdir(ops.o):
        fatal(f"Output dir doesnt exist: {ops.o}")

    # ROOT i/o
    rootlogon()
    rfile = ROOT.TFile(ops.i)
    ttree = rfile.Get(ops.t)
    if not ttree:
        fatal(f"Cannot find {ops.t} within {ops.i}")

    # plot
    plotAndSave(ops.i, ttree, ops.o)

def options():
    default_o = "/eos/atlas/atlascerngroupdisk/det-nsw/P1/trigger/analysis/tdsBcidOffset/"
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-i", help="Input ROOT file", required=True)
    parser.add_argument("-t", help="Input TTree name", default="nsw")
    parser.add_argument("-o", help="Output dirname",  default=default_o)
    return parser.parse_args()

def plotAndSave(inpath, ttree, outdir):

    # get metadata
    _ = ttree.GetEntry(0)
    padname = str(ttree.pad_trigger)
    padoffset = ttree.pad_trigger_bcid_offset

    # fill hist
    xtitle = f"TDS BCID offset (pad offset = {padoffset:#03x})"
    ytitle = f"PFEB number"
    ztitle = f"TDS/Pad Trigger BCID aligned"
    hist = ROOT.TH2D("hist", f";{xtitle}; {ytitle}; {ztitle}",
                     NBCS, -0.5, NBCS-0.5,
                     NPFEBS, -0.5, NPFEBS-0.5)
    for ent in range(ttree.GetEntries()):
        _ = ttree.GetEntry(ent)
        tds_offset = ttree.tds_bcid_offset
        error_word = ttree.pfeb_bcid_error
        for pfeb in range(NPFEBS):
            error = (error_word >> pfeb) & 0b1
            hist.Fill(tds_offset, pfeb, not error)
            if not error:
                print(f"No error at TDS BCID offset = {tds_offset} for PFEB {pfeb}")
    hist.SetMinimum(0.0)
    hist.SetMaximum(1.0)
    style(hist)

    # annotations
    padtex = ROOT.TLatex(0.15, 0.95, padname)
    padtex.SetTextSize(0.025)
    padtex.SetNDC()

    # create output name
    bname = os.path.basename(inpath)
    bname = bname.rstrip(".root")
    bname = bname + ".pdf"
    fullpath = os.path.join(outdir, bname)

    # plot hist
    cname = "padTdsBcidOffset"
    canv = ROOT.TCanvas(cname, cname, 800, 800)
    canv.Draw()
    hist.Draw("colzsame")
    padtex.Draw()
    canv.SaveAs(fullpath)

def fatal(msg):
    sys.exit(f"Fatal: {msg}")

def rootlogon():
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetPadTickX(1)
    ROOT.gStyle.SetPadTickY(1)
    ROOT.gStyle.SetPaintTextFormat(".2f")
    ROOT.gStyle.SetTextFont(42)
    ROOT.gStyle.SetFillColor(10)
    ROOT.gStyle.SetPalette(ROOT.kBlackBody)
    ROOT.TColor.InvertPalette()
    ROOT.gStyle.SetPadTopMargin(0.06)
    ROOT.gStyle.SetPadRightMargin(0.19)
    ROOT.gStyle.SetPadBottomMargin(0.12)
    ROOT.gStyle.SetPadLeftMargin(0.13)

def style(hist):
    size = 0.045
    hist.SetLineWidth(2)
    hist.GetXaxis().SetTitleSize(size)
    hist.GetXaxis().SetLabelSize(size)
    hist.GetYaxis().SetTitleSize(size)
    hist.GetYaxis().SetLabelSize(size)
    hist.GetZaxis().SetTitleSize(size)
    hist.GetZaxis().SetLabelSize(size)
    hist.GetXaxis().SetTitleOffset(1.2)
    hist.GetYaxis().SetTitleOffset(1.2)
    hist.GetZaxis().SetTitleOffset(1.5)
    hist.GetZaxis().SetLabelOffset(0.003)

if __name__ == "__main__":
    main()
