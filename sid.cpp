//bugs
//TODO fuzz if not very good at the moment
//TODO debug why switching songs doesn't always work / hangs
//TODO clamps have been disabled, how many bits are needed within the pipe?
//TODO setting harmonics all up causes an overflow when per voice distortion is enabled

//features
//TODO remove per channel fuzz?
//TODO per voice volume (to support velocity)
//TODO save instrument always to a new file/use file dialog
//TODO load all instruments at the beginning (name instr_0001 etc.)
//TODO parameter for choosing how to route velocity?
//TODO how to permanently set keylab knob mode to relative?
//TODO map keylab better: playback mode harmonics only affect the third channel, ...
//TODO can I read keylab slider position to initialize the harmonics values?
//TODO detect midi at runtime
//TODO own songs: tracker with patterns
//TODO how to set sync sources for voices > 3?
//TODO save and load params per song
//TODO integer filter implementation
//TODO echo, reverb, compress, bitcrush, overdrive, frequency modulation, filter
//TODO master volume with clipping

#define SYNTH_MODE      1

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <unistd.h>
#include <sys/time.h>
#include "AudioCoreDriver.h"
#include "PlayerLibSidplay.h"
#include <GLUT/glut.h>

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CFString.h>

#if defined(__APPLE__) || defined(MACOSX)
#include "OpenGL/OpenGL.h"
void setSwapInterval(int swapInterval)
{
	CGLContextObj cgl_context = CGLGetCurrentContext();
	CGLSetParameter(cgl_context, kCGLCPSwapInterval, &swapInterval);
}
#else
void setSwapInterval(int swapInterval)
{
	ASSERT(!"setSwapInterval not implemented!");
	printf(!"setSwapInterval not implemented!");
	exit(-1);
}
#endif

const char *STR( const char *format, ... )		{ static char string[1024]; va_list arg; va_start(arg, format); vsprintf(string, format, arg); va_end(arg); return string; }

int                 width = 1600;
int                 height = 1000;

class vec2;
class ivec2
{
public:
	ivec2()							{ }
	ivec2(int _x, int _y)			{ x = _x; y = _y; }
	ivec2(const vec2& v);

	int		x;
	int		y;
};

class vec2
{
public:
	vec2()							{ }
	vec2(float _x, float _y)		{ x = _x; y = _y; }
	vec2(const ivec2& v);

    void pixelToViewport() {
        x = x/float(width/2)-1.0f;
        y = -(y/float(height/2)-1.0f);
    }

	float x;
	float y;
};

vec2::vec2(const ivec2& v)			{ x = float(v.x)/float(width/2)-1.0f; y = -(float(v.y)/float(height/2)-1.0f); }
ivec2::ivec2(const vec2& v)			{ x = int((v.x + 1.0f)*float(width/2)); y = int(-(v.y + 1.0f)*float(height/2)); }


class Param
{
public:
	static const int MAX_NAME_LEN = 16;
	Param()							{ }
	Param(const char* _n, bool _b, void* _base, unsigned long _offset, unsigned char k, unsigned int kl, int _posx)					{ t = BOOL; base = (unsigned char*)_base; offset = (int)_offset; *(bool*)(base+offset) = _b; strncpy(name, _n, MAX_NAME_LEN); key = k; keylab = kl; posx = _posx; }
	Param(const char* _n, int _i, int _add, int _decimal_bits, int _minval, int _maxval, void* _base, unsigned long _offset, unsigned char k, unsigned int kl, int _posx)		{ t = INT; base = (unsigned char*)_base; offset = (int)_offset; iadd = _add; decimal_bits = _decimal_bits; minval = _minval; maxval = _maxval; *(int*)(base+offset) = _i; strncpy(name, _n, MAX_NAME_LEN); key = k; keylab = kl; posx = _posx; }
	Param(const char* _n, double _d, double _add, void* _base, unsigned long _offset, unsigned char k, unsigned int kl, int _posx)	{ t = DOUBLE; base = (unsigned char*)_base; offset = (int)_offset; dadd = _add, *(double*)(base+offset) = _d; strncpy(name, _n, MAX_NAME_LEN); key = k; keylab = kl; posx = _posx; }

	void up(int m = 1)
	{
		switch(t) {
		case BOOL: *(bool*)(base+offset) ^= 1; break;
		case INT: *(int*)(base+offset) += m*iadd; if (*(int*)(base+offset) > maxval) *(int*)(base+offset) = maxval; if (*(int*)(base+offset) < minval) *(int*)(base+offset) = minval; break;
		case DOUBLE: *(double*)(base+offset) += m*dadd; break;
		default: break;
		}
	}

	void down(int m = 1)
	{
		switch(t) {
		case BOOL: *(bool*)(base+offset) ^= 1; break;
		case INT: *(int*)(base+offset) -= m*iadd; if (*(int*)(base+offset) < minval) *(int*)(base+offset) = minval; break;
		case DOUBLE: *(double*)(base+offset) -= m*dadd; break;
		default: break;
		}
	}

    void set7b(unsigned int v)
    {
		switch(t) {
		case BOOL: if (v) *(bool*)(base+offset) ^= 1; break;
		case INT: *(int*)(base+offset) = minval + (((maxval-minval)*v)>>7); break;
		case DOUBLE: *(double*)(base+offset) = v*dadd; break;
		default: break;
		}
    }

	const char* get()
	{
		switch(t) {
		case BOOL: snprintf(valString, MAX_NAME_LEN, "%s", *(bool*)(base+offset)?"on":"off"); break;
		case INT: if (!decimal_bits) snprintf(valString, MAX_NAME_LEN, "%d", *(int*)(base+offset)); else snprintf(valString, MAX_NAME_LEN, "%.2f", float(*(int*)(base+offset))/float(1<<decimal_bits)); break;
		case DOUBLE: snprintf(valString, MAX_NAME_LEN, "%f", *(double*)(base+offset)); break;
		default: break;
		}
		return valString;
	}

    const char*     getName() const { return name; }
    unsigned char   getKey() const { return key; }
    unsigned int    getKeylab() const { return keylab; }
    int             getPosX() const { return posx; }

    void            setBase(void* _base) { base = (unsigned char*)_base; }
private:
	enum Type
	{
		BOOL,
		INT,
		DOUBLE
	};
	union {
		int iadd;
		double dadd;
	};
	Type t;
	char name[MAX_NAME_LEN];
	char valString[MAX_NAME_LEN];
	unsigned char key;
    unsigned int keylab;
	int decimal_bits;
	int minval;
	int maxval;
	unsigned char* base;
    int offset;
	int posx;
};

enum Keys
{
	K_NONE			= 0,
    K_FILTER        = 'q',
	K_BASSBOOST 	= 'w',
	K_TREBLEBOOST	= 'e',
	K_HARMONICS 	= 'r',
	K_FUZZ			= 't',
	K_MUTE0			= '5',
	K_MUTE1			= '6',
	K_MUTE2			= '7',
	K_MUTE3			= '8',
    K_VIBRATO       = 'y'
};

//data[0] = 0xb0, data[1] = key, data[2] = value
enum Keylab_Slider {
    Keylab_Modulation = 0x1,
    Keylab_Volume     = 0x7,
    //bank 1
    Keylab_F1         = 0x49,
    Keylab_F2         = 0x4b,
    Keylab_F3         = 0x4f,
    Keylab_F4         = 0x48,
    Keylab_F5         = 0x50,
    Keylab_F6         = 0x51,
    Keylab_F7         = 0x52,
    Keylab_F8         = 0x53,
    Keylab_F9         = 0x55,
    //bank 2
    Keylab_F11        = 0x43,
    Keylab_F12        = 0x44,
    Keylab_F13        = 0x45,
    Keylab_F14        = 0x46,
    Keylab_F15        = 0x57,
    Keylab_F16        = 0x58,
    Keylab_F17        = 0x59,
    Keylab_F18        = 0x5a,
    Keylab_F19        = 0x5c,
};
bool isSlider(unsigned int kl) {
    switch (kl) {
    case Keylab_Modulation:
    case Keylab_F1:
    case Keylab_F2:
    case Keylab_F3:
    case Keylab_F4:
    case Keylab_F5:
    case Keylab_F6:
    case Keylab_F7:
    case Keylab_F8:
    case Keylab_F9:
    case Keylab_F11:
    case Keylab_F12:
    case Keylab_F13:
    case Keylab_F14:
    case Keylab_F15:
    case Keylab_F16:
    case Keylab_F17:
    case Keylab_F18:
    case Keylab_F19:
        return true;
    default: return false;
    }
}
bool isVolume(unsigned int kl) {
    if (kl == Keylab_Volume)
        return true;
    return false;
}

