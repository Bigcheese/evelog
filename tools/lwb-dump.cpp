//===- tools/lwb-dump.cpp - lwb dumper --------------------------*- C++ -*-===//
//
// evelog
//
// This file is distributed under the Simplified BSD License. See LICENSE.TXT
// for details.
//
//===----------------------------------------------------------------------===//
//
// This file currently does everything :P.
//
//===----------------------------------------------------------------------===//


#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <evelog/Endian.h>

void print_help() {
  std::cout << "lwb-dump <input-file>\n";
}

struct parse_error : public std::runtime_error {
  parse_error(const char *msg) : std::runtime_error(msg) {}
};

struct number {
  std::uint32_t value;

  operator std::uint32_t() const {
    return value;
  }
};

std::istream &operator >>(std::istream &i, number &num) {
  evelog::ulittle8_t type;
  i >> type;

  switch (type) {
  case 0x02: {
      evelog::ulittle8_t val;
      i >> val;
      num.value = val;
      break;
    }
  case 0x03: {
      evelog::ulittle16_t val;
      i >> val;
      num.value = val;
      break;
    }
  case 0x04: {
      evelog::ulittle32_t val;
      i >> val;
      num.value = val;
      break;
    }
  default:
    throw parse_error("invalid number type");
  }

  return i;
}

struct pstring {
  std::string str;

  operator const std::string &() const {
    return str;
  }
};

std::istream &operator >>(std::istream &is, pstring &val) {
  evelog::ulittle8_t type;
  is >> type;
  if (type != 0x06)
    throw parse_error("invalid string type");

  evelog::ulittle8_t size;
  is >> size;
  if (size > 0) {
    std::vector<char> buf(size);
    is.read(&buf.front(), size);
    val.str.reserve(size);
    val.str.assign(buf.begin(), buf.end());
  }
  return is;
}

// Some weird Windows time value.
struct oletime {
  double time;
};

std::istream &operator >>(std::istream &is, oletime &t) {
  evelog::ulittle8_t type;
  is >> type;
  if (type != 0x11)
    throw parse_error("invalid time type");

  // Ok, time to decode some IEEE 754-2008!!!!!
  evelog::ulittle64_t read;
  is >> read;
  uint64_t raw = read;

  // Get the parts.
  uint64_t fraction = raw & ((1LL << 52) - 1);
  int16_t exponent_raw = (raw >> 52) & ((1LL << 11) - 1);
  int16_t exponent = exponent_raw - 1023;
  bool sign = (raw >> 63) == 1;


  switch (exponent_raw) {
  case 0x000: // Zero and subnormals.
    if (fraction == 0) { // Zero
      if (sign)
        t.time = -0.0f;
      else
        t.time = 0.0f;
    } else { // Subnormal
      throw parse_error("got subnormal time value!");
    }
    break;
  case 0x7ff: // Infinity and NaN.
    if (fraction == 0) { // Infinity
      t.time = std::numeric_limits<double>::infinity();
      if (sign)
        t.time = -t.time;
    } else { // NaN
      t.time = std::numeric_limits<double>::quiet_NaN();
    }
    break;
  default: { // Normal number.
      double result = fraction;
      // Shift down.
      result /= (1LL << 52);
      // Add the implicit 1.
      result += 1.0f;
      // Handle exponent.
      int16_t shift = exponent;
      while (shift > 0) { result *= 2.0; --shift; }
      while (shift < 0) { result /= 2.0; ++shift; }
      // Handle sign.
      if (sign)
        result *= -1;
      t.time = result;
    }
  }

  return is;
}

struct channel {
  evelog::little64_t facility_hash;
  evelog::little64_t object_hash;
  char facility[0x21];
  char object[0x21];
  char unk[0x0e];
};

static_assert(sizeof(channel) == 0x60,
  "channel must be packed! pesi and char should have guaranteed that");

struct process_module {
  pstring computer;
  pstring process_name;
  number process_id;
  number thread_id;
  pstring module;
};

std::istream &operator >>(std::istream &is, process_module &pm) {
  is.seekg(4, std::ios::cur); // Skip unknown bytes.
  is >> pm.computer;
  is >> pm.process_name;
  is >> pm.process_id;
  is >> pm.thread_id;
  is >> pm.module;
  is.seekg(8, std::ios::cur); // Skip unknown bytes.

  return is;
}

