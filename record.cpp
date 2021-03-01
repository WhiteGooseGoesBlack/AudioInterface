#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <sstream>

using namespace std;


// defining global constants
const int MAX_RECORDING_SECONDS = 15; //maximum time of recording
const int RECORDING_BUFFER_SECONDS = 16; //buffersize, providing a safe pad of 1 seconds more than
const int SCREEN_WIDTH = 640; //window width
const int SCREEN_HEIGHT = 480; //window height
//MAX_RECORDING_SECONDS

//enumerations for recording states used in switchcase instruction
enum RecordingState{
    DEVICE_SELECTION,
    WAITING,
    STOPPED,
    RECORDING,
    PAUSE,
    RECORDED,
    PLAYBACK,
    ERROR
};


int gRecordingDeviceCount = 0; //number of recording devices in host machine
SDL_AudioSpec gReceivedRecordingSpec; //the available spec for recording
SDL_AudioSpec gReceivedPlaybackSpec; //the available spec for playback
Uint8* gRecordingBuffer = NULL; //the buffer holding the recorded sound
Uint32 gBufferByteSize = 0; 
Uint32 gBufferBytePosition = 0; //specifies the current location in gRecordingBuffer
Uint32 gBufferByteMaxPosition = 0; //defines an upper bound for gBufferBytePosition
Uint32 gBufferByteRecordedPosition = 0; //defines the place where the recording has stopped
SDL_Window* gWindow; //pointer to the window
SDL_Surface* gSurface; //pointer to surface
SDL_Texture* gTexture; //pointer to texture
SDL_Renderer* gRenderer; //pointer to renderer

void audioRecordingCallback( void*, Uint8*, int);
void audioPlaybackCallback( void*, Uint8*, int);
void setRecordingSpec(SDL_AudioSpec*);
void setPlaybackSpec(SDL_AudioSpec*);
void close(); //frees the allocated buffers and terminates SDL
void reportError(const char*); //printing proper error messages to the screen
void show_menu(RecordingState); //shows appropariate menu for each state
bool loadMedia(); //show a simple window on the screen to get SDL events

int main() {
    cout<<"Recording your voice for 5 seconds. then starting the playback automatically."<<endl;
    if(SDL_Init( SDL_INIT_EVERYTHING ) < 0) //make sure SDL initilizes correctly
        reportError("Initilizing SDL error.");
    //Audio device IDs
    SDL_AudioDeviceID recordingDeviceId = 0;
    SDL_AudioDeviceID playbackDeviceId = 0;
    gRecordingDeviceCount = SDL_GetNumAudioDevices(SDL_TRUE); //get total number of recording devices
    cout << "Total recording devices found: "<< gRecordingDeviceCount << endl;
    for(int i=0; i<gRecordingDeviceCount; i++)
        cout<<i<<": "<<SDL_GetAudioDeviceName(i, SDL_TRUE);
    
    if(!loadMedia())
        exit(1);
    int index;
    int bytesPerSample;
    int bytesPerSecond;
    SDL_AudioSpec desiredRecordingSpec, desiredPlaybackSpec;
    SDL_Event e;
    RecordingState current_state = DEVICE_SELECTION; //the state variable for the main loop
    bool quit = false;
    //main loop of the program
    while(!quit) {
        while(SDL_PollEvent(&e)) {
            if( e.type == SDL_QUIT )
				quit = true;
					
            if(e.type == SDL_KEYDOWN) //if a key is pressed on keyboard
                if(e.key.keysym.sym == SDLK_q) //if q is pressed, exit
                    quit = true;
            switch(current_state) {
                
                case WAITING: //waiting for user to start recording
                    if(e.type == SDL_KEYDOWN)
                        if(e.key.keysym.sym == SDLK_r) {
                            current_state = RECORDING;
                            show_menu(current_state);
                            gBufferBytePosition = 0; //reseting the buffer
                            gBufferByteRecordedPosition = 0; //reseting the buffer
                            SDL_PauseAudioDevice( recordingDeviceId, SDL_FALSE ); //start recording
                        }
                    break;
                
                case RECORDING: //activate audio device and record
                    if(e.type == SDL_KEYDOWN) { 
                        switch(e.key.keysym.sym) {
                            case SDLK_s: // stop the recording
                                current_state = RECORDED;
                                show_menu(current_state);
                                SDL_PauseAudioDevice( recordingDeviceId, SDL_TRUE );
                                gBufferByteRecordedPosition = gBufferBytePosition;
                                gBufferBytePosition = 0; //preparing for playback
                                break;
                            
                            case SDLK_p: // pause the recording
                                current_state = PAUSE;
                                show_menu(current_state);
                                break;
                            
                            default: // do nothing if another key other than 'p' or 's' is pressed
                                break;
                        }
                    }
                    break;
                
                case RECORDED: //wait for user to save or play the recorded voice
                    if(e.type == SDL_KEYDOWN) {
                        if(e.key.keysym.sym == SDLK_p) { //start playback
                            gBufferBytePosition = 0; //preparing for playback 
                            current_state = PLAYBACK;
                            show_menu(current_state);
                            SDL_PauseAudioDevice( playbackDeviceId, SDL_FALSE ); //start playback
                        }
                    }
                    break;

            }
                
            
        }


        switch(current_state) {
            case DEVICE_SELECTION: // user selects one of available devices
                cout<<"Select one of the devices listed above for recording: ";
                //int index;
                cin >> index;
                if(index >= gRecordingDeviceCount) {
                    cout<<"Error: out of range device selected."<<endl;
                    exit(1);
                }
                cout << "Using " <<index << ": "<< SDL_GetAudioDeviceName(index, SDL_TRUE)<<" for recording"<<endl;
                //SDL_AudioSpec desiredRecordingSpec, desiredPlaybackSpec;
                setRecordingSpec(&desiredRecordingSpec);
                setPlaybackSpec(&desiredPlaybackSpec);
                //opening the device for recording
                recordingDeviceId = SDL_OpenAudioDevice( SDL_GetAudioDeviceName( index, SDL_TRUE ), SDL_TRUE, 
                &desiredRecordingSpec, &gReceivedRecordingSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE );
                //opening the device for playback
                playbackDeviceId = SDL_OpenAudioDevice( NULL, SDL_FALSE, 
                &desiredPlaybackSpec, &gReceivedPlaybackSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE );
                //calculating some audio quantities based on received spec for recording and playback
                bytesPerSample = gReceivedRecordingSpec.channels * 
                ( SDL_AUDIO_BITSIZE( gReceivedRecordingSpec.format ) / 8 );
                bytesPerSecond = gReceivedRecordingSpec.freq * bytesPerSample;
                gBufferByteSize = RECORDING_BUFFER_SECONDS * bytesPerSecond;
                gBufferByteMaxPosition = MAX_RECORDING_SECONDS * bytesPerSecond;
                gRecordingBuffer = new Uint8[ gBufferByteSize ];
                memset( gRecordingBuffer, 0, gBufferByteSize );
                current_state = WAITING;
                show_menu(current_state);
                break;
            
            case RECORDING: //activate audio device and record
                //Lock callback
                SDL_LockAudioDevice( recordingDeviceId );

                //check if the buffer has reached maximum capacity
                if( gBufferBytePosition > gBufferByteMaxPosition ) {
                    //Stop recording audio
                    SDL_PauseAudioDevice( recordingDeviceId, SDL_TRUE ); //stop recording
                    cout << "You reached recording limit!"<<endl;
                    current_state = RECORDED;
                    gBufferByteRecordedPosition = gBufferBytePosition;
                }
                SDL_UnlockAudioDevice( recordingDeviceId );
                break;
            
            case PLAYBACK: //playing the recorded voice back
                SDL_LockAudioDevice(playbackDeviceId);
                if(gBufferBytePosition > gBufferByteRecordedPosition) { //stop playback
                    SDL_PauseAudioDevice(playbackDeviceId, SDL_TRUE);
                    current_state = RECORDED;
                    show_menu(current_state);
                    cout << "Finished playback\n";
                }
                SDL_UnlockAudioDevice(playbackDeviceId);
                break;  

        }
        //SDL_Delay(40);
    }

    close();
}

