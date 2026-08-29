#include "SDL_mixer.h"

#include <algorithm>
#include <cstring>
#include <vector>

struct Mix_Music {
	Uint8 *data = nullptr;
	Uint32 length = 0;
};

namespace {

struct Channel {
	Mix_Chunk *chunk = nullptr;
	Uint32 position = 0;
	int loops = 0;
	int volume = MIX_MAX_VOLUME;
	Uint8 left = 255;
	Uint8 right = 255;
};

SDL_AudioDeviceID AudioDevice = 0;
SDL_AudioSpec AudioSpec{};
std::vector<Channel> Channels(MIX_CHANNELS);
Mix_Music *Music = nullptr;
Uint32 MusicPosition = 0;
int MusicLoops = 0;
int MusicVolume = MIX_MAX_VOLUME;
void (SDLCALL *ChannelFinishedCallback)(int) = nullptr;
void (SDLCALL *MusicFinishedCallback)() = nullptr;

class AudioLock {
public:
	AudioLock() { if (AudioDevice) SDL_LockAudioDevice(AudioDevice); }
	~AudioLock() { if (AudioDevice) SDL_UnlockAudioDevice(AudioDevice); }
};

int ClampVolume(int volume)
{
	return std::clamp(volume, 0, MIX_MAX_VOLUME);
}

Sint16 MixSample(Sint16 output, Sint16 input, int volume, int pan)
{
	const int scaled = static_cast<int>(input) * volume * pan / (MIX_MAX_VOLUME * 255);
	return static_cast<Sint16>(std::clamp(static_cast<int>(output) + scaled, -32768, 32767));
}

void MixStereo(Sint16 *output, const Sint16 *input, Uint32 frames,
	int volume, int left, int right)
{
	for (Uint32 frame = 0; frame < frames; ++frame) {
		output[frame * 2] = MixSample(output[frame * 2], input[frame * 2], volume, left);
		output[frame * 2 + 1] = MixSample(output[frame * 2 + 1], input[frame * 2 + 1], volume, right);
	}
}

void SDLCALL FillAudio(void *, Uint8 *stream, int length)
{
	SDL_memset(stream, 0, length);
	auto *output = reinterpret_cast<Sint16 *>(stream);
	const Uint32 requestedFrames = static_cast<Uint32>(length) / (sizeof(Sint16) * 2);

	if (Music) {
		Uint32 outputFrame = 0;
		while (outputFrame < requestedFrames && Music) {
			const Uint32 availableFrames = (Music->length - MusicPosition) / (sizeof(Sint16) * 2);
			if (availableFrames == 0) {
				Music = nullptr;
				MusicPosition = 0;
				if (MusicFinishedCallback) MusicFinishedCallback();
				break;
			}
			const Uint32 frames = std::min(requestedFrames - outputFrame, availableFrames);
			MixStereo(output + outputFrame * 2,
				reinterpret_cast<const Sint16 *>(Music->data + MusicPosition),
				frames, MusicVolume, 255, 255);
			MusicPosition += frames * sizeof(Sint16) * 2;
			outputFrame += frames;
			if (MusicPosition >= Music->length) {
				if (MusicLoops == -1 || MusicLoops-- > 0) {
					MusicPosition = 0;
				} else {
					Music = nullptr;
					MusicPosition = 0;
					if (MusicFinishedCallback) MusicFinishedCallback();
				}
			}
		}
	}

	for (size_t index = 0; index < Channels.size(); ++index) {
		Channel &channel = Channels[index];
		if (!channel.chunk) continue;

		Uint32 outputFrame = 0;
		while (outputFrame < requestedFrames && channel.chunk) {
			const Uint32 availableFrames =
				(channel.chunk->alen - channel.position) / (sizeof(Sint16) * 2);
			if (availableFrames == 0) {
				channel.chunk = nullptr;
				channel.position = 0;
				if (ChannelFinishedCallback) ChannelFinishedCallback(static_cast<int>(index));
				break;
			}
			const Uint32 frames = std::min(requestedFrames - outputFrame, availableFrames);
			const int volume = channel.volume * channel.chunk->volume / MIX_MAX_VOLUME;
			MixStereo(output + outputFrame * 2,
				reinterpret_cast<const Sint16 *>(channel.chunk->abuf + channel.position),
				frames, volume, channel.left, channel.right);
			channel.position += frames * sizeof(Sint16) * 2;
			outputFrame += frames;
			if (channel.position >= channel.chunk->alen) {
				if (channel.loops == -1 || channel.loops-- > 0) {
					channel.position = 0;
				} else {
					channel.chunk = nullptr;
					channel.position = 0;
					if (ChannelFinishedCallback) ChannelFinishedCallback(static_cast<int>(index));
				}
			}
		}
	}
}

Mix_Chunk *LoadChunk(SDL_RWops *source, int freeSource)
{
	if (!source) return nullptr;
	SDL_AudioSpec sourceSpec{};
	Uint8 *sourceData = nullptr;
	Uint32 sourceLength = 0;
	if (!SDL_LoadWAV_RW(source, freeSource, &sourceSpec, &sourceData, &sourceLength)) return nullptr;

	SDL_AudioCVT converter{};
	if (SDL_BuildAudioCVT(&converter, sourceSpec.format, sourceSpec.channels, sourceSpec.freq,
		AudioSpec.format, AudioSpec.channels, AudioSpec.freq) < 0) {
		SDL_FreeWAV(sourceData);
		return nullptr;
	}

	Uint8 *data = sourceData;
	Uint32 length = sourceLength;
	if (converter.needed) {
		converter.len = static_cast<int>(sourceLength);
		converter.buf = static_cast<Uint8 *>(SDL_malloc(sourceLength * converter.len_mult));
		if (!converter.buf) {
			SDL_FreeWAV(sourceData);
			return nullptr;
		}
		SDL_memcpy(converter.buf, sourceData, sourceLength);
		SDL_FreeWAV(sourceData);
		if (SDL_ConvertAudio(&converter) < 0) {
			SDL_free(converter.buf);
			return nullptr;
		}
		data = converter.buf;
		length = static_cast<Uint32>(converter.len_cvt);
	}

	auto *chunk = static_cast<Mix_Chunk *>(SDL_calloc(1, sizeof(Mix_Chunk)));
	if (!chunk) {
		SDL_free(data);
		return nullptr;
	}
	chunk->allocated = 1;
	chunk->abuf = data;
	chunk->alen = length;
	chunk->volume = MIX_MAX_VOLUME;
	return chunk;
}

void StopChannel(Channel &channel, int index)
{
	if (!channel.chunk) return;
	channel.chunk = nullptr;
	channel.position = 0;
	if (ChannelFinishedCallback) ChannelFinishedCallback(index);
}

} // namespace

