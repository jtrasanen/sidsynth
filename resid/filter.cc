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

#define __FILTER_CC__
#include "filter.h"
#include "spline.h"

#include <stdio.h>

// Maximum cutoff frequency is specified as
// FCmax = 2.6e-5/C = 2.6e-5/2200e-12 = 11818.
//
// Measurements indicate a cutoff frequency range of approximately
// 220Hz - 18kHz on a MOS6581 fitted with 470pF capacitors. The function
// mapping FC to cutoff frequency has the shape of the tanh function, with
// a discontinuity at FCHI = 0x80.
// In contrast, the MOS8580 almost perfectly corresponds with the
// specification of a linear mapping from 30Hz to 12kHz.
//
// The mappings have been measured by feeding the SID with an external
// signal since the chip itself is incapable of generating waveforms of
// higher fundamental frequency than 4kHz. It is best to use the bandpass
// output at full resonance to pick out the cutoff frequency at any given
// FC setting.
//
// The mapping function is specified with spline interpolation points and
// the function values are retrieved via table lookup.
//
// NB! Cutoff frequency characteristics may vary, we have modeled two
// particular Commodore 64s.

fc_point Filter::f0_points_6581[] =
{
    //  FC      f         FCHI FCLO
    // ----------------------------
    {    0,   220 },   // 0x00      - repeated end point
    {    0,   220 },   // 0x00
    {  128,   230 },   // 0x10
    {  256,   250 },   // 0x20
    {  384,   300 },   // 0x30
    {  512,   420 },   // 0x40
    {  640,   780 },   // 0x50
    {  768,  1600 },   // 0x60
    {  832,  2300 },   // 0x68
    {  896,  3200 },   // 0x70
    {  960,  4300 },   // 0x78
    {  992,  5000 },   // 0x7c
    { 1008,  5400 },   // 0x7e
    { 1016,  5700 },   // 0x7f
    { 1023,  6000 },   // 0x7f 0x07
    { 1023,  6000 },   // 0x7f 0x07 - discontinuity
    { 1024,  4600 },   // 0x80      -
    { 1024,  4600 },   // 0x80
    { 1032,  4800 },   // 0x81
    { 1056,  5300 },   // 0x84
    { 1088,  6000 },   // 0x88
    { 1120,  6600 },   // 0x8c
    { 1152,  7200 },   // 0x90
    { 1280,  9500 },   // 0xa0
    { 1408, 12000 },   // 0xb0
    { 1536, 14500 },   // 0xc0
    { 1664, 16000 },   // 0xd0
    { 1792, 17100 },   // 0xe0
    { 1920, 17700 },   // 0xf0
    { 2047, 18000 },   // 0xff 0x07
    { 2047, 18000 }    // 0xff 0x07 - repeated end point
};

fc_point Filter::f0_points_8580[] =
{
    //  FC      f         FCHI FCLO
    // ----------------------------
    {    0,     0 },   // 0x00      - repeated end point
    {    0,     0 },   // 0x00
    {  128,   800 },   // 0x10
    {  256,  1600 },   // 0x20
    {  384,  2500 },   // 0x30
    {  512,  3300 },   // 0x40
    {  640,  4100 },   // 0x50
    {  768,  4800 },   // 0x60
    {  896,  5600 },   // 0x70
    { 1024,  6500 },   // 0x80
    { 1152,  7500 },   // 0x90
    { 1280,  8400 },   // 0xa0
    { 1408,  9200 },   // 0xb0
    { 1536,  9800 },   // 0xc0
    { 1664, 10500 },   // 0xd0
    { 1792, 11000 },   // 0xe0
    { 1920, 11700 },   // 0xf0
    { 2047, 12500 },   // 0xff 0x07
    { 2047, 12500 }    // 0xff 0x07 - repeated end point
};


// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
Filter::Filter()
{
    m_fc = 0;
    
    m_res = 0;
    
    m_filt = 0;
    
    m_voice3off = 0;
    
    m_hp_bp_lp = 0;
    
    m_vol = 0;
    
    // State of filter.
    m_Vhp = 0;
    m_Vbp = 0;
    m_Vlp = 0;
    m_Vnf = 0;
    
    enable_filter(true);

    // Create mappings from FC to cutoff frequency.
    set_chip_model(MOS6581);
    
    /* no distortion by default. 64000 should be large enough to make no
     * clamping. On the real chip, clamping between 16000 and 32000 is probably
     * right. */
    set_distortion_properties(0, 0, 0, -64000, 64000);

    reset();

    m_w0_deriv_smoothed = 6000;
}


// ----------------------------------------------------------------------------
// Enable filter.
// ----------------------------------------------------------------------------
void Filter::enable_filter(bool enable)
{
    m_enabled = enable;
}


// ----------------------------------------------------------------------------
// Set chip model.
// ----------------------------------------------------------------------------
void Filter::set_chip_model(chip_model model)
{
    if (model == MOS6581) {
        // The mixer has a small input DC offset. This is found as follows:
        //
        // The "zero" output level of the mixer measured on the SID audio
        // output pin is 5.50V at zero volume, and 5.44 at full
        // volume. This yields a DC offset of (5.44V - 5.50V) = -0.06V.
        //
        // The DC offset is thus -0.06V/1.05V ~ -1/18 of the dynamic range
        // of one voice. See voice.cc for measurement of the dynamic
        // range.
        
        m_mixer_DC = -0xfff*0xff/18 >> 7;
        
        m_f0_points = f0_points_6581;
        m_f0_count = sizeof(f0_points_6581)/sizeof(*f0_points_6581);
    }
    else {
        // No DC offsets in the MOS8580.
        m_mixer_DC = 0;
        
        m_f0_points = f0_points_8580;
        m_f0_count = sizeof(f0_points_8580)/sizeof(*f0_points_8580);
    }

    set_filter_cutoff_table(m_f0_points, m_f0_count);
    
    set_w0();
    set_Q();
}

