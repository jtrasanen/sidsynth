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

#ifndef __FILTER_H__
#define __FILTER_H__

#include "siddefs.h"


// ----------------------------------------------------------------------------
// The SID filter is modeled with a two-integrator-loop biquadratic filter,
// which has been confirmed by Bob Yannes to be the actual circuit used in
// the SID chip.
//
// Measurements show that excellent emulation of the SID filter is achieved,
// except when high resonance is combined with high sustain levels.
// In this case the SID op-amps are performing less than ideally and are
// causing some peculiar behavior of the SID filter. This however seems to
// have more effect on the overall amplitude than on the color of the sound.
//
// The theory for the filter circuit can be found in "Microelectric Circuits"
// by Adel S. Sedra and Kenneth C. Smith.
// The circuit is modeled based on the explanation found there except that
// an additional inverter is used in the feedback from the bandpass output,
// allowing the summer op-amp to operate in single-ended mode. This yields
// inverted filter outputs with levels independent of Q, which corresponds with
// the results obtained from a real SID.
//
// We have been able to model the summer and the two integrators of the circuit
// to form components of an IIR filter.
// Vhp is the output of the summer, Vbp is the output of the first integrator,
// and Vlp is the output of the second integrator in the filter circuit.
//
// According to Bob Yannes, the active stages of the SID filter are not really
// op-amps. Rather, simple NMOS inverters are used. By biasing an inverter
// into its region of quasi-linear operation using a feedback resistor from
// input to output, a MOS inverter can be made to act like an op-amp for
// small signals centered around the switching threshold.
//
// Qualified guesses at SID filter schematics are depicted below.
//
// SID filter
// ----------
//
//     -----------------------------------------------
//    |                                               |
//    |            ---Rq--                            |
//    |           |       |                           |
//    |  ------------<A]-----R1---------              |
//    | |                               |             |
//    | |                        ---C---|      ---C---|
//    | |                       |       |     |       |
//    |  --R1--    ---R1--      |---Rs--|     |---Rs--|
//    |        |  |       |     |       |     |       |
//     ----R1--|-----[A>--|--R-----[A>--|--R-----[A>--|
//             |          |             |             |
// vi -----R1--           |             |             |
//
//                       vhp           vbp           vlp
//
//
// vi  - input voltage
// vhp - highpass output
// vbp - bandpass output
// vlp - lowpass output
// [A> - op-amp
// R1  - summer resistor
// Rq  - resistor array controlling resonance (4 resistors)
// R   - NMOS FET voltage controlled resistor controlling cutoff frequency
// Rs  - shunt resitor
// C   - capacitor
//
//
//
// SID integrator
// --------------
//
//                                   V+
//
//                                   |
//                                   |
//                              -----|
//                             |     |
//                             | ||--
//                              -||
//                   ---C---     ||->
//                  |       |        |
//                  |---Rs-----------|---- vo
//                  |                |
//                  |            ||--
// vi ----     -----|------------||
//        |   ^     |            ||->
//        |___|     |                |
//        -----     |                |
//          |       |                |
//          |---R2--                 |
//          |
//          R1                       V-
//          |
//          |
//
//          Vw
//
// ----------------------------------------------------------------------------
class Filter
{
public:
    Filter();
    
    void enable_filter(bool enable);
    void set_chip_model(chip_model model);
    
    void set_distortion_properties(int enable, int rate, int headroom, int opmin, int opmax);
    
    RESID_INLINE
    void clock(sound_sample* voice, sound_sample ext_in);
    
    void reset();
    
    // Write registers.
    void writeFC_LO(reg8);
    void writeFC_HI(reg8);
    void writeRES_FILT(reg8);
    void writeMODE_VOL(reg8);
    void writeRES(reg8);
    void writeSIDPLUSFILT(reg8);	//bit 7 = route through filter en, bits 0-6 = voice index
    
    // SID audio output (16 bits).
    sound_sample output();

    void set_filter_cutoff_table(const fc_point* table, int points);
    
protected:
    void set_w0();
    void set_Q();

    sound_sample estimate_distorted_w0(sound_sample);
    
    // Filter enabled.
    bool m_enabled;
    
    // Filter cutoff frequency.
    reg12 m_fc;
    
    // Filter resonance.
    reg8 m_res;
    
    // Selects which inputs to route through filter.
    reg8 m_filt;
    unsigned int m_filt1;
    
    // Switch voice 3 off.
    reg8 m_voice3off;
    
    // Highpass, bandpass, and lowpass filter modes.
    reg8 m_hp_bp_lp;
    
    // Output master volume.
    reg4 m_vol;
    
    // Mixer DC offset.
    sound_sample m_mixer_DC;
    
    // State of filter.
    sound_sample m_Vhp; // highpass
    sound_sample m_Vbp; // bandpass
    sound_sample m_Vlp; // lowpass
    sound_sample m_Vnf; // not filtered
    
    sound_sample m_distortion_enable, m_rate, m_headroom, m_opmin, m_opmax;
    
    // Cutoff frequency, resonance.
    sound_sample m_w0, m_w0_deriv, m_w0_deriv_smoothed;
    
    sound_sample m_1024_div_Q;
    
    // Cutoff frequency tables.
    // FC is an 11 bit register.
    sound_sample m_f0[2048];
    static fc_point f0_points_6581[];
    static fc_point f0_points_8580[];
    fc_point* m_f0_points;
    int m_f0_count;
    
