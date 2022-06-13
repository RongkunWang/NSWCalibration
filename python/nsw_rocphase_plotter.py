#!/usr/bin/env python3

from enum import Enum
from collections import OrderedDict as od
import array as arr
import ROOT

CalibrationType = Enum("CalibrationType", ["core40", "core160", "core_combined", "vmm160"])

class Plotter:
    def __init__(self, filename):
        self.filename = filename
        self.data = od()  # 3D dict
        self.timestamp = od()

    def group_data(self, data):
        """Group data per plot

        Args:
            data (dict[str, list[int]]): Data for core clocks
        Returns:
            dict[str, dict[str, list[int]]]: Data grouped for each plot
        """
        plot_max = 8
        board_total = len(data)
        board_names = list(data.keys())
        board_results = list(data.values())
        board_group_names = [board_names[i:i + plot_max] for i in range(0, board_total, plot_max)]
        board_group_results = [board_results[i:i + plot_max] for i in range(0, board_total, plot_max)]
        
        grouped_boards = od()
        for i, group in enumerate(board_group_results):
            board_group_name = str(board_group_names[i][0]) + ' - ' + str(board_group_names[i][-1])
            if board_group_name not in grouped_boards:
                grouped_boards[board_group_name] = od()
            for j, result in enumerate(group):
                grouped_boards[board_group_name][board_group_names[i][j]] = result
        return grouped_boards

    def add_data(self, data, calibration_type, timestamp):
        """Register data for different calibration type

        Args:
            data (dict[str, list[int]]): Data                                      
            calibration_type (CalibrationType): Type of calibration
        """
        self.timestamp[calibration_type] = timestamp
        if (
            calibration_type == CalibrationType.core40
            or calibration_type == CalibrationType.core160
            or calibration_type == CalibrationType.core_combined
        ):
            self.data[calibration_type] = self.group_data(data)
        elif calibration_type == CalibrationType.vmm160:
            self.data[calibration_type] = data
        else:
            raise ValueError("Invalid calibration type {}".format(calibration_type))

    def create_histogram(self, group_name, calib_type, data):            # TODO add raw plots
        """Create one histogram given a dict of lists

        Args:
            data (dict[str, list[int]]): Data for one histogram
        Returns:
            ROOT.TH2D: 2D histogram
        """
        # Spacing, axis labels
        if calib_type == CalibrationType.vmm160:
            xaxis_label = '160 MHz VMM Phase'
            yaxis_label = 'VMM ID'
        else:
            xaxis_label = '40 MHz Core Phase'
            yaxis_label = ''

        iterations = len(list(data.values())[0])
        nbins = 2*len(data) - 1
        res = arr.array('d', [])
        for x in range(nbins):
            res += arr.array('d', [0.0 + x, 0.9 + x])

        hist = ROOT.TH2D('{}'.format(group_name),';{};{}'.format(xaxis_label, yaxis_label), iterations, 0, iterations, nbins, res)
        space = 0
        for i, (name, result) in enumerate(data.items()):
            for j, value in enumerate(result):
                hist.SetBinContent(j+1, i+space, -1)
                hist.SetBinContent(j+1, i+1+space, int(value))  # x(iteration), y(board), z(phase good or bad)
            space += 1
        hist.SetStats(0)
        s=1                               
        for name in data.keys():
            hist.GetYaxis().SetBinLabel(s, str(name))
            s += 2
        return hist

    def create_canvas(self, group_name, hist, calib_type):
        """Create a canvas showing one histogram and labels

        Args:
            hist (ROOT.TH2D): Histogram
            calibration_type (CalibrationType): Type of the calibration (for label)

        Returns:
            ROOT.TCanvas: Canvas with histogram and labels
        """
        # Hist + labels
        c = ROOT.TCanvas('{}'.format(group_name), 'c', 1000, 450)
        hist.Draw('col')

        c.SetBottomMargin(.15)
        if calib_type == CalibrationType.core40 or calib_type == CalibrationType.core160:
            c.SetRightMargin(.01)
            c.SetLeftMargin(.23)                              
        elif calib_type == CalibrationType.core_combined:
            c.SetRightMargin(.05)
            c.SetLeftMargin(.23)
        elif calib_type == CalibrationType.vmm160:
            c.SetRightMargin(.05)
            c.SetLeftMargin(.06)
            title_size = .03
            #x, y = c.GetLeftMargin()+0.45, 1-c.GetTopMargin()+0.03 
            x, y = 1-c.GetRightMargin()-0.3, 1-c.GetTopMargin()+0.03
            # title = ROOT.TLatex(x, y, group_name)
            title = ROOT.TLatex()
            title.SetTextAlign(12)
            title.SetNDC()
            title.SetTextSize(title_size)
            title.SetTextFont(42)
            # title.Draw()
            title.DrawLatexNDC(x, y, group_name)        
        return c

    def set_style(self, calibration_type):
        """Set the style for the plot given the calibration type

        Args:
            calibration_type (CalibrtionType): Type of the calibration
        """
        # gStyle calls
        ROOT.gStyle.SetLabelSize(.05, 'y')
        ROOT.gStyle.SetLabelSize(.04, 'x')
        #ROOT.gStyle.SetTitleSize(.05, 'xy')
        #ROOT.gStyle.SetTitleOffset(.6, 'y')

        if (
            calibration_type == CalibrationType.core40
            or calibration_type == CalibrationType.core160
        ):
            ROOT.gStyle.SetPalette(4, arr.array('i', [0, ROOT.kRed+2, ROOT.kGreen+3, ROOT.kRed+1]))
        if (
            calibration_type == CalibrationType.core_combined  
            or calibration_type == CalibrationType.vmm160
        ):
            ROOT.gStyle.SetPalette(4, arr.array('i', [0, ROOT.kRed+2, ROOT.kGreen+3, ROOT.kSpring-6]))
        return

    def write_best_phases(self, h, data, phase_labels):       
        #phase_labels = []
        for i, (name, result) in enumerate(data.items()):
            best_phase = result.index(2)
            phase_labels.append(ROOT.TLatex(h.GetXaxis().GetBinUpEdge(h.GetXaxis().GetNbins()) * 1.02,
                                            h.GetYaxis().GetBinCenter(2*i+1),
                                            str(best_phase)
                                            )
                                )
            phase_labels[i].SetTextAlign(12)
            phase_labels[i].SetTextSize(0.04)
            phase_labels[i].SetNDC(0)
            phase_labels[i].Draw() 
        return

    def atlas_title(self, c):
        header_text = "#bf{ATLAS NSW Operations} #it{Preliminary}"
        x, y = c.GetLeftMargin(), 1-c.GetTopMargin()+0.02
        align = 11
        assert( 0 <= x <= 1 )
        assert( 0 <= y <= 1 )
        text_size_header = 0.05
        #title_element = ROOT.TLatex(x, y, header_text)
        title_element = ROOT.TLatex()
        title_element.SetTextAlign(align)
        title_element.SetNDC()
        title_element.SetTextSize(text_size_header)
        title_element.SetName("title_element")
        title_element.SetTextFont( 42 )
        #title_element.Draw()
        title_element.DrawLatexNDC(x, y, header_text)

        x, y = 1-c.GetRightMargin(), 1-c.GetTopMargin()+0.02
        align = 31
        #nsw_element = ROOT.TLatex(x, y, '2022')
        nsw_element = ROOT.TLatex()
        nsw_element.SetTextAlign(align)
        nsw_element.SetNDC()
        nsw_element.SetTextSize(text_size_header)
        nsw_element.SetName("nsw_element")
        nsw_element.SetTextFont(42)
        #nsw_element.Draw()
        nsw_element.DrawLatexNDC(x, y, '2022')

    def save(self, raw_data):
        root_file = ROOT.TFile(self.filename, "RECREATE")
        for calib_type, grouped_data in self.data.items():  # grouped data = dict[dict]
            #canvases = []
            if len(grouped_data) == 0:
                continue
            root_file.mkdir(str(calib_type))
            root_file.cd(str(calib_type))
            for i, (name, data) in enumerate(grouped_data.items()):
                hist = self.create_histogram(name, calib_type, data)
                hist.Write()
                self.set_style(calib_type)
                canvas = self.create_canvas(name, hist, calib_type)
                self.atlas_title(canvas)
                phase_labels = []
                if calib_type == CalibrationType.core_combined or calib_type == CalibrationType.vmm160:
                    self.write_best_phases(hist, data, phase_labels)
                #canvas.UseCurrentStyle()
                #canvases.append(canvas)

                pdfname = self.filename.replace('.root', '')
                if len(grouped_data) == 1:
                    canvas.Print('{}_{}_{}.pdf'.format(pdfname, calib_type, self.timestamp[calib_type]))
                else:
                    if i == 0: 
                        canvas.Print('{}_{}_{}.pdf('.format(pdfname, calib_type, self.timestamp[calib_type]))                         
                    elif i == len(grouped_data)-1:
                        canvas.Print('{}_{}_{}.pdf)'.format(pdfname, calib_type, self.timestamp[calib_type]))
                    else:
                        canvas.Print('{}_{}_{}.pdf'.format(pdfname, calib_type, self.timestamp[calib_type]))
        
        ##### plot raw csvs
        for calib_type, data in raw_data.items():
          root_file.mkdir(str(calib_type)+'_RAW')
          root_file.cd(str(calib_type)+'_RAW')
          for board, csv in data.items():
            iterations = len(csv)
            h1 = ROOT.TH2D("{}".format(board), "; iteration; status register (16xVMMs, 4xSROCs)", iterations, 0, iterations, 20, 0, 20)
            for i, iteration in enumerate(csv):
              for j, value in enumerate(iteration):
                h1.SetBinContent(i, j, value)
            h1.SetStats(0)
            h1.Write()
        root_file.Close()
        return

    def __repr__(self):
        for calib_type, grouped_data in self.data.items():
            print( 'calibration: {}\ngrouped_data: {}\n'.format(calib_type, grouped_data))
        return "done"
'''
def main():
    test_dict = od()
    test_dict2 = od()
    test_dict ["b1"] = [1, 1, 1, 0, 2, 0, 0, 1, 1]
    test_dict ["b2"] = [0, 2, 1, 1, 1, 1, 1, 0, 0]
    test_dict ["b3"] = [1, 1, 1, 0, 0, 0, 2, 1, 0]
    test_dict ["b4"] = [0, 2, 1, 1, 1, 1, 1, 1, 0]
    test_dict2 ["b5"] = [1, 1, 1, 1, 2, 1, 1, 1, 1]
    test_dict2 ["b6"] = [0, 0, 0, 2, 0, 0, 0, 0, 0]
    timestamp = '2022'
    timestamp2 = '2023'

    plotter = Plotter("myroot.root")
    plotter.add_data(test_dict, CalibrationType.core40, timestamp)
    print (plotter)
    plotter.add_data(test_dict2, CalibrationType.core_combined, timestamp2)
    print (plotter)
    plotter.save()
'''
if __name__ == "__main__":
    main()
