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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PlayerLibSidplay.h"
#include "AudioCoreDriver.h"


static const float  sBitScaleFactor = 1.0f / 32768.0f;
static int          sInstanceCount = 0;

#define MIN(A,B)	((A) < (B) ? (A) : (B))


// ----------------------------------------------------------------------------
AudioCoreDriver::AudioCoreDriver()
// ----------------------------------------------------------------------------
{
	mIsInitialized = false;
	mInstanceId = sInstanceCount;
	sInstanceCount++;
}


// ----------------------------------------------------------------------------
AudioCoreDriver::~AudioCoreDriver()
// ----------------------------------------------------------------------------
{
	deinitialize();
	sInstanceCount--;
}

// ----------------------------------------------------------------------------
OSStatus AudioCoreDriver::queryStreamFormat(AudioDeviceID deviceID, AudioStreamBasicDescription& streamFormat)
// ----------------------------------------------------------------------------
{
    UInt32 propertySize = 0;
    OSStatus err = kAudioHardwareNoError;

    //query stream IDs of the device
    AudioObjectPropertyAddress prop3 = {
        kAudioDevicePropertyStreams,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMaster 
    };
    err = AudioObjectGetPropertyDataSize(deviceID, &prop3, 0, NULL, &propertySize);
    if(err != kAudioHardwareNoError) {
        printf("AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration) failed: %i\n", err);
        return err;
    }
    int numStreams = propertySize/sizeof(AudioStreamID);
    AudioStreamID *streamList = new AudioStreamID[numStreams];
    err = AudioObjectGetPropertyData(deviceID, &prop3, 0, NULL, &propertySize, streamList);
    if(err != kAudioHardwareNoError) {
        printf("AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration) failed: %i\n", err);
        delete[] streamList;
        return err;
    }

    //get stream 0 properties
    propertySize = sizeof(AudioStreamBasicDescription);
    AudioObjectPropertyAddress prop4 = {
        kAudioStreamPropertyVirtualFormat,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMaster 
    };
    err = AudioObjectGetPropertyData(streamList[0], &prop4, 0, NULL, &propertySize, &streamFormat);
    if (err != kAudioHardwareNoError) {
        delete[] streamList;
        return err;
    }
    delete[] streamList;
    return kAudioHardwareNoError;
}

