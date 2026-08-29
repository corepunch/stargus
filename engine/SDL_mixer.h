#ifndef STARGUS_SDL_MIXER_COMPAT_H
#define STARGUS_SDL_MIXER_COMPAT_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIX_CHANNELS 8
#define MIX_DEFAULT_FREQUENCY 44100
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define MIX_DEFAULT_FORMAT AUDIO_S16LSB
#else
#define MIX_DEFAULT_FORMAT AUDIO_S16MSB
#endif
#define MIX_DEFAULT_CHANNELS 2
#define MIX_MAX_VOLUME SDL_MIX_MAXVOLUME

typedef struct Mix_Chunk {
	int allocated;
	Uint8 *abuf;
	Uint32 alen;
	Uint8 volume;
} Mix_Chunk;

typedef struct Mix_Music Mix_Music;

int Mix_Init(int flags);
void Mix_Quit(void);
int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void Mix_CloseAudio(void);
int Mix_AllocateChannels(int numchans);

Mix_Chunk *Mix_LoadWAV(const char *file);
Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *src, int freesrc);
void Mix_FreeChunk(Mix_Chunk *chunk);

Mix_Music *Mix_LoadMUS(const char *file);
Mix_Music *Mix_LoadMUS_RW(SDL_RWops *src, int freesrc);
void Mix_FreeMusic(Mix_Music *music);

int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int Mix_PlayMusic(Mix_Music *music, int loops);
int Mix_HaltChannel(int channel);
int Mix_HaltMusic(void);
int Mix_Playing(int channel);
int Mix_PlayingMusic(void);
Mix_Chunk *Mix_GetChunk(int channel);

int Mix_Volume(int channel, int volume);
int Mix_VolumeMusic(int volume);
int Mix_SetPanning(int channel, Uint8 left, Uint8 right);

void Mix_ChannelFinished(void (SDLCALL *callback)(int channel));
void Mix_HookMusicFinished(void (SDLCALL *callback)(void));
void Mix_Resume(int channel);
void Mix_ResumeMusic(void);

int Mix_GetNumChunkDecoders(void);
const char *Mix_GetChunkDecoder(int index);
int Mix_GetNumMusicDecoders(void);
const char *Mix_GetMusicDecoder(int index);
SDL_bool Mix_HasMusicDecoder(const char *name);
int Mix_SetTimidityCfg(const char *path);

#define Mix_GetError SDL_GetError

#ifdef __cplusplus
}
#endif

#endif
