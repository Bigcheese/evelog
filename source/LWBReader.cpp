//===- LWBReader.cpp - lwb reader -------------------------------*- C++ -*-===//
//
// evelog
//
// This file is distributed under the Simplified BSD License. See LICENSE.TXT
// for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the classes and operators needed to read a lwb file.
//
//===----------------------------------------------------------------------===//

#include <cstdint>
#include <iostream>

#include "evelog/LWBReader.h"
#include "evelog/Endian.h"

namespace {
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
    throw evelog::parse_error("invalid number type");
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
    throw evelog::parse_error("invalid string type");

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
    throw evelog::parse_error("invalid time type");

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
      throw evelog::parse_error("got subnormal time value!");
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
} // end annon namespace.

namespace evelog {

std::istream &operator >>(std::istream &is, Workspace &ws) {
  pstring name;
  pstring description;
  oletime created;
  oletime modified;
  pstring file_path;
  number device_count;
  number storage_count;

  // Skip first two bytes of uselessness.
  is.seekg(2, std::ios::cur);
  is >> name
     >> description
     >> created
     >> modified
     >> file_path
     >> device_count;

  ws.Name        = name;
  ws.Description = description;
  ws.Created     = created.time;
  ws.Modified    = modified.time;
  ws.FilePath    = file_path;

  for (int i = 0; i < device_count; ++i) {
    Device d;
    is >> d;
    ws.Devices.push_back(std::move(d));
  }

  is.seekg(2, std::ios::cur); // Skip unknown bytes.
  is >> storage_count;

  for (int i = 0; i < storage_count; ++i) {
    Storage s;
    is >> s;
    ws.Stores.push_back(std::move(s));
  }

  return is;
}

std::istream &operator >>(std::istream &is, Device &d) {
  pstring name;
  pstring description;
  oletime created;
  oletime modified;
  pstring file_mapping_name;
  number  flush_rate;
  number  capacity;
  number  unknown;
  number  channel_count;

  is >> name
     >> description
     >> created
     >> modified;
  is.seekg(8, std::ios::cur); // Skip unknown.
  is >> file_mapping_name
     >> flush_rate
     >> capacity;
  is.seekg(8, std::ios::cur); // Skip unknown.
  is >> unknown
     >> channel_count;

  d.Name            = name;
  d.Description     = description;
  d.Created         = created.time;
  d.Modified        = modified.time;
  d.FileMappingName = file_mapping_name;
  d.FlushRate       = flush_rate;
  d.Capacity        = capacity;

  d.Channels.resize(channel_count);
  is.read( reinterpret_cast<char*>(&d.Channels.front())
         , channel_count * sizeof(Channel)
         );

  for (int i = 0; i < channel_count; ++i) {
    process_module_list pml;
    is >> pml;
    // Ignore results for now.
  }

  return is;
}

std::istream &operator >>(std::istream &is, Storage &s) {
  pstring name;
  pstring description;
  oletime created;
  oletime modified;
  number  inital_capacity;
  number  incremental_capacity;
  number  unknown;
  number  entry_count;

  is >> name
     >> description
     >> created
     >> modified;
  is.seekg(8, std::ios::cur); // Skip unknown.
  is >> inital_capacity
     >> incremental_capacity;
  is.seekg(10, std::ios::cur); // Skip unknown.
  is >> unknown;
  is.seekg(1, std::ios::cur); // Skip unknown.
  is >> unknown;
  is.seekg(1, std::ios::cur); // Skip unknown.
  is >> entry_count
     >> unknown;

  s.Name                = name;
  s.Description         = description;
  s.Created             = created.time;
  s.Modified            = modified.time;
  s.InitialCapacity     = inital_capacity;
  s.IncrementalCapacity = incremental_capacity;

  for (int i = 0; i < entry_count; ++i) {
    StorageEntry se;
    is >> se;
    s.Entries.push_back(std::move(se));
  }

  return is;
}

std::istream &operator >>(std::istream &is, StorageEntry &se) {
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
  se.ChannelID = channel_id;
  se.ThreadID  = thread_id;
  se.TimeStamp = timestamp;
  se.Data.assign(data.begin(), data.end());
  se.ProcessID = process_id;

  return is;
}

}