//relative. Speed = data[2] - 0x40
enum Keylab_Knob {
    Keylab_Param      = 0x70,
    Keylab_Value      = 0x72,
    //bank 1
    Keylab_P1         = 0x4a,
    Keylab_P2         = 0x47,
    Keylab_P3         = 0x4c,
    Keylab_P4         = 0x4d,
    Keylab_P5         = 0x5d,
    Keylab_P6         = 0x12,
    Keylab_P7         = 0x13,
    Keylab_P8         = 0x10,
    Keylab_P9         = 0x11,
    Keylab_P10        = 0x5b,
    //bank 2
    Keylab_P11        = 0x23,
    Keylab_P12        = 0x24,
    Keylab_P13        = 0x25,
    Keylab_P14        = 0x26,
    Keylab_P15        = 0x27,
    Keylab_P16        = 0x28,
    Keylab_P17        = 0x29,
    Keylab_P18        = 0x2a,
    Keylab_P19        = 0x2b,
    Keylab_P20        = 0x2c,
};

enum Keylab_Button {    //0 = release, 7f = press
    Keylab_Param_click  = 0x71,
    Keylab_Value_click  = 0x73,
    Keylab_1            = 0x16,
    Keylab_2            = 0x17,
    Keylab_3            = 0x18,
    Keylab_4            = 0x19,
    Keylab_5            = 0x1a,
    Keylab_6            = 0x1b,
    Keylab_7            = 0x1c,
    Keylab_8            = 0x1d,
    Keylab_9            = 0x1e,
    Keylab_10           = 0x1f,
    Keylab_Rewind       = 0x35,
    Keylab_Forward      = 0x34,
    Keylab_Stop         = 0x33,
    Keylab_Play         = 0x36,
    Keylab_Record       = 0x32,
    Keylab_Loop         = 0x37,
    Keylab_Bank1        = 0x2f,
    Keylab_Bank2        = 0x2e,
    Keylab_Sound        = 0x76,
    Keylab_Multi        = 0x77,
};
bool isButton(unsigned int kl) {
    switch (kl) {
    case Keylab_Param_click:
    case Keylab_Value_click:
    case Keylab_1:
    case Keylab_2:
    case Keylab_3:
    case Keylab_4:
    case Keylab_5:
    case Keylab_6:
    case Keylab_7:
    case Keylab_8:
    case Keylab_9:
    case Keylab_10:
    case Keylab_Rewind:
    case Keylab_Forward:
    case Keylab_Stop:
    case Keylab_Play:
    case Keylab_Record:
    case Keylab_Loop:
    case Keylab_Bank1:
    case Keylab_Bank2:
    case Keylab_Sound:
    case Keylab_Multi:
        return true;
    default: return false;
    }
}

class SIDPlayer
{
public:
	static const int NUM_PARAMS = 3 + 3 + NUM_VOICES*((NUM_HARMONICS+1) + 4 + 1);
	static const int GRID_WIDTH = 9;
	static const int GRID_HEIGHT = 2 + NUM_VOICES*3;
	static const int MAX_SONGS = 256;
	static const int MAX_SONG_NAME_LENGTH = 256;
	SIDPlayer();
	~SIDPlayer();

	void drawParams(const ivec2& origin);
	void drawSongName(const ivec2& origin);
	void drawColors(const ivec2& origin);
	void drawWaveform(const ivec2& origin, const ivec2& size);
	void drawFrequencySpectrum(const ivec2& origin, const ivec2& size);
    void drawUnderrun(const ivec2& origin);

	void keyEvent(unsigned char key, bool up, int modifiers);
	void specialKeyEvent(int key, bool up, int modifiers);
	void mouseMotionFunc(int x, int y, bool pressed);
    void midiEvent(unsigned int event, unsigned int value);

private:
	void playSong(int i);
	void drawText(int x, int y, const char* s);
	void plot(const ivec2& origin, const vec2& scale, const ivec2* points, int numPoints);
	void logLogPlot(const ivec2& origin, const vec2& scale, const float* y, int numPoints);
	void plot(const ivec2& origin, const vec2& scale, const short* y, int numPoints);

    void setupMIDI();
    static void MyMIDIReadProc(const MIDIPacketList* pktlist, void* readProcRefCon, void* srcConnRefCon);
    static void MyMIDINotifyProc (const MIDINotification* message, void* refCon);

    void saveInstrument();
    void loadInstrument();
    void changeInstrument(int newInstrument);

	AudioCoreDriver* 	m_audioCoreDriver;
    PlayerLibSidplay*	m_player;
	sid_filter_t*		m_filterSettings;
	PlaybackSettings	m_playbackSettings;
	Param				m_params[NUM_PARAMS];
	int					m_numParams;
	int					m_keyToParamIndex[256];
	int					m_midiEventToParamIndex[512];
	int					m_paramGrid[GRID_WIDTH][GRID_HEIGHT];
	int					m_paramGridWidths[GRID_HEIGHT];
    int                 m_paramGridHeight;
	int					m_paramSelectionX;
	int					m_paramSelectionY;
	int					m_lastMouseX;
	int					m_lastMouseY;
	int					m_lastModifiers;
	int					m_currSong;
	int					m_numSongs;
	char				m_songNames[MAX_SONGS][MAX_SONG_NAME_LENGTH];
	int					m_subTunes[MAX_SONGS];

    static const int MAX_INSTRUMENT_NAME_LENGTH = 64;
    char                m_instrumentPath[256];
    int                 m_numInstruments;
    char                m_instrumentNames[PlayerLibSidplay::MAX_INSTRUMENTS][MAX_INSTRUMENT_NAME_LENGTH];
    int                 m_currentInstrument;
    int                 m_numInstrumentParams;

    bool                m_songMode;

	float*				m_spectrum;
	int					m_numSpectrumSamples;

    int                 m_keyFreq[256];
    int                 m_numUnderruns;
};

static const char* getString(CFStringRef r)
{
    const char* n = CFStringGetCStringPtr(r, kCFStringEncodingASCII);
    if (n)
        return n;
    static char buffer[256];
    /*bool ok =*/ CFStringGetCString(r, buffer, 256, kCFStringEncodingASCII);
    return buffer;
}

void SIDPlayer::setupMIDI()
{
	MIDIClientRef client = NULL;
	MIDIClientCreate(CFSTR("Core MIDI to System Sounds Demo"), MyMIDINotifyProc, this, &client);
	
	MIDIPortRef inPort = NULL;
    CFStringRef r2 = CFSTR("Input port");
	MIDIInputPortCreate(client, r2, MyMIDIReadProc, this, &inPort);

	unsigned long sourceCount = MIDIGetNumberOfSources();
    printf("MIDI source count = %lu\n", sourceCount);
	for (int i = 0; i < sourceCount; ++i) {
		MIDIEndpointRef src = MIDIGetSource(i);
        CFStringRef endpointName;
		OSStatus nameErr = MIDIObjectGetStringProperty(src, kMIDIPropertyName, &endpointName);
		if (noErr == nameErr) {
            printf("  source %d: %s\n", i, getString(endpointName));
		} else {
            printf("  source %d: MIDIObjectGetStringProperty failed\n", i);
        }
		MIDIPortConnectSource(inPort, src, this);
	}

    if (sourceCount > 0) {
        //midi
        double base = 262.0 / (2.0);
        for(int i=0;i<128;i++) {
            int octave = i / 12;
            int offset = i % 12;
            double c = (double)base * (double)(1<<octave);
            static const double mult[12] = {1.0, 16.0/15.0, 9.0/8.0, 6.0/5.0, 5.0/4.0, 4.0/3.0, 64.0/45.0, 3.0/2.0, 8.0/5.0, 5.0/3.0, 16.0/9.0, 15.0/8.0};
            m_keyFreq[i] = (int)(c * mult[offset]);
            //printf("%d (%d + %d) = %d\n", i, octave, offset, m_keyFreq[i]);
        }
    } else {
        //keyboard
        int base = 2000;
        m_keyFreq['c'] = base;              //c
        m_keyFreq['f'] = base * 16 / 15;    //c#
        m_keyFreq['v'] = base * 9 / 8;      //d
        m_keyFreq['g'] = base * 6 / 5;      //d#
        m_keyFreq['b'] = base * 5 / 4;      //e
        m_keyFreq['n'] = base * 4 / 3;      //f
        m_keyFreq['j'] = base * 64 / 45;    //f#
        m_keyFreq['m'] = base * 3 / 2;      //f#
        m_keyFreq['k'] = base * 8 / 5;      //g
        m_keyFreq[','] = base * 5 / 3;      //g#
        m_keyFreq['l'] = base * 16 / 9;     //h
        m_keyFreq['.'] = base * 15 / 8;     //b
        m_keyFreq['-'] = base * 2 / 1;      //c
    }
}

