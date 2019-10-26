/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

const size_t MAXFRAMESIZE = 65536;
const size_t MAXCHANNELSPERCLIENT = 4;

/*
 For lazi-ness and stateless-ness Jammernetz for now works with only a single message type for now. This is the format:

  | AUDIODATA type message - this is sent by the client to the server, and also the server sends its mixing result with this type of message
  | JammerNetzAudioHeader
  |                                  | Audio - 2 Blocks, one active data and one FEC block containing the previous active block
  | JammerNetzHeader                 | JammerNetzAudioBlock                                                                     | AudioData for Block                       |
  | magic0 magic1 magic2 messageType | timestamp messageCounter channelSetup            numChannels numberOfSamples sampleRate  | numChannels * numberOfSamples audio bytes | 
  | uint8  uint8  uint8  uint8       | double    uint64         JammerNetzChannelSetup  uint8       uint16          uint16      | uint16                                    |


*/

struct JammerNetzHeader {
	uint8 magic0;
	uint8 magic1;
	uint8 magic2;
	uint8 messageType;
};

enum JammerNetzChannelTarget {
	Unused = 0,
	Left,
	Right, 
	Mono,
	SendOnly,
};

struct JammerNetzSingleChannelSetup {
	JammerNetzSingleChannelSetup();
	JammerNetzSingleChannelSetup(uint8 target);
	uint8 target;
	float volume;
	float balanceLeftRight;
};

struct JammerNetzChannelSetup {
	JammerNetzChannelSetup();
	JammerNetzChannelSetup(std::vector<JammerNetzSingleChannelSetup> const &channelInfo);
	JammerNetzSingleChannelSetup channels[MAXCHANNELSPERCLIENT];
};

struct JammerNetzAudioBlock {
	double timestamp; // Using JUCE's high resolution timer
	juce::uint64 messageCounter;
	JammerNetzChannelSetup channelSetup;
	uint8 numchannels;
	uint16 numberOfSamples;
	uint16 sampleRate;
};

struct JammerNetzAudioHeader {
	JammerNetzHeader header;
	JammerNetzAudioBlock audioBlock[1];
};

struct AudioBlock {
	AudioBlock() = default;
	AudioBlock(AudioBlock const &other) = default;
	AudioBlock(double timestamp, uint64 messageCounter, uint16 sampleRate, JammerNetzChannelSetup const &channelSetup, std::shared_ptr<AudioBuffer<float>> audioBuffer);
	double timestamp; // Using JUCE's high resolution timer
	juce::uint64 messageCounter;
	uint16 sampleRate;
	JammerNetzChannelSetup channelSetup;
	std::shared_ptr<AudioBuffer<float>> audioBuffer;
};

class JammerNetzMessage {
public:
	enum MessageType {
		AUDIODATA = 1,
		FLARE = 255
	};

	virtual void serialize(uint8 *output, int &byteswritten) const = 0;
	static std::shared_ptr<JammerNetzMessage> deserialize(uint8 *data, int bytes);

protected:
	int writeHeader(uint8 *output, uint8 messageType) const;
};

class JammerNetzAudioData : public JammerNetzMessage {
public:
	JammerNetzAudioData(uint8 *data, int bytes);
	JammerNetzAudioData(uint64 messageCounter, double timestamp, JammerNetzChannelSetup const &channelSetup, std::shared_ptr<AudioBuffer<float>> audioBuffer);
	JammerNetzAudioData(AudioBlock const &audioBlock);

	std::shared_ptr<JammerNetzAudioData> createFillInPackage(uint64 messageNumber) const;
	std::shared_ptr<JammerNetzAudioData> createPrePaddingPackage() const;

	virtual void serialize(uint8 *output, int &byteswritten) const override;
	void serialize(uint8 *output, int &byteswritten, std::shared_ptr<AudioBlock> src, uint16 sampleRate, uint16 reductionFactor) const;

	// Read access, those use the "active block"
	std::shared_ptr<AudioBuffer<float>> audioBuffer() const;
	uint64 messageCounter() const;
	double timestamp() const;
	JammerNetzChannelSetup channelSetup() const;

private:
	void appendAudioBuffer(AudioBuffer<float> &buffer, uint8 *output, int &writeIndex, uint16 reductionFactor) const;
	std::shared_ptr<AudioBlock> readAudioHeaderAndBytes(uint8 *data, int &bytesread);
	void readAudioBytes(uint8 *data, int numchannels, int numsamples, std::shared_ptr<AudioBuffer<float>> destBuffer, int &bytesRead, int upsampleRate);

	std::shared_ptr<AudioBlock> audioBlock_;
	std::shared_ptr<AudioBlock> fecBlock_;
	std::shared_ptr<AudioBlock> activeBlock_;
};

class JammerNetzAudioOrder {
public:
	bool operator() (std::shared_ptr<JammerNetzAudioData> const &data1, std::shared_ptr<JammerNetzAudioData> const &data2) {
		return data1->messageCounter() > data2->messageCounter();
	}
};

class JammerNetzFlare : public JammerNetzMessage {
public:
	JammerNetzFlare();
	virtual void serialize(uint8 *output, int &byteswritten) const override;
};
