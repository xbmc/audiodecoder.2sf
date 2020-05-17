/*
 *  Copyright (C) 2014-2020 Arne Morten Kvarving
 *  Copyright (C) 2016-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include <algorithm>
#include <kodi/addon-instance/AudioDecoder.h>
#include <kodi/Filesystem.h>
#include "state.h"
#include "psflib.h"
#include <iostream>
#include <zlib.h>
#include <stdio.h>
#include <stdint.h>

extern "C" {

struct twosf_loader_state
{
  uint8_t * rom;
  uint8_t * state;
  size_t rom_size;
  size_t state_size;

  int initial_frames;
  int sync_type;
  int clockdown;
  int arm9_clockdown_level;
  int arm7_clockdown_level;

  twosf_loader_state()
    : rom(0), state(0), rom_size(0), state_size(0),
    initial_frames(-1), sync_type(0), clockdown(0),
    arm9_clockdown_level(0), arm7_clockdown_level(0)
  {
  }

  ~twosf_loader_state()
  {
    free(rom);
    free(state);
  }
};

struct TSFContext {
  twosf_loader_state state;
  NDS_state emu;
  int64_t len;
  int sample_rate;
  int64_t pos;
  std::string title;
  std::string artist;
  bool inited = false;
};


static void* psf_file_fopen(void* context, const char* path)
{
  kodi::vfs::CFile* file = new kodi::vfs::CFile;
  if (!file->OpenFile(path, 0))
  {
    delete file;
    return nullptr;
  }
  return file;
}

static size_t psf_file_fread( void * buffer, size_t size, size_t count, void * handle )
{
  kodi::vfs::CFile* file = static_cast<kodi::vfs::CFile*>(handle);
  return file->Read(buffer, size*count);
}

static int psf_file_fseek( void * handle, int64_t offset, int whence )
{
  kodi::vfs::CFile* file = static_cast<kodi::vfs::CFile*>(handle);
  return file->Seek(offset, whence) > -1 ? 0 : -1;
}

static int psf_file_fclose( void * handle )
{
  delete static_cast<kodi::vfs::CFile*>(handle);
  return 0;
}

static long psf_file_ftell( void * handle )
{
  kodi::vfs::CFile* file = static_cast<kodi::vfs::CFile*>(handle);
  return file->GetPosition();
}

const psf_file_callbacks psf_file_system =
{
  "\\/",
  nullptr,
  psf_file_fopen,
  psf_file_fread,
  psf_file_fseek,
  psf_file_fclose,
  psf_file_ftell
};

#define BORK_TIME 0xC0CAC01A
static unsigned long parse_time_crap(const char *input)
{
  if (!input) return BORK_TIME;
  int len = strlen(input);
  if (!len) return BORK_TIME;
  int value = 0;
  {
    int i;
    for (i = len - 1; i >= 0; i--)
    {
      if ((input[i] < '0' || input[i] > '9') && input[i] != ':' && input[i] != ',' && input[i] != '.')
      {
        return BORK_TIME;
      }
    }
  }
  std::string foo = input;
  char *bar = (char *) &foo[0];
  char *strs = bar + foo.size() - 1;
  while (strs > bar && (*strs >= '0' && *strs <= '9'))
  {
    strs--;
  }
  if (*strs == '.' || *strs == ',')
  {
    // fraction of a second
    strs++;
    if (strlen(strs) > 3) strs[3] = 0;
    value = atoi(strs);
    switch (strlen(strs))
    {
      case 1:
        value *= 100;
        break;
      case 2:
        value *= 10;
        break;
    }
    strs--;
    *strs = 0;
    strs--;
  }
  while (strs > bar && (*strs >= '0' && *strs <= '9'))
  {
    strs--;
  }
  // seconds
  if (*strs < '0' || *strs > '9') strs++;
  value += atoi(strs) * 1000;
  if (strs > bar)
  {
    strs--;
    *strs = 0;
    strs--;
    while (strs > bar && (*strs >= '0' && *strs <= '9'))
    {
      strs--;
    }
    if (*strs < '0' || *strs > '9') strs++;
    value += atoi(strs) * 60000;
    if (strs > bar)
    {
      strs--;
      *strs = 0;
      strs--;
      while (strs > bar && (*strs >= '0' && *strs <= '9'))
      {
        strs--;
      }
      value += atoi(strs) * 3600000;
    }
  }
  return value;
}

static int psf_info_meta(void* context,
                         const char* name, const char* value)
{
  TSFContext* tsf = (TSFContext*)context;
  if (!strcasecmp(name, "length"))
    tsf->len = parse_time_crap(value);
  if (!strcasecmp(name, "title"))
    tsf->title = value;
  if (!strcasecmp(name, "artist"))
    tsf->artist = value;

  return 0;
}

inline unsigned get_le32( void const* p )
{
    return (unsigned) ((unsigned char const*) p) [3] << 24 |
            (unsigned) ((unsigned char const*) p) [2] << 16 |
            (unsigned) ((unsigned char const*) p) [1] << 8 |
            (unsigned) ((unsigned char const*) p) [0];
}

static int load_twosf_map(struct twosf_loader_state *state, int issave, const unsigned char *udata, unsigned usize)
{
  if (usize < 8) return -1;

  unsigned char *iptr;
  size_t isize;
  unsigned char *xptr;
  unsigned xsize = get_le32(udata + 4);
  unsigned xofs = get_le32(udata + 0);
  if (issave)
  {
    iptr = state->state;
    isize = state->state_size;
    state->state = 0;
    state->state_size = 0;
  }
  else
  {
    iptr = state->rom;
    isize = state->rom_size;
    state->rom = 0;
    state->rom_size = 0;
  }
  if (!iptr)
  {
    size_t rsize = xofs + xsize;
    if (!issave)
    {
      rsize -= 1;
      rsize |= rsize >> 1;
      rsize |= rsize >> 2;
      rsize |= rsize >> 4;
      rsize |= rsize >> 8;
      rsize |= rsize >> 16;
      rsize += 1;
    }
    iptr = (unsigned char *) malloc(rsize + 10);
    if (!iptr)
      return -1;
    memset(iptr, 0, rsize + 10);
    isize = rsize;
  }
  else if (isize < xofs + xsize)
  {
    size_t rsize = xofs + xsize;
    if (!issave)
    {
      rsize -= 1;
      rsize |= rsize >> 1;
      rsize |= rsize >> 2;
      rsize |= rsize >> 4;
      rsize |= rsize >> 8;
      rsize |= rsize >> 16;
      rsize += 1;
    }
    xptr = (unsigned char *) realloc(iptr, xofs + rsize + 10);
    if (!xptr)
    {
      free(iptr);
      return -1;
    }
    iptr = xptr;
    isize = rsize;
  }
  memcpy(iptr + xofs, udata + 8, xsize);
  if (issave)
  {
    state->state = iptr;
    state->state_size = isize;
  }
  else
  {
    state->rom = iptr;
    state->rom_size = isize;
  }
  return 0;
}

static int load_twosf_mapz(struct twosf_loader_state *state, int issave, const unsigned char *zdata, unsigned zsize, unsigned zcrc)
{
  int ret;
  int zerr;
  uLongf usize = 8;
  uLongf rsize = usize;
  unsigned char *udata;
  unsigned char *rdata;

  udata = (unsigned char *) malloc(usize);
  if (!udata)
    return -1;

  while (Z_OK != (zerr = uncompress(udata, &usize, zdata, zsize)))
  {
    if (Z_MEM_ERROR != zerr && Z_BUF_ERROR != zerr)
    {
      free(udata);
      return -1;
    }
    if (usize >= 8)
    {
      usize = get_le32(udata + 4) + 8;
      if (usize < rsize)
      {
        rsize += rsize;
        usize = rsize;
      }
      else
        rsize = usize;
    }
    else
    {
      rsize += rsize;
      usize = rsize;
    }
    rdata = (unsigned char *) realloc(udata, usize);
    if (!rdata)
    {
      free(udata);
      return -1;
    }
    udata = rdata;
  }

  rdata = (unsigned char *) realloc(udata, usize);
  if (!rdata)
  {
    free(udata);
    return -1;
  }

  if (0)
  {
    uLong ccrc = crc32(crc32(0L, Z_NULL, 0), rdata, (uInt) usize);
    if (ccrc != zcrc)
      return -1;
  }

  ret = load_twosf_map(state, issave, rdata, (unsigned) usize);
  free(rdata);
  return ret;
}

static int twosf_loader(void * context, const uint8_t * exe, size_t exe_size,
                        const uint8_t * reserved, size_t reserved_size)
{
  struct twosf_loader_state * state = ( struct twosf_loader_state * ) context;

  if ( exe_size >= 8 )
  {
    if ( load_twosf_map(state, 0, exe, (unsigned) exe_size) )
      return -1;
  }

  if ( reserved_size )
  {
    size_t resv_pos = 0;
    if ( reserved_size < 16 )
      return -1;
    while ( resv_pos + 12 < reserved_size )
    {
      unsigned save_size = get_le32(reserved + resv_pos + 4);
      unsigned save_crc = get_le32(reserved + resv_pos + 8);
      if (get_le32(reserved + resv_pos + 0) == 0x45564153)
      {
        if (resv_pos + 12 + save_size > reserved_size)
          return -1;
        if (load_twosf_mapz(state, 1, reserved + resv_pos + 12, save_size, save_crc))
          return -1;
      }
      resv_pos += 12 + save_size;
    }
  }

  return 0;
}

static int twosf_info(void * context, const char * name, const char * value)
{
  struct twosf_loader_state * state = ( struct twosf_loader_state * ) context;
  char *end;

  if ( !strcasecmp( name, "_frames" ) )
  {
    state->initial_frames = strtol( value, &end, 10 );
  }
  else if ( !strcasecmp( name, "_clockdown" ) )
  {
    state->clockdown = strtol( value, &end, 10 );
  }
  else if ( !strcasecmp( name, "_vio2sf_sync_type") )
  {
    state->sync_type = strtol( value, &end, 10 );
  }
  else if ( !strcasecmp( name, "_vio2sf_arm9_clockdown_level" ) )
  {
    state->arm9_clockdown_level = strtol( value, &end, 10 );
  }
  else if ( !strcasecmp( name, "_vio2sf_arm7_clockdown_level" ) )
  {
    state->arm7_clockdown_level = strtol( value, &end, 10 );
  }

  return 0;
}

static void sf_status(void* context, const char* message)
{
  if (message == nullptr || strlen(message) <= 1)
    return;

  std::string msg = message;
  std::replace(msg.begin(), msg.end(), '\n', '\0');

  kodi::Log(ADDON_LOG_DEBUG, "psf status: %s", msg.c_str());
}

} /* extern "C" */

