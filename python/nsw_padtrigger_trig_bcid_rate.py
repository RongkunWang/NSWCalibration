"""
Run me at P1 like:

> python nsw_padtrigger_trig_bcid_rate.py --since "2023-Jul-02 06:20:00" --until "2023-Jul-02 10:13:00"

NB: This assumes --since/--until args are provided in the CERN timezone.
"""
import argparse
import array
import os
import sys
import time
import ROOT
ROOT.gErrorIgnoreLevel = ROOT.kWarning
ROOT.gROOT.SetBatch()

# https://gitlab.cern.ch/atlas-tdaq-software/pbeast/
# https://gitlab.cern.ch/atlas-tdaq-software/beauty/
from beauty import Beauty

from datetime import datetime
from datetime import timedelta
TEMPLATE = "%Y-%b-%d %H:%M:%S"
INTERVAL = 30 # seconds

NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")
SIDES = ["A", "C"]
SECTS = range(16)
BCIDS = ["_m3", "_m2", "_m1", "", "_p1", "_p2", "_p3"]
COLORS = {"_m3": ROOT.kOrange + 7,
          "_m2": ROOT.kAzure + 6,
          "_m1": ROOT.kYellow - 4,
          ""   : ROOT.kGreen - 7,
          "_p1": ROOT.kRed - 4,
          "_p2": ROOT.kAzure + 1,
          "_p3": ROOT.kViolet - 4}

def main():
    ops = options()
    checkArgs(ops)
    since = datetime.strptime(ops.since, TEMPLATE)
    until = datetime.strptime(ops.until, TEMPLATE)
    data = beautyAndPbeast(since, until)
    plot(since, until, data, getOutput(ops, since, until))

def options():
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-o", help="Output file of pdf", default=None)
    parser.add_argument("--since", help="Time range for plotting: since", required=True)
    parser.add_argument("--until", help="Time range for plotting: until", required=True)
    return parser.parse_args()

def getOutput(ops, since, until):
    if ops.o:
        return ops.o
    since_ = since.strftime("%Y_%m_%d_%H_%M")
    until_ = until.strftime("%Y_%m_%d_%H_%M")
    return f"nsw_padtrigger_trig_bcid_rate.since_{since_}.until_{until_}.pdf"

def checkArgs(ops):
    for name in "since", "until":
        attr = getattr(ops, name)
        try:
            _ = datetime.strptime(attr, TEMPLATE)
        except:
            fatal(f"Cant parse --{name} ({attr}). Please use {TEMPLATE}")

def beautyAndPbeast(since, until):
    data = {}
    server = "http://pc-tdq-bst-12.cern.ch:8080"
    beauty = Beauty(server)
    for side in SIDES:
        for sector in SECTS:
            name = f"{side}{sector+1:02}"
            for bcid in BCIDS:
                data[name, bcid] = query(beauty, since, until, side, sector, bcid)
    data = nativize(data)
    return data

def query(beauty, since, until, side, sector, bcid):
    part = "ATLAS"
    classname = "PadTriggerRegisters"
    rate = "trig_bcid_rate"
    obj = f".*sTGC-{side}/V0/SCA/PadTrig/S{sector}/.*"
    var = f"{rate}{bcid}"
    print(f"Read {side} {sector} {var}")
    series = beauty.timeseries(since, until, part, classname, var, source = obj)
    if len(series) != 1:
        fatal(f"Problem! len(series) = {len(series)}. Only 1 attr should match.")
    series = series[0]
    return series

def nativize(data):
    for key in data:
        data[key] = [(x.to_pydatetime(), int(y) if y > 0 else 0) for (x,  y) in zip(data[key].x, data[key].y)]
    return data

def plot(since, until, data, fname):
    rootlogon()
    print(f"Writing to {fname} ...")
    drawFirstPage(fname)
    for side in SIDES:
        for sect in SECTS:
            sector = f"{side}{sect+1:02}"
            plotOneSector(since, until, data, sector, fname)
    drawLastPage(fname)

def drawFirstPage(fname):
    cname = os.path.basename(fname)
    canv = ROOT.TCanvas(cname, cname, 800, 800)
    canv.Draw()
    latex = ROOT.TLatex()
    latex.DrawLatexNDC(0.35, 0.5, "nsw_padtrigger_trig_bcid_rate")
    latex.DrawLatexNDC(0.35, 0.4, NOW)
    canv.Print(fname + "(", "pdf")

def drawLastPage(fname):
    cname = os.path.basename(fname)
    canv = ROOT.TCanvas(cname, cname, 800, 800)
    canv.Draw()
    latex = ROOT.TLatex()
    latex.DrawLatexNDC(0.5, 0.5, "Blank last page")
    canv.Print(fname + ")", "pdf")

