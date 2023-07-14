#!/usr/bin/env tdaq_python
"""
"""
import argparse
import array
import itertools
import os
import statistics
import sys
import time
import ROOT
ROOT.gROOT.SetBatch()
ROOT.gErrorIgnoreLevel = ROOT.kWarning

NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")
EOS = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/"
NPFEBS  = 24
NPHASES = 128
NOFFSETS = 2
BCID_MAX = 16
PAIRS = [ (feb, feb+1) for feb in range(0, NPFEBS, 2) ]
QUADS = ["Q1", "Q2", "Q3"]
Q1s = range(0*8, 1*8)
Q2s = range(1*8, 2*8)
Q3s = range(2*8, 3*8)
EVENS = [pfeb for pfeb in range(NPFEBS) if pfeb % 2 == 0]
ODDS  = [pfeb for pfeb in range(NPFEBS) if pfeb % 2 == 1]

OFFSETS = range(NOFFSETS)
OFFSETPAIRS = []
for OFFSET0 in OFFSETS:
    for OFFSET1 in OFFSETS:
        if any([OFFSET0 - OFFSET1 == OF0 - OF1 for (OF0, OF1) in OFFSETPAIRS]):
            continue
        OFFSETPAIRS.append( (OFFSET0, OFFSET1) )

QUADSIDEPFEBS = {"Q1E": [pfeb for pfeb in range(NPFEBS) if pfeb in Q1s and pfeb in EVENS],
                 "Q1O": [pfeb for pfeb in range(NPFEBS) if pfeb in Q1s and pfeb in ODDS],
                 "Q2E": [pfeb for pfeb in range(NPFEBS) if pfeb in Q2s and pfeb in EVENS],
                 "Q2O": [pfeb for pfeb in range(NPFEBS) if pfeb in Q2s and pfeb in ODDS],
                 "Q3E": [pfeb for pfeb in range(NPFEBS) if pfeb in Q3s and pfeb in EVENS],
                 "Q3O": [pfeb for pfeb in range(NPFEBS) if pfeb in Q3s and pfeb in ODDS],
                 }
