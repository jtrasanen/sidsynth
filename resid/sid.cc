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

#include "sid.h"
#include <math.h>

// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
SID::SID()
{
    // Initialize pointers.
    sample = 0;
    fir = 0;

	ASSERT(NUM_VOICES >= 3);
    voice[0].set_sync_source(&voice[2]);
    voice[1].set_sync_source(&voice[0]);
    voice[2].set_sync_source(&voice[1]);
    
    set_sampling_parameters(985248, 44100);
    
    bus_value = 0;
    bus_value_ttl = 0;
    
    ext_in = 0;
}


// ----------------------------------------------------------------------------
// Destructor.
// ----------------------------------------------------------------------------
SID::~SID()
{
    delete[] sample;
    delete[] fir;
}


// ----------------------------------------------------------------------------
// Set chip model.
// ----------------------------------------------------------------------------
void SID::set_chip_model(chip_model model)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        voice[i].set_chip_model(model);
    }
    
    filter.set_chip_model(model);
    extfilt.set_chip_model(model);

	//see voice.cc for explanation
    if (model == MOS6581) {
        wave_zero = 0x380;
        voice_DC = 0x800*0xff;
    }
    else {
        wave_zero = 0x800;
        voice_DC = 0;
	}
}

void SID::set_distortion_properties(int a1, int a2, int a3, int a4, int a5)
{
    filter.set_distortion_properties(a1, a2, a3, a4, a5);
}

// ----------------------------------------------------------------------------
// SID reset.
// ----------------------------------------------------------------------------
void SID::reset()
{
    for (int i = 0; i < NUM_VOICES; i++) {
        voice[i].reset();
        fuzz[i].reset();
		mute[i] = false;
    }
    filter.reset();
    extfilt.reset();
	bassboost.reset();
	trebleboost.reset();
    fuzzMain.reset();
	Vo = 0;
    
    bus_value = 0;
    bus_value_ttl = 0;

    ext_in = 0;
}


// ----------------------------------------------------------------------------
// Write 16-bit sample to audio input.
// NB! The caller is responsible for keeping the value within 16 bits.
// Note that to mix in an external audio signal, the signal should be
// resampled to 1MHz first to avoid sampling noise.
// ----------------------------------------------------------------------------
void SID::input(int sample)
{
    // Voice outputs are 20 bits. Scale up to match three voices in order
    // to facilitate simulation of the MOS8580 "digi boost" hardware hack.
    ext_in = (sample << 4)*3;
}

// ----------------------------------------------------------------------------
// Read sample from audio output.
// Both 16-bit and n-bit output is provided.
// ----------------------------------------------------------------------------

int SID::output(int bits)
{
    const int range = 1 << bits;
    const int half = range >> 1;
    int sample = Vo/((4095*255 >> 7)*3*15*2/range);
    if (sample >= half) {
        return half - 1;
    }
    if (sample < -half) {
        return -half;
    }
    return sample;
}


// ----------------------------------------------------------------------------
// Read registers.
//
// Reading a write only register returns the last byte written to any SID
// register. The individual bits in this value start to fade down towards
// zero after a few cycles. All bits reach zero within approximately
// $2000 - $4000 cycles.
// It has been claimed that this fading happens in an orderly fashion, however
// sampling of write only registers reveals that this is not the case.
// NB! This is not correctly modeled.
// The actual use of write only registers has largely been made in the belief
// that all SID registers are readable. To support this belief the read
// would have to be done immediately after a write to the same register
// (remember that an intermediate write to another register would yield that
// value instead). With this in mind we return the last value written to
// any SID register for $2000 cycles without modeling the bit fading.
// ----------------------------------------------------------------------------
reg8 SID::read(reg8 offset)
{
    switch (offset) {
        case 0x19:
            return potx.readPOT();
        case 0x1a:
            return poty.readPOT();
        case 0x1b:
            return voice[2].wave.readOSC();
        case 0x1c:
            return voice[2].envelope.readENV();
        default:
            return bus_value;
    }
}


