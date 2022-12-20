#include "lab_png.h"

int is_png(U8 *buf, size_t n) {
    U8 expected[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    int is_png = 1;
    if(n != 8){
        printf("unexpected size\n");
        return -1;
    }
    for(int i = 0; i < n; i++){
        if(expected[i] != buf[i]){
            is_png = 0;
        }
    }
    return is_png;
}

int get_png_height(struct data_IHDR *buf) {
    return buf->height;
}

int get_png_width(struct data_IHDR *buf) {
    return buf->width;
}

/* write IHDR data to data_IHDR struct */
int get_png_data_IHDR(struct data_IHDR *out, void *buf, int counter) {
    /* width */
    memcpy(&(out->width), buf+counter, 4);
    counter += 4;

    /* height */
    memcpy(&(out->height), buf+counter, 4);
    counter += 4;

    /* bit depth */
    memcpy(&(out->bit_depth), buf+counter, 1);
    counter += 1;

    /* color type */
    memcpy(&(out->color_type), buf+counter, 1);
    counter += 1;
    
    /* compression */
    memcpy(&(out->compression), buf+counter, 1);
    counter += 1;

    /* filter */
    memcpy(&(out->filter), buf+counter, 1);
    counter += 1;

    /* interlace */
    memcpy(&(out->interlace), buf+counter, 1);
    counter += 1;

    return counter;
}