extern "C" {

int Mix_Init(int) { return 0; }
void Mix_Quit(void) {}

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize)
{
	if (format != MIX_DEFAULT_FORMAT || channels != 2) {
		return SDL_SetError("Stargus mixer requires stereo signed 16-bit audio");
	}
	SDL_AudioSpec desired{};
	desired.freq = frequency;
	desired.format = format;
	desired.channels = static_cast<Uint8>(channels);
	desired.samples = static_cast<Uint16>(chunksize);
	desired.callback = FillAudio;
	AudioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &AudioSpec, 0);
	if (!AudioDevice) return -1;
	SDL_PauseAudioDevice(AudioDevice, 0);
	return 0;
}

void Mix_CloseAudio(void)
{
	if (AudioDevice) SDL_CloseAudioDevice(AudioDevice);
	AudioDevice = 0;
	Music = nullptr;
	for (Channel &channel : Channels) channel = {};
}

int Mix_AllocateChannels(int count)
{
	AudioLock lock;
	if (count < 0) return static_cast<int>(Channels.size());
	if (count < static_cast<int>(Channels.size())) {
		for (int i = count; i < static_cast<int>(Channels.size()); ++i) StopChannel(Channels[i], i);
	}
	Channels.resize(static_cast<size_t>(count));
	return count;
}

Mix_Chunk *Mix_LoadWAV(const char *file)
{
	return LoadChunk(SDL_RWFromFile(file, "rb"), 1);
}

Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *source, int freeSource)
{
	return LoadChunk(source, freeSource);
}

void Mix_FreeChunk(Mix_Chunk *chunk)
{
	if (!chunk) return;
	AudioLock lock;
	for (size_t i = 0; i < Channels.size(); ++i) {
		if (Channels[i].chunk == chunk) StopChannel(Channels[i], static_cast<int>(i));
	}
	if (chunk->allocated) SDL_free(chunk->abuf);
	SDL_free(chunk);
}

Mix_Music *Mix_LoadMUS(const char *file)
{
	return Mix_LoadMUS_RW(SDL_RWFromFile(file, "rb"), 1);
}

Mix_Music *Mix_LoadMUS_RW(SDL_RWops *source, int freeSource)
{
	// TODO: Stream music from SDL_RWops instead of decoding the whole WAV into memory.
	Mix_Chunk *chunk = LoadChunk(source, freeSource);
	if (!chunk) return nullptr;
	auto *music = new Mix_Music;
	music->data = chunk->abuf;
	music->length = chunk->alen;
	chunk->allocated = 0;
	SDL_free(chunk);
	return music;
}