// ----------------------------------------------------------------------------
void AudioCoreDriver::initialize(PlayerLibSidplay* player, int sampleRate, int bitsPerSample)
// ----------------------------------------------------------------------------
{
	if (mInstanceId != 0)
		return;

	mPlayer = player;
	mNumSamplesInBuffer = 512;
	mIsPlaying = false;
	mIsPlayingPreRenderedBuffer = false;
	mBufferUnderrunDetected = false;
    mBufferUnderrunCount = 0;
    mSpectrumTemporalSmoothing = 0.5f;
	
	mPreRenderedBuffer = NULL;
	mPreRenderedBufferSampleCount = 0;
	mPreRenderedBufferPlaybackPosition = 0;
	
	if (!mIsInitialized)
	{
        OSStatus err;

        //get default output device
		UInt32 propertySize = sizeof(mDeviceID);
        AudioObjectPropertyAddress prop1 = {kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop1, 0, NULL, &propertySize, &mDeviceID);
        if (err != kAudioHardwareNoError) {
            printf("AudioObjectGetPropertyData(kAudioHardwarePropertyDefaultOutputDevice) failed\n");
            return;
        }

		if (mDeviceID == kAudioDeviceUnknown)
			return;

        err = queryStreamFormat(mDeviceID, mStreamFormat);
        if (err != kAudioHardwareNoError) {
            printf("queryStreamFormat failed\n");
            return;
        }

        //add property listeners
#if USE_NEW_API
        AudioObjectPropertyAddress prop5 = {
            kAudioDevicePropertyStreamFormat,       //TODO this is deprecated, how to get this notification?
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster 
        };
        err = AudioObjectAddPropertyListener(mDeviceID, &prop5, streamFormatChanged, (void*)this);
        if (err != kAudioHardwareNoError) {
            printf("AudioObjectAddPropertyListener(streamFormatChanged) failed\n");
            return;
        }

        AudioObjectPropertyAddress prop6 = {
            kAudioDeviceProcessorOverload,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster 
        };
        err = AudioObjectAddPropertyListener(mDeviceID, &prop6, overloadDetected, (void*)this);
        if (err != kAudioHardwareNoError) {
            printf("AudioObjectAddPropertyListener(overloadDetected) failed\n");
            return;
        }

        AudioObjectPropertyAddress prop7 = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster 
        };
        err = AudioObjectAddPropertyListener(mDeviceID, &prop7, deviceChanged, (void*)this);
        if (err != kAudioHardwareNoError) {
            printf("AudioObjectAddPropertyListener(deviceChanged) failed\n");
            return;
        }
#else
		if (AudioDeviceAddPropertyListener(mDeviceID, 0, false, kAudioDevicePropertyStreamFormat, streamFormatChanged, (void*) this) != kAudioHardwareNoError)
			return;
		if (AudioDeviceAddPropertyListener(mDeviceID, 0, false, kAudioDeviceProcessorOverload, overloadDetected, (void*) this) != kAudioHardwareNoError)
			return;
		if (AudioHardwareAddPropertyListener(kAudioHardwarePropertyDefaultOutputDevice, deviceChanged, (void*) this)  != kAudioHardwareNoError)
			return;
#endif
		
		mSampleBuffer1 = new short[mNumSamplesInBuffer];
		memset(mSampleBuffer1, 0, sizeof(short) * mNumSamplesInBuffer);
		mSampleBuffer2 = new short[mNumSamplesInBuffer];
		memset(mSampleBuffer2, 0, sizeof(short) * mNumSamplesInBuffer);
        mSpectrumBuffer = new float[mNumSamplesInBuffer/2];
		memset(mSpectrumBuffer, 0, sizeof(float) * (mNumSamplesInBuffer/2));

        mSampleBuffer = mSampleBuffer1;
        mRetSampleBuffer = mSampleBuffer2;

		int bufferByteSize = mNumSamplesInBuffer * mStreamFormat.mChannelsPerFrame * sizeof(float);
		propertySize = sizeof(bufferByteSize);
#if USE_NEW_API
        AudioObjectPropertyAddress prop8 = {
            kAudioDevicePropertyBufferSize,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster 
        };
        err = AudioObjectSetPropertyData(mDeviceID, &prop8, 0, NULL, propertySize, &bufferByteSize);
        if (err != kAudioHardwareNoError) {
            printf("AudioObjectSetPropertyData(kAudioDevicePropertyBufferSize) failed\n");
            return;
        }
#else
		if (AudioDeviceSetProperty(mDeviceID, NULL, 0, false, kAudioDevicePropertyBufferSize, propertySize, &bufferByteSize) != kAudioHardwareNoError)
			return;
#endif
		mScaleFactor = sBitScaleFactor;
		mPreRenderedBufferScaleFactor = sBitScaleFactor;
		
		if (AudioDeviceCreateIOProcID(mDeviceID, emulationPlaybackProc, (void*) this, &mEmulationPlaybackProcID) != kAudioHardwareNoError)
		{
			delete[] mSampleBuffer1;
			mSampleBuffer1 = NULL;
			delete[] mSampleBuffer2;
			mSampleBuffer2 = NULL;
            mSampleBuffer = NULL;
            mRetSampleBuffer = NULL;
			delete[] mSpectrumBuffer;
			mSpectrumBuffer = NULL;
			return;
		}

		if (AudioDeviceCreateIOProcID(mDeviceID, preRenderedBufferPlaybackProc, (void*) this, &mPreRenderedBufferPlaybackProcID) != kAudioHardwareNoError)
		{
			delete[] mSampleBuffer1;
			mSampleBuffer1 = NULL;
			delete[] mSampleBuffer2;
			mSampleBuffer2 = NULL;
            mSampleBuffer = NULL;
            mRetSampleBuffer = NULL;
			delete[] mSpectrumBuffer;
			mSpectrumBuffer = NULL;
			return;
		}
	}
	
	mVolume = 1.0f;
	mIsInitialized = true;
}


