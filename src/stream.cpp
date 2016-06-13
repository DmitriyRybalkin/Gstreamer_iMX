/*
* Filename: stream.cpp
* Author: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
* Created: Mon Jul 21 14:56:30 2014 +0600
* Version: 1.4
* Last-Updated: Wed. Nov. 18 13:16:09 ALMT 2015
* By: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
*
* Target platform for cross-compilation: ARCH=arm && proc=imx6
*/

#include "stream.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

/* define/undef to turn on/off std output */
#define DEBUG

#define FILE_LENGTH 0x100
#define FILE_NAME  "/usr/local/GStreamer_iMX/video_state"
#define FILE_HDMI_IN "/sys/bus/i2c/devices/2-0048/state"
#define FILE_ENCODER_IP "/usr/local/GStreamer_iMX/encoder_ip"
#define DEFAULT_VIDEO_STATE "1"
#define BUFFER_SIZE 64

/* Main thread loop, it executes the following:
 * 1. builds the pipeline
 * 2. starts playing the pipeline
 * 3. checks pipeline message bus
 * 4. sets the video source active (HDMI_IN, AVI_IN) 
 * depending on params _video_state, _hdmi_state, see Stream::read_params
 */
void Stream::start_stream() {
  if(!is_gst_init) {
    gst_init(NULL, NULL);
    gboolean is_init = gst_is_initialized();
    if(is_init == TRUE) std::cout << "Stream_init::TRUE" << std::endl;
    else std::cout << "Stream_init::FALSE" << std::endl;
      
    g_print("Gstreamer version: %s\n", gst_version_string());
    is_gst_init = true;
  }
  //this->read_encoder_ip();
  
  this->build_audio_stream();
  boost::this_thread::sleep(boost::posix_time::seconds(1));
  
  this->build_stream();
  boost::this_thread::sleep(boost::posix_time::seconds(1));
  
  this->play_stream();
  boost::this_thread::sleep(boost::posix_time::seconds(1));
  
  for(;;) {
    
#if defined DEBUG
    std::cout << "Stream::start_stream:thread_loop" << std::endl;
#endif
    
    /* Video bus messages */
    if (pipeline && bus) {
      msg = gst_bus_timed_pop_filtered (bus, 0, GST_MESSAGE_ANY);
      
      if (msg != NULL) {
	switch (GST_MESSAGE_TYPE(msg)) {
	  case GST_MESSAGE_ERROR:
	    GError *gerror;
	    gchar *debug;
	    gst_message_parse_error(msg, &gerror, &debug);
	    
	    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), gerror->message);
	    g_printerr("Debugging information: %s\n", debug ? debug : "none");
	    g_error_free(gerror);
	    g_free(debug);

	    gst_message_unref(msg);
	    msg = NULL;
	    break;
	  case GST_MESSAGE_EOS:
	    g_message ("Video:got_EOS");
	    
	    gst_message_unref(msg);
	    msg = NULL;
	    
	    break;
	    
	  case GST_MESSAGE_WARNING:
	    gst_message_parse_warning (msg, &gerror, &debug);
	    gst_object_default_error (GST_MESSAGE_SRC (msg), gerror, debug);
	    
	    g_error_free (gerror);
	    g_free (debug);

	    
	    gst_message_unref(msg);
	    msg = NULL;
	    
	    break;
	    
	   default:
	    gst_message_unref(msg);
	    msg = NULL;
	    break;
	}
      }
    }
    
    /* Audio bus messages */
    if (pipeline_hdmi_audio && bus_audio) {
      msg_audio = gst_bus_timed_pop_filtered (bus_audio, 0, GST_MESSAGE_ANY);
      
      if (msg_audio != NULL) {
	switch (GST_MESSAGE_TYPE(msg_audio)) {
	  case GST_MESSAGE_ERROR:
	    GError *gerror;
	    gchar *debug;
	    gst_message_parse_error(msg_audio, &gerror, &debug);
	    
	    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME (msg_audio->src), gerror->message);
	    g_printerr("Debugging information: %s\n", debug ? debug : "none");
	    g_error_free(gerror);
	    g_free(debug);

	    gst_message_unref(msg_audio);
	    msg_audio = NULL;
	    break;
	  case GST_MESSAGE_EOS:
	    g_message ("Audio:got_EOS");
	    
	    gst_message_unref(msg_audio);
	    msg_audio = NULL;
	    
	    break;
	    
	  case GST_MESSAGE_WARNING:
	    gst_message_parse_warning (msg_audio, &gerror, &debug);
	    gst_object_default_error (GST_MESSAGE_SRC (msg_audio), gerror, debug);
	    
	    g_error_free (gerror);
	    g_free (debug);

	    gst_message_unref(msg_audio);
	    msg_audio = NULL;
	    
	    break;
	    
	   default:
	    gst_message_unref(msg_audio);
	    msg_audio = NULL;
	    break;
	}
      }
    }
    
    if(_hdmi_state == 1) {
      this->play_audio_stream();
      
      if(_video_state == 1 )
	this->set_analog_src();
      else
	this->set_hdmi_src();
    } else {
      this->restart_pipeline_analog();
      this->stop_audio();
    }
    
    boost::this_thread::sleep(boost::posix_time::seconds(2));
  
  }
  
}