// ----------------------------------------------------------------------------
// Write registers.
// ----------------------------------------------------------------------------
void SID::write(reg8 offset, reg8 value)
{
    bus_value = value;
    bus_value_ttl = 0x2000;

	ASSERT(NUM_VOICES >= 0);
    switch (offset) {
        case SID_VOICE0_WAVE_FREQ_LO:		voice[0].wave.writeFREQ_LO(value); break;
        case SID_VOICE0_WAVE_FREQ_HI:		voice[0].wave.writeFREQ_HI(value); break;
        case SID_VOICE0_WAVE_PW_LO:			voice[0].wave.writePW_LO(value); break;
        case SID_VOICE0_WAVE_PW_HI:			voice[0].wave.writePW_HI(value); break;
        case SID_VOICE0_CONTROL_REG:		voice[0].writeCONTROL_REG(value); break;
        case SID_VOICE0_ENV_ATTACK_DECAY:	voice[0].envelope.writeATTACK_DECAY(value); break;
        case SID_VOICE0_ENV_SUSTAIN_RELEASE:voice[0].envelope.writeSUSTAIN_RELEASE(value); break;

        case SID_VOICE1_WAVE_FREQ_LO:		voice[1].wave.writeFREQ_LO(value); break;
        case SID_VOICE1_WAVE_FREQ_HI:		voice[1].wave.writeFREQ_HI(value); break;
        case SID_VOICE1_WAVE_PW_LO:			voice[1].wave.writePW_LO(value); break;
        case SID_VOICE1_WAVE_PW_HI:			voice[1].wave.writePW_HI(value); break;
        case SID_VOICE1_CONTROL_REG:		voice[1].writeCONTROL_REG(value); break;
        case SID_VOICE1_ENV_ATTACK_DECAY:	voice[1].envelope.writeATTACK_DECAY(value); break;
        case SID_VOICE1_ENV_SUSTAIN_RELEASE:voice[1].envelope.writeSUSTAIN_RELEASE(value); break;

        case SID_VOICE2_WAVE_FREQ_LO:		voice[2].wave.writeFREQ_LO(value); break;
        case SID_VOICE2_WAVE_FREQ_HI:		voice[2].wave.writeFREQ_HI(value); break;
        case SID_VOICE2_WAVE_PW_LO:			voice[2].wave.writePW_LO(value); break;
        case SID_VOICE2_WAVE_PW_HI:			voice[2].wave.writePW_HI(value); break;
        case SID_VOICE2_CONTROL_REG:		voice[2].writeCONTROL_REG(value); break;
        case SID_VOICE2_ENV_ATTACK_DECAY:	voice[2].envelope.writeATTACK_DECAY(value); break;
        case SID_VOICE2_ENV_SUSTAIN_RELEASE:voice[2].envelope.writeSUSTAIN_RELEASE(value); break;

        case SID_FILTER_FC_LO:				filter.writeFC_LO(value); break;
        case SID_FILTER_FC_HI:				filter.writeFC_HI(value); break;
        case SID_FILTER_RES_FILT:			filter.writeRES_FILT(value); break;
        case SID_FILTER_MODE_VOL:			filter.writeMODE_VOL(value); break;

		//SID+ registers
		case SIDPLUS_BASSBOOST_GAIN_LO:		bassboost.writeGAIN_LO(value); break;
		case SIDPLUS_BASSBOOST_GAIN_HI:		bassboost.writeGAIN_HI(value); break;
		case SIDPLUS_BASSBOOST_CUTOFF_LO:	bassboost.writeCUTOFF_LO(value); break;
		case SIDPLUS_BASSBOOST_CUTOFF_HI:	bassboost.writeCUTOFF_HI(value); break;

		case SIDPLUS_TREBLEBOOST_GAIN_LO:	trebleboost.writeGAIN_LO(value); break;
		case SIDPLUS_TREBLEBOOST_GAIN_HI:	trebleboost.writeGAIN_HI(value); break;
		case SIDPLUS_TREBLEBOOST_CUTOFF_LO:	trebleboost.writeCUTOFF_LO(value); break;
		case SIDPLUS_TREBLEBOOST_CUTOFF_HI:	trebleboost.writeCUTOFF_HI(value); break;

        case SIDPLUS_FILTER_RES:            filter.writeRES(value); break;

		case SIDPLUS_FUZZ_GAIN_LO:          fuzzMain.writeGAIN_LO(value); break;
		case SIDPLUS_FUZZ_GAIN_HI:		fuzzMain.writeGAIN_HI(value); break;
		case SIDPLUS_FUZZ_MULT_LO:		fuzzMain.writeMULT_LO(value); break;
		case SIDPLUS_FUZZ_MULT_HI:		fuzzMain.writeMULT_HI(value); break;
		case SIDPLUS_FUZZ_MIX:			fuzzMain.writeMIX(value); break;

        default:
            break;
    }

    ASSERT(NUM_HARMONICS <= 8);
	if (offset >= SIDPLUS_EXT_VOICE_BASE) {
		int v = (offset - SIDPLUS_EXT_VOICE_BASE) / SIDPLUS_VOICE_NUM_REGS;	//the first three voice regs are aliases for the original SID regs
		int r = (offset - SIDPLUS_EXT_VOICE_BASE) % SIDPLUS_VOICE_NUM_REGS;
		ASSERT(v < NUM_VOICES);

		switch (r) {
        case SIDPLUS_VOICE_WAVE_FREQ_LO:		voice[v].wave.writeFREQ_LO(value); break;
        case SIDPLUS_VOICE_WAVE_FREQ_HI:		voice[v].wave.writeFREQ_HI(value); break;
        case SIDPLUS_VOICE_WAVE_PW_LO:			voice[v].wave.writePW_LO(value); break;
        case SIDPLUS_VOICE_WAVE_PW_HI:			voice[v].wave.writePW_HI(value); break;
        case SIDPLUS_VOICE_CONTROL_REG:			voice[v].writeCONTROL_REG(value); break;
        case SIDPLUS_VOICE_ENV_ATTACK_DECAY:	voice[v].envelope.writeATTACK_DECAY(value); break;
        case SIDPLUS_VOICE_ENV_SUSTAIN_RELEASE:	voice[v].envelope.writeSUSTAIN_RELEASE(value); break;
		case SIDPLUS_VOICE_HVOL_0:				voice[v].wave.writeHVOL_0(value); break;
		case SIDPLUS_VOICE_HVOL_1:				voice[v].wave.writeHVOL_1(value); break;
		case SIDPLUS_VOICE_HVOL_2:				voice[v].wave.writeHVOL_2(value); break;
		case SIDPLUS_VOICE_HVOL_3:				voice[v].wave.writeHVOL_3(value); break;
		case SIDPLUS_VOICE_HVOL_4:				voice[v].wave.writeHVOL_4(value); break;
		case SIDPLUS_VOICE_HVOL_5:				voice[v].wave.writeHVOL_5(value); break;
		case SIDPLUS_VOICE_HVOL_6:				voice[v].wave.writeHVOL_6(value); break;
		case SIDPLUS_VOICE_HVOL_7:				voice[v].wave.writeHVOL_7(value); break;
		case SIDPLUS_VOICE_FUZZ_GAIN_LO:		fuzz[v].writeGAIN_LO(value); break;
		case SIDPLUS_VOICE_FUZZ_GAIN_HI:		fuzz[v].writeGAIN_HI(value); break;
		case SIDPLUS_VOICE_FUZZ_MULT_LO:		fuzz[v].writeMULT_LO(value); break;
		case SIDPLUS_VOICE_FUZZ_MULT_HI:		fuzz[v].writeMULT_HI(value); break;
		case SIDPLUS_VOICE_FUZZ_MIX:			fuzz[v].writeMIX(value); break;
		case SIDPLUS_VOICE_FILT:				filter.writeSIDPLUSFILT((value<<7)|(v&0x7f)); break;
		default: break;
		}
	}
}


// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
SID::State::State()
{
    int i;
    
    for (i = 0; i < NUM_SID_REGS; i++) {
        sid_register[i] = 0;
    }
    
    ASSERT(NUM_VOICES == 3);	//TODO check this function
    bus_value = 0;
    bus_value_ttl = 0;
    
    for (i = 0; i < NUM_VOICES; i++) {
        accumulator[i] = 0;
        shift_register[i] = 0x7ffff8;
        rate_counter[i] = 0;
        rate_counter_period[i] = 9;
        exponential_counter[i] = 0;
        exponential_counter_period[i] = 1;
        envelope_counter[i] = 0;
        envelope_state[i] = EnvelopeGenerator::RELEASE;
        hold_zero[i] = true;
    }
}


// ----------------------------------------------------------------------------
// Read state.
// ----------------------------------------------------------------------------
SID::State SID::read_state()
{
    State state;
    int i, j;
    ASSERT(NUM_VOICES == 3);	//TODO check this function
    for (i = 0, j = 0; i < NUM_VOICES; i++, j += 7) {
        WaveformGenerator& wave = voice[i].wave;
        EnvelopeGenerator& envelope = voice[i].envelope;
        state.sid_register[j + 0] = wave.freq & 0xff;
        state.sid_register[j + 1] = wave.freq >> 8;
        state.sid_register[j + 2] = wave.pw & 0xff;
        state.sid_register[j + 3] = wave.pw >> 8;
        state.sid_register[j + 4] =
        (wave.waveform << 4)
        | (wave.test ? 0x08 : 0)
        | (wave.ring_mod ? 0x04 : 0)
        | (wave.sync ? 0x02 : 0)
        | (envelope.gate ? 0x01 : 0);
        state.sid_register[j + 5] = (envelope.attack << 4) | envelope.decay;
        state.sid_register[j + 6] = (envelope.sustain << 4) | envelope.release;
    }
    
    state.sid_register[j++] = filter.m_fc & 0x007;
    state.sid_register[j++] = filter.m_fc >> 3;
    state.sid_register[j++] = (filter.m_res << 4) | filter.m_filt;
    state.sid_register[j++] =
    (filter.m_voice3off ? 0x80 : 0)
    | (filter.m_hp_bp_lp << 4)
    | filter.m_vol;
    
    // These registers are superfluous, but included for completeness.
    for (; j < 0x1d; j++) {
        state.sid_register[j] = read(j);
    }
    for (; j < NUM_SID_REGS; j++) {
        state.sid_register[j] = 0;
    }
    
    state.bus_value = bus_value;
    state.bus_value_ttl = bus_value_ttl;
    
    for (i = 0; i < NUM_VOICES; i++) {
        state.accumulator[i] = voice[i].wave.accumulator;
        state.shift_register[i] = voice[i].wave.shift_register;
        state.rate_counter[i] = voice[i].envelope.rate_counter;
        state.rate_counter_period[i] = voice[i].envelope.rate_period;
        state.exponential_counter[i] = voice[i].envelope.exponential_counter;
        state.exponential_counter_period[i] = voice[i].envelope.exponential_counter_period;
        state.envelope_counter[i] = voice[i].envelope.envelope_counter;
        state.envelope_state[i] = voice[i].envelope.state;
        state.hold_zero[i] = voice[i].envelope.hold_zero;
    }
    
    return state;
}


