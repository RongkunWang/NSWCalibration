#!/usr/bin/env tdaq_python
"""
And lo, if death is for us, who can be against us?

Recommendation: python3, and any ROOT version.

Run like:
> setupATLAS && lsetup 'views LCG_98python3 x86_64-centos7-gcc8-opt'
> python nsw_art_input_phase.py -i my_run.root -r -n

NB: partial runs can still be analyzed, but the -n option crashes.
Consider not using this for partial runs!
"""
import argparse
import collections
import json
import os
import sys
import time
NOW = time.strftime("%Y_%m_%d_%Hh%Mm%Ss")
PY3 = sys.version_info >= (3,)
EOS = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/"

try:
    import ROOT
    ROOT.gROOT.SetBatch()
    ROOT.PyConfig.IgnoreCommandLineOptions = True
    ROOT.gErrorIgnoreLevel = ROOT.kWarning
except:
    print(__doc__)
    raise

DEFAULT_PHASE = 4

def options():
    connec_data = "/afs/cern.ch/work/n/nswdaq/public/nswmmartconnectivitytest/data/"
    config_json = "/afs/cern.ch/user/n/nswdaq/public/sw/config-ttc/config-files/config_json/"
    parser = argparse.ArgumentParser(usage=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-d", help="Required string in input", default="MMARTPhase")
    parser.add_argument("-t", help="Input ROOT TTree name",    default="decodedData")
    parser.add_argument("-i", help="Input ROOT file",       default=os.path.join(connec_data, "dummy.root"))
    parser.add_argument("-c", help="Config JSON file",      default=os.path.join(config_json, "BB5/A16/full_small_sector_a16_bb5_internalPulser_ADDC_TP.json"))
    parser.add_argument("-j", help="Pattern JSON file",     default=os.path.join(EOS, "nsw_art_input_phase.json"))
    parser.add_argument("-s", help="Sector name",           default=None)
    parser.add_argument("-l", help="Lab name",              default=None)
    parser.add_argument("-o", help="Output ROOT file",      default=None)
    parser.add_argument("-n", help="Make a new JSON file",     action="store_true")
    parser.add_argument("-f", help="Go fast: skip VMM plots",  action="store_true")
    parser.add_argument("-r", help="Make plots w/root2html",   action="store_true")
    parser.add_argument("--debug", help="Enable debug output", action="store_true")
    parser.add_argument("--rebinx", help="Rebin x-axis of VMM plots", default=1)
    parser.add_argument("--rebiny", help="Rebin y-axis of VMM plots", default=1)
    return parser.parse_args()

def main():
    rootlogon()
    announce()
    ops = options()
    outrfile = ROOT.TFile(output(), "recreate")
    dataman = load_data()
    dataman = measure_efficiency(dataman, outrfile)
    best_phase = plot_efficiency(dataman, outrfile)
    if ops.n:
        make_new_json(best_phase)
    outrfile.Close()
    if ops.r:
        root2html(output())
    print("")
    print("Done! ^.^")
    print("")

def announce():
    ops = options()
    new_json = output().replace(".root", ".json")
    json_log = output().replace(".root", ".txt")
    print("")
    print("User input:")
    print(" Input ROOT file:      %s" % (ops.i))
    print(" Input file substring: %s" % (ops.d))
    print(" Input JSON config:    %s" % (ops.c))
    print(" Input JSON patterns:  %s" % (ops.j))
    print(" Output ROOT file:     %s" % (output()))
    print(" Make new JSON file?   %s" % ("No" if not ops.n else "Yes: " + new_json))
    print(" Make JSON log file?   %s" % ("No" if not ops.n else "Yes: " + json_log))
    print(" root2html website?    %s" % ("No" if not ops.r else "Yes"))
    print("")

def output():
    ops = options()
    if not ops.o:
        dname = os.path.join(EOS, "mmtp/artphase/")
        bname = "art_phase.%s.%s.root" % (run_number(), NOW)
        return os.path.join(dname, bname)
    else:
        return ops.o

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
    if not os.path.isfile(ops.i):
        fatal("Cannot find input file: %s" % (ops.i))
    if not ops.d in os.path.basename(ops.i):
        fatal("Cannot find required substring (%s) in %s" % (ops.d, ops.i))
    rfile   = ROOT.TFile(ops.i)
    ttree   = rfile.Get(ops.t)
    patts   = Patterns(ops.j)
    dataman = DataManager()

    now_ph = -1
    now_ch = -1

    l1id_prev = -1
    l1id_curr = -1

    # loop
    ents  = ttree.GetEntries()
    start = time.time()
    for ent in range(ents):

        _ = ttree.GetEntry(ent)
        if ent % 1000 == 0:
            progress(time.time()-start, ent, ents)
        l1id_prev = l1id_curr
        l1id_curr = ttree.level1Id

        # warnings
        if l1id_curr > 100:
            print("")
            print_me = "Warning: weird. Ent = %s, L1ID = %s, previous L1ID = %s. Should have rolled over at 100. Skipping."
            print(print_me % (ent, l1id_curr, l1id_prev))
            print("")
            continue

        # DELIMITER
        if (l1id_curr % 100 == 0) or (l1id_prev > 90 and l1id_curr < 10):
            if ops.debug:
                print("Rolling over: Ent = %s, L1ID = %s, previous L1ID = %s." % (ent, l1id_curr, l1id_prev))
            patts.next()
            now_ph = patts.phase()
            now_ch = patts.channel()
            for tup in patts.hits():
                dataman.create(tup)

        # tree data
        # ch_obs is the observed channel for that layer, vmm, phase
        # keep a list of ch_obs for each expectation
        layers    = list(ttree.v_artHit_octupletLayer)
        vmmposs   = list(ttree.v_artHit_vmmPosition)
        ch_obss   = list(ttree.v_artHit_ch)
        hits_n  = len(layers)
        for ih in range(hits_n):
            layer    = layers[ih]
            vmmpos   = vmmposs[ih]
            ch_obs   = ch_obss[ih]
            tup = (layer, vmmpos, now_ch, now_ph)
            if dataman.add(tup, ch_obs) == -1:
                print("dataman.add failed on Ent = %s, L1ID = %s, previous L1ID = %s" % (ent, l1id_curr, l1id_prev))

    print("")
    return dataman

def measure_efficiency(dataman, outrfile):

    ops = options()
    print("Analyzing observed hits vs expected hits per VMM...")
    rootlogon("2d")

    # extract expectations
    layer_vmm = []
    phases = []
    chs_exp = []
    for (layer, vmm, ch_exp, ph) in dataman.hits.keys():
        layer_vmm.append( (layer, vmm) )
        chs_exp.append(ch_exp)
        phases.append(ph)
    layer_vmm = sorted(list(set(layer_vmm)))
    phases    = sorted(list(set(phases)))
    chs_exp   = sorted(list(set(chs_exp)))

    # plot: ch_exp vs ch_obs. one plot per VMM.
    xmin, xmax   = -0.5, len(phases)*64-0.5
    ymin, ymax   = -64.5, 63.5
    xbins, ybins = int((xmax-xmin)/int(ops.rebinx)), int((ymax-ymin)/int(ops.rebiny))
    start = time.time()
    for ilv, lv in enumerate(layer_vmm):
        if ilv % 10 == 0:
            progress(time.time()-start, ilv, len(layer_vmm))
        name = "hits_Layer%i_VMM%04i" % (lv)
        hist = ROOT.TH2F(name, ";Expected channel + 64*ART phase;Observed #minus expected channel",
                         xbins, xmin, xmax, ybins, ymin, ymax)
        for tup in dataman.hits.keys():
            (layer, vmm, ch_exp, ph) = tup
            if (layer, vmm) != lv:
                continue
            chs_obs = dataman.hits[tup]
            for ch_obs in chs_obs:
                hist.Fill(ch_exp + ph*64, ch_obs-ch_exp)
            if (layer, vmm, ph) not in dataman.numer:
                dataman.numer[layer, vmm, ph] = 0.0
                dataman.denom[layer, vmm, ph] = 0.0
            for ch_obs in chs_obs:
                dataman.denom[layer, vmm, ph] += 1.0
                if ch_obs == ch_exp:
                    dataman.numer[layer, vmm, ph] += 1.0

        name = name.replace("hits_", "canv_")
        ROOT.gStyle.SetPalette(ROOT.kPastel)
        lines, texts = [], []
        style(hist)
        canv = ROOT.TCanvas(name, name, 800, 800)
        canv.Draw()
        hist.Draw("colzsame")
        for ph in phases:
            if ph==0:
                texts.append(ROOT.TLatex(0, ymax+4, "ART phase"))
                texts[-1].SetTextSize(0.020)
            texts.append(ROOT.TLatex((ph+0.5)*64, ymax+2, str(ph)))
            texts[-1].SetTextAlign(22)
            texts[-1].SetTextSize(0.020)
            if ph==0:
                continue
            lines.append(ROOT.TLine(ph*64-0.5, ymin, ph*64-0.5, ymax))
            lines[-1].SetLineColor(19)
        for text in texts:
            text.Draw()
        for line in lines:
            line.Draw()

        # save
        if ops.f:
            continue
        (layer, vmm) = lv
        outrfile.cd()
        pos = int(int(vmm) / 8)
        str_lay = "Layer_%i" % (layer)
        str_pos = "Position_%02i" % (pos)
        laydir = outrfile.Get(str_lay)
        if not laydir:
            laydir = outrfile.mkdir(str_lay)
        posdir = laydir.Get(str_pos)
        if not posdir:
            posdir = laydir.mkdir(str_pos)
        posdir.cd()
        canv.Write()

    print("")
    dataman.calculate_efficiency()
    return dataman

def plot_efficiency(dataman, outrfile):

    ops = options()

    # extract expectations
    layers, vmms = [], []
    phases = []
    chs_exp = []
    for (layer, vmm, ch_exp, ph) in dataman.hits.keys():
        layers.append(layer)
        vmms.append(vmm)
        chs_exp.append(ch_exp)
        phases.append(ph)
    layers  = sorted(list(set(layers)))
    vmms    = sorted(list(set(vmms)))
    phases  = sorted(list(set(phases)))
    chs_exp = sorted(list(set(chs_exp)))

    # the hist
    title = ";VMM position;"
    nbinsx = len(vmms)
    nbinsy = len(layers) * len(phases)
    hgood = ROOT.TH2F("h_good_phases", title,
                      nbinsx, vmms[0]-0.5, vmms[-1]+0.5,
                      nbinsy, layers[0]*len(phases)+phases[0]-0.5, layers[-1]*len(phases)+phases[-1]+0.5)
    heffi = ROOT.TH2F("h_effi_phases", title,
                      nbinsx, vmms[0]-0.5, vmms[-1]+0.5,
                      nbinsy, layers[0]*len(phases)+phases[0]-0.5, layers[-1]*len(phases)+phases[-1]+0.5)

    # fill it
    for vmm in vmms:
        binx = hgood.GetXaxis().FindBin(vmm)
        if len(vmms) < 16 or vmm % 8 == 0:
            hgood.GetXaxis().SetBinLabel(binx, str(vmm))
            heffi.GetXaxis().SetBinLabel(binx, str(vmm))
        for layer in layers:
            for ph in phases:
                biny = hgood.GetYaxis().FindBin(layer*len(phases) + ph)
                hgood.SetBinContent(binx, biny, 1 if dataman.effic[layer, vmm, ph]==1 else 0)
                heffi.SetBinContent(binx, biny,      dataman.effic[layer, vmm, ph])
                ### labeling
                if ph % 2 == 0:
                    prefix = ""
                    if phases.index(ph) == 0 and layers.index(layer) == 0:
                        prefix = "Phase = "
                    elif phases.index(ph) == int(len(phases)/2)+2:
                        prefix = "Layer    "
                    elif phases.index(ph) == int(len(phases)/2):
                        prefix = str(layer) + "        "
                    hgood.GetYaxis().SetBinLabel(biny, prefix + hex(ph).replace("0x", ""))
                    heffi.GetYaxis().SetBinLabel(biny, prefix + hex(ph).replace("0x", ""))
                else:
                    hgood.GetYaxis().SetBinLabel(biny, "")
                    heffi.GetYaxis().SetBinLabel(biny, "")

    # find the best phase
    best_phase = {}
    best_texts = []
    for layer in layers:
        for vmm in vmms:
            effs = [dataman.effic[layer, vmm, ph] for ph in phases]
            best_phase[layer, vmm] = plateau_center(effs)
            if best_phase[layer, vmm] >= 0:
                best_texts.append(ROOT.TLatex(vmm, layer*len(phases) + phases[best_phase[layer, vmm]], "."))
            postscript = "!!!!" if (best_phase[layer, vmm] == -1 or best_phase[layer, vmm] > 7) else ""
            if ops.debug:
                print("Layer, VMM = %i, %03i: Best phase = %s %s" % (layer, vmm, phases[best_phase[layer, vmm]] if best_phase[layer, vmm] >= 0 else -1, postscript))

    # style
    for hist in [hgood, heffi]:
        style(hist)
        hist.SetMinimum(-0.01)
        hist.SetMaximum(1.01)
        hist.GetXaxis().SetLabelFont(82)
        hist.GetXaxis().SetLabelSize(0.04)
        hist.GetYaxis().SetLabelFont(82)
        hist.GetYaxis().SetLabelSize(0.02)
    heffi.SetMaximum(1.00)
    heffi.GetZaxis().SetTitle("Efficiency")
    heffi.GetZaxis().SetTitleOffset(1.6)

    # draw good
    ROOT.gStyle.SetPalette(ROOT.kGreyScale)
    canv = ROOT.TCanvas("good_phases", "good_phases", 800, 800)
    canv.Draw()
    hgood.Draw("colsame")

    # draw label
    (lab, sector, run) = lab_and_sector_and_run()
    label = ROOT.TLatex(0.21, 0.95, "%s in %s, Run %s" % (sector, lab, run))
    label.SetTextFont(42)
    label.SetNDC()
    label.SetTextColor(ROOT.kBlack)
    label.SetTextSize(0.030)
    label.Draw()

    # save good
    outrfile.cd()
    canv.Write()
    canv.Close()

    # draw effi, no numbers
    ROOT.gStyle.SetPaintTextFormat("0.3f")
    ROOT.gStyle.SetPadRightMargin(0.19)
    ROOT.gStyle.SetPalette(ROOT.kCherry)
    canv = ROOT.TCanvas("effi_phases", "effi_phases", 800, 800)
    canv.Draw()
    heffi.Draw("colzsame")
    for text in best_texts:
        text.SetTextColor(ROOT.kBlue)
        text.SetTextAlign(22)
        text.Draw()
    label.Draw()

    # save effi, no numbers
    outrfile.cd()
    canv.Write()
    canv.Close()

    # draw effi, with numbers
    ROOT.gStyle.SetPaintTextFormat("0.3f")
    ROOT.gStyle.SetPadRightMargin(0.19)
    ROOT.gStyle.SetPalette(ROOT.kCherry)
    canv = ROOT.TCanvas("effi_phases_annotated", "effi_phases_annotated", 800, 800)
    canv.Draw()
    heffi.SetMarkerSize(0.4)
    heffi.Draw("textcolzsame")
    label.Draw()

    # save effi, with numbers
    outrfile.cd()
    canv.Write()
    canv.Close()

    return best_phase

def make_new_json(best_phase):
    #
    # TP L1A data gives relationship between VMM and fiber
    # Config json gives relationship between ART and fiber
    #
    ops = options()
    outlogname = output().replace(".root", ".txt")
    outlog = open(outlogname, "w")

    geoman = GeoManager(ops.c, ops.i, ops.t)
    if ops.debug:
        geoman.dump()

    # new json!
    with open(ops.c) as json_file:
        newconf = json.load(json_file, object_pairs_hook=collections.OrderedDict)
    for addc in newconf:
        if not addc.startswith("ADDC_"):
            continue
        for art in ["art0", "art1"]:
            lvps = geoman.art2layervmmpos["%s_%s" % (addc, art)]
            if len(lvps) != 32:
                fatal("ART with N(VMM) != 32: %s %s, %s" % (addc, art, lvps))

            connectorandvmm2phase = {}
            conn       = -1
            vmmperconn = 8
            for iv, (layer, vmmpos) in enumerate(lvps):
                if iv % vmmperconn == 0:
                    conn += 1
                phase  = best_phase.get((layer, vmmpos), DEFAULT_PHASE)
                febvmm = int(vmmpos2vmm(vmmpos) % 8)
                connectorandvmm2phase[conn, febvmm] = phase
                outlog.write("%s %s: (layer, vmmpos) = phase -> (%i, %02i) = %s\n" % (addc, art, layer, vmmpos, phase))
            regs = convert2regs(connectorandvmm2phase)
            for reg in regs:
                outlog.write("%s %s: reg = phase -> %s = %s\n" % (addc, art, reg, regs[reg]))
                if "art_ps" in newconf[addc][art]:
                    newconf[addc][art]["art_ps"][reg] = regs[reg]
                else:
                    newconf[addc][art]["art_ps"] = {reg: regs[reg]}

    # close log file
    print("Wrote log file with phases: %s" % (outlogname))
    outlog.close()

    # write to file
    outfilename = output().replace(".root", ".json")
    with open(outfilename, 'w') as json_file:
        json.dump(newconf, json_file, indent=4)
    print("Wrote config file with phases: %s" % (outfilename))

def convert2regs(connvmm2phase):
    ret = {}
    nconn = 4
    vmmperconn = 8
    for conn in range(nconn):
        regs = connector2regs(conn)
        for (ir, reg) in enumerate(regs):
            chan1 = ir*2 + 1
            chan0 = ir*2
            ret[reg] = {}
            ret[reg]["phaseSelectChannel%sinput" % (chan1)] = connvmm2phase[conn, chan1]
            ret[reg]["phaseSelectChannel%sinput" % (chan0)] = connvmm2phase[conn, chan0]
    return ret

def connector2regs(conn):
    if conn == 0:
        return ("06", "07", "08", "09")
    elif conn == 1:
        return ("21", "22", "23", "24")
    elif conn == 2:
        return ("36", "37", "38", "39")
    elif conn == 3:
        return ("51", "52", "53", "54")
    else:
        fatal("Unrecognized ART connector: %s" % (conn))

class DataManager:
    # tup: layer, vmmpos, ch_exp, phase
    def __init__(self):
        self.hits  = {}
        self.numer = {}
        self.denom = {}
        self.effic = {}
    def create(self, tup):
        if tup in self.hits:
            self.fatal("\n\n %s already in hits. Bad.\n\n" % (str(tup)))
        self.hits[tup] = []
    def add(self, tup, ch_obs):
        if not tup in self.hits:
            msg = "Warning:"
            msg += " Unexpected (layer, vmmpos, ch_exp, phase)"
            msg += " = %s was encountered." % (str(tup))
            msg += " This can indicate fishy data was acquired."
            msg += " Please check the BoardVsLevel1Id plot."
            print("%s" % (msg))
            return -1
        self.hits[tup].append(ch_obs)
    def fatal(self, msg):
        sys.exit("Fatal error: %s" % (msg))
    def calculate_efficiency(self):
        for key in self.numer:
            self.effic[key] = self.numer[key]/self.denom[key] if self.denom[key] else 0.0

class GeoManager:
    def __init__(self, jname, rname, tname):
        self.art2fiber         = {}
        self.layervmmpos2fiber = {}
        self.art2layervmmpos   = {}
        self.layervmm2artreg   = {}
        self.jname = jname
        self.rname = rname
        self.tname = tname
        self.read_json()
        self.read_root()
        self.map_layervmm_to_art()
    def read_json(self):
        bitkey = "TP_GBTxAlignmentBit"
        print("Reading JSON file to map ART ASIC to TP fiber...")
        with open(self.jname) as json_file:
            conf = json.load(json_file, object_pairs_hook=collections.OrderedDict)
        for addc in conf:
            if not addc.startswith("ADDC_"):
                continue
            bit0 = conf[addc]["art0"][bitkey]
            bit1 = conf[addc]["art1"][bitkey]
            self.art2fiber["%s_%s" % (addc, "art0")] = int(bit0)
            self.art2fiber["%s_%s" % (addc, "art1")] = int(bit1)
    def read_root(self):
        rfile = ROOT.TFile(self.rname)
        ttree = rfile.Get(self.tname)
        ents  = ttree.GetEntries()
        start = time.time()
        print("Reading ROOT file to map VMM position to TP fiber...")
        for ent in range(ents):
            if ent % 5000 == 0:
                progress(time.time()-start, ent, ents)
            _ = ttree.GetEntry(ent)
            layers    = list(ttree.v_artHit_octupletLayer)
            vmmposs   = list(ttree.v_artHit_vmmPosition)
            fibers    = list(ttree.v_artHit_fiber)
            pipelines = list(ttree.v_artHit_pipeline)
            for (layer, vmmpos, fiber, pipe) in zip(layers, vmmposs, fibers, pipelines):
                self.layervmmpos2fiber[layer, vmmpos] = pipe*8 + fiber
        print("")
    def map_layervmm_to_art(self):
        for art in self.art2fiber:
            layervmmposs = []
            fiber = self.art2fiber[art]
            for layervmmpos in self.layervmmpos2fiber:
                if fiber == self.layervmmpos2fiber[layervmmpos]:
                    layervmmposs.append( layervmmpos )
            self.art2layervmmpos[art] = sorted(layervmmposs, reverse=addc_flipped(art))
            # check
            the_layer = -1
            for (layer, vmmpos) in layervmmposs:
                if the_layer == -1:
                    the_layer = layer
                if layer != the_layer:
                    fatal("This ART has VMMPOS on diff layers: %s => %s" % (art, layervmmposs))
    def map_vmm_to_artchannel(self):
        for art in self.art2layervmmpos:
            vmmposs    = self.art2layervmmpos[art]
            iconn      = -1
            vmmperconn = 8
            for (iv, vmmpos) in enumerate(vmmposs):
                if iv % vmmperconn == 0:
                    iconn += 1
                print("%s: Conn = %s, VMMPos = %s" % (art, iconn, vmmpos))
            break
    def dump(self, debug=False):
        if debug:
            for key in self.art2fiber:
                print("%s => %s" % (key, self.art2fiber[key]))
            for key in self.layervmmpos2fiber:
                print("%s => %s" % (key, self.layervmmpos2fiber[key]))
        for key in self.art2layervmmpos:
            print("%s (%s):: " % (key, "flipped" if addc_flipped(key) else "normalx"), eval('end=""' if PY3 else ""))
            for i,layervmm in enumerate(self.art2layervmmpos[key]):
                layer, vmm = layervmm
                if i == 0:
                    print(" Layer %s," % (layer), eval('end=""' if PY3 else ""))
                print(" %03i" % (vmm), eval('end=""' if PY3 else ""))
            print("")

class Patterns:
    def __init__(self, jname):
        if not os.path.isfile(jname):
            fatal("Does not exist: %s" % (jname))
        self.jname = jname
        self.jdict = self.read()
        self.keys  = list(self.jdict.keys())
        self.index = -1
        print("Found %s patterns in %s" % (len(self.jdict), self.jname))
    def fatal(self, msg):
        sys.exit("Fatal error: %s" % (msg))
    def read(self):
        with open(self.jname) as json_file:
            data = json.load(json_file, object_pairs_hook=collections.OrderedDict)
            return data
    def next(self):
        self.index += 1
    def current(self):
        return self.jdict[self.keys[self.index]]
    def phase(self):
        curr = self.current()
        for key in curr:
            if "phase" in key:
                return int(curr[key])
        self.fatal("No phase in current pattern!")
    def channel(self):
        # assumes all VMMs are pulsing the same channel
        curr = self.current()
        for key in curr:
            if "febpatt" in key:
                for feb in curr[key]:
                    for vmm in curr[key][feb]:
                        for ch in curr[key][feb][vmm]:
                            return int(ch)
        self.fatal("No channel in current pattern!")
    def hits(self):
        tups = []
        curr = self.current()
        for key in curr:
            if "febpatt" in key:
                for feb in curr[key]:
                    for vmm in curr[key][feb]:
                        for ch in curr[key][feb][vmm]:
                            layer_  = self.layer(feb)
                            vmmpos_ = self.vmmpos(feb, int(vmm))
                            tups.append((int(layer_), int(vmmpos_), int(ch), int(self.phase())))
        if len(tups) == 0:
            self.fatal("No hits in current pattern!")
        return tups
    def layer(self, feb):
        if "IP" in feb:
            if "L1" in feb: return 0
            if "L2" in feb: return 1
            if "L3" in feb: return 2
            if "L4" in feb: return 3
        if "HO" in feb:
            if "L4" in feb: return 4
            if "L3" in feb: return 5
            if "L2" in feb: return 6
            if "L1" in feb: return 7
    def boardpos(self, feb):
        # MMFE8_L1P2_HOR -> 2
        pcb  = int(feb.split("_")[1][-1])
        lay  = self.layer(feb)
        left = feb.endswith("L")
        bottom_half_of_pcb = (lay % 2 == 0 and left) or (lay % 2 == 1 and not left)
        return 2*(pcb - 1) + (0 if (bottom_half_of_pcb) else 1)
    def flipped(self, feb):
        return self.boardpos(feb) % 2 == 0
    def vmmpos(self, feb, vmm):
        return 8*self.boardpos(feb) + (vmm if not self.flipped(feb) else 7 - vmm)

def plateau_center(li, debug=False):
    best, worst = max(li), min(li)
    nbest = li.count(best)
    if debug:
        print("plateau_center: best  = %s" % (best))
        print("plateau_center: worst = %s" % (worst))
        print("plateau_center: nbest = %s" % (nbest))
    if best == worst:
        return -1
    plateau_start  = -1
    plateau_end    = -1
    plateau_in     = 0
    plateau_best   = [-1, -1]
    plateau_wiggle = 0.99 if nbest > len(li)/2 else 0.95
    for i,obj in enumerate(li):
        if obj == best or obj > plateau_wiggle*best:
            if plateau_in:
                if debug:
                    print("plateau_center: Step %s => if/if" % (i))
                continue
            else:
                if debug:
                    print("plateau_center: Step %s => if/el" % (i))
                plateau_start = i
                plateau_in = 1
        else:
            if plateau_in:
                if debug:
                    print("plateau_center: Step %s => el/if" % (i))
                plateau_end = i-1
                plateau_in = 0
                if (plateau_end - plateau_start) > (max(plateau_best) - min(plateau_best) + 2) or plateau_best == [-1, -1]:
                    if debug:
                         print("plateau_center: New plateau at %s to %s" % (plateau_start, plateau_end))
                    plateau_best = [plateau_start, plateau_end]
            else:
                if debug:
                    print("plateau_center: Step %s => el/el" % (i))
                continue
    if debug:
        print("plateau_center: Found %s to %s" % (plateau_best[0], plateau_best[1]))
    return int( (max(plateau_best) + min(plateau_best)) / 2)

def rootlogon(opt="1d"):
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetPadTickX(1)
    ROOT.gStyle.SetPadTickY(1)
    ROOT.gStyle.SetPaintTextFormat(".2f")
    ROOT.gStyle.SetTextFont(42)
    ROOT.gStyle.SetFillColor(10)
    if opt == "1d":
        ROOT.gStyle.SetPadTopMargin(0.06)
        ROOT.gStyle.SetPadRightMargin(0.04)
        ROOT.gStyle.SetPadBottomMargin(0.12)
        ROOT.gStyle.SetPadLeftMargin(0.13)
    elif opt == "2d":
        ROOT.gStyle.SetPalette(ROOT.kBlackBody);
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

def vmmpos2vmm(vmmpos):
    febpos    = int(vmmpos / 8)
    vmm_mod_8 = int(vmmpos % 8)
    return febpos*8 + vmm_mod_8 if (febpos % 2 == 1) else febpos*8 + (7 - vmm_mod_8)

def addc_flipped(addc):
    # normal : connector 0 closest to beamline
    # flipped: connector 0 furthest from beamline
    if "IPR" in addc and "L1" in addc: return 0
    if "IPL" in addc and "L1" in addc: return 1
    if "IPR" in addc and "L4" in addc: return 1
    if "IPL" in addc and "L4" in addc: return 0
    if "HOR" in addc and "L1" in addc: return 1
    if "HOL" in addc and "L1" in addc: return 0
    if "HOR" in addc and "L4" in addc: return 0
    if "HOL" in addc and "L4" in addc: return 1

def connector2regs(conn):
    if conn == 0:
        return ("06", "07", "08", "09")
    elif conn == 1:
        return ("21", "22", "23", "24")
    elif conn == 2:
        return ("36", "37", "38", "39")
    elif conn == 3:
        return ("51", "52", "53", "54")
    else:
        fatal("Unrecognized ART connector: %s" % (conn))

def lab_and_sector_and_run():
    ops = options()
    lab, sector = "", ""
    if ops.l:
        lab = ops.l
    elif lab_guess():
        lab = lab_guess()
        print("Lab guess: %s" % (lab))
    if ops.s:
        sector = ops.s
    elif sector_guess():
        sector = sector_guess()
        print("Sector guess: %s" % (sector))
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
    print("Cannot guess lab based on %s" % (fname))
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
    print("Cannot guess sector based on file = %s" % (ops.i))
    print("Please use `-s` to specify which sector is under test")
    return ""

def run_guess():
    ops = options()
    fname = os.path.basename(ops.i)
    try:
        run = int(fname.split(".")[1])
    except:
        fatal("Cannot extract run number from %s" % (ops.i))
    return run

def root2html(fname):
    print("Converting TCanvas into html with root2html...")
    if not os.path.isfile(fname):
        fatal("Bad ROOT file for root2html: %s" % (fname))
    script = "/eos/atlas/atlascerngroupdisk/det-nsw/191/trigger/root2html.py"
    os.system("%s %s" % (script, fname))
    print("Plots:")
    print("  https://www.cern.ch/nsw191/trigger/mmtp/artphase/%s" % (os.path.basename(fname.rstrip(".root"))))

def fatal(msg):
    sys.exit("Fatal error: %s" % (msg))

def progress(time_diff, nprocessed, ntotal):
    nprocessed, ntotal = float(nprocessed), float(ntotal)
    rate = (nprocessed+1)/time_diff
    msg = "\r > %6i / %6i | %2i%% | %8.2fHz | %6.1fm elapsed | %6.1fm remaining"
    msg = msg % (nprocessed, ntotal, 100*nprocessed/ntotal, rate, time_diff/60, (ntotal-nprocessed)/(rate*60))
    sys.stdout.write(msg)
    sys.stdout.flush()

if __name__ == "__main__":
    main()