/* Start video pipeline */
void Stream::play_stream() {
#if defined DEBUG
  std::cout << "Stream::play_stream()" << std::endl;
#endif
  
  if(!is_playing) {
    gst_element_set_state(pipeline, GST_STATE_READY);
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    is_playing = true;
  }
}

/* Start HDMI audio pipeline */
void Stream::play_audio_stream() {
#if defined DEBUG
  std::cout << "Stream::play_audio_stream()" << std::endl;
#endif  
  
  if(!is_audio_playing) {
    gst_element_set_state(pipeline_hdmi_audio, GST_STATE_READY);
    gst_element_set_state(pipeline_hdmi_audio, GST_STATE_PAUSED);
    gst_element_set_state(pipeline_hdmi_audio, GST_STATE_PLAYING);
    
    is_audio_playing = true;
    is_audio_stop = false;
  }
}

/* Stop HDMI audio pipeline */
void Stream::stop_audio() {
#if defined DEBUG
  std::cout << "Stream::stop_audio()" << std::endl;
#endif  
  if(!is_audio_stop) {
    gst_element_send_event(pipeline_hdmi_audio, gst_event_new_eos ());
    
    gst_element_set_state(pipeline_hdmi_audio, GST_STATE_READY);
    
    is_audio_stop = true;
    is_audio_playing = false;
  }
}
/* Build pipeline with only one active src plugin - tvsrc (AVI_IN)
 * HDMI will be added or left unplugged depending on _hdmi_state
 */
void Stream::build_stream() {
#if defined DEBUG
    std::cout << "Stream::build_stream()" << std::endl;
#endif
    
    if(!is_built) {
      std::stringstream udpsink_str;
      udpsink_str << "vpuenc name=video_enc codec=avc cbr=true bitrate=650000 seqheader-method=3 timestamp-method=0 force-framerate=true framerate-nu=15 ! rtph264pay name=video_rtp config-interval=10 ! udpsink name=video_sink host=192.168.1.3 port=9010 sync=false async=false";
      
      udpsink = gst_parse_bin_from_description(udpsink_str.str().c_str(), TRUE, FALSE);
      
      tvsrc = gst_element_factory_make("tvsrc", "analog");
      g_object_set(G_OBJECT(tvsrc), "device", "/dev/video1", NULL);
      g_object_set(G_OBJECT(tvsrc), "queue-size", 2, NULL);
      
      mfw_v4lsrc = gst_element_factory_make("mfw_v4lsrc", "hdmi");
      g_object_set(G_OBJECT(mfw_v4lsrc), "device", "/dev/video0", NULL);
      g_object_set(G_OBJECT(mfw_v4lsrc), "queue-size", 2, NULL);
      
      signal_selector = gst_element_factory_make("input-selector", "Selector");
      
      pipeline = gst_pipeline_new("Operator_Stream");
      
      gst_bin_add_many(GST_BIN(pipeline), tvsrc, signal_selector, udpsink, NULL);
      
      sink_pad1 = gst_element_get_request_pad(signal_selector, "sink%d");
      
      sink_pad2 = gst_element_get_request_pad(signal_selector, "sink%d");
      
      output_sig_pad1 = gst_element_get_static_pad(tvsrc, "src");
      output_sig_pad2 = gst_element_get_static_pad(mfw_v4lsrc, "src");

      if(gst_pad_link (output_sig_pad1, sink_pad1) != GST_PAD_LINK_OK) {
	g_print("Stream::gst_pad_link_1:Error\n");
      }
      
      if(!gst_element_link(signal_selector, udpsink)) {
       g_print("\n Cannot link signal_selector with the udpsink \n");
      }
      
      g_object_set(G_OBJECT(signal_selector), "active-pad", sink_pad1, NULL);
      
      gst_element_set_state(pipeline, GST_STATE_NULL);
      
      bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
      
      if (!pipeline) {
	g_printerr ("Pipeline is not initialized!\n");
      }
      
      udpsink_str.clear();
      
      is_built = true;
      is_restart_analog = true;
      is_hdmi_src = false;
    }
}

