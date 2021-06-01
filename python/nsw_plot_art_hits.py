#!/usr/bin/env tdaq_python

"""
To setup the environment, try:
setupATLAS && lsetup 'views LCG_98python3 x86_64-centos7-gcc8-opt'
"""
import argparse
import os
import sys
import time
import ROOT
ROOT.gROOT.SetBatch()

NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")
EOS = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/"
NVMM_PER_ART   = 32
NVMM_PER_LAYER = 128
NVMM_PER_FEB   = 8
NFEB_PER_LAYER = 16
NLAYER         = 8

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
    ttree = rfile.Get("nsw")

    #
    # make output
    #
    print(f"Creating output file ({output()})...")
    ofile = ROOT.TFile.Open(output(), "recreate")

    #
    # parse and plot
    #
    addc_art_names = get_names(ttree)
    plot_last(ttree, addc_art_names, ofile)
    if ops.t:
        plot_versus_time(ttree, addc_art_names, ofile)

    # convert to website
    ofile.Close()
    if ops.r:
        root2html(output())

def options():
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-i", help="Input ROOT file",       default="")
    parser.add_argument("-o", help="Output ROOT file",      default="")
    parser.add_argument("-s", help="Sector under test",     default="")
    parser.add_argument("-l", help="Lab name, e.g. 191",    default="")
    parser.add_argument("-t", help="Make plots versus time", action="store_true")
    parser.add_argument("-r", help="Make plots w/root2html", action="store_true")
    return parser.parse_args()

def output():
    ops = options()
    if not ops.o:
        dname = os.path.join(EOS, "art_counters/")
        bname = f"art_counters.{NOW}.canv.root"
        return os.path.join(dname, bname)
    else:
        return ops.o
    
def get_names(ttree):
    #
    # e.g.:
    # ["ADDC_L1P3_HOL", "art1"]
    #
    names = []
    ents = ttree.GetEntries()
    for ent in range(ents):
        _ = ttree.GetEntry(ent)
        addc = str(ttree.addc_address)
        art  = str(ttree.art_name)
        name = [addc, art]
        if not name in names:
            names.append(name)
    print("")
    print(f"Found {len(names)} ADDC/ARTs")
    for name in names:
        print(f"  {'/'.join(name)}")
    print("")
    return names