class ATTRIBUTE_HIDDEN C2SFCodec : public kodi::addon::CInstanceAudioDecoder
{
public:
  C2SFCodec(KODI_HANDLE instance, const std::string& version) :
    CInstanceAudioDecoder(instance, version) { }

  virtual ~C2SFCodec()
  {
    if (ctx.inited)
      state_deinit(&ctx.emu);
  }

  bool Init(const std::string& filename, unsigned int filecache,
            int& channels, int& samplerate,
            int& bitspersample, int64_t& totaltime,
            int& bitrate, AEDataFormat& format,
            std::vector<AEChannel>& channellist) override
  {
    ctx.pos = 0;
    if (psf_load(filename.c_str(), &psf_file_system, 0x24,
                 nullptr, nullptr,
                 psf_info_meta, &ctx, 0,
                 sf_status, nullptr) <= 0)
      return false;

    if (psf_load(filename.c_str(), &psf_file_system, 0x24,
                 twosf_loader, &ctx.state,
                 twosf_info, &ctx.state, 1,
                 sf_status, nullptr) < 0)
      return false;

    ctx.inited = true;
    state_init(&ctx.emu);

    ctx.emu.dwInterpolation = 1;
    ctx.emu.dwChannelMute = 0;
    ctx.emu.arm7_clockdown_level = ctx.state.arm7_clockdown_level;
    ctx.emu.arm9_clockdown_level = ctx.state.arm9_clockdown_level;

    state_setrom(&ctx.emu, ctx.state.rom, ctx.state.rom_size, 0);
    state_loadstate(&ctx.emu, ctx.state.state, ctx.state.state_size);

    totaltime = ctx.len;
    static enum AEChannel map[3] = {
      AE_CH_FL, AE_CH_FR, AE_CH_NULL
    };
    format = AE_FMT_S16NE;
    channellist = { AE_CH_FL, AE_CH_FR };
    channels = 2;

    bitspersample = 16;
    bitrate = 0.0;

    samplerate = ctx.sample_rate = 44100;

    ctx.len = ctx.sample_rate*4*totaltime/1000;

    return true;
  }