// ----------------------------------------------------------------------------
// Write state.
// ----------------------------------------------------------------------------
void SID::write_state(const State& state)
{
    int i;
    ASSERT(NUM_VOICES == 3);	//TODO check this function
    
    for (i = 0; i < NUM_SID_REGS; i++) {
        write(i, state.sid_register[i]);
    }
    
    bus_value = state.bus_value;
    bus_value_ttl = state.bus_value_ttl;
    
    for (i = 0; i < NUM_VOICES; i++) {
        voice[i].wave.accumulator = state.accumulator[i];
        voice[i].wave.shift_register = state.shift_register[i];
        voice[i].envelope.rate_counter = state.rate_counter[i];
        voice[i].envelope.rate_period = state.rate_counter_period[i];
        voice[i].envelope.exponential_counter = state.exponential_counter[i];
        voice[i].envelope.exponential_counter_period = state.exponential_counter_period[i];
        voice[i].envelope.envelope_counter = state.envelope_counter[i];
        voice[i].envelope.state = state.envelope_state[i];
        voice[i].envelope.hold_zero = state.hold_zero[i];
    }
}


// ----------------------------------------------------------------------------
// Enable filter.
// ----------------------------------------------------------------------------
void SID::enable_filter(bool enable)
{
    filter.enable_filter(enable);
}


