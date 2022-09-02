"""
nsw_pads_deskew.py: A script for making plots of PFEB/PadTrigger deskewing
                    based on log files from the ATLAS partition.

Run like:
> nsw_pads_deskew.py

PS: I'm sorry about passing a dictionary of ROOT objects between functions.
    As far as I'm aware, its necessary to ensure the ROOT objects are persistent.
    Otherwise they are destroyed at the end of their function.
"""
import argparse
import array
import glob
import os
import sys
import time
import ROOT
ROOT.gROOT.SetBatch()
ROOT.gErrorIgnoreLevel = ROOT.kWarning

NPFEBS = 24
NPHASES = 16
NBCIDS = 16
WHEELS = ["A", "C"]
SECTORS = range(1, 17)
NROWS = 4
NCOLUMNS = 8
NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")

def main():
    # parse CL args
    ops = options()
    if not ops.i:
        fatal("Please provide input directory with -i")
    if not os.path.isdir(ops.i):
        fatal(f"Cannot find input directory {ops.i}")
    if not os.path.isdir(ops.o):
        fatal(f"Cannot find output directory {ops.o}")

    # make plots
    rootlogon()
    plotAllEpochs()

def plotAllEpochs():
    ops = options()
    epochs = getEpochs(ops.i)
    if len(epochs) == 0:
        fatal("No epochs found at {ops.i}")
    for epoch in epochs:
        print(f"Plotting {epoch} ({epochs.index(epoch)+1}/{len(epochs)})")
        plotEpoch(ops.i,
                  epoch,
                  ops.o,
                  "deskew",
                  epoch == epochs[0],
                  epoch == epochs[-1],
                  )

def getEpochs(dirpath):
    return list(reversed(sorted(os.listdir(dirpath))))

def fileIsEmpty(fpath):
    return len(open(fpath).readlines()) == 0

def plotEpoch(topdir, epochTime, outdir, outPrefix, firstEpoch, lastEpoch):
    # open canvas
    canv = ROOT.TCanvas(f"{outPrefix}_{epochTime}_{NOW}",
                        f"{outPrefix}_{epochTime}_{NOW}", 1600, 800)
    ROOT.SetOwnership(canv, False)
    canv.Divide(NCOLUMNS, NROWS)
    canv.Draw()
    log = {}

    # draw sectors
    for (iwheel, wheel) in enumerate(WHEELS):
        for (idx, sector) in enumerate(SECTORS):
            canv.cd(iwheel*len(SECTORS) + idx + 1)
            secname = f"{wheel}{sector:02}"
            globpathSurface = os.path.join(topdir, epochTime, f"*{secname}*")
            globpathEnrico  = os.path.join(topdir, epochTime, f"*E{wheel}-S{sector:02g}*")
            fnames = glob.glob(globpathSurface) + glob.glob(globpathEnrico)
            if len(fnames) == 0:
                continue
            elif len(fnames) > 1:
                fatal("Found too many files at {globpath}")
            fname = fnames[0]
            if fileIsEmpty(fname):
                continue
            plotSector(canv, log, fname, secname, epochTime)

    # write canvas
    suffix = "(" if firstEpoch else ")" if lastEpoch else ""
    if suffix or len(log) > 0:
        canv.Print(f"{outdir}/{outPrefix}_{NOW}.pdf{suffix}", "pdf")
    else:
        print(f"Skipping {epochTime}, no valid sectors")
    if lastEpoch:
        if wheel == WHEELS[0]:
            print("")
            print("Online:")
        print(f" {eos2www(outdir).rstrip('/')}/{outPrefix}_{NOW}.pdf")

def plotSector(canv, log, fname, secname, epochTime):
    data = DeskewData(fname)
    hname = f"hist_{epochTime}_{secname}"
    log[hname] = ROOT.TH2D(hname,
                           f";Delay [4.167ns];PFEB;BCID & 0xF",
                           NPHASES, -0.5, NPHASES-0.5,
                           NPFEBS, -0.5, NPFEBS-0.5)
    ROOT.SetOwnership(log[hname], False)
    for (pfeb, bcids) in enumerate(data.pfebBcids):
        for (phase, bcid) in enumerate(bcids):
            log[hname].Fill(phase, pfeb, bcid)
    texs = []
    for (pfeb, delay) in enumerate(data.targetDelays):
        tex = ROOT.TLatex(delay, pfeb, f"{data.pfebBcids[pfeb][delay]}")
        tex.SetTextSize(0.025)
        tex.SetTextFont(42)
        tex.SetTextAlign(22)
        # texs.append(tex)
    labelSector = ROOT.TLatex(-0.5, NPFEBS + 0.5, secname)
    labelSector.SetTextAlign(12)
    labelSector.SetTextSize(0.042)
    texs.append(labelSector)
    labelEpoch = ROOT.TLatex(NPHASES * 0.3, NPFEBS + 0.5, epochTime)
    labelEpoch.SetTextAlign(22)
    labelEpoch.SetTextSize(0.042)
    texs.append(labelEpoch)
    labelHumanTime = ROOT.TLatex(NPHASES + 0.5, NPFEBS + 0.5,
                                 time.strftime('%Y/%m/%d %H:%M:%S', time.localtime(int(epochTime))))
    labelHumanTime.SetTextAlign(32)
    labelHumanTime.SetTextSize(0.042)
    texs.append(labelHumanTime)
    log["texs", secname] = texs
    log[hname].SetMinimum(-0.5)
    log[hname].SetMaximum(NBCIDS-0.5)
    style(log[hname])
    log[hname].Draw("colzsame")
    for tex in log["texs", secname]:
        tex.Draw()
    canv.Modified()
    canv.Update()