/* Build HDMI audio pipeline */
void Stream::build_audio_stream() {
#if defined DEBUG
    std::cout << "Stream::build_audio_stream()" << std::endl;
#endif
  if(!is_audio_built) {
    GError *error = NULL;
    
    std::stringstream audio_str;
    audio_str << "alsasrc name=audio_src device=sysdefault:CARD=tda1997xaudio ! audio/x-raw-int,rate=8000,width=16,channels=1 ! speexenc mode=3 name=audio_enc ! rtpspeexpay ! udpsink name=audio_sink host=192.168.1.3 port=10500 sync=false async=false";
    
    pipeline_hdmi_audio = gst_parse_launch(audio_str.str().c_str(), &error);
    if (!pipeline_hdmi_audio)
      g_print ("Stream::build_audio_stream:Parse error: %s\n", error->message);
    
    gst_element_set_state(pipeline_hdmi_audio, GST_STATE_NULL);
    
    bus_audio = gst_pipeline_get_bus(GST_PIPELINE(pipeline_hdmi_audio));
    
    audio_str.clear();
    
    is_audio_built = true;
    
  }
}
    
/* Set tvsrc (AVI_IN) active in pipeline
 * after we send EOS event and rebuild the pipeline
 */
void Stream::set_analog_src() {
  
  if(!is_analog) {
    gst_element_send_event (pipeline, gst_event_new_eos ());
    
    this->restart_pipeline();
    
    g_object_set(G_OBJECT(signal_selector), "active-pad", sink_pad1, NULL);
    
    is_analog = true;
    is_hdmi = false;
  }
}

/* Set mfw_v4lsrc (HDMI_IN) active in pipeline
 * after we send EOS event and rebuild the pipeline
 */
void Stream::set_hdmi_src() {
  if(!is_hdmi) {
    gst_element_send_event (pipeline, gst_event_new_eos ());
    
    this->restart_pipeline();
    
    g_object_set(G_OBJECT(signal_selector), "active-pad", sink_pad2, NULL);
    
    is_hdmi = true;
    is_hdmi_src = true;
    is_analog = false;
  }
}

/* Rebuild the pipeline if HDMI_IN is active */
void Stream::restart_pipeline() {
#if defined DEBUG
  std::cout << "Stream::restart_pipeline()" << std::endl;
#endif
  gst_element_set_state(pipeline, GST_STATE_READY);
  
  gst_pad_unlink (output_sig_pad1, sink_pad1);
  if(is_hdmi_src)
    gst_pad_unlink (output_sig_pad2, sink_pad2);
  
  gst_element_unlink(signal_selector, udpsink);
  
  gst_bin_remove(GST_BIN(pipeline), tvsrc);
  if(is_hdmi_src)
    gst_bin_remove(GST_BIN(pipeline), mfw_v4lsrc);
  
  gst_bin_remove(GST_BIN(pipeline), signal_selector);
  gst_bin_remove(GST_BIN(pipeline), udpsink);
  
  gst_bin_add(GST_BIN(pipeline), tvsrc);
  if(!gst_bin_get_by_name(GST_BIN(pipeline), "hdmi"))
    gst_bin_add(GST_BIN(pipeline), mfw_v4lsrc);
  
  gst_bin_add(GST_BIN(pipeline), signal_selector);
  gst_bin_add(GST_BIN(pipeline), udpsink);
  
  gst_pad_link (output_sig_pad1, sink_pad1);

  gst_pad_link (output_sig_pad2, sink_pad2);
      
  gst_element_link(signal_selector, udpsink);
 
  if(_video_state == 1)
    g_object_set(G_OBJECT(signal_selector), "active-pad", sink_pad1, NULL);
  else
    g_object_set(G_OBJECT(signal_selector), "active-pad", sink_pad2, NULL);
  
  gst_element_set_state(pipeline, GST_STATE_PAUSED);
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  
  is_restart_analog = false;
}

