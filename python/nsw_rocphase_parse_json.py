#!/usr/bin/env python3

import json
import re

class JsonParser:
    REGEX_TRAILING_COMMA = re.compile(r",(?=\s*[}\]])")
    REGEX_COMMENT_CPP = re.compile(r"\s*\/\/.*")
    REGEX_COMMENT_PYTHON = re.compile(r"\s*#.*")
    NUM_VMMS = 8
    NUM_SROCS = 4

    def __init__(self, filename):
        self.filename = filename
        self.new_values = {}

    @property
    def content(self):
        with open(self.filename, "r") as f:
            return f.read()

    @property
    def filtered_content(self):
        filtered_content = self.content
        filtered_content = re.sub(self.REGEX_COMMENT_CPP, "", filtered_content)
        filtered_content = re.sub(self.REGEX_COMMENT_PYTHON, "", filtered_content)
        filtered_content = re.sub(self.REGEX_TRAILING_COMMA, "", filtered_content)
        return filtered_content

    @property
    def data(self):
        if not hasattr(self, '_data'):
            self._data = json.loads(self.filtered_content)
        return self._data

    def _find_name(self, opc_node):
        if "common" in opc_node:
            return opc_node
        for name, config in self.data.items():
            if name == "DisabledBoards" or "common" in name:
                continue
            if config["OpcNodeId"].replace("/", "_") == opc_node:
                return name
        raise ValueError("Did not find board" + opc_node)

    def _read_sroc_enable(self, opc_node, id):
        return self.data[self._find_name(opc_node)]["rocCoreDigital"]["reg007sRocEnable"]["enableSROC" + str(id)]

    def _read_vmm_enable(self, opc_node):
        connection_registers = ["reg002sRoc0VmmConnections", "reg003sRoc1VmmConnections", "reg004sRoc2VmmConnections", "reg005sRoc3VmmConnections"]
        enabled_vmms = {}
        for connection_register in connection_registers:
            enabled_vmms[connection_register] = [None] * self.NUM_VMMS
            if "rocCoreDigital" not in self.data[self._find_name(opc_node)] or connection_register not in self.data[self._find_name(opc_node)]["rocCoreDigital"]:
                continue
            base = self.data[self._find_name(opc_node)]["rocCoreDigital"][connection_register]
            for id in range(self.NUM_VMMS):
                vmm = "vmm{}".format(id)
                if vmm in base:
                    enabled_vmms[connection_register][id] = base[vmm]
        return enabled_vmms

    @property
    def default_srocs(self):
        default_block_name = "roc_common_config"
        return [self._read_sroc_enable(default_block_name, id) for id in range(self.NUM_SROCS)]

    @property
    def default_vmms(self):
        default_block_name = "roc_common_config"
        return self._read_vmm_enable(default_block_name)

    def get_enabled_srocs(self, name):
        result = self.default_srocs
        for id in range(self.NUM_SROCS):
            try:
                result[id] = self._read_sroc_enable(name, id)
            except KeyError:
                pass
        return result

    def get_enabled_vmms(self, name):
        defaults = self.default_vmms
        overwrites = self._read_vmm_enable(name)
        result = [0] * 8
        for sroc in defaults:
            for id, (default, overwrite) in enumerate(zip(defaults[sroc], overwrites[sroc])):
                if result[id] == 1:
                    continue
                if overwrite is None:
                    result[id] = default
                else:
                    result[id] = overwrite

        return result

    def update_value_core(self, opc_node, value):
        name = self._find_name(opc_node)
        value_40mhz = value
        value_160mhz = value % 32
        value_160mhz_4 = (value_160mhz >> 4) & 1
        value_160mhz_30 = value_160mhz & 0b1111
        regs_40mhz = [["reg096ePllTdc", "ePllPhase40MHz_0"],
                      ["reg097ePllTdc", "ePllPhase40MHz_1"],
                      ["reg098ePllTdc", "ePllPhase40MHz_2"],
                      ["reg099ePllTdc", "ePllPhase40MHz_3"],
                      ["reg064ePllVmm0", "ePllPhase40MHz_0"],
                      ["reg065ePllVmm0", "ePllPhase40MHz_1"],
                      ["reg066ePllVmm0", "ePllPhase40MHz_2"],
                      ["reg067ePllVmm0", "ePllPhase40MHz_3"],
                      ["reg080ePllVmm1", "ePllPhase40MHz_0"],
                      ["reg081ePllVmm1", "ePllPhase40MHz_1"],
                      ["reg082ePllVmm1", "ePllPhase40MHz_2"],
                      ["reg083ePllVmm1", "ePllPhase40MHz_3"],
                      ["reg115", "ePllPhase40MHz_0"],
                      ["reg116", "ePllPhase40MHz_1"],
                      ["reg117", "ePllPhase40MHz_2"]]
        regs_160mhz_4 = [["reg115", "ePllPhase160MHz_0[4]"],
                         ["reg116", "ePllPhase160MHz_1[4]"],
                         ["reg117", "ePllPhase160MHz_2[4]"]]
        regs_160mhz_30 = [["reg118", "ePllPhase160MHz_0[3:0]"],
                          ["reg118", "ePllPhase160MHz_1[3:0]"],
                          ["reg119", "ePllPhase160MHz_2[3:0]"]]
        
        def update(reg, subreg, value):
            analog = "rocPllCoreAnalog"
            if analog not in self.data[name]:
                self.data[name][analog] = {}
            if reg not in self.data[name][analog]:
                self.data[name][analog][reg] = {}
            self.data[name][analog][reg][subreg] = value

        for reg_40mhz in regs_40mhz:
            update(reg_40mhz[0], reg_40mhz[1], value_40mhz)
        for reg_160mhz in regs_160mhz_4:
            update(reg_160mhz[0], reg_160mhz[1], value_160mhz_4)
        for reg_160mhz in regs_160mhz_30:
            update(reg_160mhz[0], reg_160mhz[1], value_160mhz_30)

    def update_value_vmm(self, opc_node, id, value):
        name = self._find_name(opc_node)
        value_160mhz_4 = (value >> 4) & 1
        value_160mhz_30 = value & 0b1111
        regs_160mhz_4 = {0: ["reg064ePllVmm0", "ePllPhase160MHz_0[4]"],
                         1: ["reg065ePllVmm0", "ePllPhase160MHz_1[4]"],
                         2: ["reg066ePllVmm0", "ePllPhase160MHz_2[4]"],
                         3: ["reg067ePllVmm0", "ePllPhase160MHz_3[4]"],
                         4: ["reg080ePllVmm1", "ePllPhase160MHz_0[4]"],
                         5: ["reg081ePllVmm1", "ePllPhase160MHz_1[4]"],
                         6: ["reg082ePllVmm1", "ePllPhase160MHz_2[4]"],
                         7: ["reg083ePllVmm1", "ePllPhase160MHz_3[4]"]}
        regs_160mhz_30 = {1: ["reg068ePllVmm0", "ePllPhase160MHz_1[3:0]"],
                          0: ["reg068ePllVmm0", "ePllPhase160MHz_0[3:0]"],
                          3: ["reg069ePllVmm0", "ePllPhase160MHz_3[3:0]"],
                          2: ["reg069ePllVmm0", "ePllPhase160MHz_2[3:0]"],
                          5: ["reg084ePllVmm1", "ePllPhase160MHz_1[3:0]"],
                          4: ["reg084ePllVmm1", "ePllPhase160MHz_0[3:0]"],
                          7: ["reg085ePllVmm1", "ePllPhase160MHz_3[3:0]"],
                          6: ["reg085ePllVmm1", "ePllPhase160MHz_2[3:0]"]}
        
        def update(reg, subreg, value):
            analog = "rocPllCoreAnalog"
            if analog not in self.data[name]:
                self.data[name][analog] = {}
            if reg not in self.data[name][analog]:
                self.data[name][analog][reg] = {}
            self.data[name][analog][reg][subreg] = value

        update(regs_160mhz_4[id][0], regs_160mhz_4[id][1], value_160mhz_4)
        update(regs_160mhz_30[id][0], regs_160mhz_30[id][1], value_160mhz_30)

    def save_json(self, new_name):
        with open(new_name, 'w') as f:
            json.dump(self.data, f, indent=4)

def main():
    parser = JsonParser("test.json")
    print(parser.get_enabled_srocs("MMFE8_0001"))
    print(parser.get_enabled_vmms("MMFE8_0001"))
    parser.update_value_core("MMFE8_0001", 120)
    parser.update_value_vmm("MMFE8_0001", 2, 10)
    parser.save_json("new.json")

if __name__ == "__main__":
<<<<<<< HEAD
    main()
=======
    main()
>>>>>>> f92eed7b0922ad0298f5f4e2a35278819030723a
