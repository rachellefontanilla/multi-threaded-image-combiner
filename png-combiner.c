/* GROUP 20: David Wang, Rachelle Fontanilla */

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h> /* opendir */
#include <errno.h>
#include <sys/stat.h> /* mkdir */
#include <arpa/inet.h>
#include "lib/crc.h"
#include "lib/lab_png.h"
#include "lib/zutil.h"

pthread_mutex_t lock;

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })\

/******************************************************************************
 * DEFINED MACROS 
 *****************************************************************************/

#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define ECE252_HEADER "X-Ece252-Fragment: "


/******************************************************************************
 * STRUCTURES and TYPEDEFS 
 *****************************************************************************/

/* 1: Threads ***************************************************************/

/* thread input parameters struct */
struct thread_args {
    /* image we are searching for: 1,2,3 */
    char image[2];
    /* device we are using for: 1,2,3 */
    char device[2];
    /* array keeping track of which images we have retrieved */
    int *retrieved;
    /* how many images have we retrieved */
    int *count;
    /* array storing structs with all 50 received images */
    struct simple_PNG **PNG_array_p;
    /* stores final height for all.png */
    // U32 total_height;
    /* stores total length of idat data */ 
    U32 *total_length; 
};

/* receive buffer used for curl. from starter code */
struct RECV_BUF {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
};

/* initialize the RECV_BUF struct */
int recv_buf_init(struct RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
    return 2;
    }
    
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

/* deallocate memory for the RECV_BUF struct. from starter code */
int recv_buf_cleanup(struct RECV_BUF *ptr)
{
    if (ptr == NULL) {
    return 1;
    }
    
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}


/******************************************************************************
 * HELPER FUNCTIONS
 *****************************************************************************/

/* 1: Threads ***************************************************************/

/* callback function for the data. from starter code */
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    struct RECV_BUF *p = (struct RECV_BUF *)p_userdata;
 
    /* if data received is too big reallocate enough space for p */
    if (p->size + realsize + 1 > p->max_size) { /* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

/* callback function for the header. from starter code */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    struct RECV_BUF *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
    strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
    p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}

/* 2.1: PNG concatenate ******************************************************/