// ----------------------------------------------------------------------------
void AudioCoreDriver::deinitialize()
// ----------------------------------------------------------------------------
{
	if (!mIsInitialized)
		return;
	
	stopPlayback();
	
	AudioDeviceDestroyIOProcID(mDeviceID, mEmulationPlaybackProcID);
	AudioDeviceDestroyIOProcID(mDeviceID, mPreRenderedBufferPlaybackProcID);
#if USE_NEW_API
    OSStatus err;
    AudioObjectPropertyAddress prop5 = {
        kAudioDevicePropertyStreamFormat,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    };
    err = AudioObjectRemovePropertyListener(mDeviceID, &prop5, streamFormatChanged, (void*)this);
    if (err != kAudioHardwareNoError)
            printf("AudioObjectRemovePropertyListener(streamFormatChanged) failed\n");

    AudioObjectPropertyAddress prop6 = {
        kAudioDeviceProcessorOverload,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    };
    err = AudioObjectRemovePropertyListener(mDeviceID, &prop6, overloadDetected, (void*)this);
    if (err != kAudioHardwareNoError)
            printf("AudioObjectRemovePropertyListener(overloadDetected) failed\n");

    AudioObjectPropertyAddress prop7 = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    };
    err = AudioObjectRemovePropertyListener(mDeviceID, &prop7, deviceChanged, (void*)this);
    if (err != kAudioHardwareNoError)
            printf("AudioObjectRemovePropertyListener(deviceChanged) failed\n");
#else
	AudioDeviceRemovePropertyListener(mDeviceID, 0, false, kAudioDevicePropertyStreamFormat, streamFormatChanged);
	AudioDeviceRemovePropertyListener(mDeviceID, 0, false, kAudioDeviceProcessorOverload, overloadDetected);
	AudioHardwareRemovePropertyListener(kAudioHardwarePropertyDefaultOutputDevice, deviceChanged);
#endif

	delete[] mSampleBuffer1;
	mSampleBuffer1 = NULL;
	delete[] mSampleBuffer2;
	mSampleBuffer2 = NULL;
    mSampleBuffer = NULL;
    mRetSampleBuffer = NULL;
    delete[] mSpectrumBuffer;
	mSpectrumBuffer = NULL;
	mIsInitialized = false;
}


// ----------------------------------------------------------------------------
void AudioCoreDriver::fillBuffer()
// ----------------------------------------------------------------------------
{
	if (!mIsPlaying)
		return;
	
    mPlayer->fillBuffer(mSampleBuffer, mNumSamplesInBuffer * sizeof(short));

#if 1       //compute frequency spectrum
	//discrete fourier transform
	//X_k = sum_[0..N-1] (x_n * e^(-i * 2 * pi * k * n / N))
	//e^(ix) = cos(x) + i*sin(x)
	//(a+bi)*(c+di) = (ac - bd) + (bc + ad)i
/*
	bool inverse = false;
	for(int k=0;k<numSamples;k++)
	{
		output[k].re = 0.0f;
		output[k].im = 0.0f;

		for(int n=0;n<numSamples;n++)
		{
			float t = -2.0f * 3.1415926535897932385f * k * n / numSamples;
			if( inverse )
				t = -t;

			float re = cosf(t);
			float im = sinf(t);

			output[k].re += input[n].re * re - input[n].im * im;
			output[k].im += input[n].im * re + input[n].re * im;
		}

		if( inverse )
		{
			output[k].re /= numSamples;
			output[k].im /= numSamples;
		}
	}
*/
	//specialized for forward transform, real input, and magnitude output
	for(int k=0;k<mNumSamplesInBuffer/2;k++)
	{
		float output_re = 0.0f;
		float output_im = 0.0f;

		for(int n=0;n<mNumSamplesInBuffer;n++)
		{
			float t = -2.0f * 3.1415926535897932385f * k * n / mNumSamplesInBuffer;

			float re = cosf(t);
			float im = sinf(t);
			float input_re = mSampleBuffer[n]/32768.0f;

			output_re += input_re * re;
			output_im += input_re * im;
		}
		mSpectrumBuffer[k] = mSpectrumTemporalSmoothing * mSpectrumBuffer[k] + (1.0f - mSpectrumTemporalSmoothing) * sqrtf(output_re*output_re + output_im*output_im) * 8192.0f;
	}
#endif

    short* s = mSampleBuffer;
    mSampleBuffer = mRetSampleBuffer;
    mRetSampleBuffer = s;
}


