/*
* Filename: stream.h
* Author: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
* Created: Mon Jul 21 14:56:30 2014 +0600
* Version: 1.4
* Last-Updated: Thu Oct 22 10:11:15 2015 +0600
* By: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
*
* Target platform for cross-compilation: ARCH=arm && proc=imx6
*/

#ifndef STREAM_H
#define STREAM_H

#include <gst/gst.h>
#include <glib.h>
#include <gobject/gobject.h>

#include <boost/thread.hpp>

class Stream {

  public:
    Stream(): _video_state(0), _hdmi_state(0), is_gst_init(false), is_playing(false), 
    is_built(false), is_analog(false), is_hdmi(false), is_hdmi_src(false), is_restart_analog(false),
    is_audio_built(false), is_audio_playing(false), is_audio_stop(false) {
      
    };
    
    void start_stream();
    void read_params();
    
  private:
    void play_stream();
    void play_audio_stream();
    void build_stream();
    void build_audio_stream();
    void set_analog_src();
    void set_hdmi_src();
    void restart_pipeline();
    void restart_pipeline_analog();
    void stop_audio();
    void read_encoder_ip();
    
    GstElement *pipeline;
    GstElement *pipeline_hdmi_audio;
    GstElement *signal_selector;
    GstPad *sink_pad1, *sink_pad2, *output_sig_pad1, *output_sig_pad2;
    GstElement *tvsrc, *mfw_v4lsrc, *udpsink;
    GstBus *bus, *bus_audio;
    GstMessage *msg, *msg_audio;
    
    bool is_gst_init, is_playing, is_audio_playing, is_built, is_analog, is_hdmi, is_hdmi_src, is_restart_analog, 
      is_audio_built, is_audio_stop;
    int _video_state;
    int _hdmi_state;
    char _encoder_ip[16];
};

#endif