void setRecordingSpec(SDL_AudioSpec* desired) {
    SDL_zero(*desired);
    desired -> freq = 44100;
    desired -> format = AUDIO_F32;
    desired -> channels = 2;
    desired -> samples = 4096;
    desired -> callback = audioRecordingCallback;
}

void setPlaybackSpec(SDL_AudioSpec* desired) {
    SDL_zero(*desired);
    desired -> freq = 44100;
    desired -> format = AUDIO_F32;
    desired -> channels = 2;
    desired -> samples = 4096;
    desired -> callback = audioPlaybackCallback;
}

void audioRecordingCallback( void* userdata, Uint8* stream, int len ) {
    memcpy( &gRecordingBuffer[ gBufferBytePosition ], stream, len );
    gBufferBytePosition += len;
}

void audioPlaybackCallback( void* userdata, Uint8* stream, int len )
{
    //Copy audio to stream
    memcpy( stream, &gRecordingBuffer[ gBufferBytePosition ], len );

    //Move along buffer
    gBufferBytePosition += len;
}

void close() {
    if(gRecordingBuffer != NULL) {
        delete[] gRecordingBuffer;
        gRecordingBuffer = NULL;
    }
    SDL_Quit();
}

void reportError(const char* msg) { //reports the proper error message then terminates
    cout<<"An error happend. "<<msg<<" "<<"SDL_ERROR: "<<SDL_GetError()<<endl;
    exit(1);
}

void show_menu(RecordingState current_state) { //showing different texts on the screen due to each state
    switch(current_state) {
        case WAITING:
            cout << "1: Press 'r' to start recording\n2: Press 'q' to quit" << endl;
            break;
        case RECORDING:
            cout << "Recording..."<<endl;
            cout << "1: Press 'p' to start playback\n2: Press 'p' to pause\n3: Press 'q' to quit"<<endl;
            break;
        case RECORDED:
            cout << "Your voice has been recorded" << endl;
            cout << "1: Press 'p' to start playback\n2: Press 'q' to quit" << endl;
            break;
        case PLAYBACK:
            cout << "Playback..." << endl;
            break;
        default:
            cout << "Undefined menu! press 'q' to quit and debug the source :)" << endl;
    }
}

bool loadMedia() { //creats a simple window to receive events
    bool success = true; //we assume everything will be ok
    gWindow = SDL_CreateWindow("A simple recording program", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN); //create the window
    if(gWindow == NULL) {
        reportError("Window creation failed");
        success = false;
    }
    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED); //create a renderer
    if(gRenderer == NULL) {
        reportError("Renderer creation failed");
        success = false;
    }
    SDL_SetRenderDrawColor(gRenderer, 0, 0xff, 0xff, 0); //setting the color for clearing renderer
    SDL_RenderClear(gRenderer); //clearing the renderer with set color
    SDL_RenderPresent(gRenderer); //drawing the renderer to the window
    return success;
}
