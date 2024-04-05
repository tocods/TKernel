#! /bin/bash

sudo make -j 8

sudo make modules_install

sudo make install

sudo update-grub