struct process_module_list {
  pstring name;
  pstring description;
  oletime created;
  oletime modified;
  number count;
  std::vector<process_module> list;
};

std::istream &operator >>(std::istream &is, process_module_list &pml) {
  is >> pml.name;
  is >> pml.description;
  is >> pml.created;
  is >> pml.modified;
  is >> pml.count;

  for (int i = 0; i < pml.count; ++i) {
    process_module pm;
    is >> pm;
    pml.list.push_back(std::move(pm));
  }

  return is;
}

struct device {
  pstring name;
  pstring description;
  oletime created;
  oletime modified;
  pstring file_mapping_name;
  number flush_rate;
  number capacity;
  number channel_count;
  std::vector<channel> channels;
  std::vector<process_module_list> pml;
};

std::istream &operator >>(std::istream &is, device &d) {
  number unknown;

  is >> d.name;
  is >> d.description;
  is >> d.created;
  is >> d.modified;
  is.seekg(8, std::ios::cur); // Skip unknown bytes.
  is >> d.file_mapping_name;
  is >> d.flush_rate;
  is >> d.capacity;
  is.seekg(8, std::ios::cur); // Skip unknown bytes.
  is >> unknown; // Ignore unknown number.
  is >> d.channel_count;

  // This is well defined because channel consists entirely of char arrays.
  d.channels.resize(d.channel_count);
  is.read( reinterpret_cast<char*>(&d.channels.front())
         , d.channel_count * sizeof(channel)
         );

  for (int i = 0; i < d.channel_count; ++i) {
    process_module_list pml;
    is >> pml;
    d.pml.push_back(std::move(pml));
  }

  return is;
}

struct storage_entry {
  uint16_t channel_id;
  uint32_t thread_id;
  uint64_t timestamp;
  std::string data;
  uint32_t process_id;
};

std::istream &operator >>(std::istream &is, storage_entry &se) {
  evelog::ulittle16_t channel_id;
  evelog::ulittle32_t thread_id;
  evelog::ulittle64_t timestamp;
  evelog::ulittle32_t len;
  evelog::ulittle32_t process_id;

  // Read
  is >> channel_id;
  is >> thread_id;
  is >> timestamp;
  is.seekg(4, std::ios::cur); // Skip unknown bytes.
  is >> len;
  std::vector<char> data(len);
  is.read(&data.front(), len);
  is >> process_id;
  is.seekg(4, std::ios::cur); // Skip unknown bytes.

  // Store
  se.channel_id = channel_id;
  se.thread_id = thread_id;
  se.timestamp = timestamp;
  se.data.assign(data.begin(), data.end());
  se.process_id = process_id;

  return is;
}

struct storage {
  pstring name;
  pstring description;
  oletime created;
  oletime modified;
  number inital_capacity;
  number incremental_capacity;
  number entry_count;
  std::vector<storage_entry> entries;
};

std::istream &operator >>(std::istream &is, storage &s) {
  number unknown;

  is >> s.name;
  is >> s.description;
  is >> s.created;
  is >> s.modified;
  is.seekg(8, std::ios::cur); // Skip unknown bytes.
  is >> s.inital_capacity;
  is >> s.incremental_capacity;
  is.seekg(10, std::ios::cur); // Skip unknown bytes.
  is >> unknown;
  is.seekg(1, std::ios::cur); // Skip unknown bytes.
  is >> unknown;
  is.seekg(1, std::ios::cur); // Skip unknown bytes.
  is >> s.entry_count;
  is >> unknown;

  for (int i = 0; i < s.entry_count; ++i) {
    storage_entry se;
    is >> se;
    s.entries.push_back(std::move(se));
  }

  return is;
}

struct workspace {
  pstring name;
  pstring description;
  oletime created;
  oletime modified;
  pstring file_path;
  number device_count;
  std::vector<device> devices;
  number storage_count;
  std::vector<storage> stores;
};

