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

NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")
EOS = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/"
NPFEBS  = 24
NPHASES = 128
BCID_MAX = 16
PAIRS = [ (feb, feb+1) for feb in range(0, NPFEBS, 2) ]
QUADS = ["Q1", "Q2", "Q3"]
Q1s = range(0*8, 1*8)
Q2s = range(1*8, 2*8)
Q3s = range(2*8, 3*8)
EVENS = [pfeb for pfeb in range(NPFEBS) if pfeb % 2 == 0]
ODDS  = [pfeb for pfeb in range(NPFEBS) if pfeb % 2 == 1]

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
    for quad in ["QX"] + QUADS:
        hist[quad] = ROOT.TH2D(f"{name}_{quad}", f";{xtitle};{ytitle};{ztitle}",
                               int(NPHASES/step_size), -0.5, NPHASES-0.5,
                               int(NPHASES/step_size), -0.5, NPHASES-0.5)

    #
    # parse the TTree into a data structure
    #
    datas = Datas(ttree)
    pad_delays = datas.padDelays()
    roc_phases = datas.rocPhases()
    pfebs = datas.pfebs()
    print(f"Found {len(pfebs)} decent PFEBs")

    #
    # fill avg BCID diff histograms
    #
    scaling = 1.0/len(pad_delays)
    for pad_delay in pad_delays:
        print(f"Analyzing pad delay {pad_delay}...")
        datas_paddelay = datas.subset(datas.datas, range(NPFEBS), roc_phases, [pad_delay])
        for roc_phase_even in roc_phases:
            datas_even    = datas.subset(datas_paddelay, EVENS, [roc_phase_even], [pad_delay])
            datas_even_Q1 = datas.subset(datas_even,     Q1s,   [roc_phase_even], [pad_delay])
            datas_even_Q2 = datas.subset(datas_even,     Q2s,   [roc_phase_even], [pad_delay])
            datas_even_Q3 = datas.subset(datas_even,     Q3s,   [roc_phase_even], [pad_delay])
            for roc_phase_odd in roc_phases:
                datas_odd    = datas.subset(datas_paddelay, ODDS, [roc_phase_odd], [pad_delay])
                datas_odd_Q1 = datas.subset(datas_odd,      Q1s,  [roc_phase_odd], [pad_delay])
                datas_odd_Q2 = datas.subset(datas_odd,      Q2s,  [roc_phase_odd], [pad_delay])
                datas_odd_Q3 = datas.subset(datas_odd,      Q3s,  [roc_phase_odd], [pad_delay])
                diff_Q1 = avgDiff([data.bcid for data in datas_even_Q1],
                                  [data.bcid for data in datas_odd_Q1])
                diff_Q2 = avgDiff([data.bcid for data in datas_even_Q2],
                                  [data.bcid for data in datas_odd_Q2])
                diff_Q3 = avgDiff([data.bcid for data in datas_even_Q3],
                                  [data.bcid for data in datas_odd_Q3])
                hist["Q1"].Fill(roc_phase_even, roc_phase_odd, diff_Q1 * scaling)
                hist["Q2"].Fill(roc_phase_even, roc_phase_odd, diff_Q2 * scaling)
                hist["Q3"].Fill(roc_phase_even, roc_phase_odd, diff_Q3 * scaling)
                hist["QX"].Fill(roc_phase_even, roc_phase_odd, statistics.mean([diff_Q1, diff_Q2, diff_Q3]) * scaling)

    #
    # fill BCID diff histograms
    #
    scaling = 1.0 / (len(pfebs) / 2.0)
    print("Making BCID diff histograms...")
    for key in PAIRS:
        (feb_0, feb_1) = key
        print(f"PFEB{feb_0:02} vs PFEB{feb_1:02}")
        datas_0 = datas.subset(datas.datas, [feb_0], roc_phases, pad_delays)
        datas_1 = datas.subset(datas.datas, [feb_1], roc_phases, pad_delays)
        if len(datas_0) == 0 or len(datas_1) == 0:
            print(" -> Skipping because empty")
            continue
        for phase_0 in roc_phases:
            datas_0_phase = datas.subset(datas_0, [feb_0], [phase_0], pad_delays)
            for phase_1 in roc_phases:
                datas_1_phase = datas.subset(datas_1, [feb_1], [phase_1], pad_delays)
                diff = avgDiff([data.bcid for data in datas_0_phase],
                               [data.bcid for data in datas_1_phase])
                hist["all"].Fill(phase_0, phase_1, diff * scaling)
                hist[ key ].Fill(phase_0, phase_1, diff)

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
    for key in ["QX"] + QUADS + ["all"] + PAIRS + ["error"]:

        #
        # metadata
        #
        if key in ["all", "error"]:
            tag = "All PFEB pairs"
        elif key == "QX":
            tag = "All quads"
        elif key in QUADS:
            tag = key
        else:
            tag = f"PFEB {key[0]:02} vs {key[1]:02}"
        metadata = ROOT.TLatex(0.18, 0.95, f"{sector} in {lab}, Run {run}           {tag}")
        metadata.SetTextFont(42)
        metadata.SetNDC()
        metadata.SetTextColor(ROOT.kBlack)
        metadata.SetTextSize(0.030)

        #
        # draw and save
        #
        hist[key].SetMinimum(-0.01)
        hist[key].SetMaximum(1.2)
        style(hist[key])
        canv = ROOT.TCanvas(hist[key].GetName(),
                            hist[key].GetName(),
                            800, 800)
        hist[key].Draw("colzsame")
        metadata.Draw()
        if key == "QX":
            canv.Print(ofile + "(", "pdf")
        elif key == "error":
            canv.Print(ofile + ")", "pdf")
        else:
            canv.Print(ofile,       "pdf")

