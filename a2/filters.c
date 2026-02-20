/* ------------
 * This code is provided solely for the personal and private use of 
 * students taking the CSC367 course at the University of Toronto.
 * Copying for purposes other than this use is expressly prohibited. 
 * All forms of distribution of this code, whether as given or with 
 * any changes, are expressly prohibited. 
 * 
 * Authors: Bogdan Simion, Maryam Dehnavi, Felipe de Azevedo Piovezan
 * 
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 Bogdan Simion and Maryam Dehnavi
 * -------------
*/

#include "filters.h"
#include <pthread.h>
#include <stdlib.h>

/************** FILTER CONSTANTS*****************/
/* laplacian */
int8_t lp3_m[] =
    {
        0, 1, 0,
        1, -4, 1,
        0, 1, 0,
    };
filter lp3_f = {3, lp3_m};

int8_t lp5_m[] =
    {
        -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1,
        -1, -1, 24, -1, -1,
        -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1,
    };
filter lp5_f = {5, lp5_m};

/* Laplacian of gaussian */
int8_t log_m[] =
    {
        0, 1, 1, 2, 2, 2, 1, 1, 0,
        1, 2, 4, 5, 5, 5, 4, 2, 1,
        1, 4, 5, 3, 0, 3, 5, 4, 1,
        2, 5, 3, -12, -24, -12, 3, 5, 2,
        2, 5, 0, -24, -40, -24, 0, 5, 2,
        2, 5, 3, -12, -24, -12, 3, 5, 2,
        1, 4, 5, 3, 0, 3, 5, 4, 1,
        1, 2, 4, 5, 5, 5, 4, 2, 1,
        0, 1, 1, 2, 2, 2, 1, 1, 0,
    };
filter log_f = {9, log_m};

/* Identity */
int8_t identity_m[] = {1};
filter identity_f = {1, identity_m};

filter *builtin_filters[NUM_FILTERS] = {&lp3_f, &lp5_f, &log_f, &identity_f};

/* Normalizes a pixel given the smallest and largest integer values
 * in the image */
void normalize_pixel(int32_t *target, int32_t pixel_idx, int32_t smallest, 
        int32_t largest)
{
    if (smallest == largest)
    {
        return;
    }
    
    target[pixel_idx] = ((target[pixel_idx] - smallest) * 255) / (largest - smallest);
}
/*************** COMMON WORK ***********************/
/* Process a single pixel and returns the value of processed pixel
 * TODO: you don't have to implement/use this function, but this is a hint
 * on how to reuse your code.
 * */
int32_t apply2d(const filter *f, const int32_t *original, int32_t *target,
        int32_t width, int32_t height,
        int row, int column)
{
    int32_t n = f->dimension;
    int32_t radius = n / 2;
    int32_t sum = 0;

    for (int fr = -radius; fr <= radius; fr++) {
        for (int fc = -radius; fc <= radius; fc++) {

            
            int ir = row + fr; //immidiate row
            int ic = column + fc; //immidirate col 

            if (ir < 0 || ir >= height || ic < 0 || ic >= width)
                continue;

            int32_t pixel = original[ir * width + ic];
            int32_t coeff =
                f->matrix[(fr + radius) * n + (fc + radius)];

            sum += pixel * coeff;
        }
    }

    return sum;
}

/*********SEQUENTIAL IMPLEMENTATIONS ***************/
void apply_filter2d(const filter *f, 
        const int32_t *original, int32_t *target,
        int32_t width, int32_t height)
{
  
  int32_t min = INT32_MAX;
  int32_t max = INT32_MIN; 

  for(int32_t row = 0; row < height; row++){
    for(int32_t col =0; col < width; col++){
        
        int32_t sum = apply2d(f,original,target,width,height,row,col);


        target[row*width+col] = sum; 

        //Update intervals
        if(sum < min) min = sum; 
        if(sum > max) max = sum;
    }
  }

  //Normalize
    for(int32_t i = 0; i < width*height; i++){
    	normalize_pixel(target,i,min,max);
    }

}

/****************** ROW/COLUMN SHARDING ************/
//For all to share 
typedef struct common_work_t{
    const filter *filter; 
    const int32_t *original_image; 
    int32_t *target;
    int32_t width; 
    int32_t height; 
    parallel_method method; 
    int32_t nthreads;
    pthread_barrier_t barrier;
}common_work;  


//For each thread in sharding.
typedef struct thread_work_t{ 
    common_work *c_work; 
    int32_t tid;
}thread_work;

typedef struct tile_t{
    int32_t row;
    int32_t col;
} tile;

typedef struct q_work_t{
    tile *tiles; 
    int32_t next; 
    int32_t total; 
    pthread_mutex_t lock; //needed for when we grab something from the queue.
}tile_queue;

typedef struct common_work_q{
    common_work *c_work;
    tile_queue *queue;
    int32_t chunk;
}common_work_q;

