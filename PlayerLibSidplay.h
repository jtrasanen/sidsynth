
#ifndef _PlayerLibSidplay_H
#define _PlayerLibSidplay_H

#include "sidplay2.h"
#include "resid.h"
#include "resid-emu.h"

#include "AudioDriver.h"
#include <ostream>
#include <istream>
#include <sstream>

enum SPFilterType
{
	SID_FILTER_6581_Resid = 0,
	SID_FILTER_6581R3,
	SID_FILTER_6581_Galway,
	SID_FILTER_6581R4,
	SID_FILTER_8580,
	SID_FILTER_CUSTOM
};


struct PlaybackSettings
{
    PlaybackSettings() : mFrequency(44100), mBits(16), mStereo(false), mOversampling(1), mSidModel(0), mForceSidModel(false), mClockSpeed(0), mOptimization(0), mOverrideCutoffCurve(false) {}
	int				mFrequency;
	int				mBits;
	int				mStereo;

	int				mOversampling;
	int				mSidModel;
	bool			mForceSidModel;
	int				mClockSpeed;
	int				mOptimization;
    bool            mOverrideCutoffCurve;
};

struct Instrument
{
    unsigned int pulse_width;           //12b
    //control
    unsigned int waveform;              //4b (1 = T, 2 = S, 4 = P, 8 = N)
    unsigned int test;                  //1b
    unsigned int ring_modulate;         //1b
    unsigned int sync;                  //1b
    //envelope
    unsigned int attack;                //4b
    unsigned int decay;                 //4b
    unsigned int sustain;               //4b
    unsigned int release;               //4b

    unsigned int harmonics_en;          //1b
    unsigned int harmonics[NUM_HARMONICS];  //8b

    //fuzz
    unsigned int fuzz_en;               //1b
    unsigned int fuzz_gain;             //8.8b
    unsigned int fuzz_mult;             //8.8b
    unsigned int fuzz_mix;              //8b

    unsigned int filter_en;             //1b

    unsigned int vibrato_en;            //1b
    unsigned int vibrato_freq;          //8.8b
    unsigned int vibrato_amplitude;     //8.8b

    //filter fc
    unsigned int sid_filter_cutoff;     //11b
    //filter res_filt
    unsigned int sid_filter_resonance;  //4b
    //filter mode_vol
    unsigned int sid_filter_highpass;   //1b
    unsigned int sid_filter_bandpass;   //1b
    unsigned int sid_filter_lowpass;    //1b
    unsigned int sid_filter_vol;        //4b

    unsigned int bassboost_en;          //1b
    unsigned int bassboost_gain;        //8.8b
    unsigned int bassboost_cutoff;      //16b

    unsigned int trebleboost_en;        //1b
    unsigned int trebleboost_gain;      //8.8b
    unsigned int trebleboost_cutoff;    //16b

    void save(std::ostream& o)
    {
#define SAVE(A) { o << #A" = " << A << std::endl; }
        SAVE(pulse_width);
        SAVE(waveform);
        SAVE(test);
        SAVE(ring_modulate);
        SAVE(sync);
        SAVE(attack);
        SAVE(decay);
        SAVE(sustain);
        SAVE(release);
        SAVE(harmonics_en);
        for(int i=0;i<NUM_HARMONICS;i++) {
            o << "harmonics[" << i << "] = " << harmonics[i] << std::endl;
        }
        SAVE(fuzz_en);
        SAVE(fuzz_gain);
        SAVE(fuzz_mult);
        SAVE(fuzz_mix);
        SAVE(filter_en);
        SAVE(vibrato_en);
        SAVE(vibrato_freq);
        SAVE(vibrato_amplitude);

        SAVE(sid_filter_cutoff);
        SAVE(sid_filter_resonance);
        SAVE(sid_filter_highpass);
        SAVE(sid_filter_bandpass);
        SAVE(sid_filter_lowpass);
        SAVE(sid_filter_vol);
        SAVE(bassboost_en);
        SAVE(bassboost_gain);
        SAVE(bassboost_cutoff);
        SAVE(trebleboost_en);
        SAVE(trebleboost_gain);
        SAVE(trebleboost_cutoff);
#undef SAVE
    }