def rollsOver(vals):
    return any([val < BCID_MAX/4 for val in vals]) and any([val > 3*BCID_MAX/4 for val in vals])

def bcidAvg(vals):
    if not rollsOver(vals):
        return statistics.mean(vals)
    else:
        rotated_vals = [val + BCID_MAX if val < BCID_MAX/2 else val for val in vals]
        avg = statistics.mean(rotated_vals)
        return avg if avg < BCID_MAX else avg - BCID_MAX

def bcidDiff(val_0, val_1):
    # NB: 4 bits of BCID
    # Therefore BCID rolls over 15 -> 0
    simple_diff = abs(val_0 - val_1)
    return min(simple_diff, abs(simple_diff - BCID_MAX))

def avgDiff(bcids0, bcids1):
    return bcidDiff(bcidAvg(bcids0), bcidAvg(bcids1))

class Datas:
    def __init__(self, ttree):
        self.datas = []
        self.connected = []
        for ent in range(ttree.GetEntries()):
            _ = ttree.GetEntry(ent)
            phase      = int(ttree.phase)
            pad_delay  = int(ttree.pad_delay)
            pfeb_index = list(ttree.pfeb_index)
            pfeb_bcid  = list(ttree.pfeb_bcid)
            self.datas.extend([
                Data(index, phase, pad_delay, bcid) for (index, bcid) in zip(pfeb_index, pfeb_bcid)
            ])
        print(f"Found {len(self.datas)} datas")
        for pfeb in self.pfebs():
            bcids = [data.bcid for data in self.datas if data.pfeb == pfeb]
            bcids_uniq = list(set(bcids))
            bcids_frac = [bcids.count(bcid)/len(bcids) for bcid in bcids_uniq]
            if len(bcids_uniq) == 1 or max(bcids_frac) > 0.6:
                print(f"Disconnected pfeb: {pfeb}")
                for data in self.datas:
                    if data.pfeb == pfeb:
                        data.bcid = None
            else:
                self.connected.append(pfeb)
        self.datas = [data for data in self.datas if data.bcid is not None]

    def pfebs(self):
        return list(sorted(list(set([data.pfeb for data in self.datas]))))

    def rocPhases(self):
        return list(sorted(list(set([data.roc_phase for data in self.datas]))))

    def padDelays(self):
        return list(sorted(list(set([data.pad_delay for data in self.datas]))))

    def evenOddAvgDiff(self, pad_delay, roc_phase_even, roc_phase_odd, pfebs):
        bcids_even = [data.bcid for data in self.datas if all([data.bcid is not None,
                                                               data.even,
                                                               data.pad_delay == pad_delay,
                                                               data.roc_phase == roc_phase_even,
                                                               data.pfeb in pfebs])]
        bcids_odd  = [data.bcid for data in self.datas if all([data.bcid is not None,
                                                               data.odd,
                                                               data.pad_delay == pad_delay,
                                                               data.roc_phase == roc_phase_odd,
                                                               data.pfeb in pfebs])]
        # print(f"{bcids_even} -> {bcidAvg(bcids_even)}")
        # print(f"{bcids_odd} -> {bcidAvg(bcids_odd)}")
        return bcidDiff(bcidAvg(bcids_even), bcidAvg(bcids_odd))

    def subset(self, datas, pfebs, roc_phases, pad_delays):
        return [data for data in datas if all([data.bcid is not None,
                                               data.pfeb in pfebs,
                                               data.roc_phase in roc_phases,
                                               data.pad_delay in pad_delays,
                                           ])]

class Data:
    def __init__(self, pfeb, roc_phase, pad_delay, bcid):
        self.pfeb      = pfeb
        self.roc_phase = roc_phase
        self.pad_delay = pad_delay
        self.bcid      = bcid
        self.odd       = self.pfeb % 2 == 1
        self.even      = self.pfeb % 2 == 0
        self.q1        = self.pfeb in Q1s
        self.q2        = self.pfeb in Q2s
        self.q3        = self.pfeb in Q3s

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
            p1sector = f"E{side}-S{sect}"
            if p1sector in fname:
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
    # colors = [0xffffff, 0xfff056, 0xf08a4b, 0xc81d25, 0x000000]
    colors = [0x01d000, 0xfff056, 0xf08a4b, 0xc81d25, 0x000000]
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