int add_PNG_struct_data(struct simple_PNG **out_PNG, char *buf, U32 *total_length, int seq){
    /* counter to keep track of index in buffer */
    int counter = 8;

    /* allocate memory for PNG struct */
    out_PNG[seq]->p_IHDR = malloc(sizeof(struct chunk));
    out_PNG[seq]->p_IDAT = malloc(sizeof(struct chunk));
    out_PNG[seq]->p_IEND = malloc(sizeof(struct chunk));
    out_PNG[seq]->p_IHDR->p_data_IHDR_chunk = malloc(sizeof(struct data_IHDR));

    /* copy values to PNG struct */
    /* ---------- IHDR ---------- */
    /* IHDR length */
    memcpy(&(out_PNG[seq]->p_IHDR->length), buf+counter, CHUNK_LEN_SIZE);
    counter += CHUNK_LEN_SIZE;

    /* IHDR type */
    memcpy(&(out_PNG[seq]->p_IHDR->type), buf+counter, CHUNK_TYPE_SIZE);
    counter += CHUNK_TYPE_SIZE;

    /* IHDR data */
    counter = get_png_data_IHDR(out_PNG[seq]->p_IHDR->p_data_IHDR_chunk, buf, counter);

    /* IHDR crc */
    memcpy(&(out_PNG[seq]->p_IHDR->crc), buf+counter, CHUNK_CRC_SIZE);
    counter += CHUNK_CRC_SIZE;

    /* ---------- IDAT ---------- */
    /* IDAT length */
    memcpy(&(out_PNG[seq]->p_IDAT->length), buf+counter, CHUNK_LEN_SIZE);
    counter += CHUNK_LEN_SIZE;

    /* IDAT type */
    memcpy(&(out_PNG[seq]->p_IDAT->type), buf+counter, CHUNK_TYPE_SIZE);
    counter += CHUNK_TYPE_SIZE;

    /* IDAT data */
    /* malloc memory for p_data based on length */
    U32 IDAT_dlen = ntohl(out_PNG[seq]->p_IDAT->length);
    out_PNG[seq]->p_IDAT->p_data = malloc(IDAT_dlen);

    /* increase total length for final catpng */
    *total_length += IDAT_dlen;
    memcpy(out_PNG[seq]->p_IDAT->p_data, buf+counter, IDAT_dlen);
    counter += IDAT_dlen;

    /* IDAT crc */
    memcpy(&(out_PNG[seq]->p_IDAT->crc), buf+counter, CHUNK_CRC_SIZE);
    counter += CHUNK_CRC_SIZE;
    
    /* ---------- IEND ---------- */
    /* IEND length */
    memcpy(&(out_PNG[seq]->p_IEND->length), buf+counter, CHUNK_LEN_SIZE);
    counter += CHUNK_LEN_SIZE;

    /* IEND type */
    memcpy(&(out_PNG[seq]->p_IEND->type), buf+counter, CHUNK_TYPE_SIZE);
    counter += CHUNK_TYPE_SIZE;

    /* IEND data */
    /* malloc memory for p_data based on length -- should be 0 */
    U32 IEND_dlen = ntohl(out_PNG[seq]->p_IEND->length);
    out_PNG[seq]->p_IEND->p_data = malloc(IEND_dlen);
    memset(out_PNG[seq]->p_IEND->p_data, 0, IEND_dlen);

    /* data should be NULL */
    memcpy(out_PNG[seq]->p_IEND->p_data, buf+counter, IEND_dlen);
    counter += IEND_dlen;

    memcpy(&(out_PNG[seq]->p_IEND->crc), buf+counter, CHUNK_CRC_SIZE);
    counter += CHUNK_CRC_SIZE;

    return 0;
}