/*
	@param			readProcRefCon	
					The refCon you passed to MIDIInputPortCreate or
					MIDIDestinationCreate
	@param			srcConnRefCon
					A refCon you passed to MIDIPortConnectSource, which
					identifies the source of the data.
*/
void SIDPlayer::MyMIDIReadProc(const MIDIPacketList* pktlist, void* readProcRefCon, void* srcConnRefCon)
{
    SIDPlayer* sid = (SIDPlayer*)srcConnRefCon;

/*
struct MIDIPacket { 
    MIDITimeStamp timeStamp;    //typedef UInt64 MIDITimeStamp;
    UInt16 length; 
    Byte data[256]; 
};  
*/
    const MIDIPacket *packet = &pktlist->packet[0];
    for (int p = 0;p < pktlist->numPackets;p++) {
        Byte midiCommand = packet->data[0] >> 4;
        if (midiCommand != 0x09 && midiCommand != 0x08 && midiCommand != 0x0b) {
            printf("MIDI command %d p[0]=0x%x p[1]=0x%x p[2]=0x%x\n", midiCommand, packet->data[0], packet->data[1], packet->data[2]);
        }
        if (sid) {
            //synth mode
            if (midiCommand == 0x08) {
                int note = packet->data[1] & 0x7F;
                //printf("Note OFF. Note=%d\n", note);
                int ov = -1;
                for(int i=0;i<NUM_VOICES;i++) {
                    if ((sid->m_player->m_keyPlaying[i] == (int)note) || (sid->m_player->m_keyPressed[i] == (int)note)) {
                        ov = i;
                        break;
                    }
                }
                if (ov == -1)
                    printf("key release didn't find a playing key\n");
    //            assert(ov >= 0 && ov < NUM_VOICES);
                sid->m_player->m_keyReleased[ov] = 1;
            }
            else if (midiCommand == 0x09) {
                int note = packet->data[1] & 0x7F;
                int velocity = packet->data[2] & 0x7F;
                //printf("Note ON. Note=%d, Velocity=%d Freq=%d\n", note, velocity, sid->m_keyFreq[note]);

                int ov = -1;
                for(int i=0;i<NUM_VOICES;i++) {
                    if (sid->m_player->m_keyPlaying[i] == 0) {
                        ov = i;
                        break;
                    }
                }
                if (ov == -1)
                    printf("couldn't find a free voice!\n");
                else {
                    sid->m_player->m_keyPressed[ov] = note;
                    sid->m_player->m_keyPlaying[ov] = note;
                    sid->m_player->m_keyFreq[ov] = sid->m_keyFreq[note];
                    sid->m_player->m_keyVelocity[ov] = velocity;
                }
            } else if (midiCommand == 0xb) {
                printf("MIDI command %d p[0]=0x%x p[1]=0x%x p[2]=0x%x\n", midiCommand, packet->data[0], packet->data[1], packet->data[2]);
                unsigned int k = packet->data[1];
                unsigned int v = packet->data[2];
                sid->midiEvent(k, v);
            }
        }
        packet = MIDIPacketNext(packet);
    }
}

/*
	@param			refCon
					The client's refCon passed to MIDIClientCreate.
*/
void SIDPlayer::MyMIDINotifyProc (const MIDINotification* message, void* refCon)
{
    printf("MIDI Notify messageId = %d\n", message->messageID);
/* USB disconnect
MIDI Notify messageId = 4
MIDI Notify messageId = 3
MIDI Notify messageId = 3
MIDI Notify messageId = 1
*/
/* USB connect
MIDI Notify messageId = 4
MIDI Notify messageId = 2
MIDI Notify messageId = 2
MIDI Notify messageId = 1
*/
/*
enum { // MIDINotificationMessageID 
    kMIDIMsgSetupChanged = 1, 
    kMIDIMsgObjectAdded = 2, 
    kMIDIMsgObjectRemoved = 3, 
    kMIDIMsgPropertyChanged = 4, 
    kMIDIMsgThruConnectionsChanged = 5, 
    kMIDIMsgSerialPortOwnerChanged = 6, 
    kMIDIMsgIOError = 7 
};  
Constants
kMIDIMsgSetupChanged = 1
Some aspect of the current MIDISetup has changed. No data. Should ignore this message if messages 2-6 are handled.

kMIDIMsgObjectAdded = 2
A device, entity or endpoint was added. Structure is MIDIObjectAddRemoveNotification. New for CoreMIDI 1.3.

kMIDIMsgObjectRemoved = 3
A device, entity or endpoint was removed. Structure is MIDIObjectAddRemoveNotification. New for CoreMIDI 1.3.

kMIDIMsgPropertyChanged = 4
An object's property was changed. Structure is MIDIObjectPropertyChangeNotification. New for CoreMIDI 1.3.

kMIDIMsgThruConnectionsChanged
A persistent MIDI Thru connection was created or destroyed. No data. New for CoreMIDI 1.3.

kMIDIMsgSerialPortOwnerChanged
A persistent MIDI Thru connection was created or destroyed. No data. New for CoreMIDI 1.3.

kMIDIMsgIOError
A driver I/O error occurred.

struct MIDIObjectAddRemoveNotification { 
    MIDINotificationMessageID messageID; 
    UInt32 messageSize; 
    MIDIObjectRef parent; 
    MIDIObjectType parentType; 
    MIDIObjectRef child; 
    MIDIObjectType childType; 
};  
Fields
messageID
type of message

messageSize
size of the entire message, including messageID and messageSize

parent
the parent of the added or removed object (possibly NULL)

parentType
the type of the parent object (undefined if parent is NULL)

child
the added or removed object

childType
the type of the added or removed object

struct MIDIObjectPropertyChangeNotification { 
    MIDINotificationMessageID messageID; 
    UInt32 messageSize; 
    MIDIObjectRef object; 
    MIDIObjectType objectType; 
    CFStringRef propertyName; 
};  
Fields
messageID
type of message

messageSize
size of the entire message, including messageID and messageSize

object
the object whose property has changed

objectType
the type of the object whose property has changed

propertyName
the name of the changed property


*/
}