    friend class SID;
};


// ----------------------------------------------------------------------------
// Inline functions.
// The following functions are defined inline because they are called every
// time a sample is calculated.
// ----------------------------------------------------------------------------

#if RESID_INLINING || defined(__FILTER_CC__)

RESID_INLINE
sound_sample Filter::estimate_distorted_w0(sound_sample source)
{
    const double pi = 3.1415926535897932385;

	ASSERT(source >= SAMPLE_MIN && source <= SAMPLE_MAX);
    
    /* smoothly ramp distortion from 0 */
    if (source <= 0)
        source = 0;
    else
        source = source * source / m_rate;        //TODO multiply by 1/rate?
    
    /* This might be better modelled by allowing source go negative. */
    source -= m_headroom * m_rate >> 8;
    
    sound_sample w0_eff = m_w0 + m_w0_deriv_smoothed * source / m_rate;
    
    /* The maximum is not exactly known, but it's probably not much above 18.5
     * kHz because the FET seems to saturate. Similarly, 170 is the bottom
     * limit for R4 (220 for R3) given by a fixed resistor. */
    const sound_sample w0_min_1 = static_cast<sound_sample>(2 * pi * 170 * 1.048576);
    const sound_sample w0_max_1 = static_cast<sound_sample>(2 * pi * 20000 * 1.048576);
    
    /* protect against extreme w0:s */
    if (w0_eff < w0_min_1)
        w0_eff = w0_min_1;
    if (w0_eff > w0_max_1)
        w0_eff = w0_max_1;
    
    return w0_eff;
}


// ----------------------------------------------------------------------------
// SID clocking - 1 cycle.
// ----------------------------------------------------------------------------
RESID_INLINE
void Filter::clock(sound_sample* voice,
                   sound_sample ext_in)
{
	for(int i=0;i<NUM_VOICES;i++) {
		//ASSERT(voice[i] >= SAMPLE_MIN && voice[i] <= SAMPLE_MAX);
	    // Scale each voice down from 20 to 13 bits.
        voice[i] >>= 7;
    }
    ext_in >>= 7;
    
    // NB! Voice 3 is not silenced by voice3off if it is routed through
    // the filter.
    if (m_voice3off && !(m_filt & 0x04)) {
        voice[2] = 0;
    }
    
    // This is handy for testing.
    if (!m_enabled) {
        m_Vnf = ext_in;
        for(int i=0;i<NUM_VOICES;i++)
        	m_Vnf += voice[i];
        m_Vhp = m_Vbp = m_Vlp = 0;
        return;
    }
    
    sound_sample Vi = m_Vnf = 0;
    for(int i=0;i<NUM_VOICES;i++) {
        if (m_filt1 & (1<<i))
            Vi += voice[i];
        else
            m_Vnf += voice[i];
    }
    ((m_filt & 8) ? Vi : m_Vnf) += ext_in;
    
    sound_sample w0_eff = m_distortion_enable ? estimate_distorted_w0(m_Vhp+m_Vbp) : m_w0;
    if (m_w0_deriv_smoothed < m_w0_deriv)
        m_w0_deriv_smoothed++;
    if (m_w0_deriv_smoothed > m_w0_deriv)
        m_w0_deriv_smoothed--;
    
    /* This procedure is no longer appropriate for 8580, because the bandpass
     * phase is flipped in this calculation. It should be mixed in with reverse
     * phase to correct for it. */
    m_Vhp = (-m_Vbp*m_1024_div_Q >> 10);
    if (m_Vhp < m_opmin)
        m_Vhp = m_opmin;
    if (m_Vhp > m_opmax)
        m_Vhp = m_opmax;
    m_Vhp -= m_Vlp + Vi;
    if (m_Vhp < m_opmin)
        m_Vhp = m_opmin;
    if (m_Vhp > m_opmax)
        m_Vhp = m_opmax;
    m_Vlp += w0_eff*m_Vbp >> 20;
    if (m_Vlp < m_opmin)
        m_Vlp = m_opmin;
    if (m_Vlp > m_opmax)
        m_Vlp = m_opmax;
    m_Vbp += w0_eff*m_Vhp >> 20;
    if (m_Vbp < m_opmin)
        m_Vbp = m_opmin;
    if (m_Vbp > m_opmax)
        m_Vbp = m_opmax;
}

// ----------------------------------------------------------------------------
// SID audio output (20 bits).
// ----------------------------------------------------------------------------
RESID_INLINE
sound_sample Filter::output()
{
    // This is handy for testing.
    if (!m_enabled) {
        return /*clamp*/((m_Vnf + m_mixer_DC)*static_cast<sound_sample>(m_vol));
    }
    
    sound_sample Vf = 0;
    if (m_hp_bp_lp & 1)
        Vf += m_Vlp;
    if (m_hp_bp_lp & 2)
        Vf += m_Vbp;
    if (m_hp_bp_lp & 4)
        Vf += m_Vhp;
    
    // Sum non-filtered and filtered output.
    // Multiply the sum with volume.
    return /*clamp*/((m_Vnf + Vf + m_mixer_DC)*static_cast<sound_sample>(m_vol));
}

#endif // RESID_INLINING || defined(__FILTER_CC__)

#endif // not __FILTER_H__
