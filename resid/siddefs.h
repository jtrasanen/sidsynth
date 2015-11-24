//  ---------------------------------------------------------------------------
//  This file is part of reSID, a MOS6581 SID emulator engine.
//  Copyright (C) 1999  Dag Lem <resid@nimrod.no>
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

#ifndef __SIDDEFS_H__
#define __SIDDEFS_H__

// Define bool, true, and false for C++ compilers that lack these keywords.
#define RESID_HAVE_BOOL 1

#if !RESID_HAVE_BOOL
typedef int bool;
const bool true = 1;
const bool false = 0;
#endif

// We could have used the smallest possible data type for each SID register,
// however this would give a slower engine because of data type conversions.
// An int is assumed to be at least 32 bits (necessary in the types reg24,
// cycle_count, and sound_sample). GNU does not support 16-bit machines
// (GNU Coding Standards: Portability between CPUs), so this should be
// a valid assumption.

typedef unsigned int reg4;
typedef unsigned int reg8;
typedef unsigned int reg12;
typedef unsigned int reg16;
typedef unsigned int reg24;

typedef int cycle_count;
typedef int sound_sample;
typedef sound_sample fc_point[2];

const int NUM_VOICES = 8;

const int NUM_HARMONICS = 8;

enum SidRegs
{
	SID_VOICE0_WAVE_FREQ_LO			= 0x00,
	SID_VOICE0_WAVE_FREQ_HI			= 0x01,
	SID_VOICE0_WAVE_PW_LO			= 0x02,
	SID_VOICE0_WAVE_PW_HI			= 0x03,
	SID_VOICE0_CONTROL_REG			= 0x04,
	SID_VOICE0_ENV_ATTACK_DECAY		= 0x05,
	SID_VOICE0_ENV_SUSTAIN_RELEASE	= 0x06,

	SID_VOICE1_WAVE_FREQ_LO			= 0x07,
	SID_VOICE1_WAVE_FREQ_HI			= 0x08,
	SID_VOICE1_WAVE_PW_LO			= 0x09,
	SID_VOICE1_WAVE_PW_HI			= 0x0a,
	SID_VOICE1_CONTROL_REG			= 0x0b,
	SID_VOICE1_ENV_ATTACK_DECAY		= 0x0c,
	SID_VOICE1_ENV_SUSTAIN_RELEASE	= 0x0d,

	SID_VOICE2_WAVE_FREQ_LO			= 0x0e,
	SID_VOICE2_WAVE_FREQ_HI			= 0x0f,
	SID_VOICE2_WAVE_PW_LO			= 0x10,
	SID_VOICE2_WAVE_PW_HI			= 0x11,
	SID_VOICE2_CONTROL_REG			= 0x12,
	SID_VOICE2_ENV_ATTACK_DECAY		= 0x13,
	SID_VOICE2_ENV_SUSTAIN_RELEASE	= 0x14,

	SID_FILTER_FC_LO				= 0x15,
	SID_FILTER_FC_HI				= 0x16,
	SID_FILTER_RES_FILT				= 0x17,
	SID_FILTER_MODE_VOL				= 0x18,

	SIDPLUS_BASSBOOST_GAIN_LO		= 0x19,
	SIDPLUS_BASSBOOST_GAIN_HI		= 0x1a,
	SIDPLUS_BASSBOOST_CUTOFF_LO		= 0x1b,
	SIDPLUS_BASSBOOST_CUTOFF_HI		= 0x1c,

	SIDPLUS_TREBLEBOOST_GAIN_LO		= 0x1d,
	SIDPLUS_TREBLEBOOST_GAIN_HI		= 0x1e,
	SIDPLUS_TREBLEBOOST_CUTOFF_LO	= 0x1f,
	SIDPLUS_TREBLEBOOST_CUTOFF_HI	= 0x20,

	SIDPLUS_FILTER_RES				= 0x21, //helper reg for writing just resonance

	SIDPLUS_FUZZ_GAIN_LO            = 0x22,
	SIDPLUS_FUZZ_GAIN_HI            = 0x23,
	SIDPLUS_FUZZ_MULT_LO            = 0x24,
	SIDPLUS_FUZZ_MULT_HI            = 0x25,
	SIDPLUS_FUZZ_MIX                = 0x26,

	SIDPLUS_EXT_VOICE_BASE			= 0x40,
};

enum SidPlusVoiceRegs
{
	SIDPLUS_VOICE_WAVE_FREQ_LO			= 0x00,
	SIDPLUS_VOICE_WAVE_FREQ_HI			= 0x01,
	SIDPLUS_VOICE_WAVE_PW_LO			= 0x02,
	SIDPLUS_VOICE_WAVE_PW_HI			= 0x03,
	SIDPLUS_VOICE_CONTROL_REG			= 0x04,
	SIDPLUS_VOICE_ENV_ATTACK_DECAY		= 0x05,
	SIDPLUS_VOICE_ENV_SUSTAIN_RELEASE	= 0x06,
	SIDPLUS_VOICE_HVOL_0				= 0x07,
	SIDPLUS_VOICE_HVOL_1				= 0x08,
	SIDPLUS_VOICE_HVOL_2				= 0x09,
	SIDPLUS_VOICE_HVOL_3				= 0x0a,
	SIDPLUS_VOICE_HVOL_4				= 0x0b,
	SIDPLUS_VOICE_HVOL_5				= 0x0c,
	SIDPLUS_VOICE_HVOL_6				= 0x0d,
	SIDPLUS_VOICE_HVOL_7				= 0x0e,
	SIDPLUS_VOICE_FUZZ_GAIN_LO          = 0x0f,
	SIDPLUS_VOICE_FUZZ_GAIN_HI          = 0x10,
	SIDPLUS_VOICE_FUZZ_MULT_LO          = 0x11,
	SIDPLUS_VOICE_FUZZ_MULT_HI          = 0x12,
	SIDPLUS_VOICE_FUZZ_MIX              = 0x13,
	SIDPLUS_VOICE_FILT					= 0x14,
	SIDPLUS_VOICE_NUM_REGS				= 0x20,
};

const int NUM_SID_REGS = SIDPLUS_EXT_VOICE_BASE + NUM_VOICES*SIDPLUS_VOICE_NUM_REGS;		//originally 0x20


const int SAMPLE_BITS = 20;
const int SAMPLE_MIN = -(1<<SAMPLE_BITS);
const int SAMPLE_MAX = (1<<SAMPLE_BITS)-1;

const int FILTER_DECIMAL_BITS	= 8;

enum chip_model { MOS6581, MOS8580 };

extern "C"
{
#ifndef __VERSION_CC__
    extern const char* resid_version_string;
#else
    const char* resid_version_string = "0.16"; //VERSION;
#endif
}

// Inlining on/off.
#define RESID_INLINING 1
#define RESID_INLINE inline

#if 1
RESID_INLINE sound_sample clamp(sound_sample s)
{
	if (s < SAMPLE_MIN)
		s = SAMPLE_MIN;
	else if (s > SAMPLE_MAX)
		s = SAMPLE_MAX;
	return s;
}

#include <stdio.h>
#define ASSERT(A)   { if (!(A)) { printf("assertion failed: "#A " at %s:%d\n", __FILE__, __LINE__); int* p = 0; *p = 0; } }

#else

RESID_INLINE sound_sample clamp(sound_sample s)
{
	return s;
}

#include <stdio.h>
#define ASSERT(A)
#endif

#endif // not __SIDDEFS_H__