void Mix_FreeMusic(Mix_Music *music)
{
	if (!music) return;
	AudioLock lock;
	if (Music == music) {
		Music = nullptr;
		MusicPosition = 0;
	}
	SDL_free(music->data);
	delete music;
}

int Mix_PlayChannel(int requested, Mix_Chunk *chunk, int loops)
{
	if (!chunk || !AudioDevice) return -1;
	AudioLock lock;
	int channel = requested;
	if (channel < 0) {
		channel = 0;
		while (channel < static_cast<int>(Channels.size()) && Channels[channel].chunk) ++channel;
	}
	if (channel < 0 || channel >= static_cast<int>(Channels.size())) return -1;
	Channels[channel] = {chunk, 0, loops, MIX_MAX_VOLUME, 255, 255};
	return channel;
}

int Mix_PlayMusic(Mix_Music *music, int loops)
{
	if (!music || !AudioDevice) return -1;
	AudioLock lock;
	Music = music;
	MusicPosition = 0;
	MusicLoops = loops;
	return 0;
}

int Mix_HaltChannel(int requested)
{
	AudioLock lock;
	if (requested == -1) {
		for (size_t i = 0; i < Channels.size(); ++i) StopChannel(Channels[i], static_cast<int>(i));
		return 0;
	}
	if (requested < 0 || requested >= static_cast<int>(Channels.size())) return -1;
	StopChannel(Channels[requested], requested);
	return 0;
}

int Mix_HaltMusic(void)
{
	AudioLock lock;
	Music = nullptr;
	MusicPosition = 0;
	return 0;
}

int Mix_Playing(int requested)
{
	AudioLock lock;
	if (requested == -1) {
		return static_cast<int>(std::count_if(Channels.begin(), Channels.end(),
			[](const Channel &channel) { return channel.chunk != nullptr; }));
	}
	return requested >= 0 && requested < static_cast<int>(Channels.size()) && Channels[requested].chunk;
}

int Mix_PlayingMusic(void)
{
	AudioLock lock;
	return Music != nullptr;
}

Mix_Chunk *Mix_GetChunk(int channel)
{
	AudioLock lock;
	if (channel < 0 || channel >= static_cast<int>(Channels.size())) return nullptr;
	return Channels[channel].chunk;
}

int Mix_Volume(int requested, int volume)
{
	AudioLock lock;
	if (requested == -1) {
		const int previous = Channels.empty() ? 0 : Channels.front().volume;
		if (volume >= 0) for (Channel &channel : Channels) channel.volume = ClampVolume(volume);
		return previous;
	}
	if (requested < 0 || requested >= static_cast<int>(Channels.size())) return -1;
	const int previous = Channels[requested].volume;
	if (volume >= 0) Channels[requested].volume = ClampVolume(volume);
	return previous;
}

int Mix_VolumeMusic(int volume)
{
	AudioLock lock;
	const int previous = MusicVolume;
	if (volume >= 0) MusicVolume = ClampVolume(volume);
	return previous;
}

int Mix_SetPanning(int channel, Uint8 left, Uint8 right)
{
	AudioLock lock;
	if (channel < 0 || channel >= static_cast<int>(Channels.size())) return 0;
	Channels[channel].left = left;
	Channels[channel].right = right;
	return 1;
}

void Mix_ChannelFinished(void (SDLCALL *callback)(int))
{
	AudioLock lock;
	ChannelFinishedCallback = callback;
}

void Mix_HookMusicFinished(void (SDLCALL *callback)(void))
{
	AudioLock lock;
	MusicFinishedCallback = callback;
}
void Mix_Resume(int) {}
void Mix_ResumeMusic(void) {}

int Mix_GetNumChunkDecoders(void) { return 1; }
const char *Mix_GetChunkDecoder(int index) { return index == 0 ? "WAVE" : nullptr; }
int Mix_GetNumMusicDecoders(void) { return 1; }
const char *Mix_GetMusicDecoder(int index) { return index == 0 ? "WAVE" : nullptr; }
SDL_bool Mix_HasMusicDecoder(const char *name)
{
	return name && (SDL_strcasecmp(name, "WAVE") == 0 || SDL_strcasecmp(name, "WAV") == 0)
		? SDL_TRUE : SDL_FALSE;
}
int Mix_SetTimidityCfg(const char *) { return 0; }

} // extern "C"