SIDPlayer::SIDPlayer()
{
	m_lastMouseX = -1;
	m_lastMouseY = -1;
	m_lastModifiers = 0;

	m_currSong = -1;

	m_numSongs = 0;

	m_paramSelectionX = 0;
	m_paramSelectionY = 0;

	m_spectrum = NULL;
	m_numSpectrumSamples = 0;

    m_numUnderruns = 0;

	memset(m_keyToParamIndex, 0xff, 256*sizeof(int));
	memset(m_midiEventToParamIndex, 0xff, 512*sizeof(int));
	memset(m_paramGrid, 0xff, GRID_WIDTH*GRID_HEIGHT*sizeof(int));


    m_audioCoreDriver = new AudioCoreDriver;

	m_playbackSettings.mFrequency = 44100;
    m_playbackSettings.mBits = 16;
	m_playbackSettings.mStereo = false;
	m_playbackSettings.mOversampling = 1;
    m_playbackSettings.mOverrideCutoffCurve = false;

	m_player = new PlayerLibSidplay;
	m_player->initEmuEngine(&m_playbackSettings);
	m_audioCoreDriver->initialize(m_player, 44100, 16);
    m_audioCoreDriver->setSpectrumTemporalSmoothing(0.6f);
    m_player->setAudioDriver(m_audioCoreDriver);
	m_filterSettings = m_player->getFilterSettings();
	m_player->setFilterSettings(m_filterSettings);
#if SYNTH_MODE == 1
    //instead of playing a sid tune, go into synth mode
    m_songMode = false;
    setupMIDI();
	//glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
    m_player->m_sid = new RESID::SID;
    m_player->m_sid->reset();
    m_player->m_sid->set_chip_model(MOS6581);
	m_player->m_sid->set_distortion_properties(true, 1500, 300, -200000, 200000);   //Note: need large opmin/opmax for more than 3 voices
    m_player->m_sid->enable_filter(true);
    m_player->m_sid->enable_external_filter(true);
	m_player->m_sid->set_mute(0, false);
	m_player->m_sid->set_mute(1, false);
	m_player->m_sid->set_mute(2, false);

    snprintf(m_instrumentPath, 256, "/Users/jussi/jussi_git/sid/instruments");

    //TODO must start a dummy song to start audio playback
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/H/Hubbard_Rob/Sanxion.sid"); m_subTunes[m_numSongs++] = 2;
	playSong(0);

    Instrument* instrument = m_player->getInstrument(0);

    m_paramGridHeight = 0;
	m_numParams = 0;

	int p = 0, l = 0, x = 0;

/*
enum Keylab_Slider {
    Keylab_Modulation = 0x1,
    Keylab_Volume     = 0x7,
    Keylab_P1         = 0x4a,
    Keylab_P2         = 0x47,
    Keylab_P3         = 0x4c,
    Keylab_P4         = 0x4d,
    Keylab_P5         = 0x5d,
    Keylab_P6         = 0x12,
    Keylab_P7         = 0x13,
    Keylab_P8         = 0x10,
    Keylab_P9         = 0x11,
    Keylab_P10        = 0x5b,
    Keylab_F1         = 0x49,
    Keylab_F2         = 0x4b,
    Keylab_F3         = 0x4f,
    Keylab_F4         = 0x48,
    Keylab_F5         = 0x50,
    Keylab_F6         = 0x51,
    Keylab_F7         = 0x52,
    Keylab_F8         = 0x53,
    Keylab_F9         = 0x55,
//Param        b  70 behaves strangely
//Value        b  72 behaves strangely
};
enum Keylab_Button {    //0 = release, 7f = press
    Keylab_Param_click  = 0x71,
    Keylab_Value_click  = 0x73,
    Keylab_1            = 0x16,
    Keylab_2            = 0x17,
    Keylab_3            = 0x18,
    Keylab_4            = 0x19,
    Keylab_5            = 0x1a,
    Keylab_6            = 0x1b,
    Keylab_7            = 0x1c,
    Keylab_8            = 0x1d,
    Keylab_9            = 0x1e,
    Keylab_10           = 0x1f,
    Keylab_Rewind       = 0x35,
    Keylab_Forward      = 0x34,
    Keylab_Stop         = 0x33,
    Keylab_Play         = 0x36,
    Keylab_Record       = 0x32,
    Keylab_Loop         = 0x37,
    Keylab_Bank1        = 0x2f,
    Keylab_Bank2        = 0x2e,
    Keylab_Sound        = 0x76,
    Keylab_Multi        = 0x77,
};
*/
    //sid filter
    x = 0;
	m_params[p] = Param("filter en", false, instrument, offsetof(Instrument, filter_en), K_FILTER, Keylab_1, 0);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("cutoff", 128, 16, 0, 0, (1<<11)-1, instrument, offsetof(Instrument, sid_filter_cutoff), K_NONE, Keylab_P1, 250);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("res", 8, 1, 0, 0, 15, instrument, offsetof(Instrument, sid_filter_resonance), K_NONE, Keylab_P2, 400);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("vol", 8, 1, 0, 0, 15, instrument, offsetof(Instrument, sid_filter_vol), K_NONE, Keylab_P3, 550);
    m_paramGrid[x++][l] = p;
    p++;
	m_params[p] = Param("hp", false, instrument, offsetof(Instrument, sid_filter_highpass), K_NONE, Keylab_2, 700);
    m_paramGrid[x++][l] = p;
    p++;
	m_params[p] = Param("bp", false, instrument, offsetof(Instrument, sid_filter_bandpass), K_NONE, Keylab_3, 800);
    m_paramGrid[x++][l] = p;
    p++;
	m_params[p] = Param("lp", true, instrument, offsetof(Instrument, sid_filter_lowpass), K_NONE, Keylab_4, 900);
    m_paramGrid[x++][l] = p;
    p++;
    m_paramGridWidths[l] = x;
    l++;
    ASSERT(x <= GRID_WIDTH);

	//bass boost
	x = 0;
	m_params[p] = Param("bassboost en", false, instrument, offsetof(Instrument, bassboost_en), K_BASSBOOST, Keylab_5, 0);
	m_paramGrid[x++][l] = p;
	p++;
	m_params[p] = Param("gain", 8<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-2), FILTER_DECIMAL_BITS, 0, 32<<FILTER_DECIMAL_BITS, instrument, offsetof(Instrument, bassboost_gain), K_NONE, Keylab_P4, 250);
	m_paramGrid[x++][l] = p;
	p++;
	m_params[p] = Param("cutoff", 100, 2, 0, 0, 8192, instrument, offsetof(Instrument, bassboost_cutoff), K_NONE, Keylab_P5, 400);
	m_paramGrid[x++][l] = p;
	p++;
	m_paramGridWidths[l] = x;
	l++;
	ASSERT(x <= GRID_WIDTH);

	//treble boost
	x = 0;
	m_params[p] = Param("trebleboost en", false, instrument, offsetof(Instrument, trebleboost_en), K_TREBLEBOOST, Keylab_6, 0);
	m_paramGrid[x++][l] = p;
	p++;
	m_params[p] = Param("gain", 8<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-2), FILTER_DECIMAL_BITS, 0, 32<<FILTER_DECIMAL_BITS, instrument, offsetof(Instrument, trebleboost_gain), K_NONE, Keylab_P6, 250);
	m_paramGrid[x++][l] = p;
	p++;
	m_params[p] = Param("cutoff", 2500, 10, 0, 0, 8192, instrument, offsetof(Instrument, trebleboost_cutoff), K_NONE, Keylab_P7, 400);
	m_paramGrid[x++][l] = p;
	p++;
	m_paramGridWidths[l] = x;
	l++;
	ASSERT(x <= GRID_WIDTH);

	//waveform
    x = 0;
    m_params[p] = Param("waveform", 1, 1, 0, 0, 3, instrument, offsetof(Instrument, waveform), K_NONE, Keylab_P8, 0);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("pulse w", 1024, 16, 0, 0, (1<<12)-1, instrument, offsetof(Instrument, pulse_width), K_NONE, Keylab_P9, 250);
    m_paramGrid[x++][l] = p;
    p++;
	m_params[p] = Param("ring mod en", false, instrument, offsetof(Instrument, ring_modulate), K_NONE, Keylab_7, 400);
    m_paramGrid[x++][l] = p;
    p++;
    m_paramGridWidths[l] = x;
    l++;
    ASSERT(x <= GRID_WIDTH);

	//envelope
    x = 0;
    m_params[p] = Param("attack", 0, 1, 0, 0, 15, instrument, offsetof(Instrument, attack), K_NONE, Keylab_P11, 0);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("decay", 9, 1, 0, 0, 15, instrument, offsetof(Instrument, decay), K_NONE, Keylab_P12, 250);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("sustain", 6, 1, 0, 0, 15, instrument, offsetof(Instrument, sustain), K_NONE, Keylab_P13, 400);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("release", 8, 1, 0, 0, 15, instrument, offsetof(Instrument, release), K_NONE, Keylab_P14, 550);
    m_paramGrid[x++][l] = p;
    p++;
    m_paramGridWidths[l] = x;
    l++;
    ASSERT(x <= GRID_WIDTH);

	//harmonics
	x = 0;
	m_params[p] = Param("harmonics en", false, instrument, offsetof(Instrument, harmonics_en), K_HARMONICS, Keylab_8, 0);
	m_paramGrid[x++][l] = p;
	p++;
	for(int h=0;h<NUM_HARMONICS;h++) {
        static unsigned int kl[NUM_HARMONICS] = {
            Keylab_F1, Keylab_F2, Keylab_F3, Keylab_F4, Keylab_F5, Keylab_F6, Keylab_F7, Keylab_F8
        };
		m_params[p] = Param(STR("hvol%d",h), ((h+1)&1) << (7-h), 2, 0, 0, 255, instrument, offsetof(Instrument, harmonics[h]), K_NONE, kl[h], 250 + h * 150);
		m_paramGrid[x++][l] = p;
		p++;
	}
	m_paramGridWidths[l] = x;
	l++;
	ASSERT(x <= GRID_WIDTH);

	//fuzz after the pipe
    x = 0;
    m_params[p] = Param("fuzz", false, instrument, offsetof(Instrument, fuzz_en), K_FUZZ, Keylab_9, 0);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("gain", 1<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-3), FILTER_DECIMAL_BITS, 0, 255<<FILTER_DECIMAL_BITS, instrument, offsetof(Instrument, fuzz_gain), K_NONE, Keylab_P15, 250);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("mult", 1<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-6), FILTER_DECIMAL_BITS, 0, 16<<FILTER_DECIMAL_BITS, instrument, offsetof(Instrument, fuzz_mult), K_NONE, Keylab_P16, 400);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("mix", 128, 2, 0, 0, 255, instrument, offsetof(Instrument, fuzz_mix), K_NONE, Keylab_P17, 550);
    m_paramGrid[x++][l] = p;
    p++;
    m_paramGridWidths[l] = x;
    l++;
    ASSERT(x <= GRID_WIDTH);

	//vibrato
    x = 0;
    m_params[p] = Param("vibrato", false, instrument, offsetof(Instrument, vibrato_en), K_VIBRATO, Keylab_10, 0);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("freq", 10<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-3), FILTER_DECIMAL_BITS, 0, 255<<FILTER_DECIMAL_BITS, instrument, offsetof(Instrument, vibrato_freq), K_NONE, Keylab_P18, 250);
    m_paramGrid[x++][l] = p;
    p++;
    m_params[p] = Param("amp", 25<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-6), FILTER_DECIMAL_BITS, 0, 255<<FILTER_DECIMAL_BITS, instrument, offsetof(Instrument, vibrato_amplitude), K_NONE, Keylab_P19, 400);
    m_paramGrid[x++][l] = p;
    p++;
    m_paramGridWidths[l] = x;
    l++;
    ASSERT(x <= GRID_WIDTH);


    for(int i=0;i<p;i++) {
        unsigned char k = m_params[i].getKey();
        if (k != K_NONE)
            m_keyToParamIndex[k] = i;
        unsigned int kl = m_params[i].getKeylab();
        if (kl != 0)
            m_midiEventToParamIndex[kl] = i;
    }

    m_numInstrumentParams = p;

	snprintf(m_instrumentNames[m_numInstruments], MAX_INSTRUMENT_NAME_LENGTH, "instr0.txt"); m_numInstruments++;
	snprintf(m_instrumentNames[m_numInstruments], MAX_INSTRUMENT_NAME_LENGTH, "instr1.txt"); m_numInstruments++;
    ASSERT(m_numInstruments <= PlayerLibSidplay::MAX_INSTRUMENTS);

    for(int i=0;i<m_numInstruments;i++) {
        m_currentInstrument = i;
        loadInstrument();
    }
    m_currentInstrument = 0;
    m_player->setCurrentInstrument(m_currentInstrument);

