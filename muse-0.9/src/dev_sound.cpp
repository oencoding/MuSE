/* MuSE - Multiple Streaming Engine
 * SoundDevice class interfacing Portaudio PABLIO library
 * Copyright (C) 2004 Denis Roio aka jaromil <jaromil@dyne.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published 
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
 "$Id$"
 
 */

#include <dev_sound.h>
#include <jutils.h>
#include <generic.h>
#include <config.h>

#ifdef HAVE_DARWIN
#define PA_SAMPLE_TYPE paFloat32
#else
#define PA_SAMPLE_TYPE paInt16
#endif

#define PA_SAMPLES_PER_FRAME 2
#define PA_MAX_FRAMES 4096
#define PA_NUM_SECONDS 5
#define FRAMES_PER_BUFFER   (64)
#define PA_PIPE_SIZE FRAMES_PER_BUFFER*(PA_SAMPLES_PER_FRAME*sizeof(PA_SAMPLE_TYPE))*128

#define INPUT_DEVICE  Pa_GetDefaultInputDeviceID()
#define OUTPUT_DEVICE Pa_GetDefaultOutputDeviceID()


#ifdef HAVE_JACK
int dev_jack_process(jack_nframes_t nframes, void *arg) {
  jack_nframes_t opframes;
  SoundDevice *dev = (SoundDevice*)arg;
  if(!dev->jack) return 0; // just return
  
  // take output from pipe and send it to jack
  dev->jack_out_buf = (jack_default_audio_sample_t*)
    jack_port_get_buffer(dev->jack_out_port,nframes);
  opframes = dev->jack_out_pipe->read
    (nframes * sizeof(float) * 2 , dev->jack_out_buf);

  // take input from jack and send it in pipe
  dev->jack_in_buf = (jack_default_audio_sample_t*)
    jack_port_get_buffer(dev->jack_in_port,nframes);
  dev->jack_in_pipe->write // does the float2int conversion in one pass
    (nframes * sizeof(float) * 2 , dev->jack_in_buf);

  return 0;
}

void dev_jack_shutdown(void *arg) {
  SoundDevice *dev = (SoundDevice*)arg;
  // close the jack channels
  dev->jack = false;
  jack_port_unregister(dev->jack_client, dev->jack_in_port);
  jack_port_unregister(dev->jack_client, dev->jack_out_port);
  jack_deactivate(dev->jack_client);
  delete dev->jack_in_pipe;
  delete dev->jack_out_pipe;
}
#endif


SoundDevice::SoundDevice() {
  memset(&input_device,0,sizeof(input_device));
  memset(&output_device,0,sizeof(output_device));
  pa_dev.input = &input_device;
  pa_dev.output = &output_device;
  input_device.pipe = new Pipe(PA_PIPE_SIZE);
  input_device.pipe->set_block(false,true);
  output_device.pipe = new Pipe(PA_PIPE_SIZE);
  output_device.pipe->set_block(true,false);
#ifdef HAVE_DARWIN
  input_device.pipe->set_output_type("copy_float_to_int16");
  output_device.pipe->set_input_type("copy_int16_to_float");
#endif
  jack = false;
  jack_in = false;
  jack_out = false;
}

SoundDevice::~SoundDevice() {
  close();
}

static int pa_process( void *inputBuffer, void *outputBuffer, 
			unsigned long framesPerBuffer, 
			PaTimestamp outTime, void *userData )
{
  PaDevices *dev = (PaDevices *)userData;
  long len = framesPerBuffer * sizeof(float) * 2 ;
  if(inputBuffer != NULL) {
    int numRead = dev->input->pipe->write(len,inputBuffer);
  }
  if(outputBuffer != NULL) {
    int i;
    int numRead = dev->output->pipe->read(len,outputBuffer);
    /* Zero out remainder of buffer if we run out of data. */
    for( i=numRead;i<len;i++) {
      ((char *)outputBuffer)[i] = 0;
    }
  }
  return 0;
}

bool SoundDevice::input(bool state) {
  bool res = false;
  if(jack) return true;
  if(!res) res = pa_open(state,PaInput);
  return res;
}

PaError SoundDevice::pa_real_open(int mode) {
  return Pa_OpenStream( ((mode & PaInput) == PaInput)?&input_device.stream:&output_device.stream,
    ((mode & PaInput) == PaInput)?input_device.id:paNoDevice,
    ((mode & PaInput) == PaInput)?2:0,
    PA_SAMPLE_TYPE,
    NULL,
    ((mode & PaOutput) == PaOutput)?output_device.id:paNoDevice,
    ((mode & PaOutput) == PaOutput)?2:0,
    PA_SAMPLE_TYPE,
    NULL,
    SAMPLE_RATE,
    FRAMES_PER_BUFFER,
    0,  /* number of buffers, if zero then use default minimum */
    0, // paClipOff,     /* we won't output out of range samples so don't bother clipping them */
    pa_process,
    &pa_dev );
}

