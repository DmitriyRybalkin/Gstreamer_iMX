/*
* Filename: main.cpp
* Author: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
* Created: Mon Jul 21 14:56:30 2014 +0600
* Version: 1.4
* Last-Updated: Thu Oct 22 10:11:15 2015 +0600
* By: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
*
* Target platform for cross-compilation: ARCH=arm && proc=imx6
*/

#include "stream.h"

#include <boost/bind.hpp>

int main()
{

  Stream stream;
  
  boost::thread stream_thread, params_thread;
 
  stream_thread = boost::thread(boost::bind(&Stream::start_stream, &stream));
  params_thread = boost::thread(boost::bind(&Stream::read_params, &stream));
  
  stream_thread.join();
  params_thread.join();
  
  return 0;
}