def plot_last(ttree, names, ofile):

    rootlogon("2D")
    ops = options()

    #
    # get last event
    #
    ents = ttree.GetEntries()
    ttree.GetEntry(ents-1)
    last_event = int(ttree.event)
    print(f"Last event: {last_event}")

    #
    # create histogram
    #
    print("Creating histograms...")
    hist_vmm = ROOT.TH2D("vmm_vs_layer", ";VMM;Layer;",
                         NVMM_PER_LAYER, -0.5, NVMM_PER_LAYER-0.5,
                         NLAYER, -0.5, NLAYER-0.5)
    hist_feb = ROOT.TH2D("feb_vs_layer", ";FEB;Layer;",
                         NFEB_PER_LAYER, -0.5, NFEB_PER_LAYER-0.5,
                         NLAYER, -0.5, NLAYER-0.5)


    #
    # fill histograms
    #
    found = []
    print("Filling histograms...")
    for ent in range(ents):
        _ = ttree.GetEntry(ent)
        event = ttree.event
        if event != last_event:
            continue
        addc  = str(ttree.addc_address)
        art   = str(ttree.art_name)
        hits  = list(ttree.art_hits)
        if [addc, art] in found:
            continue
        found.append( [addc, art] )
        if len(hits) != NVMM_PER_ART:
            fatal(f"Not 32 measurements for {addc}/{art}: {len(hits)} instead")
        for (artchannel, hit) in enumerate(hits):
            layer      = nsw_trigger_mapping.addc2layer(addc)
            vmm_global = nsw_trigger_mapping.artchannel2globalvmm(addc, art, artchannel)
            feb_global = int(vmm_global / 8)
            if hit < 0:
                print(f"Warning: Layer {layer}, VMM {vmm_global} has negative hits: {hit}. Setting to -1.")
                hit = -1
            hist_vmm.Fill(vmm_global, layer, hit)
            hist_feb.Fill(feb_global, layer, hit)

    #
    # create plots
    #
    style(hist_vmm)
    style(hist_feb)
    hist_vmm.SetMinimum(-1)
    hist_feb.SetMinimum(-1)
    hist_vmm.SetContour(50)
    hist_feb.SetContour(50)
    print("Creating plots...")
    ofile.cd()

    #
    # fuck the ROOT z-axis label
    #
    zlab_vmm = ROOT.TLatex(1.01*hist_vmm.GetXaxis().GetBinCenter(hist_vmm.GetNbinsX()),
                           1.08*hist_vmm.GetYaxis().GetBinCenter(hist_vmm.GetNbinsY()),
                           "Hits")
    zlab_feb = ROOT.TLatex(1.01*hist_feb.GetXaxis().GetBinCenter(hist_feb.GetNbinsX()),
                           1.08*hist_feb.GetYaxis().GetBinCenter(hist_feb.GetNbinsY()),
                           "Hits")
    zlab_vmm.SetTextSize(0.04)
    zlab_feb.SetTextSize(0.04)

    #
    # metadata
    #
    (lab, sector, run) = lab_and_sector_and_run()
    metadata = ROOT.TLatex(0.21, 0.95, "%s in %s, Run %s" % (sector, lab, run))
    metadata.SetTextFont(42)
    metadata.SetNDC()
    metadata.SetTextColor(ROOT.kBlack)
    metadata.SetTextSize(0.030)

    #
    # draw once with z-max as decided by ROOT
    #
    name_vmm = f"vmm_vs_layer_zmaxauto_{NOW}"
    canv_vmm = ROOT.TCanvas(name_vmm, name_vmm, 800, 800)
    hist_vmm.Draw("colzsame")
    zlab_vmm.Draw()
    metadata.Draw()
    canv_vmm.Write()

    name_feb = f"feb_vs_layer_zmaxauto_{NOW}"
    canv_feb = ROOT.TCanvas(name_feb, name_feb, 800, 800)
    hist_feb.Draw("colzsame")
    zlab_feb.Draw()
    metadata.Draw()
    canv_feb.Write()

    #
    # draw once with z-max as X*median bin content
    #   and mark overflow bins to avoid confusion
    #
    hist_vmm.SetMaximum(1.05 * median(hist_vmm))
    hist_feb.SetMaximum(1.05 * median(hist_feb))

    name_vmm = f"vmm_vs_layer_zmaxmedian_{NOW}"
    canv_vmm = ROOT.TCanvas(name_vmm, name_vmm, 800, 800)
    texs_vmm = overflow_tex(hist_vmm)
    hist_vmm.Draw("colzsame")
    zlab_vmm.Draw()
    for tex in texs_vmm:
        tex.Draw()
    metadata.Draw()
    canv_vmm.Write()

    name_feb = f"feb_vs_layer_zmaxmedian_{NOW}"
    canv_feb = ROOT.TCanvas(name_feb, name_feb, 800, 800)
    texs_feb = overflow_tex(hist_feb)
    hist_feb.Draw("colzsame")
    zlab_feb.Draw()
    for tex in texs_feb:
        tex.Draw()
    metadata.Draw()
    canv_feb.Write()


