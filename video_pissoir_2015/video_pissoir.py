#!/usr/bin/env python
# -*- coding: utf-8 -*-
'''
Created on April 28, 2015
python script for raspberry pi 
to receive distance sensor values via i2c interface
and playback a movie

@author: fabian@projektil.ch
'''

import os, time, random, smbus
from subprocess import call

I2C_BUS = 1
I2C_ADRESS = 0x4
MOVIE_FOLDER = '/media/usb'
MOVIE_TIME_OUT = 10.0

#############################################################

def scan_for_movies(the_folder):
  """scans for movie files in the given directory"""

  movie_extensions = [".mp4", ".m4v",".mov", ".avi"]
  found_movies = []

  for file_name in os.listdir(the_folder):
    f, ext = os.path.splitext(file_name) 
    if not f.startswith('.') and ext in movie_extensions:
      found_movies.append(os.path.join(the_folder, file_name))

  return found_movies

#############################################################

class App(object):
  def __init__(self):
    self.i2c = smbus.SMBus(I2C_BUS)
    self.movie_paths = scan_for_movies(MOVIE_FOLDER)
    self.last_movie_index = -1 
    self.time_stamp = 0
    self.running = True
    
    call(["clear"])
    print("video pissoir running, found {} movies ...\n".format(len(self.movie_paths)))

  def run(self):

    while self.running:
      try:
        # returns a block of 32 bytes
        bytes_read = self.i2c.read_i2c_block_data(I2C_ADRESS, 0)
    
        #convert to int
        value = bytes_read[3] << 24 | bytes_read[2] << 16 | bytes_read[1] << 8 | bytes_read[0]
        #print("value: {}".format(value))
        
        # response from sensor
        if value != 0 and time.time() - self.time_stamp > MOVIE_TIME_OUT:
          random_index = self.last_movie_index

          while random_index == self.last_movie_index:
            random_index = random.randint(0, len(self.movie_paths) - 1)
          
          self.last_movie_index = random_index
          random_movie = self.movie_paths[random_index]
          print("playing movie: {}".format(random_movie))
          call(["omxplayer", "-b", "-o", "local", random_movie])
          print("playback finished")
          call(["clear"])
          self.time_stamp = time.time()

        # sleep 50 ms
        time.sleep(.05)

      except IOError:
        pass
        #print("IoError")

      except KeyboardInterrupt:
        print("\n->keyboard interrupt<-")
        self.running = False
        print("ciao\n")
    
#############################################################

if __name__ == '__main__':  
  print(__doc__)
  App().run()
