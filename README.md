<p align="center">
  <img src="audiodecoder.2sf/icon.png" />
</p>

------------------

[![License: GPL-2.0-or-later](https://img.shields.io/badge/License-GPL%20v2+-blue.svg)](LICENSE.md)
[![Build and run tests](https://github.com/xbmc/audiodecoder.2sf/actions/workflows/build.yml/badge.svg?branch=Omega)](https://github.com/xbmc/audiodecoder.2sf/actions/workflows/build.yml)
[![Build Status](https://dev.azure.com/teamkodi/binary-addons/_apis/build/status/xbmc.audiodecoder.2sf?branchName=Omega)](https://dev.azure.com/teamkodi/binary-addons/_build/latest?definitionId=3&branchName=Omega)
[![Build Status](https://jenkins.kodi.tv/view/Addons/job/xbmc/job/audiodecoder.2sf/job/Omega/badge/icon)](https://jenkins.kodi.tv/blue/organizations/jenkins/xbmc%2Faudiodecoder.2sf/branches/)
<!--- [![Build Status](https://ci.appveyor.com/api/projects/status/github/xbmc/audiodecoder.2sf?branch=Omega&svg=true)](https://ci.appveyor.com/project/xbmc/audiodecoder-2sf?branch=Omega) -->

# audiodecoder.2sf addon for Kodi

This is a [Kodi](https://kodi.tv) audio decoder addon for Nintendo DS audio (2SF) files.

Dual Screen Sound Format is an audio format based on PSF. It stores audio ripped from the ROMs of Nintendo DS games. Music instructions are stored in files with a .2sflib extension and the music notation is stored in files with a .mini2sf extension.<

## Build instructions

When building the addon you have to use the correct branch depending on which version of Kodi you're building against. 
If you want to build the addon to be compatible with the latest kodi `master` commit, you need to checkout the branch with the current kodi codename.
Also make sure you follow this README from the branch in question.

### Linux

The following instructions assume you will have built Kodi already in the `kodi-build` directory 
suggested by the README.

1. `git clone --branch master https://github.com/xbmc/xbmc.git`
2. `git clone https://github.com/xbmc/audiodecoder.2sf.git`
3. `cd audiodecoder.2sf && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=audiodecoder.2sf -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/kodi-build/addons -DPACKAGE_ZIP=1 ../../xbmc/cmake/addons`
5. `make`

The addon files will be placed in `../../xbmc/kodi-build/addons` so if you build Kodi from source and run it directly 
the addon will be available as a system addon.
