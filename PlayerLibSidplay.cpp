/*
 *
 * Copyright (c) 2005, Andreas Varga <sid@galway.c64.org>
 * All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// carbon headers
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

// module headers
#include "AudioDriver.h"

// local module header
#include "PlayerLibSidplay.h"


unsigned char sid_registers[ 0x19 ];

const char*	PlayerLibSidplay::sChipModel6581 = "MOS 6581";
const char*	PlayerLibSidplay::sChipModel8580 = "MOS 8580";
const char*	PlayerLibSidplay::sChipModelUnknown = "Unknown";
const char*	PlayerLibSidplay::sChipModelUnspecified = "Unspecified";

const int sDefault8580PointCount = 31;
static const sid_fc_t sDefault8580[sDefault8580PointCount] =
{
	{ 0, 0        },
	{ 64, 400	  },
	{ 128, 800    },
	{ 192, 1200	  },
	{ 256, 1600   },
	{ 384, 2450	  },
	{ 512, 3300   },
	{ 576, 3700   },
	{ 640, 4100   },
	{ 704, 4450   },
	{ 768, 4800   },
	{ 832, 5200   },
	{ 896, 5600   },
	{ 960, 6050   },
	{ 1024, 6500  },
	{ 1088, 7000  },
	{ 1152, 7500  },
	{ 1216, 7950  },
	{ 1280, 8400  },
	{ 1344, 8800  },
	{ 1408, 9200  },
	{ 1472, 9500  },
	{ 1536, 9800  },
	{ 1600, 10150 },
	{ 1664, 10500 },
	{ 1728, 10750 },
	{ 1792, 11000 },
	{ 1856, 11350 },
	{ 1920, 11700 },
	{ 1984, 12100 },
	{ 2047, 12500 }
};



// ----------------------------------------------------------------------------
PlayerLibSidplay::PlayerLibSidplay() :
// ----------------------------------------------------------------------------
	mSidEmuEngine(NULL),
	mSidTune(NULL),
	mBuilder(NULL),
	mAudioDriver(NULL),
	mTuneLength(0),
	mCurrentSubtune(0),
	mSubtuneCount(0),
	mDefaultSubtune(0),
	mCurrentTempo(50),
	mPreviousOversamplingFactor(0),
	mOversamplingBuffer(NULL),
    m_sid(NULL),
    m_regWritePut(0),
    m_regWriteGet(0),
    m_currentInstrument(0)
{
    memset(m_instruments, 0, MAX_INSTRUMENTS*sizeof(Instrument));

    memset(m_keyPressed, 0, NUM_VOICES*sizeof(int));
    memset(m_keyPlaying, 0, NUM_VOICES*sizeof(int));
    memset(m_keyClocks, 0, NUM_VOICES*sizeof(int));
    memset(m_keyFreq, 0, NUM_VOICES*sizeof(int));
    memset(m_keyVelocity, 0, NUM_VOICES*sizeof(int));
    memset(m_keyReleased, 0, NUM_VOICES*sizeof(int));
    memset(m_keyReleasedClocks, 0, NUM_VOICES*sizeof(int));
}


// ----------------------------------------------------------------------------
PlayerLibSidplay::~PlayerLibSidplay()
// ----------------------------------------------------------------------------
{
	if (mSidTune)
	{
		delete mSidTune;
		mSidTune = NULL;
	}
	
	if (mBuilder)
	{
		delete mBuilder;
		mBuilder = NULL;
	}
	
	if (mSidEmuEngine)
	{
		delete mSidEmuEngine;
		mSidEmuEngine = NULL;
	}
}


// ----------------------------------------------------------------------------
void PlayerLibSidplay::setAudioDriver(AudioDriver* audioDriver)
// ----------------------------------------------------------------------------
{
	mAudioDriver = audioDriver;
}

// ----------------------------------------------------------------------------
static inline float approximate_dac(int x, float kinkiness)
// ----------------------------------------------------------------------------
{
    float bits = 0.0f;
    for (int i = 0; i < 11; i += 1)
        if (x & (1 << i))
            bits += pow(i, 4) / pow(10, 4);

    return x * (1.0f + bits * kinkiness);
}

// ----------------------------------------------------------------------------
void PlayerLibSidplay::setupSIDInfo()
// ----------------------------------------------------------------------------
{
	if (mSidTune == NULL)
		return;
	
	mSidTune->getInfo(mTuneInfo);
	mCurrentSubtune = mTuneInfo.currentSong;
	mSubtuneCount = mTuneInfo.songs;
	mDefaultSubtune = mTuneInfo.startSong;

	if (getCurrentChipModel() == sChipModel8580)
	{
        printf("8580\n");
        mFilterSettings.distortion_enable = true;
        mFilterSettings.rate = 3200;
        mFilterSettings.headroom = 235;
		mFilterSettings.opmin = -99999;
		mFilterSettings.opmax = 99999;

		mFilterSettings.points = sDefault8580PointCount;
		memcpy(mFilterSettings.cutoff, sDefault8580, sizeof(sDefault8580));
		mBuilder->set_filter((sid_filter_t*)NULL, mPlaybackSettings.mOverrideCutoffCurve);
	}
	else
	{
        printf("6581\n");
        mFilterSettings.distortion_enable = true;
        mFilterSettings.rate = 1500;
        mFilterSettings.headroom = 300;
		mFilterSettings.opmin = -20000;
		mFilterSettings.opmax = 20000;

        float filterKinkiness = 0.17f;
        float filterBaseLevel = 210.0f;
        float filterOffset = -375.0f;
        float filterSteepness = 120.0f;
        float filterRolloff = 5.5f;

		mFilterSettings.points = 0x800;
		for (int i = 0; i < 0x800; i++)
		{
			float i_kinked = approximate_dac(i, filterKinkiness);
			float freq = filterBaseLevel + powf(2.0f, (i_kinked - filterOffset) / filterSteepness);

			// Better expression for this required.
			// As it stands, it's kinda embarrassing.
			for (float j = 1000.f; j < 18500.f; j += 500.f)
			{
				if (freq > j)
					freq -= (freq - j) / filterRolloff;
			}
			if (freq > 18500)
				freq = 18500;

			mFilterSettings.cutoff[i][0] = i;
			mFilterSettings.cutoff[i][1] = freq;
		}

		mBuilder->set_filter(&mFilterSettings, mPlaybackSettings.mOverrideCutoffCurve);
	}
}


// ----------------------------------------------------------------------------
void PlayerLibSidplay::initEmuEngine(PlaybackSettings *settings)
// ----------------------------------------------------------------------------
{
	if (mSidEmuEngine == NULL )
		mSidEmuEngine = new sidplay2;

	if (mBuilder == NULL)
		mBuilder = new ReSIDBuilder("resid");
	
	if (mAudioDriver)
		settings->mFrequency = mAudioDriver->getSampleRate();
		
	mPlaybackSettings = *settings;

	sid2_config_t cfg = mSidEmuEngine->config();
	
	if (mPlaybackSettings.mClockSpeed == 0)
	{
		cfg.clockSpeed    = SID2_CLOCK_PAL;
		cfg.clockDefault  = SID2_CLOCK_PAL;
	}
	else
	{
		cfg.clockSpeed    = SID2_CLOCK_NTSC;
		cfg.clockDefault  = SID2_CLOCK_NTSC;
	}

	cfg.clockForced   = true;
	
	cfg.environment   = sid2_envR;
	cfg.playback	  = sid2_mono;
	cfg.precision     = mPlaybackSettings.mBits;
	cfg.frequency	  = mPlaybackSettings.mFrequency * mPlaybackSettings.mOversampling;
	cfg.forceDualSids = false;
	cfg.emulateStereo = false;

	if (mPlaybackSettings.mSidModel == 0)
		cfg.sidDefault	  = SID2_MOS6581;
	else
		cfg.sidDefault	  = SID2_MOS8580;

	if (mPlaybackSettings.mForceSidModel)
		cfg.sidModel = cfg.sidDefault;

	cfg.sidEmulation  = mBuilder;
	cfg.sidSamples	  = true;
//	cfg.sampleFormat  = SID2_BIG_UNSIGNED;
	
	// setup resid
	if (mBuilder->devices(true) == 0)
		mBuilder->create(1);
		
	mBuilder->filter(true);
	mBuilder->sampling(cfg.frequency);
	
	int rc = mSidEmuEngine->config(cfg);
	if (rc != 0)
		printf("configure error: %s\n", mSidEmuEngine->error());

	mSidEmuEngine->setRegisterFrameChangedCallback(NULL, NULL);
}


// ----------------------------------------------------------------------------
void PlayerLibSidplay::updateSampleRate(int newSampleRate)
// ----------------------------------------------------------------------------
{
	if (mSidEmuEngine == NULL)
		return;
	
	mPlaybackSettings.mFrequency = newSampleRate;
	
	sid2_config_t cfg = mSidEmuEngine->config();
	cfg.frequency	  = mPlaybackSettings.mFrequency * mPlaybackSettings.mOversampling;

	mBuilder->sampling(cfg.frequency);

	mSidEmuEngine->config(cfg);
}


// ----------------------------------------------------------------------------
bool PlayerLibSidplay::initSIDTune(PlaybackSettings* settings)
// ----------------------------------------------------------------------------
{
	//printf("init sidtune\n");

	if (mSidTune != NULL)
	{
		delete mSidTune;
		mSidTune = NULL;
		mSidEmuEngine->load(NULL);
	}

	//printf("init emu engine\n");

	initEmuEngine(settings);

	mSidTune = new SidTune((uint_least8_t *) mTuneBuffer, mTuneLength);

    if (!mSidTune)
        return false;

	//printf("created sidtune instance: 0x%08x\n", (int) mSidTune);

	mSidTune->selectSong(mCurrentSubtune);

	//printf("loading sid tune data\n");

	int rc = mSidEmuEngine->load(mSidTune);

	if (rc == -1)
	{
		delete mSidTune;
		mSidTune = NULL;
		return false;
	}

	//printf("setting sid tune info\n");

	setupSIDInfo();

	return true;
}


// ----------------------------------------------------------------------------
bool PlayerLibSidplay::playTuneByPath(const char* filename, int subtune, PlaybackSettings* settings)
// ----------------------------------------------------------------------------
{
	//printf("loading file: %s\n", filename);
	
	mAudioDriver->stopPlayback();

	bool success = loadTuneByPath( filename, subtune, settings );

	//printf("load returned: %d\n", success);
	
	if (success)
		mAudioDriver->startPlayback();

	return success;
}


// ----------------------------------------------------------------------------
bool PlayerLibSidplay::playTuneFromBuffer(char* buffer, int length, int subtune, PlaybackSettings* settings)
// ----------------------------------------------------------------------------
{
	//printf( "buffer: 0x%08x, len: %d, subtune: %d\n", (int) buffer, length, subtune );
	//printf( "buffer: %c %c %c %c\n", buffer[0], buffer[1], buffer[2], buffer[3] );

	mAudioDriver->stopPlayback();

	bool success = loadTuneFromBuffer(buffer, length, subtune, settings);

	if (success)
		mAudioDriver->startPlayback();

	return success;
}


// ----------------------------------------------------------------------------
bool PlayerLibSidplay::loadTuneByPath(const char* filename, int subtune, PlaybackSettings* settings)
// ----------------------------------------------------------------------------
{
	FILE* fp = fopen(filename, "rb");
	
	if ( fp == NULL )
		return false;

	int length = (int)fread(mTuneBuffer, 1, TUNE_BUFFER_SIZE, fp);
	
	if (length < 0)
		return false;

	//printf("file reading worked\n");

	fclose(fp);

	mTuneLength = length;
	mCurrentSubtune = subtune;

	return initSIDTune(settings);
}


// ----------------------------------------------------------------------------
bool PlayerLibSidplay::loadTuneFromBuffer(char* buffer, int length, int subtune, PlaybackSettings *settings)
// ----------------------------------------------------------------------------
{
	if (length < 0 || length > TUNE_BUFFER_SIZE)
		return false;

	if (buffer[0] != 'P' && buffer[0] != 'R')
		return false;
	
	if ( buffer[1] != 'S' ||
		buffer[2] != 'I' ||
		buffer[3] != 'D' )
	{
		return false;
	}
	
	mTuneLength = length;
	memcpy(mTuneBuffer, buffer, length);
	mCurrentSubtune = subtune;

	return initSIDTune(settings);
}


// ----------------------------------------------------------------------------
bool PlayerLibSidplay::startPrevSubtune()
// ----------------------------------------------------------------------------
{
	if (mCurrentSubtune > 1)
		mCurrentSubtune--;
	else
		return true;

	mAudioDriver->stopPlayback();

	initCurrentSubtune();
	
	mAudioDriver->startPlayback();

	return true;
}


// ----------------------------------------------------------------------------
bool PlayerLibSidplay::startNextSubtune()
// ----------------------------------------------------------------------------
{
	if (mCurrentSubtune < mSubtuneCount)
		mCurrentSubtune++;
	else
		return true;

	mAudioDriver->stopPlayback();

	initCurrentSubtune();
	
	mAudioDriver->startPlayback();

	return true;
}


// ----------------------------------------------------------------------------
bool PlayerLibSidplay::startSubtune( int which )
// ----------------------------------------------------------------------------
{
	if (which >= 1 && which <= mSubtuneCount)
		mCurrentSubtune = which;
	else
		return true;

	mAudioDriver->stopPlayback();

	initCurrentSubtune();
	
	mAudioDriver->startPlayback();

	return true;
}


// ----------------------------------------------------------------------------
bool PlayerLibSidplay::initCurrentSubtune()
// ----------------------------------------------------------------------------
{
	if (mSidTune == NULL)
		return false;

	if (mSidEmuEngine == NULL)
		return false;

	mSidTune->selectSong(mCurrentSubtune);
	mSidEmuEngine->load(mSidTune);
	
	return true;
}


// ----------------------------------------------------------------------------
void PlayerLibSidplay::setTempo(int tempo)
// ----------------------------------------------------------------------------
{
	if (mSidEmuEngine == NULL)
		return;

	// tempo is from 0..100, default is 50
	// 50 should yield a fastForward parameter of 100 (normal speed)
	// 0 should yield a fastForward parameter of 200 (half speed)
	// 100 should yield a fastForward parameter of 5 (20x speed)

	mCurrentTempo = tempo;

	tempo = 200 - tempo * 2;

	if (tempo < 5)
		tempo = 5;

	mSidEmuEngine->fastForward( 10000 / tempo );
}

// ----------------------------------------------------------------------------
void PlayerLibSidplay::setFilterSettings(sid_filter_t* filterSettings)
// ----------------------------------------------------------------------------
{
	mFilterSettings = *filterSettings;
	if (mBuilder)
		mBuilder->set_filter(&mFilterSettings, false);
}

// ----------------------------------------------------------------------------
int PlayerLibSidplay::getPlaybackSeconds()
// ----------------------------------------------------------------------------
{
	if (mSidEmuEngine == NULL)
		return 0;

	return(mSidEmuEngine->time() / 10);
}

#include <sys/time.h>
#if 0
struct timeval {
    time_t      tv_sec;     /* seconds (type is long) */
    suseconds_t tv_usec;    /* microseconds */
};
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

