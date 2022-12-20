# PNG Concatenator Program
A Linux program that uses threads to concurrently fetch images and then combine them into 1 PNG.

## Organization
The lib folder contains helper functions with a PNG struct, inflating/deflating data, and CRC calculation.

The main program is png-combiner.c

To run use the commands:
```
    make
    ./png-combiner -t [num] -n [num]
```
    
Options:
-t indicates the number of threads to run
-n indicates which of the 3 images to retrieve from the server. Possible values: 1, 2, 3.