  int ReadPCM(uint8_t* buffer, int size, int& actualsize) override
  {
    if (ctx.pos >= ctx.len)
      return 1;
    state_render(&ctx.emu, (int16_t*)buffer, size/4);
    ctx.pos += size;
    actualsize = size;
    return 0;
  }

  int64_t Seek(int64_t time) override
  {
    if (time*ctx.sample_rate*4/1000 < ctx.pos)
    {
      state_setrom(&ctx.emu, ctx.state.rom, ctx.state.rom_size,0);
      state_loadstate(&ctx.emu, ctx.state.state, ctx.state.state_size);
      ctx.pos = 0;
    }

    int64_t left = time*ctx.sample_rate*4/1000-ctx.pos;
    int16_t buf[2048];
    while (left > 4096)
    {
      state_render(&ctx.emu, buf, 1024);
      ctx.pos += 4096;
      left -= 4096;
    }

    return ctx.pos/(ctx.sample_rate*4)*1000;
  }

  bool ReadTag(const std::string& file, std::string& title,
               std::string& artist, int& length) override
  {
    TSFContext tsf;

    if (psf_load(file.c_str(), &psf_file_system, 0x24,
                 nullptr, nullptr,
                 psf_info_meta, &tsf, 0,
                 sf_status, nullptr) <= 0)
    {
      return false;
    }

    title = tsf.title;
    artist = tsf.artist;
    length = tsf.len/1000;

    return true;
  }

private:
  TSFContext ctx;
};


class ATTRIBUTE_HIDDEN CMyAddon : public kodi::addon::CAddonBase
{
public:
  CMyAddon() = default;
  ADDON_STATUS CreateInstance(int instanceType, const std::string& instanceID, KODI_HANDLE instance, const std::string& version, KODI_HANDLE& addonInstance) override
  {
    addonInstance = new C2SFCodec(instance, version);
    return ADDON_STATUS_OK;
  }
  virtual ~CMyAddon() = default;
};


ADDONCREATOR(CMyAddon);