// ----------------------------------------------------------------------------
void PlayerLibSidplay::playbackIRQ()
// ----------------------------------------------------------------------------
{
#define PUSH_WRITE(A, B)    { m_regWriteBuffer[m_regWritePut++] = (A); m_regWriteBuffer[m_regWritePut++] = (B); m_regWritePut &= REG_WRITE_BUFFER_LENGTH-1; ASSERT(m_regWritePut != m_regWriteGet); }

    Instrument& instrument = m_instruments[m_currentInstrument];

    //TODO write these only when something changes
    static bool init = false;
    PUSH_WRITE(SID_FILTER_FC_LO, instrument.sid_filter_cutoff & 7);
    PUSH_WRITE(SID_FILTER_FC_HI, (instrument.sid_filter_cutoff >> 3) & 0xff);
    PUSH_WRITE(SIDPLUS_FILTER_RES, instrument.sid_filter_resonance & 0xf);
    unsigned int voice3off = 0;
    PUSH_WRITE(SID_FILTER_MODE_VOL, ((voice3off & 1) << 7) | ((instrument.sid_filter_highpass & 1) << 6) |
                                    ((instrument.sid_filter_bandpass & 1) << 5) | ((instrument.sid_filter_lowpass & 1) << 4) |
                                    (instrument.sid_filter_vol & 0xf));

    unsigned int bbgain = instrument.bassboost_en ? instrument.bassboost_gain : 0;
    PUSH_WRITE(SIDPLUS_BASSBOOST_GAIN_LO, (bbgain & 0xff));
    PUSH_WRITE(SIDPLUS_BASSBOOST_GAIN_HI, (bbgain >> 8) & 0xff);
    PUSH_WRITE(SIDPLUS_BASSBOOST_CUTOFF_LO, (instrument.bassboost_cutoff & 0xff));
    PUSH_WRITE(SIDPLUS_BASSBOOST_CUTOFF_HI, (instrument.bassboost_cutoff >> 8) & 0xff);

    unsigned int tbgain = instrument.trebleboost_en ? instrument.trebleboost_gain : 0;
    PUSH_WRITE(SIDPLUS_TREBLEBOOST_GAIN_LO, (tbgain & 0xff));
    PUSH_WRITE(SIDPLUS_TREBLEBOOST_GAIN_HI, (tbgain >> 8) & 0xff);
    PUSH_WRITE(SIDPLUS_TREBLEBOOST_CUTOFF_LO, (instrument.trebleboost_cutoff & 0xff));
    PUSH_WRITE(SIDPLUS_TREBLEBOOST_CUTOFF_HI, (instrument.trebleboost_cutoff >> 8) & 0xff);

    unsigned int fgain = instrument.fuzz_en ? instrument.fuzz_gain : 0;
    PUSH_WRITE(SIDPLUS_FUZZ_GAIN_LO, (fgain&0xff));
    PUSH_WRITE(SIDPLUS_FUZZ_GAIN_HI, (fgain>>8)&0xff);
    PUSH_WRITE(SIDPLUS_FUZZ_MULT_LO, (instrument.fuzz_mult&0xff));
    PUSH_WRITE(SIDPLUS_FUZZ_MULT_HI, (instrument.fuzz_mult>>8)&0xff);
    PUSH_WRITE(SIDPLUS_FUZZ_MIX, instrument.fuzz_mix);

    //per voice regs
    int numActiveVoices = 0;
    bool printChanges = false;
    for(int v=0;v<NUM_VOICES;v++) {
        int base = SIDPLUS_EXT_VOICE_BASE + v * SIDPLUS_VOICE_NUM_REGS;
        if (m_keyPressed[v]) {

            //printf("v%d playing %d\n", v, m_keyFreq[v]);

            unsigned int gate = 1;   //start attack
#if 0
            //freq = (freq_hi & 0xff00) | (freq_lo & 0x00ff);
            unsigned int freq = m_keyFreq[v];
            PUSH_WRITE(base+SIDPLUS_VOICE_WAVE_FREQ_LO, freq & 0xff);
            PUSH_WRITE(base+SIDPLUS_VOICE_WAVE_FREQ_HI, (freq >> 8) & 0xff);
#endif
            //pulse width = (pw_hi & 0xf00) | (pw_lo & 0x0ff);
            PUSH_WRITE(base+SIDPLUS_VOICE_WAVE_PW_LO, instrument.pulse_width & 0xff);
            PUSH_WRITE(base+SIDPLUS_VOICE_WAVE_PW_HI, (instrument.pulse_width>>8) & 0xf);

            unsigned int attack = instrument.attack;
            if (1)  //map velocity to attacj
                attack = 15-((unsigned int)m_keyVelocity[v]>>3);
            PUSH_WRITE(base+SIDPLUS_VOICE_ENV_ATTACK_DECAY, ((attack & 0xf)<<4) | (instrument.decay & 0xf));
            PUSH_WRITE(base+SIDPLUS_VOICE_ENV_SUSTAIN_RELEASE, ((instrument.sustain & 0xf)<<4) | (instrument.release & 0xf));

            for(int i=0;i<NUM_HARMONICS;i++)
                PUSH_WRITE(base+SIDPLUS_VOICE_HVOL_0+i, instrument.harmonics_en ? instrument.harmonics[i] : 0);

            unsigned int fgain = instrument.fuzz_en ? instrument.fuzz_gain : 0;
            PUSH_WRITE(base+SIDPLUS_VOICE_FUZZ_GAIN_LO, (fgain&0xff));
            PUSH_WRITE(base+SIDPLUS_VOICE_FUZZ_GAIN_HI, (fgain>>8)&0xff);
            PUSH_WRITE(base+SIDPLUS_VOICE_FUZZ_MULT_LO, (instrument.fuzz_mult&0xff));
            PUSH_WRITE(base+SIDPLUS_VOICE_FUZZ_MULT_HI, (instrument.fuzz_mult>>8)&0xff);
            PUSH_WRITE(base+SIDPLUS_VOICE_FUZZ_MIX, instrument.fuzz_mix);

            //waveform = (control >> 4) & 0xf; b0 = T, b1 = S, b2 = P, b3 = N
            //test = control & 0x8;
            //ring_mod = control & 0x4;
            //sync = control & 0x2;
            //gate = control & 0x1; //gate on => start attack-decay-sustain, gate off => start release
            PUSH_WRITE(base+SIDPLUS_VOICE_CONTROL_REG, (((1<<instrument.waveform) & 0xf) << 4) | ((instrument.test & 1) << 3) |
                                                       ((instrument.ring_modulate & 1) << 2) | ((instrument.sync & 1) << 1) | (gate & 1));
            m_keyPlaying[v] = m_keyPressed[v];    //ack
            m_keyClocks[v] = 0;
            m_keyReleasedClocks[v] = 0;
            m_keyPressed[v] = 0;    //ack
            printChanges = true;
        }
        if (m_keyReleased[v]) {
            unsigned int gate = 0;   //start release
            PUSH_WRITE(base+SIDPLUS_VOICE_CONTROL_REG, (((1<<instrument.waveform) & 0xf) << 4) | ((instrument.test & 1) << 3) |
                                                       ((instrument.ring_modulate & 1) << 2) | ((instrument.sync & 1) << 1) | (gate & 1));
            m_keyReleased[v] = 0;   //ack
            m_keyReleasedClocks[v] = 0;
            m_keyPlaying[v] = 0;
            printChanges = true;
        }
        if (m_keyPlaying[v]) {
            numActiveVoices++;
        } else {
            m_keyReleasedClocks[v] += PLAYBACK_IRQ_CLOCK_INTERVAL;
        }

        //enabling/disabling filter causes a click
        PUSH_WRITE(base+SIDPLUS_VOICE_FILT, instrument.filter_en);

        m_keyClocks[v] += PLAYBACK_IRQ_CLOCK_INTERVAL;

        //apply vibrato
        unsigned int f = m_keyFreq[v];
        if (instrument.vibrato_en) {
            float vibratoFreq = instrument.vibrato_freq/256.0f * (2.0f*3.1415f/1000000.0f);   //should divide by getCurrentCpuClockRate()
            f += (int)(instrument.vibrato_amplitude/256.0f*sinf(m_keyClocks[v]*vibratoFreq));
        }
        PUSH_WRITE(base+SIDPLUS_VOICE_WAVE_FREQ_LO, f & 0xff);
        PUSH_WRITE(base+SIDPLUS_VOICE_WAVE_FREQ_HI, (f >> 8) & 0xff);

    }
    if (numActiveVoices > NUM_VOICES)
        printf("out of voices! %d > %d\n", numActiveVoices, NUM_VOICES);
#if 0
    if (printChanges)
        printf("active voices = %d\n", numActiveVoices);
#endif
#undef PUSH_WRITE
}