    void loadParam(std::string& line, const char* name, unsigned int* param)
    {
        if (!line.find(name)) {
            std::string::size_type offset = line.find_first_of('=');
            if(offset != std::string::npos) {
                std::string value = line.substr(offset+1, line.length() - (offset+1));
                std::istringstream iss(value);
                iss >> *param;
                //printf("loadParam: %s = %d\n", name, *param);
            }
        }
    }
    void load(std::istream& i)
    {
        while (i.good()) {
            std::string line;
            std::getline(i, line);
#define LOAD(A) loadParam(line, #A, &A);
            LOAD(pulse_width);
            LOAD(waveform);
            LOAD(test);
            LOAD(ring_modulate);
            LOAD(sync);
            LOAD(attack);
            LOAD(decay);
            LOAD(sustain);
            LOAD(release);
            LOAD(harmonics_en);
            for(int i=0;i<NUM_HARMONICS;i++) {
                char n[16];
                snprintf(n, 16, "harmonics[%d]", i);
                loadParam(line, n, harmonics+i);
            }
            LOAD(fuzz_en);
            LOAD(fuzz_gain);
            LOAD(fuzz_mult);
            LOAD(fuzz_mix);
            LOAD(filter_en);
            LOAD(vibrato_en);
            LOAD(vibrato_freq);
            LOAD(vibrato_amplitude);

            LOAD(sid_filter_cutoff);
            LOAD(sid_filter_resonance);
            LOAD(sid_filter_highpass);
            LOAD(sid_filter_bandpass);
            LOAD(sid_filter_lowpass);
            LOAD(sid_filter_vol);
            LOAD(bassboost_en);
            LOAD(bassboost_gain);
            LOAD(bassboost_cutoff);
            LOAD(trebleboost_en);
            LOAD(trebleboost_gain);
            LOAD(trebleboost_cutoff);
#undef LOAD
        }
    }
};

typedef std::vector<SIDPLAY2_NAMESPACE::SidRegisterFrame> SidRegisterLog;

const int TUNE_BUFFER_SIZE = 65536 + 2 + 0x7c;

class PlayerLibSidplay
{
public:
    static const int MAX_INSTRUMENTS = 16;

							PlayerLibSidplay();
	virtual					~PlayerLibSidplay();

	void					setAudioDriver(AudioDriver* audioDriver);

	void					initEmuEngine(PlaybackSettings *settings);
	void					updateSampleRate(int newSampleRate);
	
	bool					playTuneByPath(const char *filename, int subtune, PlaybackSettings *settings );
	bool					playTuneFromBuffer( char *buffer, int length, int subtune, PlaybackSettings *settings );

	bool					loadTuneByPath(const char *filename, int subtune, PlaybackSettings *settings );
	bool					loadTuneFromBuffer( char *buffer, int length, int subtune, PlaybackSettings *settings );

	bool					startPrevSubtune();
	bool					startNextSubtune();
	bool					startSubtune(int which);
	bool					initCurrentSubtune();

	void					fillBuffer(void* buffer, int len);

	inline int				getTempo()											{ return mCurrentTempo; }
	void					setTempo(int tempo);

    Instrument*             getInstrument(int i)                                { ASSERT(i>=0&&i<MAX_INSTRUMENTS); return m_instruments+i; }
    void                    setCurrentInstrument(int i)                         { ASSERT(i>=0&&i<MAX_INSTRUMENTS); m_currentInstrument = i; }

	sid_filter_t*			getFilterSettings()									{ return &mFilterSettings; }
	void					setFilterSettings(sid_filter_t* filterSettings);
	
	inline bool				isTuneLoaded()										{ return mSidTune != NULL; }

	int						getPlaybackSeconds();
	inline int				getCurrentSubtune()									{ return mCurrentSubtune; }
	inline int				getSubtuneCount()									{ return mSubtuneCount; }
	inline int				getDefaultSubtune()									{ return mDefaultSubtune; }
	inline int				hasTuneInformationStrings()							{ return mTuneInfo.numberOfInfoStrings >= 3; }
	inline const char*		getCurrentTitle()									{ return mTuneInfo.infoString[0]; }
	inline const char*		getCurrentAuthor()									{ return mTuneInfo.infoString[1]; }
	inline const char*		getCurrentReleaseInfo()								{ return mTuneInfo.infoString[2]; }
	inline unsigned short	getCurrentLoadAddress()								{ return mTuneInfo.loadAddr; }
	inline unsigned short	getCurrentInitAddress()								{ return mTuneInfo.initAddr; }
	inline unsigned short	getCurrentPlayAddress()								{ return mTuneInfo.playAddr; }
	inline const char*		getCurrentFormat()									{ return mTuneInfo.formatString; }
	inline int				getCurrentFileSize()								{ return mTuneInfo.dataFileLen; }
	inline char*			getTuneBuffer(int& outTuneLength)					{ outTuneLength = mTuneLength; return mTuneBuffer; }