#else   //SYNTH_MODE == 0, playback
    m_songMode = true;
    setupMIDI();

	int p = 0, l = 0, x = 0;

	//bass boost
	x = 0;
	m_params[p] = Param("bassboost en", false, m_filterSettings, offsetof(sid_filter_t, bassboost_enable), K_BASSBOOST, Keylab_1, 0);
	m_paramGrid[x++][l] = p;
	p++;
	m_params[p] = Param("gain", 8<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-2), FILTER_DECIMAL_BITS, 0, 32<<FILTER_DECIMAL_BITS, m_filterSettings, offsetof(sid_filter_t, bassboost_gain), K_NONE, Keylab_P1, 250);
	m_paramGrid[x++][l] = p;
	p++;
	m_params[p] = Param("cutoff", 100, 2, 0, 0, 8192, m_filterSettings, offsetof(sid_filter_t, bassboost_cutoff), K_NONE, Keylab_P2, 400);
	m_paramGrid[x++][l] = p;
	p++;
	m_paramGridWidths[l] = x;
	l++;
	ASSERT(x <= GRID_WIDTH);

	//treble boost
	x = 0;
	m_params[p] = Param("trebleboost en", false, m_filterSettings, offsetof(sid_filter_t, trebleboost_enable), K_TREBLEBOOST, Keylab_2, 0);
	m_paramGrid[x++][l] = p;
	p++;
	m_params[p] = Param("gain", 8<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-2), FILTER_DECIMAL_BITS, 0, 32<<FILTER_DECIMAL_BITS, m_filterSettings, offsetof(sid_filter_t, trebleboost_gain), K_NONE, Keylab_P3, 250);
	m_paramGrid[x++][l] = p;
	p++;
	m_params[p] = Param("cutoff", 2500, 10, 0, 0, 8192, m_filterSettings, offsetof(sid_filter_t, trebleboost_cutoff), K_NONE, Keylab_P4, 400);
	m_paramGrid[x++][l] = p;
	p++;
	m_paramGridWidths[l] = x;
	l++;
	ASSERT(x <= GRID_WIDTH);

	//harmonics
	for(int i=0;i<3;i++) {
		int k;
        unsigned int kl = 0;
		switch(i) {
		case 0: k = K_HARMONICS; kl = Keylab_3; break;
		case 1: k = K_HARMONICS; kl = Keylab_4; break;
		case 2: k = K_HARMONICS; kl = Keylab_5; break;
		case 3: k = K_HARMONICS; break;
		default: /*ASSERT(0);*/ break;
		}
		x = 0;
		m_params[p] = Param("harmonics en", false, m_filterSettings, offsetof(sid_filter_t, harmonics_enable[i]), k, kl, 0);
		m_paramGrid[x++][l] = p;
		p++;
		for(int h=0;h<NUM_HARMONICS;h++) {
            static unsigned int kl[NUM_HARMONICS] = {
                Keylab_F1, Keylab_F2, Keylab_F3, Keylab_F4, Keylab_F5, Keylab_F6, Keylab_F7, Keylab_F8
            };
			m_params[p] = Param(STR("hvol%d",h), ((h+1)&1) << (7-h), 2, 0, 0, 255, m_filterSettings, offsetof(sid_filter_t, harmonic_vol[i][h]), K_NONE, kl[h], 250 + h * 150);
			m_paramGrid[x++][l] = p;
			p++;
		}
		m_paramGridWidths[l] = x;
		l++;
		ASSERT(x <= GRID_WIDTH);
	}
#if 0
	//per voice distortion
	for(int i=0;i<3;i++) {
		int k;
        unsigned int kl = 0, klgain = 0, klmult = 0, klmix = 0;
		switch(i) {
		case 0: k = K_FUZZ0; kl = Keylab_6; klgain = Keylab_P11; klmult = Keylab_P12; klmix = Keylab_P13; break;
		case 1: k = K_FUZZ1; kl = Keylab_7; klgain = Keylab_P14; klmult = Keylab_P15; klmix = Keylab_P16; break;
		case 2: k = K_FUZZ2; kl = Keylab_8; klgain = Keylab_P17; klmult = Keylab_P18; klmix = Keylab_P19; break;
		case 3: k = K_FUZZ3; break;
		default: /*ASSERT(0);*/ break;
		}
		x = 0;
		m_params[p] = Param(STR("fuzz %d",i), false, m_filterSettings, offsetof(sid_filter_t, fuzz_enable[i]), k, kl, 0);
		m_paramGrid[x++][l] = p;
		p++;
		m_params[p] = Param("gain", 1<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-3), FILTER_DECIMAL_BITS, 0, 255<<FILTER_DECIMAL_BITS, m_filterSettings, offsetof(sid_filter_t, fuzz_gain[i]), K_NONE, klgain, 250);
		m_paramGrid[x++][l] = p;
		p++;
		m_params[p] = Param("mult", 1<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-6), FILTER_DECIMAL_BITS, 0, 16<<FILTER_DECIMAL_BITS, m_filterSettings, offsetof(sid_filter_t, fuzz_multiplier[i]), K_NONE, klmult, 400);
		m_paramGrid[x++][l] = p;
		p++;
		m_params[p] = Param("mix", 128, 2, 0, 0, 255, m_filterSettings, offsetof(sid_filter_t, fuzz_mix[i]), K_NONE, klmix, 550);
		m_paramGrid[x++][l] = p;
		p++;
		m_paramGridWidths[l] = x;
		l++;
		ASSERT(x <= GRID_WIDTH);
	}
#else
    {   //distortion at the end of the pipe
		int k;
        unsigned int kl = 0, klgain = 0, klmult = 0, klmix = 0;
		k = K_FUZZ; kl = Keylab_6; klgain = Keylab_P6; klmult = Keylab_P7; klmix = Keylab_P8;
		x = 0;
		m_params[p] = Param("fuzz", false, m_filterSettings, offsetof(sid_filter_t, main_fuzz_enable), k, kl, 0);
		m_paramGrid[x++][l] = p;
		p++;
		m_params[p] = Param("gain", 1<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-3), FILTER_DECIMAL_BITS, 0, 255<<FILTER_DECIMAL_BITS, m_filterSettings, offsetof(sid_filter_t, main_fuzz_gain), K_NONE, klgain, 250);
		m_paramGrid[x++][l] = p;
		p++;
		m_params[p] = Param("mult", 1<<FILTER_DECIMAL_BITS, 1<<(FILTER_DECIMAL_BITS-6), FILTER_DECIMAL_BITS, 0, 16<<FILTER_DECIMAL_BITS, m_filterSettings, offsetof(sid_filter_t, main_fuzz_multiplier), K_NONE, klmult, 400);
		m_paramGrid[x++][l] = p;
		p++;
		m_params[p] = Param("mix", 128, 2, 0, 0, 255, m_filterSettings, offsetof(sid_filter_t, main_fuzz_mix), K_NONE, klmix, 550);
		m_paramGrid[x++][l] = p;
		p++;
		m_paramGridWidths[l] = x;
		l++;
		ASSERT(x <= GRID_WIDTH);
	}