// ----------------------------------------------------------------------------
void PlayerLibSidplay::fillBuffer(void* buffer, int len)
// ----------------------------------------------------------------------------
{
#if 0
	timeval t;
	gettimeofday(&t, NULL);
	static int samples = 0;
	printf("fillBuffer %d  %ld:%d\n", samples, t.tv_sec, t.tv_usec);
	samples += len;
#endif

    //len = 512 samples
    if (m_sid) {
        //synth mode
        short* b = (short*)buffer;
#if 0
        //generate a sine wave (for testing)
        static int s = 0;
        for(int i=0;i<len/sizeof(short);i++,s++) {
            b[i] = short(15767.0f * sinf(s * 2.0f * 3.1415f * 0.025f));
        }
#else
        //simulate
        int samples = len/sizeof(short);
        cycle_count delta_t = 1;
        int interleave = 1;
        int count = 0;
        static long long cycleCounter = 0;
        for(int c=0;c<samples;) {
            //generate IRQ for SW playback
            cycleCounter++;
            if ((cycleCounter % PLAYBACK_IRQ_CLOCK_INTERVAL) == 0)
                playbackIRQ();

            //process a pending register write
            if (m_regWriteGet != m_regWritePut) {
                m_sid->write(m_regWriteBuffer[m_regWriteGet], m_regWriteBuffer[m_regWriteGet+1]);
                m_regWriteGet += 2;
                m_regWriteGet &= REG_WRITE_BUFFER_LENGTH-1;
            }

            //simulate one cycle
            delta_t = 1;
            c += m_sid->clock(delta_t, b+c, samples-c, interleave);
            count++;
        }
        //printf("count = %d\n", count);

        //alternatively could simulate multiple cycles at once (TODO how much faster is this?)
        //delta_t = 1000000*512/44100;
        //m_sid->clock(delta_t, b, samples);
#endif
        return;
    }

	if (mSidEmuEngine == NULL)
		return;
	if (mPlaybackSettings.mOversampling == 1)
		mSidEmuEngine->play(buffer, len);
	else
	{
		if (mPlaybackSettings.mOversampling != mPreviousOversamplingFactor)
		{
			delete[] mOversamplingBuffer;
			mOversamplingBuffer = new char[len * mPlaybackSettings.mOversampling];
			mPreviousOversamplingFactor = mPlaybackSettings.mOversampling;
		}
		
		// calculate n times as much sample data
		mSidEmuEngine->play(mOversamplingBuffer, len * mPlaybackSettings.mOversampling);

		short *oversampleBuffer = (short*) mOversamplingBuffer;
		short *outputBuffer = (short*) buffer;
		register long sample = 0;
		
		// downsample n:1 where n = oversampling factor
		for (int sampleCount = len / sizeof(short); sampleCount > 0; sampleCount--)
		{
			// calc arithmetic average (should rather be median?)
			sample = 0;

			for (int i = 0; i < mPlaybackSettings.mOversampling; i++ )
			{
				sample += *oversampleBuffer++;
			}

			*outputBuffer++ = (short) (sample / mPlaybackSettings.mOversampling);
		}
	}
}

// ----------------------------------------------------------------------------
void PlayerLibSidplay::sidRegisterFrameHasChanged(void* inInstance, SIDPLAY2_NAMESPACE::SidRegisterFrame& inRegisterFrame)
// ----------------------------------------------------------------------------
{
	/*
	printf("Frame %d: ", inRegisterFrame.mTimeStamp);

	for (int i = 0; i < SIDPLAY2_NAMESPACE::SidRegisterFrame::SID_REGISTER_COUNT; i++)
		printf("%02x ", inRegisterFrame.mRegisters[i]);
	
	printf("\n");
	*/
	
	PlayerLibSidplay* player = (PlayerLibSidplay*) inInstance;
	if (player != NULL)
		player->mRegisterLog.push_back(inRegisterFrame);
}