typedef struct thread_work_pool_t{
    common_work_q *cq_work; 
    int32_t tid;
}work_pool;


int32_t *min_max_arry;

void* sharding_work(void *work){
    thread_work* w = (thread_work *) work;
    common_work* c = w->c_work; 
    int32_t min = INT32_MAX;
    int32_t  max = INT32_MIN;

    if(c->method == SHARDED_ROWS){
        int32_t row_block = c->height / c->nthreads;
        int32_t row_start = w->tid * row_block;
        int32_t row_limit = w->tid == c->nthreads-1 ? c->height : row_start + row_block;

        for(int row = row_start; row < row_limit; row++){
            for(int col = 0; col < c->width; col++){
                int32_t sum = apply2d(c->filter, c->original_image, c->target, c->width, c->height, row, col);

                c->target[row*c->width + col] = sum;
                if(sum < min) min = sum;
                if(sum > max) max = sum;

            }
        }
    } else if (c->method == SHARDED_COLUMNS_COLUMN_MAJOR){
        int32_t col_block = c->width/c->nthreads; 
        int32_t col_start = w->tid * col_block; 
        int32_t col_limit = w->tid == c->nthreads-1 ? c->width : col_start + col_block;

        for(int col = col_start; col < col_limit; col++){
            for(int row = 0; row < c->height; row++){
                int32_t sum = apply2d(c->filter, c->original_image, c->target, c->width, c->height, row, col);

                c->target[row*c->width + col] = sum;
                if(sum < min) min = sum;
                if(sum > max) max = sum;
            }
        }
    }
    else{ // ROW Major
         int32_t col_block = c->width/c->nthreads; 
        int32_t col_start = w->tid * col_block; 
        int32_t col_limit = w->tid == c->nthreads-1 ? c->width : col_start + col_block;

        for(int row = 0; row < c->height; row++){
            for(int col = col_start; col < col_limit; col++){
            int32_t sum = apply2d(c->filter, c->original_image, c->target, c->width, c->height, row, col);

                c->target[row*c->width + col] = sum;
                if(sum < min) min = sum;
                if(sum > max) max = sum;
            }
        }

    }

    //Implicitly mutually exclusive since threads will fill their portions then wait
    int global_arr_idx = 2 * w->tid; 
    min_max_arry[global_arr_idx] = min; 
    min_max_arry[global_arr_idx + 1] = max;

    //By the threads wait for this to lift the arry will be full
    pthread_barrier_wait(&c->barrier); 

    //Find the global min and max
    int32_t global_min = INT32_MAX;
    int32_t global_max = INT32_MIN; 

    for(int i = 0; i < 2 * c->nthreads; i+=2){
        if (min_max_arry[i] < global_min) global_min = min_max_arry[i];
        if(min_max_arry[i+1] > global_max) global_max = min_max_arry[i+1];
    }

    //Normalization. 
    if(c->method == SHARDED_ROWS){
        int32_t row_block = c->height / c->nthreads;
        int32_t row_start = w->tid * row_block;
        int32_t row_limit = w->tid == c->nthreads-1 ? c->height : row_start + row_block;

        for(int row= row_start; row < row_limit; row++){
            for(int col = 0; col < c->width; col++){
                normalize_pixel(c->target, row*c->width + col, global_min, global_max);
            }
        }
    }else{ //Colmun sharding
        int32_t col_block = c->width/c->nthreads; 
        int32_t col_start = w->tid * col_block; 
        int32_t col_limit = w->tid == c->nthreads-1 ? c->width : col_start + col_block;

        for(int col = col_start; col < col_limit; col++){
            for(int row = 0; row < c->height; row++){
                normalize_pixel(c->target, row*c->width + col, global_min, global_max);
            }
        }
    }
    


    return NULL;
    
}

