//===- tools/lbw-dump.cpp - lbw dumper --------------------------*- C++ -*-===//
//
// evelog
//
// This file is distributed under the Simplified BSD License. See LICENSE.TXT
// for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a very simple lbw storage entry dumper using the evelog
// library.
//
//===----------------------------------------------------------------------===//

#include <fstream>
#include <iostream>

#include "evelog/Endian.h"
#include "evelog/LBWReader.h"

void print_help() {
  std::cout << "lwb-dump <input-file>\n";
}

int main(int argc, char** argv) {
  if (argc != 2) {
    print_help();
    return 0;
  }

  std::ifstream input_file(argv[1], std::ios::binary);

  try {
    evelog::Workspace w;
    input_file >> w;
    std::cout << w.Name << "\n";
    for (auto si = w.begin_stores(), se = w.end_stores(); si != se; ++si) {
      std::cout << si->Name << "\n";
      for (auto ei = si->begin_entries(), ee = si->end_entries();
                ei != ee; ++ei) {
        std::cout << ei->Data << "\n";
      }
    }
  } catch (evelog::parse_error &pe) {
    std::cout << "parse error!!! " << pe.what() << "\n@" << input_file.tellg() << "\n";
  }

  return 0;
}
