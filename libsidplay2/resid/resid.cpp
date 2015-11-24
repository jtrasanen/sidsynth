/***************************************************************************
                          c64sid.h  -  ReSid Wrapper for redefining the
                                       filter
                             -------------------
    begin                : Fri Apr 4 2001
    copyright            : (C) 2001 by Simon White
    email                : s_a_white@email.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "config.h"

#if HAVE_EXCEPTIONS
#   include <new>
#endif

#include "resid.h"
#include "resid-emu.h"


char ReSID::m_credit[];

ReSID::ReSID (sidbuilder *builder)
:sidemu(builder),
 m_context(NULL),
 m_phase(EVENT_CLOCK_PHI1),
 m_gain(100),
 m_status(true),
 m_locked(false)
{
    char *p = m_credit;
    m_error = "N/A";

    // Setup credits
    sprintf (p, "ReSID V%s Engine:", VERSION);
    p += strlen (p) + 1;
    strcpy  (p, "\t(C) 1999-2002 Simon White <sidplay2@yahoo.com>");
    p += strlen (p) + 1;
    sprintf (p, "MOS6581 (SID) Emulation (ReSID V%s):", RESID::resid_version_string);
    p += strlen (p) + 1;
    sprintf (p, "\t(C) 1999-2002 Dag Lem <resid@nimrod.no>");
    p += strlen (p) + 1;
    *p = '\0';

	if (!&m_sid)
	{
		m_error  = "RESID ERROR: Unable to create sid object";
		m_status = false;
        return;
	}

    reset (0);
}


ReSID::~ReSID ()
{
}

bool ReSID::set_filter (const sid_filter_t *filter, bool overrideCutoffCurve)
{
    if (overrideCutoffCurve) {
        if (filter == NULL)
        {   // Select default filter
            m_sid.set_filter_cutoff_table(NULL, 0);
        }
        else
        {   // Make sure there are enough filter points and they are legal
            RESID::fc_point fc[0x802];
            int points = filter->points;
            if ((points < 2) || (points > 0x800))
                return false;

            const sid_fc_t *fprev = NULL, *fin = filter->cutoff;
            RESID::fc_point *fout = fc;
            // Last check, make sure they are list in numerical order
            // for both axis
            for(int i=0;i<points;i++,fin++)
            {
                if (fprev)
                    if ((*fprev)[0] >= (*fin)[0])
                        return false;
                fout++;
                (*fout)[0] = (RESID::sound_sample) (*fin)[0];
                (*fout)[1] = (RESID::sound_sample) (*fin)[1];
                fprev      = fin;
            }
            // Updated ReSID interpolate requires we
            // repeat the end points
            (*(fout+1))[0] = (*fout)[0];
            (*(fout+1))[1] = (*fout)[1];
            fc[0][0] = fc[1][0];
            fc[0][1] = fc[1][1];

            m_sid.set_filter_cutoff_table(fc, filter->points + 2);
        }
    }

	if (filter == NULL)
	{
		m_sid.set_distortion_properties(0, 0, 0, -64000, 64000);
	}
	else
	{
		m_sid.set_distortion_properties(
			filter->distortion_enable,
			filter->rate,
			filter->headroom,
			filter->opmin,
			filter->opmax
		);
		int gain;
		gain = filter->bassboost_enable ? filter->bassboost_gain : 0;
		m_sid.write(SIDPLUS_BASSBOOST_GAIN_LO, gain & 0xff);
		m_sid.write(SIDPLUS_BASSBOOST_GAIN_HI, (gain>>8) & 0xff);
		m_sid.write(SIDPLUS_BASSBOOST_CUTOFF_LO, filter->bassboost_cutoff & 0xff);
		m_sid.write(SIDPLUS_BASSBOOST_CUTOFF_HI, (filter->bassboost_cutoff>>8) & 0xff);

		gain = filter->trebleboost_enable ? filter->trebleboost_gain : 0;
		m_sid.write(SIDPLUS_TREBLEBOOST_GAIN_LO, gain & 0xff);
		m_sid.write(SIDPLUS_TREBLEBOOST_GAIN_HI, (gain>>8) & 0xff);
		m_sid.write(SIDPLUS_TREBLEBOOST_CUTOFF_LO, filter->trebleboost_cutoff & 0xff);
		m_sid.write(SIDPLUS_TREBLEBOOST_CUTOFF_HI, (filter->trebleboost_cutoff>>8) & 0xff);

		gain = filter->main_fuzz_enable ? filter->main_fuzz_gain : 0;
		m_sid.write(SIDPLUS_FUZZ_GAIN_LO, gain & 0xff);
		m_sid.write(SIDPLUS_FUZZ_GAIN_HI, (gain>>8) & 0xff);
		m_sid.write(SIDPLUS_FUZZ_MULT_LO, filter->main_fuzz_multiplier & 0xff);
		m_sid.write(SIDPLUS_FUZZ_MULT_HI, (filter->main_fuzz_multiplier>>8) & 0xff);
		m_sid.write(SIDPLUS_FUZZ_MIX, filter->main_fuzz_mix & 0xff);

		for(int i=0;i<NUM_VOICES;i++) {
			gain = filter->fuzz_enable[i] ? filter->fuzz_gain[i] : 0;
			int base = SIDPLUS_EXT_VOICE_BASE + i * SIDPLUS_VOICE_NUM_REGS;
			m_sid.write(base+SIDPLUS_VOICE_FUZZ_GAIN_LO, gain & 0xff);
			m_sid.write(base+SIDPLUS_VOICE_FUZZ_GAIN_HI, (gain>>8) & 0xff);
			m_sid.write(base+SIDPLUS_VOICE_FUZZ_MULT_LO, filter->fuzz_multiplier[i] & 0xff);
			m_sid.write(base+SIDPLUS_VOICE_FUZZ_MULT_HI, (filter->fuzz_multiplier[i]>>8) & 0xff);
			m_sid.write(base+SIDPLUS_VOICE_FUZZ_MIX, filter->fuzz_mix[i] & 0xff);

			for(int h=0;h<NUM_HARMONICS;h++) {
				int reg = base + SIDPLUS_VOICE_HVOL_0 + h;
				int v = filter->harmonics_enable[i] ? filter->harmonic_vol[i][h] : 0;
				m_sid.write(reg, v);
			}

			m_sid.set_mute(i, filter->mute[i]);
		}
	}

    return true;
}

// Standard component options
void ReSID::reset (uint8_t volume)
{
    m_accessClk = 0;
    m_sid.reset ();
    m_sid.write (SID_FILTER_MODE_VOL, volume);
}

uint8_t ReSID::read (uint_least8_t addr)
{
    event_clock_t cycles = m_context->getTime (m_accessClk, m_phase);
    m_accessClk += cycles;
    while(cycles--)
        m_sid.clock ();
    return m_sid.read (addr);
}

void ReSID::write (uint_least8_t addr, uint8_t data)
{
    event_clock_t cycles = m_context->getTime (m_accessClk, m_phase);
    m_accessClk += cycles;
    while(cycles--)
        m_sid.clock ();
    m_sid.write (addr, data);
}

int_least32_t ReSID::output (uint_least8_t bits)
{
    event_clock_t cycles = m_context->getTime (m_accessClk, m_phase);
    m_accessClk += cycles;
    while(cycles--)
        m_sid.clock ();
    return m_sid.output (bits) * m_gain / 100;
}

void ReSID::filter (bool enable)
{
    m_sid.enable_filter (enable);
}

void ReSID::volume (uint_least8_t num, uint_least8_t volume)
{
    // Not yet supported
}
    
void ReSID::mute (uint_least8_t num, bool enable)
{
    //m_sid.mute (num, enable);
}

void ReSID::gain (int_least8_t percent)
{
    // 0 to 99 is loss, 101 - 200 is gain
    m_gain  = percent;
    m_gain += 100;
    if (m_gain > 200)
        m_gain = 200;
}

void ReSID::sampling (uint_least32_t freq)
{
    m_sid.set_sampling_parameters (1000000, freq);
}

// Set execution environment and lock sid to it
bool ReSID::lock (c64env *env)
{
    if (env == NULL)
    {
        if (!m_locked)
            return false;
        m_locked  = false;
        m_context = NULL;
    }
    else
    {
        if (m_locked)
            return false;
        m_locked  = true;
        m_context = &env->context ();
    }
    return true;
}

// Set the emulated SID model
void ReSID::model (sid2_model_t model)
{
    if (model == SID2_MOS8580)
        m_sid.set_chip_model (RESID::MOS8580);
    else
        m_sid.set_chip_model (RESID::MOS6581);
}