// ----------------------------------------------------------------------------
// Enable external filter.
// ----------------------------------------------------------------------------
void SID::enable_external_filter(bool enable)
{
    extfilt.enable_filter(enable);
}

// ----------------------------------------------------------------------------
// Enable voice muting
// ----------------------------------------------------------------------------
void SID::set_mute(int voice, bool enable)
{
	ASSERT(voice >= 0 && voice < NUM_VOICES);
	mute[voice] = enable;
}


// ----------------------------------------------------------------------------
// I0() computes the 0th order modified Bessel function of the first kind.
// This function is originally from resample-1.5/filterkit.c by J. O. Smith.
// ----------------------------------------------------------------------------
double SID::I0(double x)
{
    // Max error acceptable in I0.
    const double I0e = 1e-6;
    
    double sum, u, halfx, temp;
    int n;
    
    sum = u = n = 1;
    halfx = x/2.0;
    
    do {
        temp = halfx/n++;
        u *= temp*temp;
        sum += u;
    } while (u >= I0e*sum);
    
    return sum;
}


// ----------------------------------------------------------------------------
// Setting of SID sampling parameters.
//
// Use a clock freqency of 985248Hz for PAL C64, 1022730Hz for NTSC C64.
// The default end of passband frequency is pass_freq = 0.9*sample_freq/2
// for sample frequencies up to ~ 44.1kHz, and 20kHz for higher sample
// frequencies.
//
// For resampling, the ratio between the clock frequency and the sample
// frequency is limited as follows:
//   125*clock_freq/sample_freq < 16384
// E.g. provided a clock frequency of ~ 1MHz, the sample frequency can not
// be set lower than ~ 8kHz. A lower sample frequency would make the
// resampling code overfill its 16k sample ring buffer.
//
// The end of passband frequency is also limited:
//   pass_freq <= 0.9*sample_freq/2

