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
NPIPELINES     = 4
NFIBERS        = 32
NVMM_PER_FIBER = 32
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
    plot(ttree, ofile)

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
        dname = os.path.join(EOS, "mmtp_diagnostics/")
        bname = f"mmtp_diagnostics.{NOW}.canv.root"
        return os.path.join(dname, bname)
    else:
        return ops.o
    
def plot(ttree, ofile):

    rootlogon("2D")
    ops = options()
    ents = ttree.GetEntries()

    #
    # poll time interval
    #
    _ = ttree.GetEntry(0)
    sleep_time = ttree.sleep_time
    

    #
    # create histograms
    #
    print("Creating histograms...")
    overflow_word_vs_event = ROOT.TH2D("overflow_word_vs_event",
                                       f";Query per {sleep_time} seconds;Overflow bit;Value",
                                       ents, -0.5, ents-0.5, NPIPELINES+2, -1.5, NPIPELINES+0.5)
    align_vs_event = ROOT.TH2D(f"fiber_align_vs_event",
                               f";Query per {sleep_time} seconds;Fiber aligned;Value",
                               ents, -0.5, ents-0.5, NFIBERS+2, -1.5, NFIBERS+0.5)
    masks = {}
    hots  = {}
    for fiber in range(NFIBERS):
        masks[fiber] = ROOT.TH2D(f"fiber_masks_vs_event_{fiber:02d}",
                                 f";Query per {sleep_time} seconds;Fiber masked VMMs;Value",
                                 ents, -0.5, ents-0.5, NVMM_PER_FIBER, -0.5, NVMM_PER_FIBER-0.5)
        hots[fiber]  = ROOT.TH2D(f"fiber_hots_vs_event_{fiber:02d}",
                                 f";Query per {sleep_time} seconds;Fiber hot VMMs;Value",
                                 ents, -0.5, ents-0.5, NVMM_PER_FIBER, -0.5, NVMM_PER_FIBER-0.5)
        
    #
    # fill histograms
    #
    for ent in range(ents):
        _ = ttree.GetEntry(ent)
        overflow_word = ttree.overflow_word
        fiber_index   = list(ttree.fiber_index)
        fiber_align   = list(ttree.fiber_align)
        fiber_masks   = list(ttree.fiber_masks)
        fiber_hots    = list(ttree.fiber_hots)
        for pipeline in range(NPIPELINES):
            if overflow_word & pow(2, pipeline):
                overflow_word_vs_event.Fill(ent, pipeline)
        for fiber in fiber_index:
            align_vs_event.Fill(ent, fiber, 1 if (fiber_align[fiber]) else 0)
            for vmm in range(NVMM_PER_FIBER):
                masks[fiber].Fill(ent, vmm, 1 if (fiber_masks[fiber] & pow(2, vmm)) else 0)
                hots [fiber].Fill(ent, vmm, 1 if (fiber_hots [fiber] & pow(2, vmm)) else 0)


    #
    # create plots
    #
    style(overflow_word_vs_event)
    style(align_vs_event)
    align_vs_event.SetMaximum(align_vs_event.GetMaximum()*1.19)
    for fiber in range(NFIBERS):
        style(masks[fiber])
        style(hots [fiber])
        hots[fiber].SetMaximum(hots[fiber].GetMaximum()*1.19)
    overflow_word_vs_event.SetMaximum(overflow_word_vs_event.GetMaximum()*1.19)
    print("Creating plots...")
    ofile.cd()

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
    # draw alignment
    #
    ROOT.gStyle.SetPalette(ROOT.kBlackBody)
    name = f"fiber_align_vs_event_{NOW}"
    canv = ROOT.TCanvas(name, name, 800, 800)
    ROOT.gStyle.SetPalette(ROOT.kBlackBody)
    align_vs_event.Draw("colzsame")
    metadata.Draw()
    ROOT.gStyle.SetPalette(ROOT.kBlackBody)
    canv.Write()

    #
    # draw overflow
    #
    name_overflow_word_vs_event = f"overflow_word_vs_event_{NOW}"
    canv_overflow_word_vs_event = ROOT.TCanvas(name_overflow_word_vs_event,
                                               name_overflow_word_vs_event, 800, 800)
    ROOT.gStyle.SetPalette(ROOT.kCherry)
    ROOT.TColor.InvertPalette()
    overflow_word_vs_event.Draw("colzsame")
    metadata.Draw()
    canv_overflow_word_vs_event.Write()

    #
    # draw per-fiber plots
    #
    ROOT.gStyle.SetPalette(ROOT.kCherry)
    ROOT.TColor.InvertPalette()

    dir_hots = ofile.mkdir("fiber_hots")
    dir_hots.cd()
    for fiber in range(NFIBERS):
        name = f"fiber_hots_vs_event_{fiber:02d}_{NOW}"
        canv = ROOT.TCanvas(name, name, 800, 800)
        hots[fiber].Draw("colzsame")
        metadata.Draw()
        canv.Write()

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
    print("Converting TCanvas into html with root2html...")
    if not os.path.isfile(fname):
        fatal(f"Bad ROOT file for root2html: {fname}")
    script = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/root2html.py"
    os.system(f"{script} {fname}")
    print("Plots:")
    print(f"  https://www.cern.ch/nsw191/trigger/mmtp_diagnostics/{os.path.basename(fname.rstrip('.canv.root'))}")

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
    ROOT.gStyle.SetPadRightMargin(0.06 if opt == "1D" else 0.17)
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
    hist.GetZaxis().SetTitleOffset(1.3)
    hist.GetZaxis().SetLabelOffset(0.02)
    if not isinstance(hist, ROOT.TH2):
        hist.GetXaxis().SetNdivisions(515)

if __name__ == "__main__":
    main()