/* concatenate PNGs from PNG structs */
int cat_PNG_struct(struct simple_PNG **PNG_array, U32 total_length){
    /* create final all.png */
    FILE *all_f;
    all_f = fopen("all.png", "wb");
        if( all_f == NULL ){
        printf("Could not open all.png file\n");
        return 0;
    }

    /* write all PNG data before IDAT data */
    /* write PNG file signature to concatenated PNG */
    U8 HEADER[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(HEADER, 8, 1, all_f);

    /* IHDR */
    fwrite(&(PNG_array[0]->p_IHDR->length), 4, 1, all_f);
    fwrite(&(PNG_array[0]->p_IHDR->type), 4, 1, all_f);
    fwrite(&(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->width), 4, 1, all_f);
    int height = 0;
    for(int i = 0; i < 50; i++){
        height += ntohl(PNG_array[i]->p_IHDR->p_data_IHDR_chunk->height);
    }
    height = htonl(height);
    fwrite(&height, 4, 1, all_f);
    fwrite(&(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->bit_depth), 1, 1, all_f);
    fwrite(&(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->color_type), 1, 1, all_f);
    fwrite(&(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->compression), 1, 1, all_f);
    fwrite(&(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->filter), 1, 1, all_f);
    fwrite(&(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->interlace), 1, 1, all_f);

    /* calculate IHDR crc value based on preceeding type and data fields */
    unsigned char *p_buf_ihdr_crc = malloc(17);
    /* read type and data fields */
    memcpy(p_buf_ihdr_crc, &(PNG_array[0]->p_IHDR->type), 4);
    memcpy(p_buf_ihdr_crc+4, &(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->width), 4);
    memcpy(p_buf_ihdr_crc+8, &height, 4);
    memcpy(p_buf_ihdr_crc+12, &(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->bit_depth), 1);
    memcpy(p_buf_ihdr_crc+13, &(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->color_type), 1);
    memcpy(p_buf_ihdr_crc+14, &(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->compression), 1);
    memcpy(p_buf_ihdr_crc+15, &(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->filter), 1);
    memcpy(p_buf_ihdr_crc+16, &(PNG_array[0]->p_IHDR->p_data_IHDR_chunk->interlace), 1);
    /* calculate crc value */
    U32 IHDR_crc = 0;
    IHDR_crc = crc(p_buf_ihdr_crc, 17);
    /* clear crc buffer */
    memset(p_buf_ihdr_crc, 0, 17);
    IHDR_crc = ntohl(IHDR_crc);
    fwrite(&(IHDR_crc), 4, 1, all_f);

    /* IDAT */

    /* inflate + deflate to get IDAT data */
    /* variables for inflating */
    U64 len_inf;                        /* inflated data length */
    U8 *cat_inf_buf = malloc(70000000); /* giant buffer to hold inflated data */
    U32 total_inf_len = 0;              /* stores total inflated data length, used as index when writing to giant buffer */

    /* access each PNG struct */
    for(int i = 0; i < 50; i++){
        len_inf = 0;
        /* inflate IDAT data */
        int ret = mem_inf(cat_inf_buf+total_inf_len, &len_inf, PNG_array[i]->p_IDAT->p_data, ntohl(PNG_array[i]->p_IDAT->length));

        /* check if inflate worked */
        if(ret != 0){
            printf("mem_inf failed\n");
            return 0;
        }

        /* update total inflated data length */
        total_inf_len += len_inf;
    }

    /* variables for deflating */
    U64 len_def = 0;                                /* compressed data length */
    U8 *cat_def_buf = malloc(50000000);         /* buffer to hold deflated data */

    /* deflate buffer data with all 50 images */
    int def_ret = mem_def(cat_def_buf, &len_def, cat_inf_buf, total_inf_len, Z_DEFAULT_COMPRESSION);
    /* check if deflate worked */
    if(def_ret != 0){
        printf("mem_def failed\n");
        return 0;
    }

    /* calculate IDAT crc value based on preceeding type and data fields */
    unsigned char *p_buf_crc2 = malloc(4 + len_def);
    /* read type and data fields */
    memcpy(p_buf_crc2, PNG_array[0]->p_IDAT->type, 4);
    memcpy(p_buf_crc2+4, cat_def_buf, len_def);
    /* calculate crc value */
    U32 IDAT_crc = 0;
    IDAT_crc = crc(p_buf_crc2, 4 + len_def);
    /* clear crc buffer */
    memset(p_buf_crc2, 0, 4 + len_def);
    IDAT_crc = ntohl(IDAT_crc);

    /* continue writing to file */
    len_def = htonl(len_def);
    fwrite(&(len_def), 4, 1, all_f);
    fwrite(&(PNG_array[0]->p_IDAT->type), 4, 1, all_f);
    len_def = ntohl(len_def);
    fwrite(cat_def_buf, len_def, 1, all_f);
    fwrite(&(IDAT_crc), 4, 1, all_f);

    /* IEND */
    U8 IEND[12] = { 00, 00, 00, 00 , 73, 69, 78, 68, 174, 66, 96, 130 };
    fwrite(IEND, 12, 1, all_f);

    fclose(all_f);
    free(cat_inf_buf);
    free(cat_def_buf);
    free(p_buf_ihdr_crc);
    free(p_buf_crc2);
    return 0;
}

/* function that the threads will run */
void * get_images(void * arg){
    /* input parameters */
    struct thread_args *p_in = arg;

    /* receive buffer for curl*/
    struct RECV_BUF recv_buf;
    recv_buf_init(&recv_buf, BUF_SIZE);

    /* create the url based on input data */
    char url[256] = "http://ece252-";
    strcat(url, p_in->device);
    strcat(url, ".uwaterloo.ca:2520/image?img=");
    strcat(url, p_in->image);

    /* init a curl session */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return 0;
    }

    /* setting options for curl */
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl); 
    /* user defined data structure passed to the call back function */
    /* Set the userdata argument with the CURLOPT_WRITEDATA option. */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
    /* user defined data structure passed to the call back function */
    /* Set the userdata argument with the CURLOPT_HEADERDATA option. */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* get the images */
    while(1){
        CURLcode res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        /* error check for seq */
        if(recv_buf.seq < 0){
            printf("invalid seq number\n");
            return 0;
        }

        /* mutex lock, so that p_in cannot be edited while checking p_in->count */
        pthread_mutex_lock(&lock);

        /* if we have retrieved all 50 images, break out of while loop */
        if(*p_in->count == 50){
            pthread_mutex_unlock(&lock);
            break;
        }

        /* if we have not retrieved this image, write it into buffer array */
        if(p_in->retrieved[recv_buf.seq] == 0){
            /* update the counter, and retrieved array to indicate that we retrieved a new image */
            ++*p_in->count;
            p_in->retrieved[recv_buf.seq] = 1;

            /* copy new image into buffer array */
            add_PNG_struct_data(p_in->PNG_array_p, recv_buf.buf, p_in->total_length, recv_buf.seq);
        }
        /* unlock mutex when we finish editing p_in */
        pthread_mutex_unlock(&lock);
        /* recv_buf.size indicates the amount of valid data in the buffer */
        /* set it to 0 so we can retrieve new data into the buffer */
        recv_buf.size = 0;
    }

    /* cleanup */
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    recv_buf_cleanup(&recv_buf);

    return 0;
}

