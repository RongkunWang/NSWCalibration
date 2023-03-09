#!/user/bin/env python3

import numpy as np
import array as arr
import argparse
import pandas as pd
import json
import ROOT
import os
import logging
from collections import OrderedDict as od
from collections import defaultdict
from nsw_rocphase_parse_json import *
from nsw_rocphase_plotter import *

def parse_txt (inputfile):                                         
  board_id = inputfile.split("/")[-1].split("_", 5)[-1].rstrip('.csv')
  timestamp = inputfile.split("/")[-1].split("_", 1)[-1][:20]
  raw_data = pd.read_csv(inputfile, delimiter=";")
  data = raw_data.values  # makes numpy arrays

  for i in range(data.shape[0]):                                  
    for j in range (1, data.shape[1]):
      data[i][j] = int(data[i][j], 16)
  return board_id, timestamp, data

def apply_config_mask(raw_data, mask):
  cols = []
  for i, status in enumerate(mask):
    if status == 0:
      cols.append(i)
  raw_data = np.delete(raw_data, cols, axis=1)
  return raw_data

def transform_data (raw_data, expected_value, mask):# -> np.ndarray:   #1d array good/bad per iteration
  masked_data = apply_config_mask(raw_data, mask)
  intermediate_data = np.equal(masked_data, expected_value) #for every iteration, true/false for every sroc/vmm
  return np.all(intermediate_data, axis=1), len(np.all(intermediate_data, axis=1)) #axis=row, ie all elements of row true so now a 1d

def find_shift (data):# -> int:
  return np.argmax(data != data[0]) #converts to t/f wrt to data[0]; finds first true; - when 1st rotate, + for reversing

def rotate_data (data, shift):# -> np.ndarray: #shifted 1d
  return np.roll(data, shift)

def find_all_regions(data): #-> list[Region]:
  start = False
  start_region = None
  regions = []

  for i, iteration in enumerate(data):
    if iteration == True and start == False:
      start_region = i
      start = True
    if iteration == (not True) and start == True:
      regions.append(Region(start_region, i-1)) 
      start_region = None
      start = False
    if i == len(data) - 1 and iteration == True:          
      regions.append(Region(start_region, i))
  return regions

def find_holes(region_end, region_begin): #->list[int]
  holes = list(range(region_end + 1, region_begin))
  return holes

def merge_regions(regions): #-> list[Region]
  merged_regions = []
  hole_width = 1
  merges = 0
  max_num_holes = 1

  def merge_2_regions(region1, region2): #->Region  #input is Region object, pass any 2 regions, checks width, check if already holes
    if region1.num_holes() < max_num_holes and region2.num_holes() < max_num_holes and region2.start - region1.end <= hole_width + 1:
      new_holes = find_holes(region1.end, region2.start)
      merged_region = Region(region1.start, region2.end, region1.holes + region2.holes + new_holes) #region2.holes for saftey if reverse
      return merged_region, True
    return region1, False
 
  ismerged = True
  while ismerged:
    for i, region in enumerate(regions[:-1]): #just go over index 0, 1 then start over; merges first 2 that can be merged
      merged_region, ismerged = merge_2_regions(region, regions[i+1])
      merged_regions.append(merged_region)
      if ismerged:
        merged_regions += regions[(i+2):]
        break
    else: # for/else - if gets to end of for loop without break ie to last element
      merged_regions.append(regions[-1])
      return merged_regions
    regions = merged_regions[:] 
    merged_regions = [] # start over every time get merged region and rewrite merged_regions

def shift_back_regions(regions, shift, iterations):  # takes and returns list of Region objects
  shifted_back_regions = []
  for region in regions:
    shifted_start = (region.start + shift) % iterations
    shifted_end = (region.end + shift) % iterations
    shifted_holes = []
    for hole in region.holes:
      shifted_hole = (hole + shift) % iterations
      shifted_holes.append(shifted_hole)
    shifted_back_regions.append(Region(shifted_start, shifted_end, shifted_holes))
  return sorted(shifted_back_regions, key=lambda x: x.start)

