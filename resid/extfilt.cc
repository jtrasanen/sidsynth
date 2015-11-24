//  ---------------------------------------------------------------------------
//  This file is part of reSID, a MOS6581 SID emulator engine.
//  Copyright (C) 2004  Dag Lem <resid@nimrod.no>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  ---------------------------------------------------------------------------

#define __EXTFILT_CC__
#include "extfilt.h"


// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
ExternalFilter::ExternalFilter()
{
    reset();
    enable_filter(true);
    set_chip_model(MOS6581);
    
    // Low-pass:  R = 10kOhm, C = 1000pF; w0l = 1/RC = 1/(1e4*1e-9) = 100000
    // High-pass: R =  1kOhm, C =   10uF; w0h = 1/RC = 1/(1e3*1e-5) =    100
    // Multiply with 1.048576 to facilitate division by 1 000 000 by right-
    // shifting 20 times (2 ^ 20 = 1048576).
    
    w0lp = 104858;
    w0hp = 105;
}


// ----------------------------------------------------------------------------
// Enable filter.
// ----------------------------------------------------------------------------
void ExternalFilter::enable_filter(bool enable)
{
    enabled = enable;
}


// ----------------------------------------------------------------------------
// Set chip model.
// ----------------------------------------------------------------------------
void ExternalFilter::set_chip_model(chip_model model)
{
    if (model == MOS6581) {
        // Maximum mixer DC output level; to be removed if the external
        // filter is turned off: ((wave DC + voice DC)*voices + mixer DC)*volume
        // See voice.cc and filter.cc for an explanation of the values.
        mixer_DC = ((((0x800 - 0x380) + 0x800)*0xff*3 - 0xfff*0xff/18) >> 7)*0x0f;
    }
    else {
        // No DC offsets in the MOS8580.
        mixer_DC = 0;
    }
}


// ----------------------------------------------------------------------------
// SID reset.
// ----------------------------------------------------------------------------
void ExternalFilter::reset()
{
    // State of filter.
    Vlp = 0;
    Vhp = 0;
    Vo = 0;
}




// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
BassBoostFilter::BassBoostFilter()
{
    reset();
}

void BassBoostFilter::writeGAIN_LO(reg8 v)
{
	gain = (gain & 0xff00) | (int)v;
	setupFilter();
}

void BassBoostFilter::writeGAIN_HI(reg8 v)
{
	gain = (gain & 0x00ff) | ((int)v<<8);
	setupFilter();
}

void BassBoostFilter::writeCUTOFF_LO(reg8 v)
{
	cutoff_freq = (cutoff_freq & 0xff00) | (int)v;
	setupFilter();
}

void BassBoostFilter::writeCUTOFF_HI(reg8 v)
{
	cutoff_freq = (cutoff_freq & 0x00ff) | ((int)v<<8);
	setupFilter();
}


#include <math.h>
void BassBoostFilter::setupFilter()
{
	double V0 = pow(10.0, double(gain)/double(1<<FILTER_DECIMAL_BITS) / 20.0);
	H0_per2 = (V0 - 1.0) / 2.0;
    const double pi = 3.1415926535897932385;
	double k = tan(2 * pi * double(cutoff_freq) / 1000000.0);
	a = (k - 1.0) / (k + 1.0);
	//printf("a = %f H0_per2 = %f\n", a, H0_per2);

/*
H0 = V0 - 1
V0 = 10 ^ (G/20)
k = tan(2 * pi * f_c / f_s)
a_boost = (k - 1) / (k + 1)
a_cut = (k - V0) / (k + V0)

G = gain parameter
f_c = cut off frequency parameter
f_s = sampling rate
*/
}

void BassBoostFilter::reset()
{
    // State of filter.
	y1_curr = 0;
	y1_prev = 0;
	x_prev = 0;
	Vo = 0;

	gain = 0<<FILTER_DECIMAL_BITS;
	cutoff_freq = 100;
	setupFilter();
}


// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
TrebleBoostFilter::TrebleBoostFilter()
{
    reset();
}

void TrebleBoostFilter::writeGAIN_LO(reg8 v)
{
	gain = (gain & 0xff00) | (int)v;
	setupFilter();
}

void TrebleBoostFilter::writeGAIN_HI(reg8 v)
{
	gain = (gain & 0x00ff) | ((int)v<<8);
	setupFilter();
}

void TrebleBoostFilter::writeCUTOFF_LO(reg8 v)
{
	cutoff_freq = (cutoff_freq & 0xff00) | (int)v;
	setupFilter();
}

void TrebleBoostFilter::writeCUTOFF_HI(reg8 v)
{
	cutoff_freq = (cutoff_freq & 0x00ff) | ((int)v<<8);
	setupFilter();
}

void TrebleBoostFilter::setupFilter()
{
	double V0 = pow(10.0, double(gain)/double(1<<FILTER_DECIMAL_BITS) / 20.0);
	H0_per2 = (V0 - 1.0) / 2.0;
    const double pi = 3.1415926535897932385;
	double k = tan(2 * pi * double(cutoff_freq) / 1000000.0);
	a = (k - 1.0) / (k + 1.0);
//	printf("a = %f H0_per2 = %f\n", a, H0_per2);

/*
H0 = V0 - 1
V0 = 10 ^ (G/20)
k = tan(2 * pi * f_c / f_s)
a_boost = (k - 1) / (k + 1)
a_cut = (k - V0) / (k + V0)

G = gain parameter
f_c = cut off frequency parameter
f_s = sampling rate
*/
}

void TrebleBoostFilter::reset()
{
    // State of filter.
	y1_curr = 0;
	y1_prev = 0;
	x_prev = 0;
	Vo = 0;

	gain = 0<<FILTER_DECIMAL_BITS;
	cutoff_freq = 100;
	setupFilter();
}


// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
FuzzFilter::FuzzFilter()
{
    reset();
}

void FuzzFilter::writeGAIN_LO(reg8 v)
{
	gain = (gain & 0xff00) | (int)v;
	setupFilter();
}

void FuzzFilter::writeGAIN_HI(reg8 v)
{
	gain = (gain & 0x00ff) | ((int)v<<8);
	setupFilter();
}

void FuzzFilter::writeMULT_LO(reg8 v)
{
	multiplier = (multiplier & 0xff00) | (int)v;
	setupFilter();
}

void FuzzFilter::writeMULT_HI(reg8 v)
{
	multiplier = (multiplier & 0x00ff) | ((int)v<<8);
	setupFilter();
}

void FuzzFilter::writeMIX(reg8 v)
{
	mix = (int)v;
	setupFilter();
}

void FuzzFilter::setupFilter()
{
}

void FuzzFilter::reset()
{
	gain = 0;
	multiplier = 0;
	mix = 0;
    Vo = 0;
	setupFilter();
}
