/*
 *      Copyright (C) 2014 Arne Morten Kvarving
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "xbmc/libXBMC_addon.h"
#include "state.h"
#include "psflib.h"
#include <iostream>
#include <zlib.h>

extern "C" {
#include <stdio.h>
#include <stdint.h>

#include "xbmc/xbmc_audiodec_dll.h"

ADDON::CHelper_libXBMC_addon *XBMC           = NULL;

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!XBMC)
    XBMC = new ADDON::CHelper_libXBMC_addon;

  if (!XBMC->RegisterMe(hdl))
  {
    delete XBMC, XBMC=NULL;
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  return ADDON_STATUS_OK;
}

//-- Stop ---------------------------------------------------------------------
// This dll must cease all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Stop()
{
}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Destroy()
{
  XBMC=NULL;
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
bool ADDON_HasSettings()
{
  return false;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

void ADDON_FreeSettings()
{
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_SetSetting(const char *strSetting, const void* value)
{
  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

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
};

static void * psf_file_fopen( const char * uri )
{
  return XBMC->OpenFile(uri, 0);
}

static size_t psf_file_fread( void * buffer, size_t size, size_t count, void * handle )
{
  return XBMC->ReadFile(handle, buffer, size*count);
}

static int psf_file_fseek( void * handle, int64_t offset, int whence )
{
  return XBMC->SeekFile(handle, offset, whence) > -1?0:-1;
}

static int psf_file_fclose( void * handle )
{
  XBMC->CloseFile(handle);
  return 0;
}

static long psf_file_ftell( void * handle )
{
  return XBMC->GetFilePosition(handle);
}

const psf_file_callbacks psf_file_system =
{
  "\\/",
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

void* Init(const char* strFile, unsigned int filecache, int* channels,
           int* samplerate, int* bitspersample, int64_t* totaltime,
           int* bitrate, AEDataFormat* format, const AEChannel** channelinfo)
{
  TSFContext* result = new TSFContext;
  result->pos = 0;
  if (psf_load(strFile, &psf_file_system, 0x24, 0, 0, psf_info_meta, result, 0) <= 0)
  {
    delete result;
    return NULL;
  }
  if (psf_load(strFile, &psf_file_system, 0x24, twosf_loader, &result->state, twosf_info, &result->state, 1) < 0)
  {
    delete result;
    return NULL;
  }

  state_init(&result->emu);

  result->emu.dwInterpolation = 1;
  result->emu.dwChannelMute = 0;
  result->emu.arm7_clockdown_level = result->state.arm7_clockdown_level;
  result->emu.arm9_clockdown_level = result->state.arm9_clockdown_level;

  state_setrom(&result->emu, result->state.rom, result->state.rom_size);
  state_loadstate(&result->emu, result->state.state, result->state.state_size);
  
  *totaltime = result->len;
  static enum AEChannel map[3] = {
    AE_CH_FL, AE_CH_FR, AE_CH_NULL
  };
  *format = AE_FMT_S16NE;
  *channelinfo = map;
  *channels = 2;

  *bitspersample = 16;
  *bitrate = 0.0;

  *samplerate = result->sample_rate = 44100;

  result->len = result->sample_rate*4*(*totaltime)/1000;

  return result;
}

int ReadPCM(void* context, uint8_t* pBuffer, int size, int *actualsize)
{
  TSFContext* tsf = (TSFContext*)context;
  if (tsf->pos >= tsf->len)
    return 1;
  state_render(&tsf->emu, (int16_t*)pBuffer, size/4);
  tsf->pos += size;
  *actualsize = size;
  return 0;
}

int64_t Seek(void* context, int64_t time)
{
  TSFContext* tsf = (TSFContext*)context;
  if (time*tsf->sample_rate*4/1000 < tsf->pos)
  {
    state_setrom(&tsf->emu, tsf->state.rom, tsf->state.rom_size);
    state_loadstate(&tsf->emu, tsf->state.state, tsf->state.state_size);
    tsf->pos = 0;
  }
  
  int64_t left = time*tsf->sample_rate*4/1000-tsf->pos;
  int16_t buf[2048];
  while (left > 4096)
  {
    state_render(&tsf->emu, buf, 1024);
    tsf->pos += 4096;
    left -= 4096;
  }

  return tsf->pos/(tsf->sample_rate*4)*1000;
}

bool DeInit(void* context)
{
  TSFContext* tsf = (TSFContext*)context;
  state_deinit(&tsf->emu);
  delete tsf;
  return true;
}

bool ReadTag(const char* strFile, char* title, char* artist, int* length)
{
  TSFContext* tsf = new TSFContext;

  if (psf_load(strFile, &psf_file_system, 0x24, 0, 0, psf_info_meta, tsf, 0) <= 0)
  {
    delete tsf;
    return false;
  }

  strcpy(title, tsf->title.c_str());
  strcpy(artist, tsf->artist.c_str());
  *length = tsf->len/1000;

  delete tsf;
  return true;
}

int TrackCount(const char* strFile)
{
  return 1;
}
}
