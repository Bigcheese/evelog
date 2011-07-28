//===- LBWReader.h - lbw reader ---------------------------------*- C++ -*-===//
//
// evelog
//
// This file is distributed under the Simplified BSD License. See LICENSE.TXT
// for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the classes needed to read a lbw file.
//
//===----------------------------------------------------------------------===//

#ifndef EVELOG_LBWREADER_H
#define EVELOG_LBWREADER_H

#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>

#include "evelog/Endian.h"
#include "evelog/StringRef.h"

namespace evelog {

struct parse_error : public std::runtime_error {
  parse_error(const char *msg) : std::runtime_error(msg) {}
};

class Channel {
  little64_t facility_hash;
  little64_t object_hash;
  char facility[0x21];
  char object[0x21];
  char unk[0x0e];

public:
  StringRef getFacility() const;
  StringRef getObject() const;
};

static_assert(sizeof(Channel) == 0x60,
  "channel must be packed! pesi and char should have guaranteed that!");

class Device {
  friend std::istream &operator >>(std::istream &is, Device &d);

  std::vector<Channel> Channels;
public:
  typedef std::vector<Channel>::const_iterator channel_iterator;

  channel_iterator begin_channels() const { return Channels.begin(); }
  channel_iterator end_channels() const { return Channels.end(); }

  std::string Name;
  std::string Description;
  double Created;
  double Modified;
  std::string FileMappingName;
  uint32_t FlushRate;
  uint32_t Capacity;
  uint32_t ChannelCount;
};

std::istream &operator >>(std::istream &is, Device &d);

struct StorageEntry {
  uint16_t ChannelID;
  uint32_t ThreadID;
  uint64_t TimeStamp;
  std::string Data;
  uint32_t ProcessID;
};

std::istream &operator >>(std::istream &is, StorageEntry &se);

class Storage {
  friend std::istream &operator >>(std::istream &is, Storage &s);

  std::vector<StorageEntry> Entries;
public:
  typedef std::vector<StorageEntry>::const_iterator entry_iterator;

  entry_iterator begin_entries() const { return Entries.begin(); }
  entry_iterator end_entries() const { return Entries.end(); }

  std::string Name;
  std::string Description;
  double Created;
  double Modified;
  uint32_t InitialCapacity;
  uint32_t IncrementalCapacity;
};

std::istream &operator >>(std::istream &is, Storage &s);

class Workspace {
  friend std::istream &operator >>(std::istream &is, Workspace &ws);

  std::vector<Device> Devices;
  std::vector<Storage> Stores;
public:
  typedef std::vector<Device>::const_iterator device_iterator;
  typedef std::vector<Storage>::const_iterator storage_iterator;

  device_iterator begin_devices() const { return Devices.begin(); }
  device_iterator end_devices() const { return Devices.end(); }

  storage_iterator begin_stores() const { return Stores.begin(); }
  storage_iterator end_stores() const { return Stores.end(); }

  std::string Name;
  std::string Description;
  double Created;
  double Modified;
  std::string FilePath;
};

std::istream &operator >>(std::istream &is, Workspace &ws);

} // end namespace evelog.

#endif
