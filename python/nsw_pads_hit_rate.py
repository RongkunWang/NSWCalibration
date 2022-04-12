#!/usr/bin/env tdaq_python
"""
A script for plotting PFEB hits observed in the pad trigger
  as a function of VMM threshold. The data may be acquired
  via L1A readout or SCA status registers.
"""
import argparse
import array
import os
import sys
import time
import ROOT
ROOT.gROOT.SetBatch()

import nsw_trigger_mapping
mappingL = nsw_trigger_mapping.generate_pad_channel_mapping("L")
mappingS = nsw_trigger_mapping.generate_pad_channel_mapping("S")

NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")
EOS = "/eos/atlas/atlascerngroupdisk/det-nsw/P1/trigger/analysis/"
NPFEBS = 24
NLAYERS = 8
THRESHOLD_ADJUSTMENTS = [-30, -20, -10, 0, 10, 20, 30, 40, 50, 60, 70]

def main():

    rootlogon("1D")

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
    print(f"This data appears to be: {'L1A' if isL1A(ops.i) else 'SCA'}")
    rfile = ROOT.TFile(ops.i)
    tname = "decodedData" if isL1A(ops.i) else "nsw"
    ttree = rfile.Get(tname)

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
    parser.add_argument("-i", help="Input ROOT file",        default="")
    parser.add_argument("-o", help="Output ROOT file",       default="")
    parser.add_argument("-s", help="Sector under test",      default="")
    parser.add_argument("-l", help="Lab name, e.g. 191",     default="")
    parser.add_argument("-n", help="Nominal threshold [mV]", default="50")
    parser.add_argument("-r", help="Use root2html",          action="store_true")
    return parser.parse_args()

def isL1A(fname):
    return os.path.basename(fname).startswith("data")

def output():
    ops = options()
    if not ops.o:
        (lab, sector, run) = labSectorRun()
        daq = "L1A" if isL1A(ops.i) else "SCA"
        dname = os.path.join(EOS, f"{run}")
        bname = f"sTGCPadsHitRate.{daq}.{NOW}.{lab}.{sector}.{run}.canv.root"
        if not os.path.isdir(dname):
            os.makedirs(dname)
        return os.path.join(dname, bname)
    else:
        return ops.o

def getKey(pfeb, chan):
    return f"pfeb{pfeb:02}_chan{chan:03}"

def getLayer(pfeb):
    return pfeb % NLAYERS

def getQuad(pfeb):
    if pfeb < 1*NLAYERS:
        return "Q1"
    elif pfeb < 2*NLAYERS:
        return "Q2"
    elif pfeb < 3*NLAYERS:
        return "Q3"
    fatal("You shouldnt be here")

def validPad(val):
    return val not in ["x", "X"]

def isOdd(sector):
    sec = sector.lstrip("A")
    sec = sec.lstrip("C")
    sec = int(sec)
    return sec % 2 == 1

def minChan(listOfChans):
    validChans = list(filter(validPad, listOfChans))
    validChans = [int(chan) for chan in validChans]
    return min(validChans)

