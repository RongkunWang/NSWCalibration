import argparse
import numpy as np
#import ROOT
import array


class VmmStatusRegisterAnalyzer:
    @property
    def args(self):
        if not hasattr(self, '_args'):
            parser = argparse.ArgumentParser(description='Analyzeoutput of the VMM Status Registers read out during calibration')
            parser.add_argument('-i', '--input', type=str, required=True, help='Full output log file of the calibration')
            self._args = parser.parse_args()
        return self._args


    @property
    def raw_data(self):
        if not hasattr(self, '_raw_data'):
            self._raw_data = np.loadtxt(self.args.input, dtype=int, delimiter=' ')
        return self._raw_data


    @property
    def data(self):
        if not hasattr(self, '_data'):
            # iteration vmmid fifo coherency decoder misalignment alignment parity
            maxima = self.raw_data.max(axis=0)
            n_iterations = maxima[0]
            n_vmms = maxima[1]
            n_errors = 6
            self._data = np.zeros((n_iterations, n_vmms, n_errors), dtype=bool)
            for row in self.raw_data:
                iteration_id = row[0]
                vmm_id = row[1]
                for error_id, val in enumerate(row[2:]):
                    if val == 1:
                        self._data[iteration_id, vmm_id, error_id] = True
        return self._data


    @property
    def error_labels(self):
        return ['FIFO', 'Coherency', 'Decoder', 'Misalignment', 'Not aligned', 'Parity Counter']


    @staticmethod
    def create_hist_root(name, content):
        n_x = content.shape[0]
        n_y = content.shape[1]
        hist = ROOT.TH2D(name, n_x, -0.5, n_x - 0.5, n_y, -0.5, n_y - 0.5)
        for i, vector in enumerate(content):
            for j, value in enumerate(vector):
                hist.SetBinContent(i+1, j+1, value)

        return hist


    def create_all_hists_root(self, data):
        outfile = ROOT.TFile.Open('plots.root', 'RECREATE')
        for vmm_id in data.shape[1]:
            hist = self.create_hist_root('vmm_' + str(vmm_id), data[:,vmm_id,:])
            hist.GetXaxis().SetTitle('VMM ID')
            for i, error_label in self.error_labels:
                hist.GetYaxis().SetBinLabel(i+1, error_label)
            hist.Write()
        outfile.Close()


def main():
    analyzer = VmmStatusRegisterAnalyzer()
    print(analyzer.data)


if __name__ == '__main__':
    main()