// E.g. for a 44.1kHz sampling rate the end of passband frequency is limited
// to slightly below 20kHz. This constraint ensures that the FIR table is
// not overfilled.
// ----------------------------------------------------------------------------
bool SID::set_sampling_parameters(double clock_freq,
                                  double sample_freq, double pass_freq,
                                  double filter_scale)
{
    // Check resampling constraints.
    // Check whether the sample ring buffer would overfill.
    if (FIR_N*clock_freq/sample_freq >= RINGSIZE) {
        return false;
    }
    
    // The default passband limit is 0.9*sample_freq/2 for sample
    // frequencies below ~ 44.1kHz, and 20kHz for higher sample frequencies.
    if (pass_freq < 0) {
        pass_freq = 20000;
        if (2*pass_freq/sample_freq >= 0.9) {
            pass_freq = 0.9*sample_freq/2;
        }
    }
    // Check whether the FIR table would overfill.
    else if (pass_freq > 0.9*sample_freq/2) {
        return false;
    }
    
    // The filter scaling is only included to avoid clipping, so keep
    // it sane.
    if (filter_scale < 0.9 || filter_scale > 1.0) {
        return false;
    }
    
    clock_frequency = clock_freq;
    
    cycles_per_sample = cycle_count(clock_freq/sample_freq*(1 << FIXP_SHIFT) + 0.5);
    
    sample_offset = 0;
    sample_prev = 0;
    
    const double pi = 3.1415926535897932385;
    
    // 16 bits -> -96dB stopband attenuation.
    const double A = -20*log10(1.0/(1 << 16));
    // A fraction of the bandwidth is allocated to the transition band,
    double dw = (1 - 2*pass_freq/sample_freq)*pi;
    // The cutoff frequency is midway through the transition band.
    double wc = (2*pass_freq/sample_freq + 1)*pi/2;
    
    // For calculation of beta and N see the reference for the kaiserord
    // function in the MATLAB Signal Processing Toolbox:
    // http://www.mathworks.com/access/helpdesk/help/toolbox/signal/kaiserord.html
    const double beta = 0.1102*(A - 8.7);
    const double I0beta = I0(beta);
    
    // The filter order will maximally be 124 with the current constraints.
    // N >= (96.33 - 7.95)/(2.285*0.1*pi) -> N >= 123
    // The filter order is equal to the number of zero crossings, i.e.
    // it should be an even number (sinc is symmetric about x = 0).
    int N = int((A - 7.95)/(2.285*dw) + 0.5);
    N += N & 1;
    
    double f_samples_per_cycle = sample_freq/clock_freq;
    double f_cycles_per_sample = clock_freq/sample_freq;
    
    // The filter length is equal to the filter order + 1.
    // The filter length must be an odd number (sinc is symmetric about x = 0).
    fir_N = int(N*f_cycles_per_sample) + 1;
    fir_N |= 1;
    
    // We clamp the filter table resolution to 2^n, making the fixpoint
    // sample_offset a whole multiple of the filter table resolution.
    
    // [AV] this didn't compile in gcc 4.0
    //int res = method == SAMPLE_RESAMPLE_INTERPOLATE ? FIR_RES_INTERPOLATE : FIR_RES_FAST;
    
    int res = FIR_RES_INTERPOLATE;
    
    int n = (int)ceil(log(res/f_cycles_per_sample)/log(2));
    fir_RES = 1 << n;
    
    // Allocate memory for FIR tables.
    delete[] fir;
    fir = new short[fir_N*fir_RES];
    
    // Calculate fir_RES FIR tables for linear interpolation.
    for (int i = 0; i < fir_RES; i++) {
        int fir_offset = i*fir_N + fir_N/2;
        double j_offset = double(i)/fir_RES;
        // Calculate FIR table. This is the sinc function, weighted by the
        // Kaiser window.
        for (int j = -fir_N/2; j <= fir_N/2; j++) {
            double jx = j - j_offset;
            double wt = wc*jx/f_cycles_per_sample;
            double temp = jx/(fir_N/2);
            double Kaiser =
            fabs(temp) <= 1 ? I0(beta*sqrt(1 - temp*temp))/I0beta : 0;
            double sincwt =
            fabs(wt) >= 1e-6 ? sin(wt)/wt : 1;
            double val =
            (1 << FIR_SHIFT)*filter_scale*f_samples_per_cycle*wc/pi*sincwt*Kaiser;
            fir[fir_offset + j] = short(val + 0.5);
        }
    }
    
    // Allocate sample buffer.
    if (!sample) {
        sample = new short[RINGSIZE*2];
    }
    // Clear sample buffer.
    for (int j = 0; j < RINGSIZE*2; j++) {
        sample[j] = 0;
    }
    sample_index = 0;
    
    return true;
}


// ----------------------------------------------------------------------------
// Adjustment of SID sampling frequency.
//
// In some applications, e.g. a C64 emulator, it can be desirable to
// synchronize sound with a timer source. This is supported by adjustment of
// the SID sampling frequency.
//
// NB! Adjustment of the sampling frequency may lead to noticeable shifts in
// frequency, and should only be used for interactive applications. Note also
// that any adjustment of the sampling frequency will change the
// characteristics of the resampling filter, since the filter is not rebuilt.
// ----------------------------------------------------------------------------
void SID::adjust_sampling_frequency(double sample_freq)
{
    cycles_per_sample =
    cycle_count(clock_frequency/sample_freq*(1 << FIXP_SHIFT) + 0.5);
}