bool SoundDevice::pa_open(bool state,int mode) {
  PaDevInfo *dev,*other;
  PaDeviceID id;
  int creq,oreq;
  char dir[7];
  if(mode == PaInput) { // input requested
    dev = &input_device;
    other = &output_device;
    creq = PaInput;
    oreq = PaOutput;
    strcpy(dir,"input");
    dev->id = Pa_GetDefaultInputDeviceID();
  }
  else if(mode == PaOutput) { // output requested
    dev = &output_device;
    other = &input_device;
    creq = PaOutput;
    oreq = PaInput;
    strcpy(dir,"output");
    dev->id = Pa_GetDefaultOutputDeviceID();
  }
  if(state && ((pa_mode & creq) != creq)) {
    dev->info = Pa_GetDeviceInfo( dev->id );
    if(dev->info) func("%s device: %s",dir,dev->info->name);
    else {
      error("%s device not available",dir);
      return false;
    }
    if((pa_mode & oreq) == oreq) {
      /* input device is already opened...check if we are trying to open the same device */
      if(other->info) { 
        if(strcmp(other->info->name,dev->info->name) == 0) {
          Pa_StopStream( other->stream );
          Pa_CloseStream( other->stream );
          err = pa_real_open(PaInput|PaOutput);
          if(err == paNoError ) output_device.stream = input_device.stream;
		}
		else {
          err = pa_real_open(mode);
		}
      }
      else {
        error("Full duplex has been requested but we don't have portaudio information");
        return false;
      }
    }
    else {
      err = pa_real_open(mode);
    }
    if( err != paNoError) {
      Pa_Terminate();
      error("error opening %s sound device: %s",dir,Pa_GetErrorText( err ) );
      return false;
    }
    else {
      err = Pa_StartStream(dev->stream);
      if(err != paNoError) {
         error("error starting %s audio stream: %s",dir,Pa_GetErrorText( err ) );
         return false;
      }
      pa_mode = pa_mode | creq;
    }
  } else if(!state && dev->stream) { // XXX - i have to check if this is still right
    
    if((pa_mode & creq) == creq) {
       if((pa_mode & oreq) == oreq) {
         pa_mode = oreq;
       }
       else {
         Pa_StopStream(dev->stream);
         Pa_CloseStream(dev->stream);
         pa_mode = PaNull;
       }
    }
    dev->stream = NULL;
    dev->info = NULL;
    dev->pipe->flush();
	//delete dev->pipe;
  }
  return true;
}

bool SoundDevice::output(bool state) {
  bool res = false;
  if(jack) return true;
  if(!res) res = pa_open(state,PaOutput);
  return res;
}

bool SoundDevice::open(bool read, bool write) {

  //  notice("detecting sound device");

#ifdef HAVE_JACK
  // we try to open up a jack client
  jack_sample_size = sizeof(jack_default_audio_sample_t);
  if(!jack) // check if it is not allready on
    if( (jack_client = jack_client_new("MuSE")) !=0 ) {
      notice("jack audio daemon detected");
      act("hooking in/out sound channels");
      warning("this feature is still experimental and won't work!");
      warning("you need to stop jack and free the audio card");
      jack = true;
      jack_samplerate = jack_get_sample_rate(jack_client);
      jack_set_process_callback(jack_client, dev_jack_process, this);    
      jack_on_shutdown(jack_client,dev_jack_shutdown,this);

      jack_in_pipe = new Pipe();
      jack_in_pipe->set_output_type("copy_float_to_int16");
      jack_in_pipe->set_block(false,true);

      jack_out_pipe = new Pipe();
      jack_out_pipe->set_input_type("copy_int16_to_float");
      jack_in_pipe->set_block(true,false);

      // open the jack input channel
      jack_in_port = jack_port_register(jack_client, "capture",
					JACK_DEFAULT_AUDIO_TYPE,
					JackPortIsInput, 0);
      // open the jack output channel
      jack_out_port = jack_port_register(jack_client, "playback",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsOutput, 0);
      
      jack_activate(jack_client);
      return true;
    }
#endif
  if( ! output(write) ) return false;
  
  //if( ! input(read) ) return false;
  
  return true;
}

void SoundDevice::close() {
  if(input_device.stream)
    Pa_StopStream( input_device.stream);
    Pa_CloseStream( input_device.stream );
    input_device.stream = NULL;
    input_device.pipe->flush();
    //delete input_device.pipe;

  if(output_device.stream)
    Pa_StopStream( output_device.stream);
    Pa_CloseStream( output_device.stream );
    output_device.stream = NULL;
    output_device.pipe->flush();
    //delete output_device.pipe;
}

int SoundDevice::read(void *buf, int len) {
  // len is in samples: 4*2 32bit stereo
  int res = -1;

  if(jack) {

    res = jack_in_pipe->read(len*2,buf);

  } else if(input_device.stream) { // portaudio

    // takes number of left and right frames (stereo / 2)
    res = input_device.pipe->read(len*2,buf);
  }  
  return res;
}

int SoundDevice::write(void *buf, int len) {
  // len is in samples, for bytes must *2 (16bit)
  int res = -1;

  if(jack) { // jack audio daemon

    res = jack_out_pipe->write(len*2,buf);

  } else if(output_device.stream) { // portaudio

    // takes number of left and right frames (stereo / 2)
	res = output_device.pipe->write(len*2,buf);
  }
  return res;
}