def plot(ttree, ofile):

    ops = options()
    ents = ttree.GetEntries()
    (lab, sector, run) = labSectorRun()
    daq = "L1A" if isL1A(ops.i) else "SCA"
    name = f"sTGCPadsHitRate_{daq}_{sector}_{run}"

    #
    # get n(channels) per pfeb
    #
    nchans = None
    if isL1A(ops.i):
        ttree.GetEntry(0)
        nchans = list(ttree.v_pfeb_nchan)[:NPFEBS]
    else:
        mapping = mappingL if isOdd(sector) else mappingS
        nchans = [len(list(filter(validPad, mapping[pfeb]))) for pfeb in range(NPFEBS)]

    #
    # create histograms
    #
    print("Creating histograms...")
    xtitle = "VMM threshold (approx.) [mV]"
    ytitle = "Hit rate [Hz]"
    hist = {}
    thresholds = [int(ops.n) + adj for adj in THRESHOLD_ADJUSTMENTS]
    step_size = int((max(thresholds) - min(thresholds)) / (len(thresholds) - 1))

    # pad rate vs threshold (1D)
    for pfeb in range(NPFEBS):
        for chan in range(nchans[pfeb]):
            key = getKey(pfeb, chan)
            hist[key] = ROOT.TH1D(f"hist_{key}", f";{xtitle};{ytitle}",
                                  len(thresholds),
                                  min(thresholds) - step_size*0.5,
                                  max(thresholds) + step_size*0.5)

    # pad rate vs threshold per sector (2D)
    xtitle = "VMM threshold (approx.) [mV]"
    ytitle = "Pad"
    ztitle = "Hit rate [Hz]"
    hist2d = ROOT.TH2D(f"hist_sector", f";{xtitle};{ytitle};{ztitle}",
                       len(thresholds),
                       min(thresholds) - step_size*0.5,
                       max(thresholds) + step_size*0.5,
                       int(sum(nchans)), -0.5, sum(nchans) - 0.5)

    #
    # analyze TTree to fill histograms
    #
    if isL1A(ops.i):
        makeHistogramsL1a(ops, ttree, thresholds, nchans, hist, hist2d)
    else:
        makeHistogramsSca(ops, ttree, thresholds, nchans, hist, hist2d, mapping)

    #
    # beauty and save (1D)
    #
    ofile.cd()
    counter = 0
    canv = {}
    for key in hist:
        if not "pfeb16" in key:
            continue
        style(hist[key])
        hist[key].SetMaximum(10e6)
        hist[key].SetMinimum(0.1)
        canv[key] = ROOT.TCanvas(f"canv_{daq}_{key}", f"canv_{daq}_{key}", 400, 400)
        canv[key].Draw()
        hist[key].Draw("histesame")
        canv[key].SetLogy()
        # canv[key].Write()
        counter += 1
        if counter > 10:
            break

    #
    # labels for 2D
    #
    xlo = hist2d.GetXaxis().GetBinLowEdge(1)
    xhi = hist2d.GetXaxis().GetBinLowEdge(hist2d.GetNbinsX() + 1)
    labels, lines, blobs = [], [], []
    for pfeb in range(NPFEBS):
        layer = getLayer(pfeb)
        quad  = getQuad(pfeb)
        xpos = xlo - 0.02*(xhi - xlo)
        ypos = sum(nchans[:pfeb]) + nchans[pfeb]/2
        labels.append(ROOT.TLatex(xpos, ypos, f"L{layer}"))
        if layer == 0:
            xpos = xlo - 0.1*(xhi - xlo)
            labels.append(ROOT.TLatex(xpos, ypos, quad))
        xpos1, xpos2 = xlo, xlo + 0.05*(xhi - xlo)
        ypos = sum(nchans[:pfeb]) + nchans[pfeb]
        lines.append(ROOT.TLine(xpos1, ypos, xpos2, ypos))
    for label in labels:
        label.SetTextFont(102)
        label.SetTextAlign(32)
        label.SetTextColor(ROOT.kBlack)
        label.SetTextSize(0.030)
    blobs.append(ROOT.TLatex(0.16, 0.95, f"{sector}"))
    blobs.append(ROOT.TLatex(0.30, 0.95, f"Run {run} ({lab})"))
    blobs.append(ROOT.TLatex(0.72, 0.95, f"{'L1A' if isL1A(ops.i) else 'SCA'}"))
    for blob in blobs:
        blob.SetNDC()
        blob.SetTextSize(0.040)
    
    #
    # beauty and save (2D)
    #
    rootlogon("2D")
    ofile.cd()
    style(hist2d)
    hist2d.GetYaxis().SetLabelSize(0.0)
    hist2d.GetYaxis().SetTickLength(0.0)
    canv2d = ROOT.TCanvas(f"canv_{daq}_{sector}", f"canv_{daq}_{sector}", 800, 800)
    canv2d.Draw()
    hist2d.SetMaximum(1e7)
    hist2d.SetMinimum(0.1)
    hist2d.Draw("colzsame")
    for thing in labels + lines + blobs:
        thing.Draw()
    canv2d.SetLogz()
    canv2d.Write()