// ----------------------------------------------------------------------------
OSStatus AudioCoreDriver::emulationPlaybackProc(AudioDeviceID inDevice,
												const AudioTimeStamp *inNow,
												const AudioBufferList *inInputData,
												const AudioTimeStamp *inInputTime,
												AudioBufferList *outOutputData, 
												const AudioTimeStamp *inOutputTime,
												void *inClientData)
// ----------------------------------------------------------------------------
{
	AudioCoreDriver* driverInstance = reinterpret_cast<AudioCoreDriver*>(inClientData);

	register float* outBuffer	= (float*) outOutputData->mBuffers[0].mData;
	register short* audioBuffer = (short*) driverInstance->getSampleBuffer();
	register short* bufferEnd	= audioBuffer + driverInstance->getNumSamplesInBuffer();
	register float scaleFactor  = driverInstance->getScaleFactor();

	driverInstance->fillBuffer();

    if (driverInstance->mStreamFormat.mChannelsPerFrame == 1)
    {
        while (audioBuffer < bufferEnd)
            *outBuffer++ = (*audioBuffer++) * scaleFactor;
    }
    else if (driverInstance->mStreamFormat.mChannelsPerFrame == 2)
    {
        register float sample = 0.0f;
        
        while (audioBuffer < bufferEnd)
        {
            sample = (*audioBuffer++) * scaleFactor;
            *outBuffer++ = sample;
            *outBuffer++ = sample;
        }
    }
	else
	{
        register float sample = 0.0f;
        
        while (audioBuffer < bufferEnd)
        {
            sample = (*audioBuffer++) * scaleFactor;
			for (int i = 0; i < driverInstance->mStreamFormat.mChannelsPerFrame; i++)
				*outBuffer++ = sample;
        }
	}
	
	return 0;
}


// ----------------------------------------------------------------------------
OSStatus AudioCoreDriver::preRenderedBufferPlaybackProc(AudioDeviceID inDevice,
														const AudioTimeStamp *inNow,
														const AudioBufferList *inInputData,
														const AudioTimeStamp *inInputTime,
														AudioBufferList *outOutputData, 
														const AudioTimeStamp *inOutputTime,
														void *inClientData)
// ----------------------------------------------------------------------------
{
	AudioCoreDriver* driverInstance = reinterpret_cast<AudioCoreDriver*>(inClientData);
	
	register int samplesLeft = driverInstance->mPreRenderedBufferSampleCount - driverInstance->mPreRenderedBufferPlaybackPosition;
	if (samplesLeft <= 0)
		driverInstance->stopPreRenderedBufferPlayback();
	
	register int samplesToPlayThisSlice = MIN(samplesLeft, driverInstance->getNumSamplesInBuffer());
	register float* outBuffer	= (float*) outOutputData->mBuffers[0].mData;
	register short* audioBuffer = (short*) &driverInstance->mPreRenderedBuffer[driverInstance->mPreRenderedBufferPlaybackPosition];
	register short* bufferEnd	= audioBuffer + samplesToPlayThisSlice;
	register float scaleFactor  = driverInstance->getPreRenderedBufferScaleFactor();
	
	driverInstance->mPreRenderedBufferPlaybackPosition += samplesToPlayThisSlice;
	
	if (driverInstance->mStreamFormat.mChannelsPerFrame == 1)
    {
        while (audioBuffer < bufferEnd)
            *outBuffer++ = (*audioBuffer++) * scaleFactor;
    }
    else if (driverInstance->mStreamFormat.mChannelsPerFrame == 2)
    {
        register float sample = 0.0f;
        
        while (audioBuffer < bufferEnd)
        {
            sample = (*audioBuffer++) * scaleFactor;
            *outBuffer++ = sample;
            *outBuffer++ = sample;
        }
    }
	else
	{
        register float sample = 0.0f;
        
        while (audioBuffer < bufferEnd)
        {
            sample = (*audioBuffer++) * scaleFactor;
			for (int i = 0; i < driverInstance->mStreamFormat.mChannelsPerFrame; i++)
				*outBuffer++ = sample;
        }
	}
	 
	return 0;
}

