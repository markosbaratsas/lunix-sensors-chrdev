# OSlab Exercise 2: Linux Device Driver for a Wireless Sensor Network

This project was created by [Markos Baratsas](https://github.com/markosbaratsas) and [Maria Retsa](https://github.com/mariartc), for the purposes of the [Operationg Systems Laboratory](http://www.cslab.ece.ntua.gr/courses/compsyslab/) at [ECE NTUA](https://www.ece.ntua.gr/en).


## Description
The purpose of this exercise is the construction of a character device driver, which is used for manipulating the sensor data and displaying them in the desired format. Those sensor data are collected through a sensor network and displayed into special files (character devices) in the `/dev` directory, which can be (simultaneously if necessary) accessed through different user-space processes.