def plot_versus_time(ttree, names, ofile):

    rootlogon("1D")
    ops = options()

    #
    # create histograms
    #
    print("Creating histograms...")
    ents  = ttree.GetEntries()
    ttree.GetEntry(ents-1)
    events = int(ttree.event)
    hists = {}
    for (it, name) in enumerate(names):
        print(f"{it:02} / {len(names)} :: {'/'.join(name)}")
        for vmm in range(NVMM_PER_ART):
            name_vmm = [*name, f"VMM{vmm:02}"]
            hname = "_".join(name_vmm)
            hists[hname] = ROOT.TH1D(hname, ";Time [s];Hits", events, -0.5, events-0.5)
            hists[hname].SetMarkerColor(ROOT.kBlue)
            style(hists[hname])

    #
    # ttree loop
    #
    print("Filling histograms...")
    for ent in range(ents):
        if ent % 100 == 0:
            print(f"Entry {ent:04} / {ents:04}")
        _ = ttree.GetEntry(ent)
        event = ttree.event
        addc  = str(ttree.addc_address)
        art   = str(ttree.art_name)
        hits  = list(ttree.art_hits)
        name  = [addc, art]
        if len(hits) != NVMM_PER_ART:
            fatal(f"Not 32 measurements for {addc}/{art}: {len(hits)} instead")
        for vmm in range(NVMM_PER_ART):
            vmm_hits = hits[vmm]
            hname = "_".join([addc, art, f"VMM{vmm:02}"])
            if hname not in hists:
                continue
            # hists[hname].Fill(event, vmm_hits)
            hists[hname].SetBinContent(event, vmm_hits)

    #
    # create plots
    # one directory per ADDC/ART
    #
    print("Creating plots...")
    ofile.cd()
    canvs = {}
    for (it, name) in enumerate(names):
        print(f"{it:02} / {len(names)-1:02} :: {'/'.join(name)}")
        tdir = ofile.mkdir("_".join(name))
        tdir.cd()
        for vmm in range(NVMM_PER_ART):
            name_vmm = [*name, f"VMM{vmm:02}"]
            hname = "_".join(name_vmm)
            if hname not in hists:
                continue
            tex0 = ROOT.TLatex(0.35, 0.95, ", ".join(name_vmm))
            tex0.SetTextSize(0.030)
            tex0.SetNDC()
            canvs[hname] = ROOT.TCanvas(hname, hname, 800, 800)
            # hists[hname].Draw("colzsame")
            hists[hname].Draw("histsame")
            tex0.Draw()
            canvs[hname].Write()

def median(hist):
    if hist.GetNbinsX() == 0:
        fatal("Cannot median histogram with 0 bins: %s" % (hist))
    contents = []
    for binx in range(1, hist.GetNbinsX()+1):
        for biny in range(1, hist.GetNbinsY()+1):
            contents.append(hist.GetBinContent(binx, biny))
    med = sorted(contents)[int(len(contents)/2)]
    print(f"median of {hist.GetName()} is = {med}")
    return med

def overflow_tex(hist):
    texs = []
    for binx in range(1, hist.GetNbinsX()+1):
        for biny in range(1, hist.GetNbinsY()+1):
            if hist.GetBinContent(binx, biny) > hist.GetMaximum() or hist.GetBinContent(binx, biny) < 0:
                binwidthy = hist.GetYaxis().GetBinWidth(biny)
                offset    = (binwidthy / 10) if (binx % 2 == 0) else (-binwidthy / 10)
                texs.append( ROOT.TLatex(hist.GetXaxis().GetBinCenter(binx),
                                         hist.GetYaxis().GetBinCenter(biny) + offset,
                                         # "o",
                                         str(int(hist.GetBinContent(binx, biny))),
                                     ))
                texs[-1].SetTextAlign(22)
                texs[-1].SetTextSize(0.02)
                texs[-1].SetTextAngle(45)
    print(f"Found {len(texs)} bins whose contents exceeds GetMaximum() for {hist.GetName()}")
    return texs

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
        fatal(f"Cannot extract run number from {ops.i}")
    return run

def fatal(msg):
    sys.exit(f"Error: {msg}")

def root2html(fname):
    print("Converting TCanvas into html with root2html...")
    if not os.path.isfile(fname):
        fatal(f"Bad ROOT file for root2html: {fname}")
    script = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/root2html.py"
    os.system(f"{script} {fname}")
    print("Plots:")
    print(f"  https://www.cern.ch/nsw191/trigger/art_counters/{os.path.basename(fname.rstrip('.canv.root'))}")

def rootlogon(opt):
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetPadTickX(1)
    ROOT.gStyle.SetPadTickY(1)
    ROOT.gStyle.SetPaintTextFormat(".2f")
    ROOT.gStyle.SetTextFont(42)
    ROOT.gStyle.SetFillColor(10)
    ROOT.gStyle.SetPalette(ROOT.kBlackBody)
    # ROOT.TColor.InvertPalette()
    ROOT.gStyle.SetPadTopMargin(0.06)
    ROOT.gStyle.SetPadRightMargin(0.06 if opt == "1D" else 0.16)
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
    hist.GetXaxis().SetTitleOffset(1.3)
    hist.GetYaxis().SetTitleOffset(1.2 if isinstance(hist, ROOT.TH2) else 1.6)
    hist.GetZaxis().SetTitleOffset(0.9)
    hist.GetZaxis().SetLabelOffset(0.02)
    if not isinstance(hist, ROOT.TH2):
        hist.GetXaxis().SetNdivisions(515)

if __name__ == "__main__":
    main()