#if USE_NEW_API
// ----------------------------------------------------------------------------
OSStatus AudioCoreDriver::deviceChanged(AudioObjectID                       inObjectID,
                                              UInt32                              inNumberAddresses,
                                              const AudioObjectPropertyAddress    inAddresses[],
                                              void*                               inClientData)
// ----------------------------------------------------------------------------
{
    AudioCoreDriver* driverInstance = reinterpret_cast<AudioCoreDriver*>(inClientData);

    printf("deviceChanged\n");
    
    bool wasPlaying = driverInstance->mIsPlaying;
    Float64 oldSampleRate = driverInstance->mStreamFormat.mSampleRate;

    driverInstance->deinitialize();
    driverInstance->initialize(driverInstance->mPlayer);

    if (driverInstance->mStreamFormat.mSampleRate != oldSampleRate)
        driverInstance->mPlayer->updateSampleRate(driverInstance->mStreamFormat.mSampleRate);

    if (wasPlaying)
        driverInstance->startPlayback();

	return kAudioHardwareNoError;
}
#else
// ----------------------------------------------------------------------------
OSStatus AudioCoreDriver::deviceChanged(AudioHardwarePropertyID inPropertyID,
										void* inClientData)
// ----------------------------------------------------------------------------
{
	if (inPropertyID == kAudioHardwarePropertyDefaultOutputDevice)
	{
		AudioCoreDriver* driverInstance = reinterpret_cast<AudioCoreDriver*>(inClientData);
		
		bool wasPlaying = driverInstance->mIsPlaying;
		Float64 oldSampleRate = driverInstance->mStreamFormat.mSampleRate;
		
		driverInstance->deinitialize();
		driverInstance->initialize(driverInstance->mPlayer);
		
		if (driverInstance->mStreamFormat.mSampleRate != oldSampleRate)
			driverInstance->mPlayer->updateSampleRate(driverInstance->mStreamFormat.mSampleRate);

		if (wasPlaying)
			driverInstance->startPlayback();
	}
	
	return kAudioHardwareNoError;
}
#endif

#if USE_NEW_API
// ----------------------------------------------------------------------------
OSStatus AudioCoreDriver::streamFormatChanged(AudioObjectID                       inObjectID,
                                              UInt32                              inNumberAddresses,
                                              const AudioObjectPropertyAddress    inAddresses[],
                                              void*                               inClientData)
// ----------------------------------------------------------------------------
{
	AudioCoreDriver* driverInstance = reinterpret_cast<AudioCoreDriver*>(inClientData);

    queryStreamFormat(inObjectID, driverInstance->mStreamFormat);

	if (driverInstance->mStreamFormat.mFormatID != kAudioFormatLinearPCM)
		return kAudioHardwareNoError;

	if (driverInstance->mPlayer)
		driverInstance->mPlayer->updateSampleRate(driverInstance->mStreamFormat.mSampleRate);
	
	return kAudioHardwareNoError;
}
#else
// ----------------------------------------------------------------------------
OSStatus AudioCoreDriver::streamFormatChanged(AudioDeviceID inDevice,
											  UInt32 inChannel,
											  Boolean isInput,
											  AudioDevicePropertyID inPropertyID,
											  void* inClientData)
// ----------------------------------------------------------------------------
{
	AudioCoreDriver* driverInstance = reinterpret_cast<AudioCoreDriver*>(inClientData);
	UInt32 propertySize = sizeof(driverInstance->mStreamFormat);
    printf("streamFormatChanged\n");

	if (AudioDeviceGetProperty(inDevice, inChannel, isInput, inPropertyID, &propertySize, &driverInstance->mStreamFormat) != kAudioHardwareNoError)
		return kAudioHardwareNoError;

	if (driverInstance->mStreamFormat.mFormatID != kAudioFormatLinearPCM)
		return kAudioHardwareNoError;

	if (driverInstance->mPlayer)
		driverInstance->mPlayer->updateSampleRate(driverInstance->mStreamFormat.mSampleRate);
	
	return kAudioHardwareNoError;
}
#endif