QUADSIDES = list(QUADSIDEPFEBS.keys())
print(QUADSIDES)
QUADSIDEPAIRS = []
for QS0 in QUADSIDES:
    for QS1 in QUADSIDES:
        if QUADSIDES.index(QS1) <= QUADSIDES.index(QS0):
            continue
        QUADSIDEPAIRS.append( (QS0, QS1) )


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
    parser.add_argument("-o", help="Output pdf",         default="")
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
    hist = {}
    for (quadside0, quadside1) in QUADSIDEPAIRS:
        for (offset0, offset1) in OFFSETPAIRS:
            xtitle = f"Phase ({readableQuadside(quadside0)}, TDS offset = {offset0})"
            ytitle = f"Phase ({readableQuadside(quadside1)}, TDS offset = {offset1})"
            ztitle = "< BCID difference >"
            hist[quadside0, quadside1, offset0, offset1] = ROOT.TH2D(f"{name}_{quadside0}_{quadside1}_{offset0}_{offset1}",
                                                                     f";{xtitle};{ytitle};{ztitle}",
                                                                     int(NPHASES/step_size), 0 - step_size/2, NPHASES - step_size/2,
                                                                     int(NPHASES/step_size), 0 - step_size/2, NPHASES - step_size/2)

    #
    # parse the TTree into a data structure
    #
    datas = Datas(ttree)
    pad_delays  = datas.padDelays()
    roc_phases  = datas.rocPhases()
    tds_offsets = datas.tdsOffsets()
    pfebs = datas.pfebs()
    print(f"Found {len(pfebs)} decent PFEBs")

    #
    # fill BCID diff histograms per quad/side
    #
    scaling = 1.0/len(pad_delays)
    for (quadside0, quadside1) in QUADSIDEPAIRS:
        for (offset0, offset1) in OFFSETPAIRS:
            print(f"Analyzing {quadside0} (offset {offset0}) vs {quadside1} (offset {offset1}) ...")
            sys.stdout.flush()
            pfebs0 = QUADSIDEPFEBS[quadside0]
            pfebs1 = QUADSIDEPFEBS[quadside1]
            datas0_qs = datas.subset(datas.datas, pfebs0, roc_phases, pad_delays, [offset0])
            datas1_qs = datas.subset(datas.datas, pfebs1, roc_phases, pad_delays, [offset1])
            for pad_delay in pad_delays:
                datas0_paddelay = datas.subset(datas0_qs, pfebs0, roc_phases, [pad_delay], [offset0])
                datas1_paddelay = datas.subset(datas1_qs, pfebs1, roc_phases, [pad_delay], [offset1])
                for roc_phase_0 in roc_phases:
                    datas0 = datas.subset(datas0_paddelay, pfebs0, [roc_phase_0], [pad_delay], [offset0])
                    for roc_phase_1 in roc_phases:
                        datas1 = datas.subset(datas1_paddelay, pfebs1, [roc_phase_1], [pad_delay], [offset1])
                        diff = avgDiff([data.bcid for data in datas0],
                                       [data.bcid for data in datas1])
                        hist[quadside0, quadside1, offset0, offset1].Fill(roc_phase_0, roc_phase_1, diff * scaling)

    #
    # converting histograms to dictionaries
    #   for a more native python vibe
    #
    hist2dict = {}
    for (qs0, qs1) in QUADSIDEPAIRS:
        for (of0, of1) in OFFSETPAIRS:
            key = (qs0, qs1, of0, of1)
            for xbin in range(1, hist[key].GetNbinsX()+1):
                for ybin in range(1, hist[key].GetNbinsY()+1):
                    content = hist[key].GetBinContent(xbin, ybin)
                    xphase = hist[key].GetXaxis().GetBinCenter(xbin)
                    yphase = hist[key].GetYaxis().GetBinCenter(ybin)
                    hist2dict[qs0, qs1, of0, of1, xphase, yphase] = content

    #
    # analysis of BCID diff histograms per quad/side
    #
    possibilities = []
    not_good = 0.05
    print(f"Threshold for not_good: {not_good}")
    for (ofQ1E, phQ1E) in itertools.product(tds_offsets, roc_phases):
        print(f"ofQ1E, phQ1E = {ofQ1E}, {phQ1E}")
        sys.stdout.flush()
        for (ofQ1O, phQ1O) in itertools.product(tds_offsets, roc_phases):
            if (ofQ1E, ofQ1O) not in OFFSETPAIRS: continue
            diff_Q1E_Q1O = hist2dict["Q1E", "Q1O", ofQ1E, ofQ1O, phQ1E, phQ1O]
            if diff_Q1E_Q1O > not_good: continue
            for (ofQ2E, phQ2E) in itertools.product(tds_offsets, roc_phases):
                if (ofQ1E, ofQ2E) not in OFFSETPAIRS: continue
                if (ofQ1O, ofQ2E) not in OFFSETPAIRS: continue
                diff_Q1E_Q2E = hist2dict["Q1E", "Q2E", ofQ1E, ofQ2E, phQ1E, phQ2E]
                diff_Q1O_Q2E = hist2dict["Q1O", "Q2E", ofQ1O, ofQ2E, phQ1O, phQ2E]
                if diff_Q1E_Q2E > not_good: continue
                if diff_Q1O_Q2E > not_good: continue
                for (ofQ2O, phQ2O) in itertools.product(tds_offsets, roc_phases):
                    if (ofQ1E, ofQ2O) not in OFFSETPAIRS: continue
                    if (ofQ1O, ofQ2O) not in OFFSETPAIRS: continue
                    if (ofQ2E, ofQ2O) not in OFFSETPAIRS: continue
                    diff_Q1E_Q2O = hist2dict["Q1E", "Q2O", ofQ1E, ofQ2O, phQ1E, phQ2O]
                    diff_Q1O_Q2O = hist2dict["Q1O", "Q2O", ofQ1O, ofQ2O, phQ1O, phQ2O]
                    diff_Q2E_Q2O = hist2dict["Q2E", "Q2O", ofQ2E, ofQ2O, phQ2E, phQ2O]
                    if diff_Q1E_Q2O > not_good: continue
                    if diff_Q1O_Q2O > not_good: continue
                    if diff_Q2E_Q2O > not_good: continue
                    for (ofQ3E, phQ3E) in itertools.product(tds_offsets, roc_phases):
                        if (ofQ1E, ofQ3E) not in OFFSETPAIRS: continue
                        if (ofQ1O, ofQ3E) not in OFFSETPAIRS: continue
                        if (ofQ2E, ofQ3E) not in OFFSETPAIRS: continue
                        if (ofQ2O, ofQ3E) not in OFFSETPAIRS: continue
                        diff_Q1E_Q3E = hist2dict["Q1E", "Q3E", ofQ1E, ofQ3E, phQ1E, phQ3E]
                        diff_Q1O_Q3E = hist2dict["Q1O", "Q3E", ofQ1O, ofQ3E, phQ1O, phQ3E]
                        diff_Q2E_Q3E = hist2dict["Q2E", "Q3E", ofQ2E, ofQ3E, phQ2E, phQ3E]
                        diff_Q2O_Q3E = hist2dict["Q2O", "Q3E", ofQ2O, ofQ3E, phQ2O, phQ3E]
                        if diff_Q1E_Q3E > not_good: continue
                        if diff_Q1O_Q3E > not_good: continue
                        if diff_Q2E_Q3E > not_good: continue
                        if diff_Q2O_Q3E > not_good: continue
                        for (ofQ3O, phQ3O) in itertools.product(tds_offsets, roc_phases):
                            if (ofQ1E, ofQ3O) not in OFFSETPAIRS: continue
                            if (ofQ1O, ofQ3O) not in OFFSETPAIRS: continue
                            if (ofQ2E, ofQ3O) not in OFFSETPAIRS: continue
                            if (ofQ2O, ofQ3O) not in OFFSETPAIRS: continue
                            if (ofQ3E, ofQ3O) not in OFFSETPAIRS: continue
                            diff_Q1E_Q3O = hist2dict["Q1E", "Q3O", ofQ1E, ofQ3O, phQ1E, phQ3O]
                            diff_Q1O_Q3O = hist2dict["Q1O", "Q3O", ofQ1O, ofQ3O, phQ1O, phQ3O]
                            diff_Q2E_Q3O = hist2dict["Q2E", "Q3O", ofQ2E, ofQ3O, phQ2E, phQ3O]
                            diff_Q2O_Q3O = hist2dict["Q2O", "Q3O", ofQ2O, ofQ3O, phQ2O, phQ3O]
                            diff_Q3E_Q3O = hist2dict["Q3E", "Q3O", ofQ3E, ofQ3O, phQ3E, phQ3O]
                            if diff_Q1E_Q3O > not_good: continue
                            if diff_Q1O_Q3O > not_good: continue
                            if diff_Q2E_Q3O > not_good: continue
                            if diff_Q2O_Q3O > not_good: continue
                            if diff_Q3E_Q3O > not_good: continue
                            possibilities.append(SetOfPhases([ofQ1E, ofQ1O, ofQ2E, ofQ2O, ofQ3E, ofQ3O],
                                                             [phQ1E, phQ1O, phQ2E, phQ2O, phQ3E, phQ3O],
                                                             [diff_Q1E_Q1O,
                                                              diff_Q1E_Q2E, diff_Q1O_Q2E,
                                                              diff_Q1E_Q2O, diff_Q1O_Q2O, diff_Q2E_Q2O,
                                                              diff_Q1E_Q3E, diff_Q1O_Q3E, diff_Q2E_Q3E, diff_Q2O_Q3E,
                                                              diff_Q1E_Q3O, diff_Q1O_Q3O, diff_Q2E_Q3O, diff_Q2O_Q3O, diff_Q3E_Q3O]))


    print(f"Found {len(possibilities)} possibilities. Sorting...")
    possibilities = sorted(possibilities, key=lambda poss: (poss.maxdiff, poss.meandiff))
    print(f"Top choices:")
    for it, poss in enumerate(possibilities):
        if poss.maxdiff > 0 and it > 50:
            break
        ofQ1E, ofQ1O, ofQ2E, ofQ2O, ofQ3E, ofQ3O = poss.offsets
        phQ1E, phQ1O, phQ2E, phQ2O, phQ3E, phQ3O = poss.phases
        print_offsets = f"Offsets Q1E: {ofQ1E}, Q1O: {ofQ1O}, Q2E: {ofQ2E}, Q2O: {ofQ2O}, Q3E: {ofQ3E}, Q3O: {ofQ3O}"
        print_phases  = f"Phases Q1E: {phQ1E}, Q1O: {phQ1O}, Q2E: {phQ2E}, Q2O: {phQ2O}, Q3E: {phQ3E}, Q3O: {phQ3O}"
        print(f"{print_offsets} and {print_phases} -> max {poss.maxdiff} mean {poss.meandiff:.5f}")

    #
    # beauty and save
    #
    keys = []
    for (qs0, qs1) in QUADSIDEPAIRS:
        for (of0, of1) in OFFSETPAIRS:
            keys.append( (qs0, qs1, of0, of1) )

    for key in keys:
        #
        # title
        #
        titles = [ROOT.TLatex(0.01, 0.975, f"ROC/TDS 40MHz"),
                  ROOT.TLatex(0.02, 0.955, f"phase calibration"),
                  ]
        for title in titles:
            title.SetTextFont(42)
            title.SetNDC()
            title.SetTextColor(ROOT.kBlack)
            title.SetTextSize(0.022)

        #
        # runparams
        #
        runparams = ROOT.TLatex(0.21, 0.95, f"{sector} in {lab}, Run {run}")
        runparams.SetTextFont(42)
        runparams.SetNDC()
        runparams.SetTextColor(ROOT.kBlack)
        runparams.SetTextSize(0.038)

        #
        # quadinfo
        #
        qs0, qs1, of0, of1 = key
        tag = f"{readableQuadside(qs0)} ({of0}) vs {readableQuadside(qs1)} ({of1})"
        quadinfo = ROOT.TLatex(0.60, 0.95, tag)
        quadinfo.SetTextFont(42)
        quadinfo.SetNDC()
        quadinfo.SetTextColor(ROOT.kBlack)
        quadinfo.SetTextSize(0.022)

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
        runparams.Draw()
        quadinfo.Draw()
        for title in titles:
            title.Draw()
        suffix = "(" if key == keys[0] else ")" if key == keys[-1] else ""
        canv.Print(ofile + suffix, "pdf")

