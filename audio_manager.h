
#ifndef AUDIOINTERFACE_AUDIOMANAGER_H
#define AUDIOINTERFACE_AUDIOMANAGER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

namespace AudioInterface 
{
  class AudioManager 
  {
    public:
      AudioManager();
      ~AudioManager();
    private:
      const int max_recording_seconds;
      const int max_recording_buffer;
      Uint8* audio_buffer;
      SDL_AudioSpec desired_recording_spec;
      SDL_AudioSpec received_recording_spec;
      SDL_AudioSpec desired_playback_spec;
      SDL_AudioSpec received_playback_spec;
      Uint32 buffer_byte_position;
      Uint32 buffer_byte_max_position;
      Uint32 buffer_byte_size;
  }
}

#endif