def plotOneSector(since, until, data, sector, fname):
    xbins = int((until - since).total_seconds() / INTERVAL)
    ybins = len(BCIDS)
    xlo, xhi = -0.5, xbins - 0.5
    ylo, yhi = -ybins/2, ybins/2
    hist = ROOT.TH2F(f"{sector}", ";Time series [30sec];;Trigger rate [Hz]",
                     xbins, xlo, xhi,
                     ybins, ylo, yhi)
    style(hist)

    # fill hist
    for xbin in range(xbins):
        xtime = since + timedelta(seconds=INTERVAL*xbin)
        for ybin in range(ybins):
            bcid = BCIDS[ybin]
            for (dt, content) in data[sector, bcid]:
                dt_naive = dt.replace(tzinfo=None)
                if dt_naive < xtime:
                    continue
                hist.SetBinContent(xbin+1, ybin+1, content)
                break

    # bin labels
    hist.GetYaxis().SetLabelColor(ROOT.kWhite)
    labels = {"_m3": "BCID#minus3", "_m2": "BCID#minus2", "_m1": "BCID#minus1", "": "BCID",
              "_p1": "BCID+1", "_p2": "BCID+2", "_p3": "BCID+3"}
    binlabels = []
    label_x, label_y, label_dy = 0.01, 0.18, 0.115
    for ybin in range(ybins):
        # hist.GetYaxis().SetBinLabel(ybin+1, labels[BCIDS[ybin]])
        # hist.GetYaxis().SetBinLabel(ybin+1, " ")
        binlabels.append(ROOT.TLatex(label_x, label_y + ybin * label_dy, labels[BCIDS[ybin]]))

    # latex
    since = since.strftime("%Y/%m/%d %H:%M")
    until = until.strftime("%Y/%m/%d %H:%M")
    sect = ROOT.TLatex(0.16, 0.95, f"{sector}")
    meta = ROOT.TLatex(0.30, 0.95, f"{since} #minus {until}")
    for thing in [sect, meta] + binlabels:
        thing.SetTextFont(42)
        thing.SetNDC()
        thing.SetTextColor(ROOT.kBlack)
        thing.SetTextSize(0.05 if thing == sect else 0.043 if thing in binlabels else 0.035)

    # draw
    rootlogon()
    canv = ROOT.TCanvas(f"{sector}", f"{sector}", 800, 800)
    canv.Draw()
    hist.Draw("colzsame")
    sect.Draw()
    meta.Draw()
    for label in binlabels:
        label.Draw()
    ROOT.gPad.RedrawAxis()
    canv.Print(fname, "pdf")

    # also: TH1s
    rootlogon("1D")
    hists = [hist.ProjectionX(f"{sector}_{it:02}_prf", it+1, it+1) for (it, bcid) in enumerate(BCIDS)]
    the_max = 1.1 * max([h1.GetMaximum() for h1 in hists])
    for bcid, h1 in zip(BCIDS, hists):
        style(h1)
        h1.SetLineColor(COLORS[bcid])
        h1.SetMinimum(0)
        h1.SetMaximum(the_max)
        h1.GetYaxis().SetTitle(hist.GetZaxis().GetTitle())
    canv = ROOT.TCanvas(f"{sector}_prj", f"{sector}_prj", 800, 800)
    canv.Draw()
    for h1 in hists:
        h1.Draw("histsame")
    sect.Draw()
    meta.Draw()
    ROOT.gPad.RedrawAxis()
    canv.Print(fname, "pdf")

    # also: TProfile
    prof = hist.ProfileX(f"{sector}_pfx", 0, hist.GetNbinsY()+1)
    style(prof)
    prof.SetMinimum(ylo)
    prof.SetMaximum(yhi)
    prof.GetYaxis().SetLabelColor(ROOT.kWhite)
    canv = ROOT.TCanvas(f"{sector}_pfx", f"{sector}_pfx", 800, 800)
    canv.Draw()
    prof.Draw("colzsame")
    sect.Draw()
    meta.Draw()
    for label in binlabels:
        label.Draw()
    ROOT.gPad.RedrawAxis()
    canv.Print(fname, "pdf")

    # also: BCID offset recommendation
    vals = [prof.GetBinContent(xbin) for xbin in range(1, prof.GetNbinsX()) if prof.GetBinEffectiveEntries(xbin) > 0]
    mean = sum(vals)/len(vals)
    shift = int(abs(mean) + 0.5)
    sign = "" if shift == 0 else "-" if mean > 0 else "+"
    print(f"Mean {sector} = {mean:5.2f} -> shift BCID offset by {sign}{shift}")

def rootlogon(opt="2D"):
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetPadTickX(1)
    ROOT.gStyle.SetPadTickY(1)
    ROOT.gStyle.SetPaintTextFormat(".2f")
    ROOT.gStyle.SetTextFont(42)
    ROOT.gStyle.SetFillColor(10)
    # ROOT.gStyle.SetPalette(ROOT.kCherry)
    # ROOT.TColor.InvertPalette()
    ROOT.gStyle.SetPadTopMargin(0.06)
    ROOT.gStyle.SetPadRightMargin(0.04 if opt == "1D" else 0.19)
    ROOT.gStyle.SetPadBottomMargin(0.14)
    ROOT.gStyle.SetPadLeftMargin(0.15)
    colors = [0xffffff, 0xfff056, 0xf08a4b, 0xc81d25, 0x000000]
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
    hist.GetYaxis().SetTitleOffset(1.2 if isinstance(hist, ROOT.TH2) else 1.5)
    hist.GetZaxis().SetTitleOffset(1.4)
    hist.GetZaxis().SetLabelOffset(0.003)
    hist.GetXaxis().SetNdivisions(505)
    # if not isinstance(hist, ROOT.TH2):
    #     hist.GetXaxis().SetNdivisions(505)

def fatal(msg):
    sys.exit(f"Fatal error: {msg}")
    

if __name__ == "__main__":
    main()