#endif
	//mute
	for(int i=0;i<3;i++) {
		int k;
        unsigned int kl = 0;
		switch(i) {
		case 0: k = K_MUTE0; kl = Keylab_Rewind; break;
		case 1: k = K_MUTE1; kl = Keylab_Forward; break;
		case 2: k = K_MUTE2; kl = Keylab_Stop; break;
		case 3: k = K_MUTE3; break;
		default: /*ASSERT(0);*/ break;
		}
		x = 0;
		m_params[p] = Param(STR("mute %d",i), false, m_filterSettings, offsetof(sid_filter_t, mute[i]), k, kl, 0);
		m_paramGrid[x++][l] = p;
		p++;
		m_paramGridWidths[l] = x;
		l++;
		ASSERT(x <= GRID_WIDTH);
	}

    for(int i=0;i<p;i++) {
        unsigned char k = m_params[i].getKey();
        if (k != K_NONE)
            m_keyToParamIndex[k] = i;
        unsigned int kl = m_params[i].getKeylab();
        if (kl != 0)
            m_midiEventToParamIndex[kl] = i;
    }


	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/H/Hubbard_Rob/Sanxion.sid"); m_subTunes[m_numSongs++] = 2;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/D/Deetsay/MOS6581.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/G/Galway_Martin/Arkanoid.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/G/Galway_Martin/Game_Over.sid"); m_subTunes[m_numSongs++] = 19;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/0-9/20CC/van_Santen_Edwin/All_That_She_Wants_8580.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/H/Hubbard_Rob/Commando.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/H/Hubbard_Rob/Thrust.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/R/Ross_Mark/Knight_Rider_2.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/D/Daglish_Ben/DeathWish_III.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/Y/Yip/Netherworld.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/T/Tel_Jeroen/Turbo_Outrun.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/T/Tel_Jeroen/Supremacy.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/T/Tel_Jeroen/Myth.sid"); m_subTunes[m_numSongs++] = 1;
	snprintf(m_songNames[m_numSongs], MAX_SONG_NAME_LENGTH, "MUSICIANS/H/Huelsbeck_Chris/Third_TFMX_Song.sid"); m_subTunes[m_numSongs++] = 1;
	ASSERT(m_numSongs <= MAX_SONGS);

	playSong(0);
#endif

    m_paramGridHeight = l;

	m_numParams = p;
	ASSERT(p <= NUM_PARAMS);
	ASSERT(l <= GRID_HEIGHT);
}

void SIDPlayer::playSong(int i)
{
	if (m_currSong >= 0)
	    m_audioCoreDriver->stopPlayback();

	ASSERT(i >= 0 && i < m_numSongs);
	char path[256];
	snprintf(path, 256, "/Users/jussi/jussi/stuff/hvsids-4.1/%s", m_songNames[i]);
	bool ok = m_player->playTuneByPath(path, m_subTunes[i], &m_playbackSettings);
    printf("play %s %s\n", m_songNames[i], ok ? "ok" : "failed");
	m_currSong = i;
	m_player->setFilterSettings(m_filterSettings);
}

void SIDPlayer::drawText(int x, int y, const char* s)
{
	glRasterPos2i(x, y);
	int l = (int)strlen(s);
	void* font = GLUT_BITMAP_9_BY_15;
	for(int i=0;i<l;i++)
		glutBitmapCharacter(font, s[i]);
}

void SIDPlayer::drawParams(const ivec2& origin)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0.0, width, 0.0, height);

	const int MAX_STRING_LENGTH = 256;
	char s[MAX_STRING_LENGTH];

	for(int y=0;y<m_paramGridHeight;y++)
	{
		for(int x=0;x<m_paramGridWidths[y];x++)
		{
			int p = m_paramGrid[x][y];
			char sel1 = (x == m_paramSelectionX) && (y == m_paramSelectionY) ? '[' : ' ';
			char sel2 = (x == m_paramSelectionX) && (y == m_paramSelectionY) ? ']' : ' ';
			if (m_params[p].getKey() != 0)
				snprintf(s, MAX_STRING_LENGTH, "%c[%c] %s %s%c", sel1, m_params[p].getKey(), m_params[p].getName(), m_params[p].get(), sel2);
			else
				snprintf(s, MAX_STRING_LENGTH, "%c%s %s%c", sel1, m_params[p].getName(), m_params[p].get(), sel2);
			drawText(origin.x + m_params[p].getPosX(), height-origin.y - y*15, s);
		}
	}

	glPopMatrix();
}

void SIDPlayer::drawSongName(const ivec2& origin)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0.0, width, 0.0, height);

	char s[256];
    if (m_songMode)
        snprintf(s, 256, "%s subtune %d chip %s", m_currSong >= 0 ? m_songNames[m_currSong] : "not playing", m_currSong >= 0 ? m_subTunes[m_currSong] : 0, m_player->getCurrentChipModel());
    else
        snprintf(s, 256, "%s", m_instrumentNames[m_currentInstrument]);

	drawText(origin.x, height-origin.y, s);

	glPopMatrix();
}

void SIDPlayer::drawUnderrun(const ivec2& origin)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0.0, width, 0.0, height);

	char s[256];
    bool underrun = m_audioCoreDriver->getBufferUnderrunDetected();
    if (underrun) {
        m_numUnderruns += m_audioCoreDriver->getBufferUnderrunCount();
        m_audioCoreDriver->setBufferUnderrunDetected(false);
    }
    if (m_numUnderruns)
        snprintf(s, 256, "UNDERRUNS %d", m_numUnderruns);
    else
        snprintf(s, 256, "no underruns");

	drawText(origin.x, height-origin.y, s);

	glPopMatrix();
}

void SIDPlayer::plot(const ivec2& origin, const vec2& scale, const ivec2* points, int numPoints)
{
	if (numPoints < 2)
		return;

	glBegin(GL_LINE_STRIP);
	for(int i=0;i<numPoints;i++)
	{
		ivec2 p = origin;
		p.x += points[i].x;
		p.y += points[i].y;
		vec2 v = vec2(p);
		glVertex2f(v.x, v.y);
	}
	glEnd();
}

void SIDPlayer::logLogPlot(const ivec2& origin, const vec2& scale, const float* y, int numPoints)
{
	if (numPoints < 2)
		return;

    int xMax = numPoints;
    int xOffset = 10;
    float xScale = xMax / (log(float(xMax+xOffset)) - log(float(xOffset)));

    float yMax = 32768.0f;
    float yOffset = 10.0f;
    float yScale = yMax / (log(yMax+yOffset) - log(yOffset));

	glBegin(GL_LINE_STRIP);
	for(int i=0, x=0;i<numPoints;i++,x++)
	{
		ivec2 p = origin;
		p.x += int(scale.x*xScale*(log(float(x+xOffset))-log(float(xOffset))));
		p.y += int(scale.y*yScale*(log(y[i]+yOffset)-log(yOffset)));
		vec2 v = vec2(p);
		glVertex2f(v.x, v.y);
	}
	glEnd();
}

void SIDPlayer::plot(const ivec2& origin, const vec2& scale, const short* y, int numPoints)
{
	if (numPoints < 2)
		return;

	glBegin(GL_LINE_STRIP);
	for(int i=0, x=0;i<numPoints;i++,x++)
	{
		ivec2 p = origin;
		p.x += int(scale.x*float(x));
		p.y += int(scale.y*float(y[i]));
		vec2 v = vec2(p);
		glVertex2f(v.x, v.y);
	}
	glEnd();
}

float gaussian(float x, float stddev)
{
    return exp(-x*x/(2.0f*stddev*stddev)) / (sqrt(2.0f*3.1415f)*stddev);
}

float bspline(float t)
{
    int it = (int)floorf(t);
    assert(it >= -1 && it <= 2);
    t -= it;
    float t2 = t*t, t3 = t*t2;
    float b0, b1, b2, b3;
    b0 = 1.0f/6.0f*(-1.0f*t3 + 3.0f*t2 - 3.0f*t + 1.0f);
    b1 = 1.0f/6.0f*( 3.0f*t3 - 6.0f*t2 + 0.0f*t + 4.0f);
    b2 = 1.0f/6.0f*(-3.0f*t3 + 3.0f*t2 + 3.0f*t + 1.0f);
    b3 = 1.0f/6.0f*( 1.0f*t3 + 0.0f*t2 + 0.0f*t + 0.0f);
#if 0
    float c[7] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    return c[it+1]*b0 + c[it+2]*b1 + c[it+3]*b2 + c[it+4]*b3;
#else
    if (it == -1)
        return b2;
    else if (it == 0)
        return b1;
    else if (it == 1)
        return b0;
    return 0.0f;
#endif
/*
	//iterate the parametric b-spline
	int i, midpoints = (1<<subpoints->maxLevel);
    for(i=0;i<subpoints->numControlPoints;i++)
	{
        int l,c,n,nn,j;
        c = i;
        l = int_max(0, c-1);
        n = int_min(subpoints->numControlPoints-1, c+1);
        nn = int_min(subpoints->numControlPoints-1, c+2);
		for(j=0;j<midpoints;j++)
		{
			float t = (float)j / (float)midpoints;
			float t2 = t*t, t3 = t*t2;
			vec4 b,x,y,z;
			float r;
			b.x = 1.0f/6.0f*(-1.0f*t3 + 3.0f*t2 - 3.0f*t + 1.0f);
			b.y = 1.0f/6.0f*( 3.0f*t3 - 6.0f*t2 + 0.0f*t + 4.0f);
			b.z = 1.0f/6.0f*(-3.0f*t3 + 3.0f*t2 + 3.0f*t + 1.0f);
			b.w = 1.0f/6.0f*( 1.0f*t3 + 0.0f*t2 + 0.0f*t + 0.0f);
			vec4_set(&x, subpoints->controlPoints[l].x, subpoints->controlPoints[c].x, subpoints->controlPoints[n].x, subpoints->controlPoints[nn].x);
			r = vec4_dot(&b, &x);
		}
	}
*/
}