def choose_region_40core(good_regions, bad_regions, iterations):
  distances = [x.find_distance(iterations) for x in bad_regions]  
  smallest_bad_region = bad_regions[distances.index(min(distances))]
  for region in good_regions:
    if region.start == smallest_bad_region.end + 1:
      return region

def find_negative_regions(regions):
  negative_regions = []
  for i, region in enumerate(regions[:-1]):
    negative_regions.append(Region(region.end+1, regions[i+1].start-1))
  negative_regions.append(Region(regions[-1].end+1, regions[0].start-1))
  return sorted(negative_regions, key=lambda x: x.start)

def new_core(board, best_phase, new_phases):
  opc_node = board.replace('_', '/')
  value_40mhz = best_phase
  value_160mhz = best_phase % 32
  new_phases[opc_node]['ePllCore.ePllPhase40MHz_0']= value_40mhz
  new_phases[opc_node]['ePllCore.ePllPhase40MHz_1']= value_40mhz
  new_phases[opc_node]['ePllCore.ePllPhase40MHz_2']= value_40mhz
  new_phases[opc_node]['ePllCore.ePllPhase160MHz_0']= value_160mhz
  new_phases[opc_node]['ePllCore.ePllPhase160MHz_1']= value_160mhz
  new_phases[opc_node]['ePllCore.ePllPhase160MHz_2']= value_160mhz

  return


class Region:
  def __init__(self, start, end, holes = []):  #default holes empty
    self.start = start
    self.end = end
    self.holes = holes

  def num_holes(self):  #"self" to pass own argument
    return len(self.holes)

  def find_distance(self, iterations):
    size = self.end - self.start
    if size < 0:
      size += iterations
    return size + 1
  
  def expand_region(self, iterations):
    if self.end < self.start:
      return list(range(self.start, iterations)) + list(range(0, self.end + 1))
    return list(range(self.start, self.end + 1))

  def find_middle(self, iterations):
    expanded_region = self.expand_region(iterations)
    middle = float(len(expanded_region))/2
    if (len(expanded_region) % 2 != 0):
      best_value = expanded_region[int(middle - .5)]
    else:
      best_value = (expanded_region[int(middle-1)], expanded_region[int(middle)])
      best_value = best_value[0] # picks first, it two
    return best_value

  def split_region(self):
    region_a = Region(self.start, self.holes[0] - 1)
    region_b = Region(self.holes[-1] + 1, self.end)
    return [region_a, region_b]
 
  def __repr__(self):
    if self.num_holes() == 0:
      return '[{}, {}]'.format(self.start, self.end)
    else:
      return '[{}, {}]'.format(self.start, self.end) + ' holes: {}'.format(self.holes)

