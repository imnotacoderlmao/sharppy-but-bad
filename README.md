# SHARPpy

###### Sounding/Hodograph Analysis and Research Program in ~~Python~~ C++ since i wanted to keep the qt

[![Anaconda-Server Badge](https://anaconda.org/conda-forge/sharppy/badges/license.svg)](https://anaconda.org/conda-forge/sharppy)

SHARPpy is a collection of open source sounding and hodograph analysis routines, a sounding plotting package, and an interactive, __cross-platform__ application for analyzing real-time soundings (almost) all ~~written in Python~~ ported into C++. It was developed to provide the atmospheric science community a free and consistent source of sounding analysis routines. SHARPpy is constantly updated and vetted by professional meteorologists and climatologists within the scientific community to help maintain a standard source of sounding routines.

The version of SHARPpy in this repository allows users to access and visualize data from several sources, although some are missing (for now)

### Important links:
* HTML Documentation: http://sharppy.github.io/SHARPpy/index.html
* GitHub repository: https://github.com/sharppy/SHARPpy

**Table of Contents**

- [Install SHARPpy](#install-sharppy)
- [SHARPpy Development Team](#original-sharppy-development-team)

## Install SHARPpy

Two build systems are here, XMake or CMake. XMake is a little more user friendly due to sorting dependencies by itself, whilst cmake is more popular.

If successful, a window will open which will give you access to soundings ~~from NUCAPS, RAOBS, and select models.  For instructions on using SHARPpy, see the “Display NUCAPS in SHARPpy” quick guide.~~

### How to run SHARPpy after compilation

run the executable on $PROJECT_ROOT_DIR/build/platform/arch/mode/
you are free to move it elsewhere as long as the data/ folder is preserved for SARS, pwv and non spc database lookups

## Original SHARPpy Development Team
<sup>[[Return to Top]](#sharppy)</sup>

SHARPpy is currently managed by the following co-developers (in no particular order):
- Patrick Marsh (SPC)
- Kelton Halbert (UW-Madison)
- Greg Blumberg (NASA GSFC)
- Tim Supinie (OU School of Meteorology)
- Rebekah Esmaili (Science and Technology Corp.)
- Jeff Szkodzinski (Science and Technology Corp.)