def eos2www(path):
    return path.replace("/eos/atlas/atlascerngroupdisk/det-nsw/P1/",
                        "https://nswp1.web.cern.ch/")

def timeSinceEpoch():
    ops = options()
    if not ops.i:
        return "0"*10
    basedir = os.path.basename(ops.i.strip("/"))
    try:
        _ = int(basedir)
        return basedir
    except:
        pass
    return "0"*10

class DeskewData():
    def __init__(self, fname):
        self.fname = fname
        self.pfebBcids = []
        self.pfebViableBcids = []
        self.viableBcids = []
        self.viableDelays = []
        self.targetBcid = None
        self.targetDelays = []
        self.newBcids = []
        self.parseText(self.fname)

    def parseText(self, fname):
        lines = open(fname).readlines()
        def listify(hexstring):
            hexstring = hexstring.lstrip("0").lstrip("x")
            return [int(bcid, base=16) for bcid in reversed(hexstring)]
        for line in lines:
            if not "skew" in line.lower():
                continue
            line = line.strip()
            for pfeb in range(NPFEBS):
                tag = f"PFEB {pfeb:02g}"
                if tag in line:
                    subline = line.split(tag)[-1]
                    subline = subline.lstrip(": ")
                    (bcids, viableBcids) = subline.split()
                    bcids = listify(bcids)
                    viableBcids = listify(viableBcids.lstrip("(").rstrip(")"))
                    self.pfebBcids.append(bcids)
                    self.pfebViableBcids.append(viableBcids)
            if "Will not deskew".lower() in line.lower():
                self.viableBcids = []
                self.viableDelays = []
                self.targetBcid = None
                self.targetDelays = []
                self.newBcids = []
                break
            if "Viable BCID".lower() in line.lower():
                line = line.replace("with median delay", "")
                spl = line.split()
                viableBcid  = int(spl[-2], base=16)
                viableDelay = int(spl[-1], base=10)
                self.viableBcids.append(viableBcid)
                self.viableDelays.append(viableDelay)
            if "Target BCID".lower() in line.lower():
                self.targetBcid = int(line.split()[-1], base=16)
            if "Target delays".lower() in line.lower():
                self.targetDelays = listify(line.split()[-1])
            if "New BCIDs".lower() in line.lower():
                self.newBcids = listify(line.split()[-1])
                
def rootlogon():
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetPadTickX(1)
    ROOT.gStyle.SetPadTickY(1)
    ROOT.gStyle.SetPaintTextFormat(".2f")
    ROOT.gStyle.SetTextFont(42)
    ROOT.gStyle.SetFillColor(10)
    ROOT.gStyle.SetPadTopMargin(0.06)
    ROOT.gStyle.SetPadRightMargin(0.16)
    ROOT.gStyle.SetPadBottomMargin(0.12)
    ROOT.gStyle.SetPadLeftMargin(0.11)
    ncontours = 16
    colors = [
        0xf7fbff,
        0xdeebf7,
        0xc6dbef,
        0x9ecae1,
        0x6baed6,
        0x4292c6,
        0x2171b5,
        0x084594,
        ]
    red, blue, green, stops = [], [], [], []
    for (ic, color) in enumerate(colors):
        red  .append(((color >> 2*8) & 0xFF) / float(0xFF))
        green.append(((color >> 1*8) & 0xFF) / float(0xFF))
        blue .append(((color >> 0*8) & 0xFF) / float(0xFF))
        stops.append(ic / float(len(colors) - 1))
    stops = array.array("d", stops)
    red   = array.array("d", red)
    green = array.array("d", green)
    blue  = array.array("d", blue)
    ROOT.TColor.CreateGradientColorTable(len(stops), stops, red, green, blue, ncontours)
    ROOT.gStyle.SetNumberContours(ncontours)

def style(hist):
    size = 0.045
    hist.GetXaxis().SetTitleSize(size)
    hist.GetXaxis().SetLabelSize(size)
    hist.GetYaxis().SetTitleSize(size)
    hist.GetYaxis().SetLabelSize(size)
    hist.GetZaxis().SetTitleSize(size)
    hist.GetZaxis().SetLabelSize(size)
    hist.GetXaxis().SetTitleOffset(1.1)
    hist.GetYaxis().SetTitleOffset(1.1)
    hist.GetZaxis().SetTitleOffset(1.2)
    hist.GetZaxis().SetLabelOffset(0.01)

def fatal(msg):
    sys.exit(f"Fatal error: {msg}")

def options():
    default_i = "/eos/atlas/atlascerngroupdisk/det-nsw/P1/trigger/logs/deskewLogs/"
    default_o = "/eos/atlas/atlascerngroupdisk/det-nsw/P1/trigger/analysis/deskew/"
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-i", help="Input directory of txt files", default=default_i)
    parser.add_argument("-o", help="Output filename",              default=default_o)
    return parser.parse_args()

if __name__ == "__main__":
    main()
