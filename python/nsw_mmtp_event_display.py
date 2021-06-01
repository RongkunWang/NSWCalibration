"""
A script for converting a MMTP L1A ROOT file into event displays.

Run like:
> setupATLAS && lsetup 'views LCG_98python3 x86_64-centos7-gcc8-opt'
> python nsw_mmtp_event_display.py -i MY_FILE.root
"""
import argparse
import os
import sys
import time
try:
    import ROOT
    ROOT.gROOT.SetBatch()
    ROOT.PyConfig.IgnoreCommandLineOptions = True
    ROOT.gErrorIgnoreLevel = ROOT.kWarning
except:
    sys.exit("Error: cant import ROOT.\n %s" % (__doc__))

NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")

def main():

    ops   = options()
    tr    = ttree()
    posit = ops.n >= 0
    ents  = min(ops.n, tr.GetEntries()) if posit else tr.GetEntries()
    start = time.time()
    rootlogon()

    print("Input  :: %s"       % (ops.i))
    print("Output :: %s"       % (ops.o))
    print("Ents   :: %s of %s" % (ents if posit else ops.n, tr.GetEntries()))
    print("")

    for (ient, ent) in enumerate(tr):
        if posit:
            if ient >= ents:
                break
            first, last = (ient == 0, ient == ents - 1)
        else:
            if ient < ents - abs(ops.n):
                continue
            first, last = (ient == ents - abs(ops.n), ient == ents - 1)
        if ient > 0 and ient % 10 == 0:
            progress(time.time()-start, ient, ents)
        display(ient, ent, ops.r, ops.o, first, last)
    progress(time.time()-start, ents, ents)

    print("")
    print("")
    print("Done! ^.^")

def options():
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-i", help="Input ROOT file")
    parser.add_argument("-t", help="Input TTree name", default="decodedData")
    parser.add_argument("-o", help="Output PDF file", default="events_%s.pdf" % (NOW))
    parser.add_argument("-n", help="Maximum number of displays to draw", default=100, type=int)
    parser.add_argument("-r", help="Rebin the strips of the x-axis", default=1, type=int)
    return parser.parse_args()

def ttree():
    ops = options()
    if not ops.i:
        fatal("Please give input ROOT file with -i")
    tname = ops.t
    fname = ops.i
    rfile = ROOT.TFile.Open(fname)
    if not rfile:
        fatal("Cannot open %s" % (fname))
    ttree = rfile.Get(tname)
    if not ttree:
        fatal("Cannot get %s from %s" % (tname, fname))
    ROOT.SetOwnership(rfile, False)
    return ttree

def run():
    ops = options()
    fname = ops.i
    try:
        # /path/to/file/data_test.12345678.root
        # -> 12345678
        basename = os.path.basename(fname)
        split = basename.split(".")
        return int(split[1])
    except:
        fatal("Cannot parse run number from %s" % (fname))

def display(ient, ent, rebin, pdf, first, last):
    nstrips_mmfe8 = 512
    nstrips_total = 8192
    nlayers = 8
    title = ";Strip number;Layer;" if rebin == 1 else ";Strip number (%s strips per bin);Layer;" % (rebin)
    tmp = ROOT.TH2D("temp_%010i" % (ient), title,
                   int(nstrips_total/rebin), -0.5, nstrips_total-0.5, nlayers*9, -0.5, nlayers-0.5)
    h2  = ROOT.TH2D("hist_%010i" % (ient), title,
                   int(nstrips_total/rebin), -0.5, nstrips_total-0.5, nlayers*9, -0.5, nlayers-0.5)
    style(tmp)
    style(h2)
    chPosition    = list(ent.v_artHit_chPosition)
    octupletLayer = list(ent.v_artHit_octupletLayer)
    for (chpos, layer) in zip(chPosition, octupletLayer):
        h2.Fill(chpos, layer)
    lines = []
    for vline in range(int(int(nstrips_total) / int(nstrips_mmfe8))):
        lines.append(ROOT.TLine(vline*nstrips_mmfe8, -0.5, vline*nstrips_mmfe8, nlayers-0.5))
    for hline in range(nlayers):
        lines.append(ROOT.TLine(0, hline, nstrips_total-0.5, hline))
    texts = []
    texts.append(ROOT.TLatex(nstrips_mmfe8,      nlayers-0.4, "Entry %i" % (ient)))
    texts.append(ROOT.TLatex(nstrips_mmfe8*7,    nlayers-0.4, "Run %s" % (run())))
    texts.append(ROOT.TLatex(nstrips_total*0.98, nlayers-0.4, "Hits"))
    canv = ROOT.TCanvas("canv_%010i" % (ient), "canv_%10i" % (ient), 800, 800)
    canv.Draw()
    # tmp: necessary for drawing h2 on top of the TLines
    tmp.SetMinimum(h2.GetMinimum())
    tmp.SetMaximum(h2.GetMaximum())
    tmp.Draw("colzsame")
    for line in lines:
        line.SetLineColor(17)
        line.SetLineWidth(1)
        line.Draw()
    h2.Draw("colzsame")
    for text in texts:
        text.SetTextSize(0.040)
        text.SetTextFont(42)
        text.Draw()
    if first:
        canv.Print(pdf+"(", "pdf")
    elif last:
        canv.Print(pdf+")", "pdf")
    else:
        canv.Print(pdf,     "pdf")

def rootlogon():
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetPadTickX(1)
    ROOT.gStyle.SetPadTickY(1)
    ROOT.gStyle.SetPaintTextFormat(".2f")
    ROOT.gStyle.SetTextFont(42)
    ROOT.gStyle.SetFillColor(10)
    ROOT.gStyle.SetPalette(ROOT.kCherry);
    ROOT.TColor.InvertPalette()
    ROOT.gStyle.SetPadTopMargin(0.06)
    ROOT.gStyle.SetPadRightMargin(0.16)
    ROOT.gStyle.SetPadBottomMargin(0.12)
    ROOT.gStyle.SetPadLeftMargin(0.14)

def style(hist):
    size = 0.045
    hist.SetLineWidth(3)
    hist.GetXaxis().SetTitleSize(size)
    hist.GetXaxis().SetLabelSize(size)
    hist.GetYaxis().SetTitleSize(size)
    hist.GetYaxis().SetLabelSize(size)
    hist.GetZaxis().SetTitleSize(size)
    hist.GetZaxis().SetLabelSize(size)
    hist.GetXaxis().SetTitleOffset(1.1)
    hist.GetYaxis().SetTitleOffset(1.4)
    hist.GetZaxis().SetTitleOffset(0.5)
    hist.GetZaxis().SetLabelOffset(0.02)
    hist.GetXaxis().SetTickLength(0.001)
    hist.GetYaxis().SetTickLength(0.001)
    hist.GetXaxis().SetNdivisions(505)

def progress(time_diff, nprocessed, ntotal):
    nprocessed, ntotal = float(nprocessed), float(ntotal)
    rate = (nprocessed+1)/time_diff
    msg = "\r > %6i / %6i | %2i%% | %8.2fHz | %6.1fm elapsed | %6.1fm remaining"
    msg = msg % (nprocessed, ntotal, 100*nprocessed/ntotal, rate, time_diff/60, (ntotal-nprocessed)/(rate*60))
    sys.stdout.write(msg)
    sys.stdout.flush()

def fatal(msg):
    sys.exit("Fatal error: %s" % (msg))

if __name__ == "__main__":
    main()