std::istream &operator >>(std::istream &is, workspace &ws) {
  // Skip first two bytes of uselessness.
  is.seekg(2, std::ios::cur);
  is >> ws.name;
  is >> ws.description;
  is >> ws.created;
  is >> ws.modified;
  is >> ws.file_path;
  is >> ws.device_count;

  for (int i = 0; i < ws.device_count; ++i) {
    device d;
    is >> d;
    ws.devices.push_back(std::move(d));
  }

  is.seekg(2, std::ios::cur); // Skip unknown bytes.

  is >> ws.storage_count;

  for (int i = 0; i < ws.storage_count; ++i) {
    storage s;
    is >> s;
    ws.stores.push_back(std::move(s));
  }

  return is;
};

int main(int argc, char** argv) {
  if (argc != 2) {
    print_help();
    return 0;
  }

  std::ifstream input_file(argv[1], std::ios::binary);

  try {
    workspace w;
    input_file >> w;
    std::cout << "name: " << w.name.str << "\n"
              << "description: " << w.description.str << "\n"
              << "created: " << w.created.time << "\n"
              << "modified: " << w.modified.time << "\n"
              << "file_path: " << w.file_path.str << "\n"
              << "device_count: " << w.device_count << "\n";
    for (auto i = w.devices.begin(), e = w.devices.end(); i != e; ++i) {
      std::cout << "device -\n"
                << "  name: " << i->name.str << "\n"
                << "  description: " << i->description.str << "\n"
                << "  created: " << i->created.time << "\n"
                << "  modified: " << i->modified.time << "\n"
                << "  file_mapping_name: " << i->file_mapping_name.str << "\n"
                << "  flush_rate: " << i->flush_rate << "\n"
                << "  capacity: " << i->capacity << "\n"
                << "  channel_count: " << i->channel_count << "\n";

#ifdef WIN32
      std::cout << "  === ignoreing device data because it takes too long to "
                << "print to console on Windows ===\n";
#else
      for (auto ci = i->channels.begin(), ce = i->channels.end();
                ci != ce; ++ci) {
        std::cout << "  channel -\n"
                  << "    facility_hash: " << ci->facility_hash << "\n"
                  << "    object_hash: " << ci->object_hash << "\n"
                  << "    facility: " << ci->facility << "\n"
                  << "    object: " << ci->object << "\n";
      }
      for (auto pmli = i->pml.begin(), pmle = i->pml.end();
                pmli != pmle; ++pmli) {
        std::cout << "  process_module_list -\n"
                  << "    name: " << pmli->name.str << "\n"
                  << "    description: " << pmli->description.str << "\n"
                  << "    created: " << pmli->created.time << "\n"
                  << "    modified: " << pmli->modified.time << "\n"
                  << "    count: " << pmli->count << "\n";
        for (auto pmi = pmli->list.begin(), pme = pmli->list.end();
                  pmi != pme; ++pmi) {
          std::cout << "    process_module -\n"
                    << "      computer: " << pmi->computer.str << "\n"
                    << "      process_name: " << pmi->process_name.str << "\n"
                    << "      process_id: " << pmi->process_id << "\n"
                    << "      thread_id: " << pmi->thread_id << "\n"
                    << "      module: " << pmi->module.str << "\n";
        }
      }
#endif // WIN32
    }
    std::cout << "storage_count: " << w.storage_count << "\n";
    for (auto si = w.stores.begin(), se = w.stores.end(); si != se; ++si) {
      std::cout << "  storage -\n"
                << "    name: " << si->name.str << "\n"
                << "    description: " << si->description.str << "\n"
                << "    created: " << si->created.time << "\n"
                << "    modified: " << si->modified.time << "\n"
                << "    initial_capacity: " << si->inital_capacity << "\n"
                << "    incremental_capacity: "
                  << si->incremental_capacity << "\n"
                << "    entry_count: " << si->entry_count << "\n";
      for (auto ei = si->entries.begin(), ee = si->entries.end();
                ei != ee; ++ ei) {
        std::cout << "  entry -\n"
                  << "    channel_id: " << ei->channel_id << "\n"
                  << "    thread_id: " << ei->thread_id << "\n"
                  << "    timestamp: " << ei->timestamp << "\n"
                  << "    data: " << ei->data << "\n"
                  << "    process_id: " << ei->process_id << "\n";
      }
    }
  } catch (parse_error &pe) {
    std::cout << "parse error!!! " << pe.what() << "\n@" << input_file.tellg() << "\n";
  }

  return 0;
}