void Filter::set_distortion_properties(int enable, int rate, int headroom, int opmin, int opmax)
{
    m_distortion_enable = enable;
    m_rate = rate;
    m_headroom = headroom;
    m_opmin = opmin;
    m_opmax = opmax;

#if 0
     printf("distortion: %d, %d, %d, %d, %d\n",
        m_distortion_enable,
        m_rate,
        m_headroom,
        m_opmin,
        m_opmax
     );
#endif
}

void Filter::set_filter_cutoff_table(const fc_point* table, int points)
{
    const fc_point* f0 = table;
    if (!f0) {
        f0 = m_f0_points;
        points = m_f0_count;
    }
    interpolate (f0, f0 + (points - 1), PointPlotter<sound_sample>(m_f0), 1.0);
//    for(int i=0;i<2048;i++) printf("point %d = %d\n", i, m_f0[i]);
}

// ----------------------------------------------------------------------------
// SID reset.
// ----------------------------------------------------------------------------
void Filter::reset()
{
    m_fc = 0;
    
    m_res = 0;
    
    m_filt = 0;
	m_filt1 = 0;
    
    m_voice3off = 0;
    
    m_hp_bp_lp = 0;
    
    m_vol = 0;
    
    // State of filter.
    m_Vhp = 0;
    m_Vbp = 0;
    m_Vlp = 0;
    m_Vnf = 0;
    
    set_w0();
    set_Q();
    m_w0_deriv_smoothed = 0;
}


// ----------------------------------------------------------------------------
// Register functions.
// ----------------------------------------------------------------------------
void Filter::writeFC_LO(reg8 fc_lo)
{
    m_fc = (m_fc & 0x7f8) | (fc_lo & 0x007);
    set_w0();
}

void Filter::writeFC_HI(reg8 fc_hi)
{
    m_fc = ((fc_hi << 3) & 0x7f8) | (m_fc & 0x007);
    set_w0();
}

void Filter::writeRES_FILT(reg8 res_filt)
{
    m_res = (res_filt >> 4) & 0x0f;
    set_Q();
    m_filt = res_filt & 0x0f;
	m_filt1 &= ~7;
	m_filt1 |= res_filt & 7;
}

void Filter::writeMODE_VOL(reg8 mode_vol)
{
    m_voice3off = mode_vol & 0x80;
    
    m_hp_bp_lp = (mode_vol >> 4) & 0x07;
    
    m_vol = mode_vol & 0x0f;
}

void Filter::writeRES(reg8 res)
{
    m_res = (res >> 4) & 0x0f;
    set_Q();
}

void Filter::writeSIDPLUSFILT(reg8 filt)
{
	int voice = filt & 0x7f;
	int value = (filt >> 7) & 1;
	ASSERT(voice < 32);		//currently space for 32 voices due to filt1 being uint
	if (value)
		m_filt1 |= 1<<voice;
	else
		m_filt1 &= ~(1<<voice);
}

//TODO tabulate w0, w0_deriv (2048 entries), _1024_div_Q (16 entries)
//TODO what's the range?

// Set filter cutoff frequency.
void Filter::set_w0()
{
    const double pi = 3.1415926535897932385;

    ASSERT(m_fc >= 0 && m_fc < 2048);
    
    // Multiply with 1.048576 to facilitate division by 1 000 000 by right-
    // shifting 20 times (2 ^ 20 = 1048576).
    m_w0 = static_cast<sound_sample>(2*pi*m_f0[m_fc]*1.048576);
    // Limit w0 to 4kHz to keep delta_t cycle filter stable.
    const sound_sample w0_max_dt = static_cast<sound_sample>(2*pi*4000*1.048576);
    m_w0 = (m_w0 <= w0_max_dt) ? m_w0 : w0_max_dt;   //TODO not needed for cycle accurate emulation?

    /* the derivate is used to scale the distortion factor, because the
     * true shape of the distortion is related to the fc curve itself. By and
     * large the distortion acts as if the FC value itself was changed. */
    int fc_min = (int) m_fc - 64;
    int fc_max = (int) m_fc + 64;
    if (fc_min < 0)
        fc_min = 0;
    if (fc_max > 2047)
        fc_max = 2047;
    
    /* If this estimate passes the 1023->1024 kink, cancel it out to better
     * estimate the derivate. The other kinks seem too small to bother with.
     * This should be the right thing to do because the DAC distortion does not
     * affect the filter distortion; the DAC merely serves as a bias. */
    int kinkfix = 0;
    if (fc_min <= 1023 && fc_max >= 1024)
        kinkfix = m_f0[1024] - m_f0[1023];
    
    m_w0_deriv = (m_f0[fc_max] - m_f0[fc_min] - kinkfix) * 256 / (fc_max - fc_min);
}

// Set filter resonance.
void Filter::set_Q()
{
    // Q is controlled linearly by res. Q has approximate range [0.707, 1.7].
    // As resonance is increased, the filter must be clocked more often to keep
    // stable.
    
    // The coefficient 1024 is dispensed of later by right-shifting 10 times
    // (2 ^ 10 = 1024).
    ASSERT(m_res >= 0 && m_res < 16);
    m_1024_div_Q = static_cast<sound_sample>(1024.0/(0.707 + 1.0*m_res/0x0f));
}
