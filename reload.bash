#! /bin/bash

make
sudo rmmod driver 
sudo insmod driver.ko
sudo chmod a+rw /dev/driver0