int main( int argc, char** argv ){
    /* parse the options to get the image and thread numbers */
    int c;
    /* default is 1 thread, image 1 */
    int t = 1;
    int n = 1;
    char *str = "option requires an argument";
    
    /* remove the print statements on submission */
    while ((c = getopt (argc, argv, "t:n:")) != -1) {
        switch (c) {
        case 't':
            t = strtoul(optarg, NULL, 10);
            //t should be greater than 0
            if (t <= 0) {
                fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }
            break;
        case 'n':
            n = strtoul(optarg, NULL, 10);
            //image needs to be 1 2 or 3
            if (n <= 0 || n > 3) {
                fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
                return -1;
            }
            break;
        default:
            return -1;
        }
    }

    /* arguments */
    int retrieved[50] = {0}; /* retrieved array tells us which images we have already retrieved already */
    struct simple_PNG *PNG_array[50]; /* image buffer array stores all retrieved images */
    for(int j = 0; j < 50; j++){
        PNG_array[j] = malloc(sizeof(struct simple_PNG));
    }

    /* thread id array */
    pthread_t *tid = malloc(sizeof(pthread_t)*t);
    struct thread_args *arguments = malloc(sizeof(struct thread_args)*t);
    U32 total_length = 0;
    int num = 0;

    /* init mutex */
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;
    }

    /* initialize the arguments for the threads */
    for(int i = 0; i < t; i++){
        sprintf(arguments[i].image, "%d", n);
        sprintf(arguments[i].device, "%d", (i%3)+1);
        arguments[i].retrieved = retrieved;
        arguments[i].count = &num;
        arguments[i].PNG_array_p = PNG_array;
        arguments[i].total_length = &total_length;


        /* create the thread */
        pthread_create(&tid[i], NULL, get_images, arguments + i);
    }

    //once the threads finish running join them
    for(int i = 0; i < t; i++){
        pthread_join(tid[i], NULL);
    }

    if(num != 50){
        printf("did not retrieve all 50 images\n");
        return 1;
    }

    /* concatenate images in buffer array */
    cat_PNG_struct(PNG_array, total_length);

    /* cleanup */
    for(int i = 0; i < 50; i++){
        free(PNG_array[i]->p_IHDR->p_data_IHDR_chunk);
        free(PNG_array[i]->p_IDAT->p_data);
        free(PNG_array[i]->p_IEND->p_data);
        free(PNG_array[i]->p_IHDR);
        free(PNG_array[i]->p_IDAT);
        free(PNG_array[i]->p_IEND);
        free(PNG_array[i]);
    }
    pthread_mutex_destroy(&lock);
    free(tid);
    free(arguments);

    return 0;
}