// ----------------------------------------------------------------------------
// SID clocking - 1 cycle.
// ----------------------------------------------------------------------------
void SID::clock()
{
    int i;
    
    // Age bus value.
    if (--bus_value_ttl <= 0) {
        bus_value = 0;
        bus_value_ttl = 0;
    }
    
    // Clock amplitude modulators.
    for (i = 0; i < NUM_VOICES; i++) {
        voice[i].envelope.clock();
    }
    
    // Clock oscillators.
    for (i = 0; i < NUM_VOICES; i++) {
        voice[i].wave.clock();
    }
    
    // Synchronize oscillators.
    for (i = 0; i < NUM_VOICES; i++) {
        voice[i].wave.synchronize();
    }
#if 0   //original SID
    // Clock filter.
	sound_sample s[NUM_VOICES];
    for (i = 0; i < NUM_VOICES; i++) {
		s[i] = voice[i].output();
    }
    filter.clock(s, ext_in);

    // Clock external filter.
    extfilt.clock(filter.output());

	Vo = extfilt.output();
#else
#if 0
    //per voice fuzz
	// Clock fuzz filter
	// fuzz works only with a signal with zero DC offset, so we can't use voice.output (which offsets DC) on 6581
    for (i = 0; i < NUM_VOICES; i++) {
        fuzz[i].clock(/*clamp*/((voice[i].wave.output() - 0x800) * voice[i].envelope.output()));
    }

	//add 6581 DC offset
    //clamp((wave.output() - 0x800) * envelope.output() + (0x800 - wave_zero)*envelope.output() + voice_DC);
	sound_sample s[NUM_VOICES];
	for (i = 0; i < NUM_VOICES; i++) {
		s[i] = /*clamp*/(fuzz[i].output() + (0x800-wave_zero)*voice[i].envelope.output() + voice_DC);
		if (mute[i])
			s[i] = 0;
	}
#else
	sound_sample s[NUM_VOICES];
    for (i = 0; i < NUM_VOICES; i++) {
		s[i] = voice[i].output();
    }
#endif

    // Clock filter.
    filter.clock(s, ext_in);
    
	// Clock bassboost filter
	bassboost.clock(filter.output());

	// Clock trebleboost filter
	trebleboost.clock(bassboost.output());

    fuzzMain.clock(trebleboost.output());

    // Clock external filter.
    extfilt.clock(fuzzMain.output());

	Vo = clamp(extfilt.output());
#endif
}

// ----------------------------------------------------------------------------
// SID clocking with audio sampling.
// Fixpoint arithmetics is used.
//
// The example below shows how to clock the SID a specified amount of cycles
// while producing audio output:
//
// while (delta_t) {
//   bufindex += sid.clock(delta_t, buf + bufindex, buflength - bufindex);
//   write(dsp, buf, bufindex*2);
//   bufindex = 0;
// }
//
// ----------------------------------------------------------------------------
int SID::clock(cycle_count& delta_t, short* buf, int n, int interleave)
{
//    return clock_resample_interpolate(delta_t, buf, n, interleave); //slow
    return clock_interpolate(delta_t, buf, n, interleave);    //fast
}

RESID_INLINE
int SID::clock_interpolate(cycle_count& delta_t, short* buf, int n, int interleave)
{
  int s = 0;
  int i;

  for (;;) {
    cycle_count next_sample_offset = sample_offset + cycles_per_sample;
    cycle_count delta_t_sample = next_sample_offset >> FIXP_SHIFT;
    if (delta_t_sample > delta_t) {
      break;
    }
    if (s >= n) {
      return s;
    }
    for (i = 0; i < delta_t_sample - 1; i++) {
      clock();
    }
    if (i < delta_t_sample) {
      sample_prev = output();
      clock();
    }

    delta_t -= delta_t_sample;
    sample_offset = next_sample_offset & FIXP_MASK;

    short sample_now = output();
    buf[s++*interleave] =
      sample_prev + (sample_offset*(sample_now - sample_prev) >> FIXP_SHIFT);
    sample_prev = sample_now;
  }

  for (i = 0; i < delta_t - 1; i++) {
    clock();
  }
  if (i < delta_t) {
    sample_prev = output();
    clock();
  }
  sample_offset -= delta_t << FIXP_SHIFT;
  delta_t = 0;
  return s;
}