def makeHistogramsL1a(ops, ttree, thresholds, nchans, hist, hist2d):
    ents = ttree.GetEntries()

    #
    # get the number of BCs in the readout
    #
    ttree.GetEntry(0)
    nbc = len(list(set(list(ttree.v_bcid_rel))))
    print(f"Found {nbc} BC per readout packet")
    if nbc == 0:
        fatal("0 BC per readout packet. Something is wrong!")

    #
    # count how many events recorded for each iteration
    #   for converting hits into rate
    #
    nevents = []
    iteration = -1
    previous_flag = -1
    for ent in range(ents):
        _ = ttree.GetEntry(ent)
        if isBadEvent(ttree, nbc):
            continue
        flag = int(ttree.sectId)
        if flag != previous_flag:
            iteration += 1
            nevents.append(0)
        previous_flag = flag
        nevents[iteration] += 1
    print(f"Found {iteration} calib iterations")
    print(f"N(events) per calib iteration = {nevents}")
    scale = [40e6 / (nev * nbc) for nev in nevents]
    if len(nevents) != len(THRESHOLD_ADJUSTMENTS):
        fatal("The data suggests there are more calib loops than thresholds!")

    #
    # fill histogram
    #
    print(f"Analyzing {ents} entries...")
    iteration = -1
    previous_flag = -1
    for ent in range(ents):
        _ = ttree.GetEntry(ent)

        #
        # TTree interface
        #
        if isBadEvent(ttree, nbc):
            continue
        flag  = int(ttree.sectId)
        pfebs = list(ttree.v_hit_pfeb)
        chans = list(ttree.v_hit_ch)
        if flag != previous_flag:
            iteration += 1
        previous_flag = flag

        #
        # fill
        #
        for (pfeb, chan) in zip(pfebs, chans):
            xval = thresholds[iteration]
            key = getKey(pfeb, chan)
            hist[key].Fill(xval, scale[iteration])
            hist2d.Fill(xval, chan + sum(nchans[:pfeb]), scale[iteration])


def isBadEvent(ttree, nbc):
    nbctmp = len(list(set(list(ttree.v_bcid_rel))))
    return nbc != nbctmp


def makeHistogramsSca(ops, ttree, thresholds, nchans, hist, hist2d, mapping):
    ents = ttree.GetEntries()

    #
    # fill histogram
    #
    print(f"Analyzing {ents} entries...")
    for ent in range(ents):
        _ = ttree.GetEntry(ent)

        #
        # TTree interface
        #
        rate      = int(ttree.rate)
        threshold = int(ttree.threshold)
        pfeb      = int(ttree.pfeb_addr)
        tds_chan  = int(ttree.tds_chan)
        chan = mapping[pfeb][tds_chan]
        if not validPad(chan):
            continue
        chan = mapping[pfeb][tds_chan] - minChan(mapping[pfeb])

        #
        # fill
        #
        xval = thresholds[threshold]
        key = getKey(pfeb, chan)
        hist[key].Fill(xval, rate)
        hist2d.Fill(xval, chan + sum(nchans[:pfeb]), rate)

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
    print("Will assume this data is from P1")
    return "P1"

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
    colors = [
        0xffffff, # white
        0x66ccff, # blue
        0x33cc33, # green
        0xfff056, # yellow
        0xf08a4b, # orange
        0xc81d25, # red
        0x1a0000, # black
    ]
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
    hist.GetXaxis().SetTitleOffset(1.2 if isinstance(hist, ROOT.TH2) else 1.2)
    hist.GetYaxis().SetTitleOffset(1.5 if isinstance(hist, ROOT.TH2) else 1.5)
    hist.GetZaxis().SetTitleOffset(1.5)
    hist.GetZaxis().SetLabelOffset(0.003)
    if not isinstance(hist, ROOT.TH2):
        hist.GetXaxis().SetNdivisions(505)

def root2html(fname):
    webpath = os.path.dirname(fname)
    webpath = webpath.replace("/eos/atlas/atlascerngroupdisk/det-nsw/P1", "https://nswp1.web.cern.ch")
    print("Converting TCanvas into html with root2html...")
    if not os.path.isfile(fname):
        fatal(f"Bad ROOT file for root2html: {fname}")
    script = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/root2html.py"
    os.system(f"{script} {fname}")
    print("Plots:")
    print(f"  {webpath}/{os.path.basename(fname.rstrip('.canv.root'))}")

if __name__ == "__main__":
    main()