/***************** WORK QUEUE *******************/
void* queue_work(void *work)
{
    work_pool* w = (work_pool*) work;
    tile_queue *q = w->cq_work->queue; 
    common_work *c = w->cq_work->c_work;
    int32_t chunk = w->cq_work->chunk;

    int32_t min = INT32_MAX;
    int32_t max = INT32_MIN; 

    while(1){
        tile tile;
        //Grab a tile 
        pthread_mutex_lock(&q->lock);
        if(q->next >= q->total){ //we reached the end of the queue;
            pthread_mutex_unlock(&q->lock);
            break;
        }
        
        //get next tile
        tile = q->tiles[q->next];
        q->next++;
        pthread_mutex_unlock(&q->lock);

        //Compute the tile 
        int row_end = tile.row + chunk; 
        int col_end = tile.col + chunk; 

        if(row_end > c->height) row_end = c->height; //we don't want to more down 
        if(col_end > c->width) col_end = c->width;

        for(int row = tile.row; row < row_end; row++){
            for(int col = tile.col; col < col_end; col++){
                int32_t sum = apply2d(c->filter, c->original_image, c->target, c->width, c->height, row,col);
                c->target[row * c->width + col] = sum; 
                if(sum < min) min = sum;
                if(sum > max) max = sum;
            }
        }  
    }

    int global_arr_idx = 2 * w->tid; 
    min_max_arry[global_arr_idx] = min; 
    min_max_arry[global_arr_idx + 1] = max;

    //Wait on barrier for the array to be filled.
    
    pthread_barrier_wait(&c->barrier);
    if(w->tid == 0){ // using T_0 as an example
        q->next = 0; // we want to go through the queue again when we normalize so by the time every gets here the next property for everyone 
    }
    pthread_barrier_wait(&c->barrier);

    int32_t global_min = INT32_MAX;
    int32_t global_max = INT32_MIN; 

    for(int i = 0; i < 2 * c->nthreads; i+=2){
        if (min_max_arry[i] < global_min) global_min = min_max_arry[i];
        if(min_max_arry[i+1] > global_max) global_max = min_max_arry[i+1];
    }

    
    //Go through the queue again but this time to normalize 
    while(1){
        tile tile;
        pthread_mutex_lock(&q->lock);
        if (q->next >= q->total) {
            pthread_mutex_unlock(&q->lock);
            break;
        }
        tile = q->tiles[q->next];
        q->next++;
        pthread_mutex_unlock(&q->lock);

        int row_end = tile.row + chunk; 
        int col_end = tile.col + chunk; 

        if(row_end > c->height) row_end = c->height; //we don't want to more down 
        if(col_end > c->width) col_end = c->width;

        for(int row = tile.row; row < row_end; row++){
            for(int col = tile.col; col < col_end; col++){
                normalize_pixel(c->target, row*c->width + col, global_min,global_max);
            }
        }  

    }

    return NULL;
}

/***************** MULTITHREADED ENTRY POINT ******/
void apply_filter2d_threaded(const filter *f,
        const int32_t *original, int32_t *target,
        int32_t width, int32_t height,
        int32_t num_threads, parallel_method method, int32_t work_chunk)
{    
  


    //init the global array containing local and max;
    // Each thread would need to store 2 things 
    min_max_arry = malloc(sizeof(int32_t) * num_threads * 2);  
    common_work *common = malloc(sizeof(common_work));

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_threads);

    common->filter = f;
    common->original_image = original;
    common->target = target;
    common->width = width; 
    common->height = height; 
    common->method = method; 
    common->barrier = barrier; 
    common->nthreads = num_threads;

    if(method == WORK_QUEUE){
        //Divide up the cols and rows by the chunk
        int32_t tiles_per_row = (width  + work_chunk - 1) / work_chunk; //ceil() function basically because we take the upper bound.
        int32_t tiles_per_col = (height + work_chunk - 1) / work_chunk;
        int32_t total_tiles = tiles_per_row * tiles_per_col;

        tile *tiles = malloc(sizeof(tile) * total_tiles);

        int idx = 0;
        for(int row = 0; row < height; row+= work_chunk){
            for(int col = 0; col < width; col+=work_chunk){
                tiles[idx] = (tile){row, col};
                idx++; 
            }
        }

        
        //Set up our queue
        tile_queue *queue = malloc(sizeof(tile_queue));
        queue->tiles = tiles;
        queue->next = 0;
        queue->total = total_tiles; 
        pthread_mutex_init(&queue->lock, NULL);

        common_work_q *qcw = malloc(sizeof(common_work_q));
        qcw->c_work = common; 
        qcw->chunk = work_chunk; 
        qcw->queue = queue; 
        
        pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
        work_pool *work = malloc(sizeof(work_pool) * num_threads); 

        for(int i = 0; i < num_threads; i++){
            pthread_t worker; 
            work[i].cq_work = qcw; 
            work[i].tid = i; 

            pthread_create(&worker,NULL, queue_work, &work[i]);        
            threads[i] = worker;
        }

        for (int i = 0; i < num_threads; i++){
            pthread_join(threads[i], NULL);
        }

        pthread_mutex_destroy(&queue->lock);
        pthread_barrier_destroy(&barrier);
        free(threads);
        free(work);
        free(queue);
        free(common);
        free(min_max_arry);
        return;
    }


    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    thread_work *t_work = malloc(sizeof(thread_work) * num_threads); 

    

    //Make the threads.
    for(int i = 0; i < num_threads; i++){
        pthread_t worker; 
        t_work[i].c_work = common; 
        t_work[i].tid = i; 

        pthread_create(&worker,NULL, sharding_work, &t_work[i]);        
        threads[i] = worker;
    }

    //Wait for all the threads to come back 
    for (int i = 0; i < num_threads; i++){
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&barrier);
    free(t_work);
    free(threads);
    free(common);
    free(min_max_arry);
}