def readableQuadside(qs):
    if not len(qs) == 3:
        fatal(f"Idk how to interpret {qs}")
    quad = qs[:2]
    side = qs[2]
    if side not in ["E", "O"]:
        fatal(f"Idk how to interpret {qs} -> {side}")
    readable = "even" if side == "E" else "odd"
    return f"{quad} {readable}"

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

class SetOfPhases:
    def __init__(self, offsets, phases, diffs):
        # if len(phases) != len(QUADSIDES):
        #     fatal(f"Got wrong number of phases")
        self.offsets  = offsets
        self.phases   = phases
        self.diffs    = diffs
        self.maxdiff  = max(diffs)
        self.meandiff = statistics.mean(diffs)

class Datas:
    def __init__(self, ttree):
        self.datas = []
        self.connected = []
        for ent in range(ttree.GetEntries()):
            _ = ttree.GetEntry(ent)
            phase      = int(ttree.phase)
            pad_delay  = int(ttree.pad_delay)
            tds_offset = int(ttree.bcid_offset)
            pfeb_index = list(ttree.pfeb_index)
            pfeb_bcid  = list(ttree.pfeb_bcid)
            # if tds_offset != 0:
            #     continue
            self.datas.extend([
                Data(index, phase, pad_delay, tds_offset, bcid) for (index, bcid) in zip(pfeb_index, pfeb_bcid)
            ])
        print(f"Found {len(self.datas)} datas")

        # check for weird data
        for pfeb in self.pfebs():
            bcids = [data.bcid for data in self.datas if data.pfeb == pfeb]
            bcids_uniq = sorted(list(set(bcids)))
            bcids_frac = [bcids.count(bcid)/len(bcids) for bcid in bcids_uniq]
            thr_hi = 0.12
            thr_lo = 0.08
            njumps = [(bcids_frac[it] > thr_hi and bcids_frac[(it+1) % len(bcids_frac)] < thr_lo) or
                      (bcids_frac[it] < thr_lo and bcids_frac[(it+1) % len(bcids_frac)] > thr_hi)
                      for it in range(len(bcids_frac))].count(True)
            if len(bcids_uniq) == 1 or max(bcids_frac) > 0.6 or njumps > 2:
                print(f"Disconnected pfeb: {pfeb}")
                for data in self.datas:
                    if data.pfeb == pfeb:
                        data.bcid = None
            else:
                self.connected.append(pfeb)

        # filter weird data
        self.datas = [data for data in self.datas if data.bcid is not None]

    def pfebs(self):
        return list(sorted(list(set([data.pfeb for data in self.datas]))))

    def rocPhases(self):
        return list(sorted(list(set([data.roc_phase for data in self.datas]))))

    def padDelays(self):
        return list(sorted(list(set([data.pad_delay for data in self.datas]))))

    def tdsOffsets(self):
        return list(sorted(list(set([data.tds_offset for data in self.datas]))))

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
        return bcidDiff(bcidAvg(bcids_even), bcidAvg(bcids_odd))

    def subset(self, datas, pfebs, roc_phases, pad_delays, tds_offsets):
        return [data for data in datas if all([data.bcid is not None,
                                               data.pfeb in pfebs,
                                               data.roc_phase in roc_phases,
                                               data.pad_delay in pad_delays,
                                               data.tds_offset in tds_offsets,
                                           ])]

class Data:
    def __init__(self, pfeb, roc_phase, pad_delay, tds_offset, bcid):
        self.pfeb       = pfeb
        self.roc_phase  = roc_phase
        self.pad_delay  = pad_delay
        self.tds_offset = tds_offset
        self.bcid       = bcid
        self.odd        = self.pfeb % 2 == 1
        self.even       = self.pfeb % 2 == 0
        self.q1         = self.pfeb in Q1s
        self.q2         = self.pfeb in Q2s
        self.q3         = self.pfeb in Q3s

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
