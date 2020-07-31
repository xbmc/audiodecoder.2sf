/*
 *  Copyright (C) 2014-2020 Arne Morten Kvarving
 *  Copyright (C) 2016-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "psflib.h"
#include "state.h"

#include <kodi/addon-instance/AudioDecoder.h>

struct twosf_loader_state
{
  uint8_t* rom;
  uint8_t* state;
  size_t rom_size;
  size_t state_size;

  int initial_frames;
  int sync_type;
  int clockdown;
  int arm9_clockdown_level;
  int arm7_clockdown_level;

  twosf_loader_state()
    : rom(0),
      state(0),
      rom_size(0),
      state_size(0),
      initial_frames(-1),
      sync_type(0),
      clockdown(0),
      arm9_clockdown_level(0),
      arm7_clockdown_level(0)
  {
  }

  ~twosf_loader_state()
  {
    free(rom);
    free(state);
  }
};

struct TSFContext
{
  twosf_loader_state state;
  NDS_state emu;
  int64_t len;
  int sample_rate;
  int64_t pos;
  std::string title;
  std::string artist;
  std::string game;
  std::string copyright;
  std::string year;
  std::string comment;
  bool inited = false;
};

class ATTRIBUTE_HIDDEN C2SFCodec : public kodi::addon::CInstanceAudioDecoder
{
public:
  C2SFCodec(KODI_HANDLE instance, const std::string& version);

  virtual ~C2SFCodec();

  bool Init(const std::string& filename,
            unsigned int filecache,
            int& channels,
            int& samplerate,
            int& bitspersample,
            int64_t& totaltime,
            int& bitrate,
            AudioEngineDataFormat& format,
            std::vector<AudioEngineChannel>& channellist) override;
  int ReadPCM(uint8_t* buffer, int size, int& actualsize) override;
  int64_t Seek(int64_t time) override;
  bool ReadTag(const std::string& file, kodi::addon::AudioDecoderInfoTag& tag) override;

private:
  TSFContext ctx;
};
