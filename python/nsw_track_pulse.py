"""
Recommendation: python3, and any ROOT version.

Run like:
> setupATLAS && lsetup 'lcgenv -p LCG_96python3 x86_64-centos7-gcc8-opt ROOT'
> python nsw_track_pulse.py -i my_run.root
"""
import argparse
import glob
import collections
import json
import math
import os
import sys
import time
NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")
PY3 = sys.version_info >= (3,)
EOS = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/"

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True
ROOT.gErrorIgnoreLevel = ROOT.kWarning

DEFAULT_PHASE = 4

ROOT.gROOT.LoadMacro("/afs/cern.ch/user/x/xjia/atlasstyle/AtlasStyle.C")
ROOT.gROOT.LoadMacro("/afs/cern.ch/user/x/xjia/atlasstyle/AtlasLabels.C");
ROOT.SetAtlasStyle()
ROOT.gROOT.SetBatch(0)

def getATLASLabels(runNum, pad, x, y, text=None, selkey=None):
    l = ROOT.TLatex(x, y, 'ATLAS')
    l.SetNDC()
    l.SetTextFont(72)
    l.SetTextSize(0.055)
    l.SetTextAlign(11)
    l.SetTextColor(ROOT.kBlack)
    l.Draw()
    delx = 0.05*pad.GetWh()/(pad.GetWw())
    labs = [l]
    if True:
        p = ROOT.TLatex(x+0.12, y, ' Internal')
        p.SetNDC()
        p.SetTextFont(42)
        p.SetTextSize(0.055)
        p.SetTextAlign(11)
        p.SetTextColor(ROOT.kBlack)
        p.Draw()
        labs += [p]
    if True:
        a = ROOT.TLatex(x, y-0.04, 'B191, MM, %s'%(runNum))
        a.SetNDC()
        a.SetTextFont(42)
        a.SetTextSize(0.04)
        a.SetTextAlign(12)
        a.SetTextColor(ROOT.kBlack)
        a.Draw()
        labs += [a]
    return labs


def options():
    connec_data = ""
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-t", help="Input ROOT TTree name", default="decodedData")
    parser.add_argument("-i", help="Input ROOT file")
    return parser.parse_args()

def main():
    announce()
    ops = options()
    loc = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/mmtp/trackpulse/"
    dataman = load_data()
    plot_shape(dataman,loc)
    plot_err(dataman,loc)
    plot_layers(dataman,loc)
    print("")
    print("Done! ^.^")
    print("")

def announce():
    ops = options()
    print("")
    print("User input:")
    print(" Input ROOT file:      %s" % (ops.i))
    print("")

def run_number():
    """
    In:
      /x/y/z/data_test.1594820699._.daq.RAW._lb0000._0001.root
    Out:
      1594820699
    """
    ops = options()
    bname = os.path.basename(ops.i)
    try:
        return bname.split(".")[1]
    except:
        fatal("Cannot extract run number from %s" % (ops.i))

def load_data():
    print("Loading TTree data into memory...")

    ops = options()
    files = glob.glob(ops.i)
    dataman = ROOT.TChain(ops.t)
    for f in files: dataman.Add(f)

    return dataman

def plot_shape(dataman,loc):

    ops = options()
    cham = ROOT.TFile("/afs/cern.ch/user/x/xjia/public/SM_chamber.root")
    sm = cham.Get("SM")

    gap = (2210.0+1350.0-35.35*2)/8192
    h2d = ROOT.TH2F("h2d", "h2d", 50, 35.35, 3524.65, 25, -2000.0, 2000.0)

    ys = ["stripNumberPlaneX0", "stripNumberPlaneX1", "stripNumberPlaneU0", "stripNumberPlaneV0", "stripNumberPlaneU1", "stripNumberPlaneV1", "stripNumberPlaneX2", "stripNumberPlaneX3"]
    for event in dataman:
#        if event.err==0: continue
        xav = []
        uav = []
        vav = []
        xa = -999999.0
        ua = -999999.0
        va = -999999.0
        xfill = -999999.0
        yfill = -999999.0
        yn = -1.0
        for yvar in ys:
            yn = yn+1
            y = "yf=event." + yvar
            exec(y)
            if (yn==0 or yn==1 or yn==6 or yn==7) and yf>0:
                xav.append(float(yf))
            elif (yn==2 or yn==4) and yf>0:
                uav.append(float(yf))
            elif (yn==3 or yn==5) and yf>0:
                vav.append(float(yf))
        if len(xav)!=0: xa = sorted(xav)[len(xav)/2]
        if len(uav)!=0: ua = sorted(uav)[len(uav)/2]
        if len(vav)!=0: va = sorted(vav)[len(vav)/2]
        if xa!=-999999.0: xfill = 35.35+xa*gap
        if (ua!=-999999.0 and va!=-999999.0): yfill = (ua-va)/(2*math.tan(1.5*math.pi/180))*gap
        elif ua!=-999999.0: yfill = (ua-xa)/(math.tan(1.5*math.pi/180))*gap
        elif va!=-999999.0: yfill = -(va-xa)/(math.tan(1.5*math.pi/180))*gap
        h2d.Fill(xfill, yfill)

    sm.SetTitle("Trigger Chamber; x [mm]; y [mm]")
    sm.SetMaximum(2000)
    sm.SetMinimum(-2000)
    can = ROOT.TCanvas()
    sm.Draw("APL")
    h2d.Draw("colz, same")
    texts = getATLASLabels(run_number(),can, 0.20, 0.88)
    for text in texts:
        text.Draw()
    can.SaveAs("%sshape%s.pdf"%(loc,run_number()))

def plot_err(dataman,loc):

    enum = dataman.GetEntries()
    h2d = ROOT.TH2F("h2d", "h2d", 100, 0.0, enum, 15, 0.0, 15.0)

    for event in dataman:
        h2d.Fill(event.level1Id, event.err)

    h2d.SetTitle("level1IdErr; L1ID; err")
    can = ROOT.TCanvas()
    h2d.Draw("colz")
    texts = getATLASLabels(run_number(),can, 0.20, 0.88)
    for text in texts:
        text.Draw()
    can.SaveAs("%serr%s.pdf"%(loc,run_number()))

def plot_layers(dataman,loc):

    enum = dataman.GetEntries()
    ys = ["stripNumberPlaneX0", "stripNumberPlaneX1", "stripNumberPlaneX2", "stripNumberPlaneX3", "stripNumberPlaneU0", "stripNumberPlaneU1", "stripNumberPlaneV0", "stripNumberPlaneV1", "meanStripNumber", "medianStripNumber"]

    for yvar in ys:

        h2d = ROOT.TH2F("h2d", "h2d", 100, 0.0, enum, 100, 0.0, 8700.0)

        for event in dataman:
            y = "yfill=event." + yvar
            exec(y)
            h2d.Fill(event.level1Id, yfill)

        h2d.SetTitle("%s vs. level1Id; level1Id; %s"%(yvar,yvar))
        can = ROOT.TCanvas("c_%s"%(yvar), "c_%s"%(yvar))
        h2d.Draw("colz")
        texts = getATLASLabels(run_number(),can, 0.20, 0.88)
        for text in texts:
            text.Draw()
        can.SaveAs("%slayer%s%s.pdf"%(loc,yvar,run_number()))

if __name__ == "__main__":
    main()