def main():
  parser = argparse.ArgumentParser(description='analyze roc phase scan results')
  parser.add_argument('--input-core40', nargs='+', help="40 MHz core clock csvs containing roc register values for each phase")
  parser.add_argument('--input-core160', nargs='+', help="160 MHz core clock csvs containing roc register values for each phase")
  parser.add_argument('--input-vmm160', nargs='+', help="160 MHz vmm clock csvs containing roc register values for each phase")
  parser.add_argument('--json', required=True, help="configuration json file for sector")
  parser.add_argument('--output-json', required=True,  help="json file with calibrated phases")
  parser.add_argument('--output-root', required=True, help="root file for plots")

  args = parser.parse_args()
  new_json = args.output_json
  histName = args.output_root
  logName = histName.replace('.root', '.log')

  parser = JsonParser(args.json)
  plotter = Plotter(histName)
  logging.basicConfig(filename=logName, filemode='w', format='%(levelname)s %(message)s')
  logger = logging.getLogger()
  logger.setLevel(logging.DEBUG)

  srON = True # true if want sr regions in plots
  debug = False # print statements
  index_ttc_fif0_full = 17  
  value_ttc_fifo_full = 4
  value_soft_reset = 255
  value_good_parity = 0
  value_good_capture = 1
  value_no_sroc_errors = 0
  expected_num_regions = 2
  
  unrotated_results = od()
  regions_40core_results = od()
  regions_160core_results = od()
  raw_data = od()

  logger.info('current json: %s', args.json)
  if args.input_core40 is not None:
    csv_data = od()
    for inputfile in args.input_core40:
      board_id, timestamp, data = parse_txt(inputfile)
      logger.info('\n')
      logger.info('%s', inputfile)
      sroc_data = data[:, index_ttc_fif0_full:]
      all_data = data[:,1:]
      mask_sroc = parser.get_enabled_srocs(board_id)
      mask_vmm = parser.get_enabled_vmms(board_id)
      mask_full = mask_vmm + mask_vmm + mask_sroc
      logger.info('sroc configuration: %s', mask_sroc)
      
      ##### find good regions
      data1D, iterations = transform_data (sroc_data, value_ttc_fifo_full, mask_sroc)
      shift = find_shift(data1D)
      rotated_1D = rotate_data(data1D, -1*shift)
      regions = find_all_regions(rotated_1D)
      if len(regions) == 0:
        logger.error('NO GOOD REGIONS FOR %s', board_id)
        continue
      elif len(regions) == 1:
        logger.error('ONLY ONE GOOD REGION FOR %s', board_id)
        continue
      else:
        merged_regions = merge_regions(regions)
      if debug:
        print ('shift {}'.format(shift))
        print ('unrotated' + '\n' + str(data1D))
        print ('rotated' + '\n' + str(rotated_1D))
        print ('shifted good regions: {}'.format(regions))
        print ('shifted merged regions: {}'.format(merged_regions))
      shifted_back_regions = shift_back_regions(merged_regions, shift, iterations)
      logger.info('merged good regions: %s', shifted_back_regions) #shifted back merged good regions

      ##### find soft resest regions
      data1D_sr, iterations = transform_data (all_data, value_soft_reset, mask_full)
      shift_sr = find_shift(data1D_sr)
      rotated_1D_sr = rotate_data(data1D_sr, -1*shift_sr)
      regions_sr = find_all_regions(rotated_1D_sr) # no need to merge regions
      shifted_back_regions_sr = shift_back_regions(regions_sr, shift_sr, iterations)
      logger.info('soft reset regions: %s', shifted_back_regions_sr)

      ##### find bad regions - merge if hole
      shifted_back_regions_bad = find_negative_regions(shifted_back_regions)     
      if len(shifted_back_regions_bad) > expected_num_regions:
        shifted_back_regions_bad = merge_regions(shifted_back_regions_bad)
        logger.info('bad regions merged for: %s', board_id)
      logger.info('bad regions: %s', shifted_back_regions_bad)

      ##### find L1A region
      if len(shifted_back_regions_bad) > expected_num_regions:
        logger.warning('Unexpected results. Check control plots - more than 2 bad regions for %s', board_id)
        region_L1A = shifted_back_regions[-1] # default expect to be last good region
      else:
        region_L1A = choose_region_40core(shifted_back_regions, shifted_back_regions_bad, iterations)
        ##### choose largest sub good region if holes in L1A region
        if region_L1A:
          if region_L1A.num_holes() > 0:
            split_regions = region_L1A.split_region()
            if debug: print ('split L1A region {} into {}'.format(region_L1A, split_regions))
            distances = [x.find_distance(iterations) for x in split_regions]
            region_L1A = split_regions[distances.index(max(distances))]
        else:
          logger.error('NO L1A REGION FOUND FOR %s', board_id)
          continue

      ##### for plots and combined analysis
      data1D = [int(x) for x in data1D]
      if srON:
        for i, value in enumerate(data1D_sr):
          if value == 1:
            data1D[i] = 2
      csv_data[board_id] = all_data
      unrotated_results[board_id] = data1D
      regions_40core_results[board_id] = region_L1A  # good 40 core region

    ##### plotting
    timestamp_40core = timestamp
    iterations_40core = iterations
    plotter.add_data(unrotated_results, CalibrationType.core40, timestamp)
    raw_data[CalibrationType.core40] = csv_data
    unrotated_results.clear()

  if args.input_core160 is not None:
    csv_data = od()
    for inputfile in args.input_core160:
      board_id, timestamp, data = parse_txt(inputfile)
      logger.info('\n')
      logger.info('%s', inputfile)
      sroc_data = data[:, index_ttc_fif0_full:]
      parity_data = data[:, 9:17]
      capture_data = data[:, 1:9]
      all_data = data[:,1:]
      mask_sroc = parser.get_enabled_srocs(board_id)
      mask_vmm = parser.get_enabled_vmms(board_id)
      mask_full = mask_vmm + mask_vmm + mask_sroc
      logger.info('sroc configuration: %s', mask_sroc)

      ##### find good regions                                                                                                              
      data1D, iterations = transform_data (sroc_data, value_no_sroc_errors, mask_sroc) 
      shift = find_shift(data1D)
      rotated_1D = rotate_data(data1D, -1*shift)
      regions = find_all_regions(rotated_1D)
      if len(regions) != 0:
        merged_regions = regions  # merging not needed for 160 core
      else:
        logger.error('NO GOOD REGIONS FOR %s', board_id)
        continue
      shifted_back_regions = shift_back_regions(merged_regions, shift, iterations)
      logger.info('good regions: %s', shifted_back_regions)

      ##### find soft resest regions
      data1D_sr, iterations = transform_data (all_data, value_soft_reset, mask_full)
      shift_sr = find_shift(data1D_sr)
      rotated_1D_sr = rotate_data(data1D_sr, -1*shift_sr)
      regions_sr = find_all_regions(rotated_1D_sr) # no need to merge regions
      shifted_back_regions_sr = shift_back_regions(regions_sr, shift_sr, iterations)
      logger.info('soft reset regions: %s', shifted_back_regions_sr)

      ##### for plots and combined analysis
      data1D = [int(x) for x in data1D]
      if srON:
        for i, value in enumerate(data1D_sr):
          if value == 1:
            data1D[i] = 2
      csv_data[board_id] = all_data
      unrotated_results[board_id] = data1D
      regions_160core_results[board_id] = shifted_back_regions

    ##### plotting
    plotter.add_data(unrotated_results, CalibrationType.core160, timestamp)
    raw_data[CalibrationType.core160] = csv_data
    unrotated_results.clear()

  if args.input_vmm160 is not None:
    csv_data = od()
    for inputfile in args.input_vmm160:
      board_id, timestamp, data = parse_txt(inputfile)
      logger.info('\n')
      logger.info('%s', inputfile)
      capture_data = data[:, 1:9]
      parity_data = data[:, 9:17]
      all_data = data[:,1:]
      mask_vmm = parser.get_enabled_vmms(board_id)
      logger.info('vmm configuration: %s', mask_vmm)

      unrotated_results[board_id] = od() # dictionary of dictionaries of lists
      board_exist = False
      for i, mask in enumerate(mask_vmm):
        if mask == 1:
          mask_vmm_single = [0 for j in range(8)]
          mask_vmm_single[i] = 1
          logger.info('single vmm configuration: %s', mask_vmm_single)

          ##### find good regions
          data1D_a, iterations = transform_data (parity_data, value_good_parity, mask_vmm_single)         
          data1D_b, iterations = transform_data (capture_data, value_good_capture, mask_vmm_single)
          data1D = data1D_a & data1D_b
          shift = find_shift(data1D)
          rotated_1D = rotate_data(data1D, -1*shift)
          regions = find_all_regions(rotated_1D)
          if len(regions) != 0:
            board_exist = True
            merged_regions = regions # don't need to merge regions for vmms
          else:
            logger.error('%s HAD NO GOOD REGIONS FOR VMM %d (check configuration if repeated vmm)', board_id, i)
            continue
          shifted_back_regions = shift_back_regions(merged_regions, shift, iterations)
          logger.info('good regions: %s', shifted_back_regions)
          distances = [x.find_distance(iterations) for x in shifted_back_regions]  

          ##### check expected results
          if len(distances) == 2:
            if abs(max(distances) - min(distances)) > 2:
              logger.info('good regions vary in size for %s VMM %d', board_id, i)
          else:
            logger.warning('Unexpected results. Check control plots: > or < 2 good regions for %s VMM %d', board_id, i)

          ##### pick best region and phase
          best_region_index = distances.index(max(distances))  # if same, picks first
          best_region = shifted_back_regions[best_region_index]
          best_phase_vmm = best_region.find_middle(iterations)
          logger.info('width of best region %d', max(distances))
          logger.info('best phase for VMM %d: region %d value %d', i, best_region_index + 1, best_phase_vmm)

          ##### write to new json
          parser.update_value_vmm(board_id, i, best_phase_vmm)

          ##### for plotting
          data1D = [int(x) for x in data1D]
          data1D[best_phase_vmm] = 2 # change for plotter
          unrotated_results[board_id][i] = data1D

      if not board_exist: del unrotated_results[board_id]
      csv_data[board_id] = all_data

    ##### plotting
    plotter.add_data(unrotated_results, CalibrationType.vmm160, timestamp)
    raw_data[CalibrationType.vmm160] = csv_data
    #unrotated_results.clear()

  ##### combined core results
  if args.input_core40 is not None and args.input_core160 is not None:
    combined_results = od()
    new_core_phases = defaultdict(dict)
    logger.info('\n')
    logger.info('COMBINED CORE RESULTS')
    for board, region in regions_40core_results.items():
      if board in regions_160core_results.keys():
        logger.info('\n')
        logger.info('%s', board)
        expanded_region_L1A = region.expand_region(iterations_40core)
        expanded_regions_160 = [x.expand_region(iterations_40core) for x in regions_160core_results[board]]

        ##### find overlaping region and best phase
        overlap_core_regions = [list(filter(lambda x: x in expanded_region_L1A, sublist)) for sublist in expanded_regions_160]
        max_overlap_region = max(overlap_core_regions, key=len)
        if debug:
          print ([len(x) for x in overlap_core_regions])
          print ('40 MHz L1A region' + str(expanded_region_L1A))
          print ('overlap regions: {}'.format(overlap_core_regions))
          print ('largest overlap region: {}'.format(max_overlap_region))
        if len(max_overlap_region) == 0:
          logger.error('NO CORE CLOCK OVERLAP REGION FOR %s', board)
          continue
        logger.info('largest overlap region: %s', max_overlap_region)
        logger.info('width of largest region %d', len(max_overlap_region))
        best_region_combined = Region(max_overlap_region[0], max_overlap_region[-1])
        best_phase_combined = best_region_combined.find_middle(iterations_40core)

        ##### for plotting
        overlap_data1D = [0 for x in range(iterations_40core)]
        for iteration in max_overlap_region:
          overlap_data1D[iteration] = 1
        if debug: print ('{} and {}'.format(max_overlap_region, overlap_data1D))        
        overlap_data1D[best_phase_combined] = 2 # change for plotter
        combined_results[board] = overlap_data1D # dictionary of lists
        logger.info('best phase for core clocks: %d', best_phase_combined)

        ##### write to new json
        parser.update_value_core(board, best_phase_combined)
        ##### just core phases for vmm calibration
        new_core(board, best_phase_combined, new_core_phases)
      else:
        logger.error('MISSING core data for combined analysis for %s', board)

    ##### plotting
    plotter.add_data(combined_results, CalibrationType.core_combined, timestamp_40core)
    ##### json for vmm calibration
    with open("new_core_phases.json", "w") as outfile:
      json.dump(new_core_phases, outfile, indent=3)
      print (f'json for vmm calibration {outfile}')
  else:
    logger.info('core combined analysis not performed: need both 40 and 160 MHz core clock data')
  
  plotter.save(raw_data)
  parser.save_json(new_json)
  print ('\nupdated phases in {}'.format(new_json))
  logger.info('updated phases in %s', new_json)
  print ('please check {} for any errors or warnings'.format(logName))

if __name__ == '__main__':
  main()