// ----------------------------------------------------------------------------
// SID clocking with audio sampling - cycle based with audio resampling.
//
// This is the theoretically correct (and computationally intensive) audio
// sample generation. The samples are generated by resampling to the specified
// sampling frequency. The work rate is inversely proportional to the
// percentage of the bandwidth allocated to the filter transition band.
//
// This implementation is based on the paper "A Flexible Sampling-Rate
// Conversion Method", by J. O. Smith and P. Gosset, or rather on the
// expanded tutorial on the "Digital Audio Resampling Home Page":
// http://www-ccrma.stanford.edu/~jos/resample/
//
// By building shifted FIR tables with samples according to the
// sampling frequency, this implementation dramatically reduces the
// computational effort in the filter convolutions, without any loss
// of accuracy. The filter convolutions are also vectorizable on
// current hardware.
//
// Further possible optimizations are:
// * An equiripple filter design could yield a lower filter order, see
//   http://www.mwrf.com/Articles/ArticleID/7229/7229.html
// * The Convolution Theorem could be used to bring the complexity of
//   convolution down from O(n*n) to O(n*log(n)) using the Fast Fourier
//   Transform, see http://en.wikipedia.org/wiki/Convolution_theorem
// * Simply resampling in two steps can also yield computational
//   savings, since the transition band will be wider in the first step
//   and the required filter order is thus lower in this step.
//   Laurent Ganier has found the optimal intermediate sampling frequency
//   to be (via derivation of sum of two steps):
//     2 * pass_freq + sqrt [ 2 * pass_freq * orig_sample_freq
//       * (dest_sample_freq - 2 * pass_freq) / dest_sample_freq ]
//
// NB! the result of right shifting negative numbers is really
// implementation dependent in the C++ standard.
// ----------------------------------------------------------------------------
RESID_INLINE
int SID::clock_resample_interpolate(cycle_count& delta_t, short* buf, int n,
                                    int interleave)
{
    int s = 0;
    
    for (;;) {
        cycle_count next_sample_offset = sample_offset + cycles_per_sample;
        cycle_count delta_t_sample = next_sample_offset >> FIXP_SHIFT;
        if (delta_t_sample > delta_t) {
            break;
        }
        if (s >= n) {
            return s;
        }
        for (int i = 0; i < delta_t_sample; i++) {
            clock();
            sample[sample_index] = sample[sample_index + RINGSIZE] = output();
            ++sample_index;
            sample_index &= 0x3fff;
        }
        delta_t -= delta_t_sample;
        sample_offset = next_sample_offset & FIXP_MASK;
        
        int fir_offset = sample_offset*fir_RES >> FIXP_SHIFT;
        int fir_offset_rmd = sample_offset*fir_RES & FIXP_MASK;
        short* fir_start = fir + fir_offset*fir_N;
        short* sample_start = sample + sample_index - fir_N + RINGSIZE;
        
        // Convolution with filter impulse response.
        int v1 = 0;
        for (int j = 0; j < fir_N; j++) {
            v1 += sample_start[j]*fir_start[j];
        }
        
        // Use next FIR table, wrap around to first FIR table using
        // previous sample.
        if (++fir_offset == fir_RES) {
            fir_offset = 0;
            --sample_start;
        }
        fir_start = fir + fir_offset*fir_N;
        
        // Convolution with filter impulse response.
        int v2 = 0;
        for (int j = 0; j < fir_N; j++) {
            v2 += sample_start[j]*fir_start[j];
        }
        
        // Linear interpolation.
        // fir_offset_rmd is equal for all samples, it can thus be factorized out:
        // sum(v1 + rmd*(v2 - v1)) = sum(v1) + rmd*(sum(v2) - sum(v1))
        int v = v1 + (fir_offset_rmd*(v2 - v1) >> FIXP_SHIFT);
        
        v >>= FIR_SHIFT;
        
        // Saturated arithmetics to guard against 16 bit sample overflow.
        const int half = 1 << 15;
        if (v >= half) {
            v = half - 1;
        }
        else if (v < -half) {
            v = -half;
        }
        
        buf[s++*interleave] = v;
    }
    
    for (int i = 0; i < delta_t; i++) {
        clock();
        sample[sample_index] = sample[sample_index + RINGSIZE] = output();
        ++sample_index;
        sample_index &= 0x3fff;
    }
    sample_offset -= delta_t << FIXP_SHIFT;
    delta_t = 0;
    return s;
}
