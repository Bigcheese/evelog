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

#ifdef WIN32
# include <boost/asio.hpp>
# include <dir-monitor/dir_monitor.hpp>
#endif

#include "evelog/StringRef.h"
#include "evelog/LBWReader.h"

void dump_file(evelog::StringRef file_path) {
  std::ifstream input_file(file_path, std::ios::binary);

  if (!input_file) {
    std::cout << "Failed to open: " << file_path.str() << "\n";
    return;
  }

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
    std::cout << "parse error!!! " << pe.what()
              << "\n@" << input_file.tellg() << "\n";
  }
}

#ifdef WIN32
boost::asio::io_service io_service;
boost::asio::dir_monitor dm(io_service);

void create_file_handler(const boost::system::error_code &ec,
                         const boost::asio::dir_monitor_event &ev) {
  // Setup the monitor again.
  dm.async_monitor(create_file_handler);

  if (ev.type != boost::asio::dir_monitor_event::added) return;
  if (!evelog::StringRef(ev.filename).endswith(".lbw")) return;

  // HACK: Sleep for 100ms before trying to open the file... This should
  //       actually poll until it can be opened.
  boost::this_thread::sleep(boost::posix_time::milliseconds(100));

  std::string file_path = ev.dirname + "\\" + ev.filename;
  dump_file(file_path);
}

std::string get_eve_online_directory() {
  HKEY key;
  LONG result = ::RegOpenKeyExA(
       HKEY_CURRENT_USER
    , "Software\\Valve\\Steam"
    , 0
    , KEY_QUERY_VALUE
    , &key
    );
  if (result != ERROR_SUCCESS) return "";

  DWORD type;
  DWORD size;

  // Get size.
  result = ::RegQueryValueExA(
      key
    , "SteamPath"
    , NULL
    , &type
    , NULL
    , &size
    );

  if (result != ERROR_SUCCESS) return "";

  // Read value.
  std::vector<char> buf(size);
  result = ::RegQueryValueExA(
      key
    , "SteamPath"
    , NULL
    , &type
    , (LPBYTE)&buf.front()
    , &size
    );

  if (result != ERROR_SUCCESS || type != REG_SZ) return "";

  return std::string(&buf.front()) + "\\steamapps\\common\\eve online";
}
#endif

void print_help() {
  std::cout << "lbw-dump [input file]\n"
"\tWINDOWS ONLY\n"
"\tIf called without arguments, the EVE Online directory is watched for\n"
"\tchanges. Each time a file is written it is dumped.\n";
}

int main(int argc, char** argv) {
#ifdef WIN32
  if (argc == 1) {
    std::string eve_path = get_eve_online_directory();
    if (eve_path == "") {
      std::cout << "Failed to get EVE Online path.\n";
      return 1;
    }

    std::cout << "Watching for lbw files in " << eve_path << "\n";

    dm.add_directory(eve_path);
    dm.async_monitor(create_file_handler);
    try {
      io_service.run();
    } catch(...) {
      std::cout << "Uncaught exception!\n";
    }
    return 0;
  }
#endif

  if (argc == 2) {
    dump_file(argv[1]);
    return 0;
  }

  print_help();
  return 1;
}
