#!/usr/bin/env tdaq_python
"""
To setup the environment, try:
setupATLAS && lsetup 'views LCG_98python3 x86_64-centos7-gcc8-opt'
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
NPIPELINES     = 4
NFIBERS        = 32
NVMM_PER_FIBER = 32
NVMM_PER_LAYER = 128
NLAYERS        = 8
NCHANNELS_PER_LAYER = 8192
NMMFE8_PER_LAYER    = 16
NCHANNELS_PER_MMFE8 = 512
NPCBS_PER_LAYER     = 8
NCHANNELS_PER_PCB   = 1024
MIN_HIT_RATE = 5e-1
MAX_HIT_RATE = 9e6
import nsw_trigger_mapping

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
    parser.add_argument("-c", help="Max number of cycles",  default="")
    parser.add_argument("-t", help="Make plots versus time", action="store_true")
    parser.add_argument("-r", help="Make plots w/root2html", action="store_true")
    return parser.parse_args()

def output():
    ops = options()
    if not ops.o:
        dname = os.path.join(EOS, "mmtp_channelrates/")
        bname = f"mmtp_channelrates.{NOW}.canv.root"
        return os.path.join(dname, bname)
    else:
        return ops.o
    
def plot(ttree, ofile):

    rootlogon("1D")
    ops = options()
    ents = ttree.GetEntries()

    #
    # get the number of cycles
    #
    _ = ttree.GetEntry(ttree.GetEntries()-1)
    cycle_n = int(ttree.cycle)
    if ops.c:
        cycle_n = min(cycle_n, int(ops.c))
        print(f"User requested {ops.c} cycles. Will do {cycle_n} cycles.")

    #
    # create histograms
    #
    print("Creating histograms...")
    rates_per_layer = {}
    rates_all_layer = {}
    for layer in range(NLAYERS):
        det_layer = layer2detector(layer)
        for cycle in range(cycle_n):
            rates_per_layer[layer, cycle] = ROOT.TH1D(f"rates_per_layer_Layer{layer}_Cycle{cycle:04d}",
                                                      f"Measurement {cycle};Channel on Layer {layer} ({det_layer});Hit rate [Hz]",
                                                      NCHANNELS_PER_LAYER, -0.5, NCHANNELS_PER_LAYER-0.5)
    for cycle in range(cycle_n):
        rates_all_layer[cycle] = ROOT.TH2D(f"rates_per_layer_Cycle{cycle:04d}",
                                           f";Channel (Cycle {cycle});Layer;Hit rate [Hz]",
                                           NCHANNELS_PER_LAYER, -0.5, NCHANNELS_PER_LAYER-0.5,
                                           NLAYERS, -0.5, NLAYERS-0.5,
                                           )

    #
    # fill histograms
    #
    cycle_current = -1
    for ent in range(ents):
        _ = ttree.GetEntry(ent)

        # check the cycle
        cycle = ttree.cycle
        if cycle == cycle_current:
            pass
        elif cycle == cycle_n:
            print(f"Breaking on cycle {cycle_n}")
            break
        elif cycle == cycle_current + 1:
            cycle_current = int(cycle)
        else:
            fatal(f"New cycle doesnt make sense! {cycle} vs {cycle_current}. Possible data corruption.")

        # TTree interface
        layer    = ttree.octupletLayer
        channels = list(ttree.v_chPosition)
        rates    = list(ttree.v_channel_rate)

        # fill
        for (channel, rate) in zip(channels, rates):
            rates_per_layer[layer, cycle].Fill(channel, rate)
            rates_all_layer[cycle].Fill(channel, layer, rate)

    #
    # create plots
    #
    for (layer, cycle) in rates_per_layer:
        style(rates_per_layer[layer,cycle])
    for cycle in rates_all_layer:
        style(rates_all_layer[cycle])
    print("Creating plots...")
    ofile.cd()

    #
    # disable error bars
    #
    for (layer, cycle) in rates_per_layer:
        for xbin in range(rates_per_layer[layer,cycle].GetNbinsX()+1):
            rates_per_layer[layer,cycle].SetBinError(xbin, 0)

    #
    # metadata
    #
    (lab, sector, run) = lab_and_sector_and_run()
    metadata = ROOT.TLatex(0.15, 0.95, "%s in %s, Run %s" % (sector, lab, run))
    metadata.SetTextFont(42)
    metadata.SetNDC()
    metadata.SetTextColor(ROOT.kBlack)
    metadata.SetTextSize(0.030)
    lines = []
    texts = []
    for mmfe8 in range(NMMFE8_PER_LAYER):
        xcoord = mmfe8 * NCHANNELS_PER_MMFE8
        if mmfe8 > 0:
            lines.append( ROOT.TLine(xcoord, MIN_HIT_RATE, xcoord, MAX_HIT_RATE) )
            lines[-1].SetLineColor(ROOT.kGray)
            lines[-1].SetLineWidth(1)
        xcoord = xcoord + NCHANNELS_PER_MMFE8/2
        texts.append(ROOT.TLatex(xcoord, MAX_HIT_RATE * 0.7, str(mmfe8)))
        texts[-1].SetTextFont(42)
        texts[-1].SetTextColor(ROOT.kBlack)
        texts[-1].SetTextSize(0.025)
        texts[-1].SetTextAlign(22)
    for pcb in range(NPCBS_PER_LAYER):
        xcoord = pcb * NCHANNELS_PER_PCB + NCHANNELS_PER_PCB/2
        texts.append(ROOT.TLatex(xcoord, MAX_HIT_RATE * 0.4, f"PCB{pcb+1}"))
        texts[-1].SetTextFont(42)
        texts[-1].SetTextColor(ROOT.kBlack)
        texts[-1].SetTextSize(0.025)
        texts[-1].SetTextAlign(22)

    #
    # create TDirectories
    #
    dirs = {}
    for layer in range(NLAYERS):
        dirs[layer] = ofile.mkdir(f"Layer_{layer}")
    dirs["all"] = ofile.mkdir(f"Layer_all")

    #
    # draw rates
    #
    for (layer, cycle) in rates_per_layer:
        mmfe8_texts = []
        for mmfe8 in range(NMMFE8_PER_LAYER):
            xcoord = mmfe8*NCHANNELS_PER_MMFE8 + NCHANNELS_PER_MMFE8/1.8
            first_part, second_part = cable_name(layer, mmfe8).split("_")
            # mmfe8_texts.append(ROOT.TLatex(xcoord, MAX_HIT_RATE * 0.7, cable_name(layer, mmfe8)))
            mmfe8_texts.append(ROOT.TLatex(xcoord, MAX_HIT_RATE * 0.7, first_part))
            mmfe8_texts.append(ROOT.TLatex(xcoord, MAX_HIT_RATE * 0.4, second_part))
        for text in mmfe8_texts:
            text.SetTextFont(42)
            text.SetTextColor(ROOT.kBlack)
            text.SetTextSize(0.023)
            text.SetTextAlign(22)
        dirs[layer].cd()
        name = f"rates_per_layer_Layer{layer}_Cycle{cycle:04d}_{NOW}"
        canv = ROOT.TCanvas(name, name, 1600, 800)
        dummy = rates_per_layer[layer, cycle].Clone()
        dummy.Reset()
        dummy.SetMinimum(MIN_HIT_RATE)
        dummy.SetMaximum(MAX_HIT_RATE)
        dummy.Draw("same")
        metadata.Draw()
        for line in lines:
            line.Draw()
        # for text in texts:
        #     text.Draw()
        for text in mmfe8_texts:
            text.Draw()
        rates_per_layer[layer, cycle].Draw("same")
        ROOT.gPad.RedrawAxis()
        canv.SetLogy(True)
        canv.Write()

    rootlogon("2D")
    for cycle in rates_all_layer:
        dirs["all"].cd()
        name = f"rates_per_layer_Cycle{cycle:04d}_{NOW}"
        canv = ROOT.TCanvas(name, name, 1600, 800)
        rates_all_layer[cycle].Draw("colzsame")
        canv.SetLogy(False)
        canv.SetLogz(True)
        metadata.Draw()
        canv.Write()


def layer2detector(layer):
    if layer == 0: return "IP L1"
    if layer == 1: return "IP L2"
    if layer == 2: return "IP L3"
    if layer == 3: return "IP L4"
    if layer == 4: return "HO L4"
    if layer == 5: return "HO L3"
    if layer == 6: return "HO L2"
    if layer == 7: return "HO L1"
    fatal(f"Cant convert {layer} in layer2detector")

def layerposition2side(layer, position):
    if layer % 2 == 0:
        if position % 2 == 0:
            return "L"
        else:
            return "R"
    else:
        if position % 2 == 0:
            return "R"
        else:
            return "L"

def cable_name(layer, position):
    pcb = int(position/2 + 1)
    side = layerposition2side(layer, position)
    det, det_layer = layer2detector(layer).split()
    return f"{det_layer}P{pcb}_{det}{side}"

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

def stoplight():
    # green, yellow, orange, red
    ncontours = 20
    stops = array.array("d", [0.00, 0.33, 0.66, 1.00])
    red   = array.array("d", [1.00, 1.00, 1.00, 0.00])
    blue  = array.array("d", [0.00, 0.00, 0.00, 0.00])
    green = array.array("d", [0.00, 0.66, 1.00, 1.00])
    ROOT.TColor.CreateGradientColorTable(len(stops), stops, red, green, blue, ncontours)
    ROOT.gStyle.SetNumberContours(ncontours)

def fatal(msg):
    sys.exit(f"Error: {msg}")

def root2html(fname):
    webpath = "https://www.cern.ch/nsw191/trigger/mmtp_channelrates"
    print("Converting TCanvas into html with root2html...")
    if not os.path.isfile(fname):
        fatal(f"Bad ROOT file for root2html: {fname}")
    script = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/root2html.py"
    os.system(f"{script} {fname}")
    print("Plots:")
    print(f"  {webpath}/{os.path.basename(fname.rstrip('.canv.root'))}")

    # gifs!
    print("Bonus: GIFs! Running...")
    dname = fname.strip('.canv.root')
    (lab, sector, run) = lab_and_sector_and_run()
    for layer in list(range(NLAYERS)) + ["all"]:
        lname = os.path.join(dname, f"mmtp_channelrates_{lab}_{sector}_Run_{run}_Layer_{layer}_{NOW}.gif")
        cmd = f"convert -delay 20 -loop 0 {dname}/Layer_{layer}/*.png {lname}"
        os.system(cmd)
        print(f"Layer {layer} @ {webpath}/{os.path.basename(dname)}/{os.path.basename(lname)}")
    print("Bonus: GIFs! Done ^.^")
    print("To save space, consider running:")
    print(f" rm -f {dname}/Layer_*/*.pdf")
    print(f" rm -f {dname}/Layer_*/*.png")
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
    ROOT.gStyle.SetPadRightMargin(0.04 if opt == "1D" else 0.17)
    ROOT.gStyle.SetPadBottomMargin(0.14)
    ROOT.gStyle.SetPadLeftMargin(0.08)

def style(hist):
    # size = 0.045
    size = 0.055
    hist.SetLineWidth(2)
    hist.GetXaxis().SetTitleSize(size)
    hist.GetXaxis().SetLabelSize(size)
    hist.GetYaxis().SetTitleSize(size)
    hist.GetYaxis().SetLabelSize(size)
    hist.GetZaxis().SetTitleSize(size)
    hist.GetZaxis().SetLabelSize(size)
    hist.GetXaxis().SetTitleOffset(1.2 if isinstance(hist, ROOT.TH2) else 1.1)
    hist.GetYaxis().SetTitleOffset(0.6 if isinstance(hist, ROOT.TH2) else 0.7)
    hist.GetZaxis().SetTitleOffset(0.8)
    hist.GetZaxis().SetLabelOffset(0.003)
    if not isinstance(hist, ROOT.TH2):
        hist.GetXaxis().SetNdivisions(505)
    hist.GetXaxis().SetTickLength(0.0001)

if __name__ == "__main__":
    main()