#if USE_NEW_API
// ----------------------------------------------------------------------------
OSStatus AudioCoreDriver::overloadDetected(AudioObjectID                    inObjectID,
                                        UInt32                              inNumberAddresses,
                                        const AudioObjectPropertyAddress    inAddresses[],
                                        void*                               inClientData)
// ----------------------------------------------------------------------------
{
    AudioCoreDriver* driverInstance = reinterpret_cast<AudioCoreDriver*>(inClientData);
        
    driverInstance->mBufferUnderrunCount++;
        
    if (driverInstance->mBufferUnderrunCount >= sBufferUnderrunLimit)
        driverInstance->setBufferUnderrunDetected(true);

	return kAudioHardwareNoError;
}
#else
// ----------------------------------------------------------------------------
OSStatus AudioCoreDriver::overloadDetected(AudioDeviceID inDevice,
										   UInt32 inChannel,
										   Boolean isInput,
										   AudioDevicePropertyID inPropertyID,
										   void* inClientData)
// ----------------------------------------------------------------------------
{
	if (inPropertyID == kAudioDeviceProcessorOverload)
	{
		AudioCoreDriver* driverInstance = reinterpret_cast<AudioCoreDriver*>(inClientData);
        
        driverInstance->mBufferUnderrunCount++;
        
        if (driverInstance->mBufferUnderrunCount > sBufferUnderrunLimit)
            driverInstance->setBufferUnderrunDetected(true);
	}
	
	return kAudioHardwareNoError;
}
#endif

// ----------------------------------------------------------------------------
bool AudioCoreDriver::startPlayback()
// ----------------------------------------------------------------------------
{
	if (mInstanceId != 0)
		return false;

	if (!mIsInitialized)
		return false;
		
	stopPreRenderedBufferPlayback();
	
	mIsPlaying = true;
	
	memset(mSampleBuffer, 0, sizeof(short) * mNumSamplesInBuffer);
	AudioDeviceStart(mDeviceID, mEmulationPlaybackProcID);

	return true;
}


// ----------------------------------------------------------------------------
void AudioCoreDriver::stopPlayback()
// ----------------------------------------------------------------------------
{
	if (!mIsInitialized)
		return;

	AudioDeviceStop(mDeviceID, mEmulationPlaybackProcID);

	mIsPlaying = false;
}


// ----------------------------------------------------------------------------
bool AudioCoreDriver::startPreRenderedBufferPlayback()
// ----------------------------------------------------------------------------
{
	if (mInstanceId != 0)
		return false;
	
	if (!mIsInitialized)
		return false;
	
	stopPlayback();

	mIsPlayingPreRenderedBuffer = true;
	
	memset(mSampleBuffer, 0, sizeof(short) * mNumSamplesInBuffer);
	AudioDeviceStart(mDeviceID, mPreRenderedBufferPlaybackProcID);
	
	return true;
}


// ----------------------------------------------------------------------------
void AudioCoreDriver::stopPreRenderedBufferPlayback()
// ----------------------------------------------------------------------------
{
	if (!mIsInitialized)
		return;
	
	AudioDeviceStop(mDeviceID, mPreRenderedBufferPlaybackProcID);
	
	mIsPlayingPreRenderedBuffer = false;
}


// ----------------------------------------------------------------------------
void AudioCoreDriver::setPreRenderedBuffer(short* inBuffer, int inBufferLength)
// ----------------------------------------------------------------------------
{
	mPreRenderedBuffer = inBuffer;
	mPreRenderedBufferSampleCount = inBufferLength;
}


// ----------------------------------------------------------------------------
void AudioCoreDriver::setVolume(float volume)
// ----------------------------------------------------------------------------
{
	mVolume = volume;
	mScaleFactor = sBitScaleFactor * volume;
}


// ----------------------------------------------------------------------------
void AudioCoreDriver::setPreRenderedBufferVolume(float volume)
// ----------------------------------------------------------------------------
{
	mPreRenderedBufferVolume = volume;
	mPreRenderedBufferScaleFactor = sBitScaleFactor * volume;
}