void SIDPlayer::drawColors(const ivec2& origin)
{
	if (!m_audioCoreDriver)
		return;

	float* samples = m_audioCoreDriver->getSpectrumBuffer();
	int numSamples = m_audioCoreDriver->getNumSamplesInSpectrum();

    const int numShapes = 16;
    glDisable(GL_CULL_FACE);

    const float PI = 3.1415f;
    const float shapeRadius = 400.0f;
    int numTriangles = numSamples;
    static int a = 0;
    for(int i=0;i<numShapes;i++) {
        float aa = 2.0f*PI*(float)i/(float)numShapes;
        float angle = (float)a / 50.0f + aa;
        vec2 p;
        p.x = origin.x + shapeRadius * sinf(angle);
        p.y = origin.y + shapeRadius * cosf(angle);
        glColor4f(0.5f+0.5f*sinf(angle), 0.5f+0.5f*sinf(angle+1.0f/3.0f*2.0f*PI), 0.5f+0.5f*sinf(angle+2.0f/3.0f*2.0f*PI), 1.0f);
        float aaa = 0.0f;
        vec2 pp = p;
        pp.pixelToViewport();
        //gaussian average of the frequency samples around a freq slice
        int numFreqSamples = numSamples / numShapes;
        int origin = i * numFreqSamples;
        int minSample = origin - numFreqSamples*2;
        if (minSample < 0)
            minSample = 0;
        int maxSample = origin + numFreqSamples*2;
        if (maxSample > numSamples)
            maxSample = numSamples;

        float yMax = 32768.0f;
        float yOffset = 2.0f;
        float yScale = yMax / (log(yMax+yOffset) - log(yOffset));
        float r = 0.0f, sumw = 0.0f;
        for(int j=minSample;j<maxSample;j++) {
            float d = float(abs(j-origin)) / (float)numFreqSamples;
//            float w = gaussian(d, 0.5f);
            float w = bspline(d);
            float s = yScale*(log(samples[j]+yOffset)-log(yOffset));
            r += w * s;
            sumw += w;
        }
        r /= sumw;
#if 1
        glBegin(GL_LINE_STRIP);
        for(int j=minSample;j<maxSample;j++) {
            float d = float(abs(j-origin)) / (float)numFreqSamples;
//            float w = gaussian(d, 0.25f);
            float w = bspline(d);

            ivec2 p = ivec2(810, 750);
            vec2 scale;
            scale.x = 780.0f;
            scale.y = 500.0f;
            p.x += int(scale.x*j/(float)numSamples);
            p.y -= int(scale.y*w*r/32768.0f);
            vec2 v = vec2(p);
            glVertex2f(v.x, v.y);
        }
        glEnd();
#else
        r = 150.0f + r * 256.0f/32768.0f;
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        glBlendFunc(GL_ONE, GL_ONE);
        glBegin(GL_TRIANGLES);
        for(int j=0;j<numTriangles;j++) {
            float aaa2 = 2.0f*PI*(float)(j+1)/(float)numTriangles;
            if (j == numTriangles-1)
                aaa2 = 0.0f;
            vec2 p1;
            p1.x = p.x + r*sinf(aaa);
            p1.y = p.y + r*cosf(aaa);
            p1.pixelToViewport();
            glVertex2f(p1.x, p1.y);
            vec2 p2;
            p2.x = p.x + r*sinf(aaa2);
            p2.y = p.y + r*cosf(aaa2);
            p2.pixelToViewport();
            glVertex2f(p2.x, p2.y);
            glVertex2f(pp.x, pp.y);
            aaa = aaa2;
        }
        glEnd();
#endif
    }
//    a++;
    glColor3f(0.0f, 0.0f, 0.0f);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
/*
	vec2 s(float(size.x)/float(numSamples), -float(size.y) / 65536.0f);
	plot(origin, s, samples, numSamples);
	glBegin(GL_LINES);
	ivec2 a, b;
	vec2 fa, fb;
	a = origin;
	b = origin;
	b.x += size.x;
	fa = vec2(a);
	fb = vec2(b);
	glVertex2f(fa.x, fa.y);
	glVertex2f(fb.x, fb.y);
	a.y += size.y >> 1;
	b.y += size.y >> 1;
	fa = vec2(a);
	fb = vec2(b);
	glVertex2f(fa.x, fa.y);
	glVertex2f(fb.x, fb.y);
	a.y -= size.y;
	b.y -= size.y;
	fa = vec2(a);
	fb = vec2(b);
	glVertex2f(fa.x, fa.y);
	glVertex2f(fb.x, fb.y);
	glEnd();
*/
}

void SIDPlayer::drawWaveform(const ivec2& origin, const ivec2& size)
{
	if (!m_audioCoreDriver)
		return;

	short* samples = m_audioCoreDriver->getSampleBuffer();
	int numSamples = m_audioCoreDriver->getNumSamplesInBuffer();

	vec2 s(float(size.x)/float(numSamples), -float(size.y) / 65536.0f);
	plot(origin, s, samples, numSamples);
	glBegin(GL_LINES);
	ivec2 a, b;
	vec2 fa, fb;
	a = origin;
	b = origin;
	b.x += size.x;
	fa = vec2(a);
	fb = vec2(b);
	glVertex2f(fa.x, fa.y);
	glVertex2f(fb.x, fb.y);
	a.y += size.y >> 1;
	b.y += size.y >> 1;
	fa = vec2(a);
	fb = vec2(b);
	glVertex2f(fa.x, fa.y);
	glVertex2f(fb.x, fb.y);
	a.y -= size.y;
	b.y -= size.y;
	fa = vec2(a);
	fb = vec2(b);
	glVertex2f(fa.x, fa.y);
	glVertex2f(fb.x, fb.y);
	glEnd();
}

void SIDPlayer::drawFrequencySpectrum(const ivec2& origin, const ivec2& size)
{
	if (!m_audioCoreDriver)
		return;

	float* spectrum = m_audioCoreDriver->getSpectrumBuffer();
	int numSamples = m_audioCoreDriver->getNumSamplesInSpectrum();

	vec2 s(float(size.x)/float(numSamples), -float(size.y) / 65536.0f);
	logLogPlot(origin, s, spectrum, numSamples);
	glBegin(GL_LINES);
	ivec2 a, b;
	vec2 fa, fb;
	a = origin;
	b = origin;
	b.x += size.x;
	fa = vec2(a);
	fb = vec2(b);
	glVertex2f(fa.x, fa.y);
	glVertex2f(fb.x, fb.y);
	a.y -= size.y;
	b.y -= size.y;
	fa = vec2(a);
	fb = vec2(b);
	glVertex2f(fa.x, fa.y);
	glVertex2f(fb.x, fb.y);
	glEnd();
}

void SIDPlayer::keyEvent(unsigned char key, bool up, int modifiers)
{
    if (m_player->m_sid) {
        //synth mode
        static int v = 0;
        if (up) {
            //printf("%c up\n", key);
            int ov = -1;
            for(int i=0;i<NUM_VOICES;i++) {
                if ((m_player->m_keyPlaying[i] == (int)key) || (m_player->m_keyPressed[i] == (int)key)) {
                    ov = i;
                    break;
                }
            }
            if (ov == -1)
                printf("key release didn't find a playing key\n");
//            assert(ov >= 0 && ov < NUM_VOICES);
            m_player->m_keyReleased[ov] = 1;
        }
        else {
            //printf("%c\n", key);
            m_player->m_keyPressed[v] = (int)key;
            m_player->m_keyFreq[v] = m_keyFreq[key];
            m_player->m_keyVelocity[v] = 127;
            v = (v+1) % NUM_VOICES;
        }
    }

	if (up)
		return;
	bool update = false;

	int i = m_keyToParamIndex[key];
	if (i != -1) {
		m_params[i].up();
		update = true;
	}
	if (update) {
		m_player->setFilterSettings(m_filterSettings);
	}
}

void SIDPlayer::midiEvent(unsigned int event, unsigned int value)
{
	bool update = false;

    if (isVolume(event)) {
		m_params[m_paramGrid[m_paramSelectionX][m_paramSelectionY]].up((int)value-0x40);
        update = true;
    } else {
        int i = m_midiEventToParamIndex[event];
        if (i != -1) {
            if (isSlider(event) || isButton(event))
                m_params[i].set7b(value);
            else
                m_params[i].up((int)value-0x40);
            update = true;
        }
    }
	if (update) {
		m_player->setFilterSettings(m_filterSettings);
	}
}

