#! /bin/bash

make
sudo rmmod myled
sudo insmod myled.ko
sudo chmod a+rw /dev/myled0
