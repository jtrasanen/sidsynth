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

#ifndef __EXTFILT_H__
#define __EXTFILT_H__

#include "siddefs.h"

// ----------------------------------------------------------------------------
// The audio output stage in a Commodore 64 consists of two STC networks,
// a low-pass filter with 3-dB frequency 16kHz followed by a high-pass
// filter with 3-dB frequency 16Hz (the latter provided an audio equipment
// input impedance of 1kOhm).
// The STC networks are connected with a BJT supposedly meant to act as
// a unity gain buffer, which is not really how it works. A more elaborate
// model would include the BJT, however DC circuit analysis yields BJT
// base-emitter and emitter-base impedances sufficiently low to produce
// additional low-pass and high-pass 3dB-frequencies in the order of hundreds
// of kHz. This calls for a sampling frequency of several MHz, which is far
// too high for practical use.
// ----------------------------------------------------------------------------
class ExternalFilter
{
public:
    ExternalFilter();
    
    void enable_filter(bool enable);
    void set_chip_model(chip_model model);
    
    RESID_INLINE void clock(sound_sample Vi);
    void reset();
    
    // Audio output (20 bits).
    RESID_INLINE sound_sample output();
    
protected:
    // Filter enabled.
    bool enabled;
    
    // Maximum mixer DC offset.
    sound_sample mixer_DC;
    
    // State of filters.
    sound_sample Vlp; // lowpass
    sound_sample Vhp; // highpass
    sound_sample Vo;
    
    // Cutoff frequencies.
    sound_sample w0lp;
    sound_sample w0hp;
    
    friend class SID;
};


// ----------------------------------------------------------------------------
// Inline functions.
// The following functions are defined inline because they are called every
// time a sample is calculated.
// ----------------------------------------------------------------------------

#if RESID_INLINING || defined(__EXTFILT_CC__)

// ----------------------------------------------------------------------------
// SID clocking - 1 cycle.
// ----------------------------------------------------------------------------
RESID_INLINE
void ExternalFilter::clock(sound_sample Vi)
{
    // This is handy for testing.
    if (!enabled) {
        // Remove maximum DC level since there is no filter to do it.
        Vlp = Vhp = 0;
        Vo = Vi - mixer_DC;
        return;
    }

	//ASSERT(Vi >= SAMPLE_MIN && Vi <= SAMPLE_MAX);

    // delta_t is converted to seconds given a 1MHz clock by dividing
    // with 1 000 000.
    
    // Calculate filter outputs.
    // Vo  = Vlp - Vhp;
    // Vlp = Vlp + w0lp*(Vi - Vlp)*delta_t;
    // Vhp = Vhp + w0hp*(Vlp - Vhp)*delta_t;
    
    sound_sample dVlp = (w0lp >> 8)*(Vi - Vlp) >> 12;
    sound_sample dVhp = w0hp*(Vlp - Vhp) >> 20;
    Vo = /*clamp*/(Vlp - Vhp);
    Vlp += dVlp;
    Vhp += dVhp;

	//ASSERT(Vo >= SAMPLE_MIN && Vo <= SAMPLE_MAX);
}

// ----------------------------------------------------------------------------
// Audio output (19.5 bits).
// ----------------------------------------------------------------------------
RESID_INLINE
sound_sample ExternalFilter::output()
{
    return Vo;
}

#endif // RESID_INLINING || defined(__EXTFILT_CC__)



/*
y1[n] = a * x[n] + x[n-1] - a * y1[n-1]
y[n] = H0/2 * (x[n] +- y1[n]) + x[n]	//+ for low pass, - for high pass
H0 = V0 - 1
V0 = 10 ^ (G/20)
k = tan(2 * pi * f_c / f_s)
a_boost = (k - 1) / (k + 1)
a_cut = (k - V0) / (k + V0)

G = gain parameter
f_c = cut off frequency parameter
f_s = sampling rate
*/

class BassBoostFilter
{
public:
    BassBoostFilter();

    void writeGAIN_LO(reg8);
    void writeGAIN_HI(reg8);
    void writeCUTOFF_LO(reg8);
    void writeCUTOFF_HI(reg8);

    RESID_INLINE void clock(sound_sample Vi);
    void reset();
    
    // Audio output (20 bits).
    RESID_INLINE sound_sample output();
    
protected:
	void setupFilter();

    // State of filters.
	sound_sample y1_curr, y1_prev, x_prev;
    sound_sample Vo;

	int		gain, cutoff_freq;
	double	H0_per2, a;

    friend class SID;
};

#if RESID_INLINING