void SIDPlayer::saveInstrument()
{
    char n[256];
    snprintf(n, 256, "%s/%s", m_instrumentPath, m_instrumentNames[m_currentInstrument]);
    std::ofstream fo(n, std::ios::out);
    if (!fo.is_open()) {
        printf("couldn't open instrument file %s for writing\n", n);
    } else {
        m_player->getInstrument(m_currentInstrument)->save(fo);
        printf("saved %s\n", n);
    }
}

void SIDPlayer::loadInstrument()
{
    char n[256];
    snprintf(n, 256, "%s/%s", m_instrumentPath, m_instrumentNames[m_currentInstrument]);
    std::ifstream fi(n, std::ios::in);
    if (!fi.is_open()) {
        printf("couldn't open instrument file %s for reading\n", n);
    } else {
        m_player->getInstrument(m_currentInstrument)->load(fi);
        printf("loaded %s\n", n);
    }
}

void SIDPlayer::changeInstrument(int newInstrument)
{
    if (newInstrument >= m_numInstruments)
        m_currentInstrument = 0;
    else if (newInstrument < 0)
        m_currentInstrument = m_numInstruments-1;
    else
        m_currentInstrument = newInstrument;
    m_player->setCurrentInstrument(m_currentInstrument);
    Instrument* inst = m_player->getInstrument(m_currentInstrument);
    for(int i=0;i<m_numInstrumentParams;i++) {
        m_params[i].setBase(inst);
    }
}

void SIDPlayer::specialKeyEvent(int key, bool up, int modifiers)
{
	if (up)
		return;

	if (modifiers != GLUT_ACTIVE_ALT)
	{
		switch(key)
		{
		case GLUT_KEY_LEFT: m_paramSelectionX--; if (m_paramSelectionX < 0) m_paramSelectionX = m_paramGridWidths[m_paramSelectionY]-1; break;
		case GLUT_KEY_RIGHT: m_paramSelectionX++; if (m_paramSelectionX >= m_paramGridWidths[m_paramSelectionY]) m_paramSelectionX = 0; break;
		case GLUT_KEY_UP: m_paramSelectionY--; if (m_paramSelectionY < 0) m_paramSelectionY = m_paramGridHeight-1; break;
		case GLUT_KEY_DOWN: m_paramSelectionY++; if (m_paramSelectionY >= m_paramGridHeight) m_paramSelectionY = 0; break;
		case GLUT_KEY_F1: if (m_songMode) { playSong((m_currSong+m_numSongs-1)%m_numSongs); } else changeInstrument(m_currentInstrument-1); break;
		case GLUT_KEY_F2: if (m_songMode) { playSong((m_currSong+1)%m_numSongs);  } else changeInstrument(m_currentInstrument+1); break;
        case GLUT_KEY_F7: if (!m_songMode) saveInstrument(); break;
        case GLUT_KEY_F9: if (!m_songMode) loadInstrument(); break;
		default: break;
		}
		if (m_paramSelectionX >= m_paramGridWidths[m_paramSelectionY])
			m_paramSelectionX = m_paramGridWidths[m_paramSelectionY] - 1;
	} else {
		bool update = false;
		if (key == GLUT_KEY_UP) {
			m_params[m_paramGrid[m_paramSelectionX][m_paramSelectionY]].up();
			update = true;
		}
		if (key == GLUT_KEY_DOWN) {
			m_params[m_paramGrid[m_paramSelectionX][m_paramSelectionY]].down();
			update = true;
		}
		if (update) {
			m_player->setFilterSettings(m_filterSettings);
		}
	}
}

void SIDPlayer::mouseMotionFunc(int x, int y, bool pressed)
{
	if (!pressed) {
		m_lastMouseX = -1;
		m_lastMouseY = -1;
		return;
	}
	if (pressed && m_lastMouseY != -1)
	{
		int my = y - m_lastMouseY;
		if (my < -0)
			m_params[m_paramGrid[m_paramSelectionX][m_paramSelectionY]].up(-my);
		if (my > 0)
			m_params[m_paramGrid[m_paramSelectionX][m_paramSelectionY]].down(my);
		m_player->setFilterSettings(m_filterSettings);
	}
	m_lastMouseX = x;
	m_lastMouseY = y;
}

SIDPlayer::~SIDPlayer()
{
	glutSetKeyRepeat(GLUT_KEY_REPEAT_ON);
    m_audioCoreDriver->stopPlayback();
	delete m_audioCoreDriver;
	delete m_player;
	delete[] m_spectrum;
}

static SIDPlayer* s_sidplayer = NULL;


char programName[] = "sid";

void mainloop();
void exitfunc();
void idlefunc()									{ glutPostRedisplay(); }
void reshape( int w, int h )					{ width = w; height = h; glViewport(0, 0, w, h); glutSetWindowTitle(STR("%s  (%d, %d)",programName,width,height)); }

static vec2 p[4];
static vec2 mouse;
static int pressed = 0;
static int picked = 0;
void mousefunc(int x, int y)
{
	mouse = vec2(ivec2(x, y));
    if(pressed)
    {
        p[picked] = mouse;
    }
	if (s_sidplayer) {
		s_sidplayer->mouseMotionFunc(x, y, pressed);
	}
}
void mouseButtonFunc(int button, int state, int x, int y)
{
    pressed = !state;
    {
        int i;
		vec2 f = vec2(ivec2(x, y));
        picked = 0;
        float cx = p[0].x-f.x, cy = p[0].y-f.y;
        float dp = cx*cx+cy*cy;
        for(i=1;i<4;i++)
        {
            float dx = p[i].x-f.x, dy = p[i].y-f.y;
            float d = dx*dx+dy*dy;
            if(d < dp)
            {
                picked = i;
                dp = d;
            }
        }
    }
}

void mainloop()
{
#if 0
	static timeval lastt;
	timeval t;
	gettimeofday(&t, NULL);
	long long td = (((long long)t.tv_sec) * 1000000 + (long long)t.tv_usec) - (((long long)lastt.tv_sec) * 1000000 + (long long)lastt.tv_usec);
	printf("mainloop  %ld:%d td %d\n", t.tv_sec, t.tv_usec, (int)td);
	lastt = t;
#endif

    glClearColor(1,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT);

	glPointSize(5.0f);
    glLineWidth(1.0f);
	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

    s_sidplayer->drawColors(ivec2(800, 500));
	s_sidplayer->drawWaveform(ivec2(10, 500), ivec2(780, 500));
	s_sidplayer->drawFrequencySpectrum(ivec2(810, 750), ivec2(780, 500));
	s_sidplayer->drawParams(ivec2(10, 35));
	s_sidplayer->drawSongName(ivec2(10, 15));
    s_sidplayer->drawUnderrun(ivec2(10, 780));
#if 0
	glBegin(GL_POINTS);
	glVertex2f(mouse.x, mouse.y);
	glEnd();
#endif
    glutReportErrors();
    glutSwapBuffers();
}

void kbdfunc(unsigned char key, int x, int y)
{
	if (key == 27)	//ESC
		exitfunc();

	if (s_sidplayer) {
		s_sidplayer->keyEvent(key, false, glutGetModifiers());
	}
}
void specialfunc(int key, int x, int y)
{
	if (s_sidplayer) {
		s_sidplayer->specialKeyEvent(key, false, glutGetModifiers());
	}
}
void kbdupfunc(unsigned char key, int x, int y)
{
	if (s_sidplayer) {
		s_sidplayer->keyEvent(key, true, glutGetModifiers());
	}
}
void specialupfunc(int key, int x, int y)
{
	if (s_sidplayer) {
		s_sidplayer->specialKeyEvent(key, true, glutGetModifiers());
	}
}

int main()
{
	int c = 2;
	char debug[] = "";//"-gldebug";
	char* commandline[] = { programName, debug };
	glutInit(&c, commandline);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow(programName);
//	glutFullScreen();

	GLint sampleBuffers,samples;
	glGetIntegerv(GL_SAMPLE_BUFFERS, &sampleBuffers);
	glGetIntegerv(GL_SAMPLES, &samples);
//	glEnable(GL_MULTISAMPLE);

	glutSetKeyRepeat(GLUT_KEY_REPEAT_ON);
	glutKeyboardFunc(kbdfunc);
	glutSpecialFunc(specialfunc);
	glutKeyboardUpFunc(kbdupfunc);
	glutSpecialUpFunc(specialupfunc);
	glutPassiveMotionFunc(mousefunc);
	glutMotionFunc(mousefunc);
	glutMouseFunc(mouseButtonFunc);
	glutReshapeFunc(reshape);
	glutIdleFunc(idlefunc);

//	glEnable(GL_CULL_FACE);
//	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
    glDisable(GL_DEPTH_TEST);

	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	s_sidplayer = new SIDPlayer;

	setSwapInterval(1);

	glutDisplayFunc(mainloop);
	glutMainLoop();

	return 0;
}

void exitfunc()
{
	delete s_sidplayer;
	exit(0);
}