/* Rebuild the pipeline if HDMI_IN is inactive */
void Stream::restart_pipeline_analog() {
#if defined DEBUG
  std::cout << "Stream::restart_pipeline_analog()" << std::endl;
#endif
  
  if(!is_restart_analog) {
    gst_element_set_state(pipeline, GST_STATE_READY);
    
    gst_pad_unlink (output_sig_pad1, sink_pad1);
    if(is_hdmi_src)
      gst_pad_unlink (output_sig_pad2, sink_pad2);
    
    gst_element_unlink(signal_selector, udpsink);
    
    gst_bin_remove(GST_BIN(pipeline), tvsrc);
    if(gst_bin_get_by_name(GST_BIN(pipeline), "hdmi"))
      gst_bin_remove(GST_BIN(pipeline), mfw_v4lsrc);
    
    gst_bin_remove(GST_BIN(pipeline), signal_selector);
    gst_bin_remove(GST_BIN(pipeline), udpsink);
    
    gst_bin_add(GST_BIN(pipeline), tvsrc);
    gst_bin_add(GST_BIN(pipeline), signal_selector);
    gst_bin_add(GST_BIN(pipeline), udpsink);
    
    gst_pad_link (output_sig_pad1, sink_pad1);
	
    gst_element_link(signal_selector, udpsink);
  
    g_object_set(G_OBJECT(signal_selector), "active-pad", sink_pad1, NULL);
    
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    is_restart_analog = true;
    is_hdmi_src = false;
    is_hdmi = false;
  }
}

/* Read params for the pipeline:
 * _encoder_ip - is the IP address of SNMP manager (Video/Audio encoder) being read from file FILE_ENCODER_IP
 */
void Stream::read_encoder_ip() {

  int fd;
  ssize_t read_bytes;
  char buffer[BUFFER_SIZE+1];

  strcpy(_encoder_ip, "192.168.1.3");
  
  fd = open (FILE_ENCODER_IP, O_RDONLY);

  if(fd < 0) {
#if defined DEBUG
    std::cout << "Stream::read_encoder_ip():_encoder_ip:Can't read" << std::endl;
#endif
  } else {
    while ((read_bytes = read (fd, buffer, BUFFER_SIZE))) {
      buffer[read_bytes] = 0;
    }

    strcpy(_encoder_ip, buffer);
      
    close(fd);
  
#if defined DEBUG
    std::cout << "Stream::read_encoder_ip():_encoder_ip=" << _encoder_ip << std::endl;
#endif
  }
}

/* Read params for the pipeline:
 * _video_state - it is read from the file being updated with SNMP scripts, 
 * it is activated if SNMP manager sends requests, see FILE_NAME
 * _hdmi_state  - it is read from the protected file that is belonged to tda1997x driver, see FILE_HDMI_IN
 */
void Stream::read_params() {
#if defined DEBUG
  std::cout << "Stream::read_params()" << std::endl;
#endif
  
  int fd;
  void* file_memory;
  ssize_t read_bytes;
  char buffer[BUFFER_SIZE+1];
  
  for(;;) {
#if defined DEBUG
    std::cout << "Stream::read_params():thread_loop" << std::endl;
#endif
    
    /* Read video_state */
    fd = open (FILE_NAME, O_RDONLY);
    
    if(fd < 0) {
      int fd = open (FILE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      
      write(fd, DEFAULT_VIDEO_STATE, sizeof(DEFAULT_VIDEO_STATE));
      
      close(fd);
    } else {

      file_memory = mmap (0, FILE_LENGTH, PROT_READ, MAP_SHARED, fd, 0);
    
      _video_state = std::atoi((char*) file_memory);

      munmap (file_memory, FILE_LENGTH);
      close (fd);

#if defined DEBUG      
      std::cout << "Stream::read_params():_video_state=" << _video_state << std::endl;
#endif
      
    }
    
    /* Read hdmi_state */
    fd = open (FILE_HDMI_IN, O_RDONLY);
    
    if(fd < 0) {
#if defined DEBUG
      std::cout << "Stream::read_params():_hdmi_state:Can't read" << std::endl;
#endif      
      _hdmi_state = 0;
    } else {
      
      while ((read_bytes = read (fd, buffer, BUFFER_SIZE))) {
	buffer[read_bytes] = 0;
      }
      
      std::string buffer_str(buffer);
      buffer_str.erase(buffer_str.find_last_not_of(" \n\r\t")+1);
      
      if(buffer_str == "locked")
	_hdmi_state = 1;
      else
	_hdmi_state = 0;
      
      close (fd);

      buffer_str.clear();
#if defined DEBUG
      std::cout << "Stream::read_params():_hdmi_state=" << _hdmi_state << std::endl;
#endif
    }
    
    boost::this_thread::sleep(boost::posix_time::seconds(3));
  }
}