// ----------------------------------------------------------------------------
// SID clocking - 1 cycle.
// ----------------------------------------------------------------------------
RESID_INLINE
void BassBoostFilter::clock(sound_sample Vi)
{
    if (!gain) {		//gain = 0 means the filter is disabled
        Vo = Vi;
        return;
    }

	ASSERT(Vi >= SAMPLE_MIN && Vi <= SAMPLE_MAX);
    
    // delta_t is converted to seconds given a 1MHz clock by dividing
    // with 1 000 000.
    
    // Calculate filter outputs.
	sound_sample x_curr = Vi;
	y1_curr = /*clamp*/((sound_sample)(a * (x_curr - y1_prev)) + x_prev);	//TODO get rid of double multiply
	Vo = /*clamp*/((sound_sample)(H0_per2 * (x_curr + y1_curr)) + x_curr);	//TODO get rid of double multiply
	x_prev = x_curr;
	y1_prev = y1_curr;

	//ASSERT(Vo >= SAMPLE_MIN && Vo <= SAMPLE_MAX);
}

RESID_INLINE sound_sample BassBoostFilter::output()
{
    return Vo;
}

#endif // RESID_INLINING

class TrebleBoostFilter
{
public:
    TrebleBoostFilter();

    void writeGAIN_LO(reg8);
    void writeGAIN_HI(reg8);
    void writeCUTOFF_LO(reg8);
    void writeCUTOFF_HI(reg8);

    RESID_INLINE void clock(sound_sample Vi);
    void reset();

    // Audio output (20 bits).
    RESID_INLINE sound_sample output();
    
protected:
	void setupFilter();

    // State of filters.
	sound_sample y1_curr, y1_prev, x_prev;
    sound_sample Vo;

	int		gain, cutoff_freq;
	double	H0_per2, a;

    friend class SID;
};

#if RESID_INLINING

// ----------------------------------------------------------------------------
// SID clocking - 1 cycle.
// ----------------------------------------------------------------------------
RESID_INLINE
void TrebleBoostFilter::clock(sound_sample Vi)
{
    if (!gain) {
        Vo = Vi;
        return;
    }

	//ASSERT(Vi >= SAMPLE_MIN && Vi <= SAMPLE_MAX);
    
    // delta_t is converted to seconds given a 1MHz clock by dividing
    // with 1 000 000.
    
    // Calculate filter outputs.
	sound_sample x_curr = Vi;
	y1_curr = /*clamp*/((sound_sample)(a * (x_curr - y1_prev)) + x_prev);	//TODO get rid of double multiply
	Vo = /*clamp*/((sound_sample)(H0_per2 * (x_curr - y1_curr)) + x_curr);	//TODO get rid of double multiply
	x_prev = x_curr;
	y1_prev = y1_curr;

//	ASSERT(Vo >= SAMPLE_MIN && Vo <= SAMPLE_MAX);
}

RESID_INLINE sound_sample TrebleBoostFilter::output()
{
    return Vo;
}

#endif // RESID_INLINING


/*
q = x * gain / max(abs(x));
z = sign(-q).*(1-exp(sign(-q).*q));
y = mix * z * max(abs(x)) / max(abs(z)) + (1-mix) * x;
y = y * max(abs(x)) / max(abs(y));
*/
class FuzzFilter
{
public:
    FuzzFilter();

    void writeGAIN_LO(reg8);
    void writeGAIN_HI(reg8);
    void writeMULT_LO(reg8);
    void writeMULT_HI(reg8);
    void writeMIX(reg8);

    RESID_INLINE void clock(sound_sample Vi);
    void reset();
    
    // Audio output (20 bits).
    RESID_INLINE sound_sample output();
    
protected:
	void setupFilter();

    sound_sample Vo;

	int	gain, multiplier;
	int mix;
    
    friend class SID;
};

#if RESID_INLINING

// ----------------------------------------------------------------------------
// SID clocking - 1 cycle.
// ----------------------------------------------------------------------------
#include <math.h>
RESID_INLINE
void FuzzFilter::clock(sound_sample Vi)
{
    // This is handy for testing.
    if (!gain) {
        Vo = Vi;
        return;
    }
	//ASSERT(Vi >= SAMPLE_MIN && Vi <= SAMPLE_MAX);

	double q = fabs(double(Vi)) * double(gain) / double(1<<FILTER_DECIMAL_BITS) / double(SAMPLE_MAX);
	double sign = Vi >= 0 ? 1.0 : -1.0;
	double z = sign * (1.0 - exp(-q));
	z *= double(SAMPLE_MAX) * double(multiplier) / double(1<<FILTER_DECIMAL_BITS);

	if (z < SAMPLE_MIN)
		z = SAMPLE_MIN;
	if (z >= SAMPLE_MAX)
		z = SAMPLE_MAX;

	sound_sample zi = /*clamp*/((sound_sample)z);
	Vo = (mix * zi + (256 - mix) * Vi) >> 8;

	//ASSERT(Vo >= SAMPLE_MIN && Vo <= SAMPLE_MAX);

	//TODO riku says different clamping on positive and negative sides gives an interesting sound
}

RESID_INLINE sound_sample FuzzFilter::output()
{
    return Vo;
}

#endif // RESID_INLINING

#endif // not __EXTFILT_H__
