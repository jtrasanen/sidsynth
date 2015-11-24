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

#ifndef __SID_H__
#define __SID_H__

#include "siddefs.h"
#include "voice.h"
#include "filter.h"
#include "extfilt.h"
#include "pot.h"

class SID
{
public:
    SID();
    ~SID();
    
    void set_chip_model(chip_model model);
	void set_distortion_properties(int enable, int rate, int headroom, int opmin, int opmax);
    void set_filter_cutoff_table(const fc_point* table, int points) { filter.set_filter_cutoff_table(table, points); }
    void enable_filter(bool enable);
    void enable_external_filter(bool enable);
	void set_mute(int voice, bool enable);
    bool set_sampling_parameters(double clock_freq,
                                 double sample_freq, double pass_freq = -1,
                                 double filter_scale = 0.97);
    void adjust_sampling_frequency(double sample_freq);

    void clock();
    int clock(cycle_count& delta_t, short* buf, int n, int interleave = 1);
    void reset();
    
    // Read/write registers.
    reg8 read(reg8 offset);
    void write(reg8 offset, reg8 value);
    
    // Read/write state.
    class State
    {
    public:
        State();
        
        char sid_register[NUM_SID_REGS];
        
        reg8 bus_value;
        cycle_count bus_value_ttl;
        
        reg24 accumulator[NUM_VOICES];
        reg24 shift_register[NUM_VOICES];
        reg16 rate_counter[NUM_VOICES];
        reg16 rate_counter_period[NUM_VOICES];
        reg16 exponential_counter[NUM_VOICES];
        reg16 exponential_counter_period[NUM_VOICES];
        reg8 envelope_counter[NUM_VOICES];
        EnvelopeGenerator::State envelope_state[NUM_VOICES];
        bool hold_zero[NUM_VOICES];
    };
    
    State read_state();
    void write_state(const State& state);
    
    // 16-bit input (EXT IN).
    void input(int sample);
    
    // n-bit output (AUDIO OUT).
    int output(int bits = 16);
    
protected:
    static double I0(double x);
    RESID_INLINE int clock_resample_interpolate(cycle_count& delta_t, short* buf,
                                                int n, int interleave);
    RESID_INLINE int clock_interpolate(cycle_count& delta_t, short* buf,
                                                int n, int interleave);

    Voice voice[NUM_VOICES];
    Filter filter;
    ExternalFilter extfilt;
	BassBoostFilter bassboost;
	TrebleBoostFilter trebleboost;
	FuzzFilter fuzzMain;
	FuzzFilter fuzz[NUM_VOICES];
    Potentiometer potx;
    Potentiometer poty;
	sound_sample	Vo;
	bool	mute[NUM_VOICES];

    // Waveform D/A zero level.
    sound_sample wave_zero;
    
    // Multiplying D/A DC offset.
    sound_sample voice_DC;
    
    reg8 bus_value;
    cycle_count bus_value_ttl;
    
    double clock_frequency;
    
    // External audio input.
    int ext_in;
    
    // Resampling constants.
    // The error in interpolated lookup is bounded by 1.234/L^2,
    // while the error in non-interpolated lookup is bounded by
    // 0.7854/L + 0.4113/L^2, see
    // http://www-ccrma.stanford.edu/~jos/resample/Choice_Table_Size.html
    // For a resolution of 16 bits this yields L >= 285 and L >= 51473,
    // respectively.
    static const int FIR_N = 125;
    static const int FIR_RES_INTERPOLATE = 285;
    static const int FIR_RES_FAST = 51473;
    static const int FIR_SHIFT = 15;
    static const int RINGSIZE = 16384;
    
    // Fixpoint constants (16.16 bits).
    static const int FIXP_SHIFT = 16;
    static const int FIXP_MASK = 0xffff;
    
    // Sampling variables.
    cycle_count cycles_per_sample;
    cycle_count sample_offset;
    int sample_index;
    short sample_prev;
    int fir_N;
    int fir_RES;
    
    // Ring buffer with overflow for contiguous storage of RINGSIZE samples.
    short* sample;
    
    // FIR_RES filter tables (FIR_N*FIR_RES).
    short* fir;
};

#endif // not __SID_H__