	inline const char* getCurrentChipModel()				
	{
		if (mTuneInfo.sidModel == SIDTUNE_SIDMODEL_6581)
			return sChipModel6581;
		
		if (mTuneInfo.sidModel == SIDTUNE_SIDMODEL_8580)
			return sChipModel8580;
		
		return sChipModelUnspecified;
	}

	inline double getCurrentCpuClockRate()
	{
		if (mSidEmuEngine != NULL)
		{
			const sid2_config_t& cfg = mSidEmuEngine->config();
			if (cfg.clockSpeed == SID2_CLOCK_PAL)
				return 985248.4;
			else if (cfg.clockSpeed == SID2_CLOCK_NTSC)
				return 1022727.14;
		}
		
		return 985248.4;
	}
	
	inline SIDPLAY2_NAMESPACE::SidRegisterFrame getCurrentSidRegisters() const 
	{ 
		if (mSidEmuEngine != NULL) 
			return mSidEmuEngine->getCurrentRegisterFrame();
		else
			return SIDPLAY2_NAMESPACE::SidRegisterFrame();
	}

	inline void enableRegisterLogging(bool inEnable)
	{
		if (mSidEmuEngine != NULL)
			mSidEmuEngine->setRegisterFrameChangedCallback((void*) this, inEnable ? sidRegisterFrameHasChanged : NULL);
		
		if (inEnable)
			mRegisterLog.clear();
	}
	
	inline const SidRegisterLog& getRegisterLog() const		{ return mRegisterLog; }

    //synth mode
   	RESID::SID*     m_sid;
    int             m_keyPressed[NUM_VOICES];
    int             m_keyPlaying[NUM_VOICES];
    int             m_keyClocks[NUM_VOICES];
    int             m_keyFreq[NUM_VOICES];
    int             m_keyVelocity[NUM_VOICES];
    int             m_keyReleased[NUM_VOICES];
    int             m_keyReleasedClocks[NUM_VOICES];
    static const int REG_WRITE_BUFFER_LENGTH = 512;         //must be a power of two
    static const int PLAYBACK_IRQ_CLOCK_INTERVAL = 1000000/100;   //generates playbackIRQ at ~50Hz (should actually depend on PAL/NTSC clock: cycles = clockSpeed / 50)
    reg8            m_regWriteBuffer[REG_WRITE_BUFFER_LENGTH];
    int             m_regWritePut;
    int             m_regWriteGet;
    Instrument      m_instruments[MAX_INSTRUMENTS];
    int             m_currentInstrument;
    void            playbackIRQ();

	static void sidRegisterFrameHasChanged(void* inInstance, SIDPLAY2_NAMESPACE::SidRegisterFrame& inRegisterFrame);

	static const char*	sChipModel6581;
	static const char*	sChipModel8580;
	static const char*	sChipModelUnknown;
	static const char*	sChipModelUnspecified;

private:

	bool initSIDTune(PlaybackSettings *settings);
	void setupSIDInfo();

	sidplay2*			mSidEmuEngine;
	SidTune*			mSidTune;
	ReSIDBuilder*		mBuilder;
	SidTuneInfo			mTuneInfo;
	PlaybackSettings	mPlaybackSettings;
	
	AudioDriver*		mAudioDriver;

	char				mTuneBuffer[TUNE_BUFFER_SIZE];
	int					mTuneLength;

	int					mCurrentSubtune;
	int					mSubtuneCount;
	int					mDefaultSubtune;
	int					mCurrentTempo;
	
	int					mPreviousOversamplingFactor;
	char*				mOversamplingBuffer;
	
	sid_filter_t		mFilterSettings;
	
	SidRegisterLog		mRegisterLog;
};

#endif