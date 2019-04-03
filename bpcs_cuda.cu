#include "units.h" // for uintN_t typedefs
#include "config.h" // for N_BITPLANES, N_CHANNELS
#define SET_THREAD_COORD(row, x) int row = (blockIdx.y * blockDim.y) + threadIdx.y
#define SET_THREAD_COORDS() SET_THREAD_COORD(row, x); SET_THREAD_COORD(col, y)

/* See https://en.wikipedia.org/wiki/CUDA for variious hardware-specific constraints. */


/* Device */

__global__ void gpu_rgb2cgc(int w, int h, uint8_t* arr){
    /*
        Inputs:
            row_ptrs:
                Matrix of 8-bit pixels
    */
    //int row = (blockIdx.y * blockDim.y) + threadIdx.y;
    //int col = (blockIdx.x * blockDim.x) + threadIdx.x;
    SET_THREAD_COORDS();
    if (row < h && col < w){
        // Order is because most images are landscape, therefore - and this is just a totally uneducated guess - I would expect the height condition to fail more often
        arr[col*row] ^= (arr[col*row] >> 1);
    }
}
/*
inline __device__ uint8_t gpu_calculate_grid_complexity(uint8_t bitgrid[81]){
    uint8_t c = 0;
    for (int i=0;  i < 9*9 - 1;  ++i)
        if (i % 9 != 9 - 1)
            c += bitgrid[i] ^ bitgrid[i+1];
    for (int i=0;  i < 9;  ++i)
        for (int j=0;  j < 8;  ++j)
            c += bitgrid[9*j + i] ^ bitgrid[9*(j+1) + i];
    return c;
}
*/
/*
inline __device__ void gpu_conjugate_grid(int row, int col, uint8_t** row_ptrs){
    for (int i=0; i<8; ++i)
        row_ptrs[row][col] &= (row + col) & 1;
}
*/
inline __device__ uint8_t gpu_calculate_grid_complexity(int row, int col, uint32_t w, uint8_t*& arr){
    uint8_t c = 0;
    for (int j=0; j<8; ++j)
        for (int i=0; i<9; ++i)
            c  +=  arr[w*(row + j) + (col + i)]  ^  arr[w*(row + j + 1) + (col + i)];
    for (int j=0; j<9; ++j)
        for (int i=0; i<8; ++i)
            c  +=  arr[w*(row + j) + (col + i)]  ^  arr[w*(row + j)     + (col + i + 1)];
    return c;
}

__global__ void gpu_extract_bytes(uint8_t t, uint32_t w, uint32_t h, uint32_t n_hztl_grids, uint32_t n_vtcl_grids, uint8_t*& arr, uint8_t*& extraced_bytes){
    /*
        Inputs:
            t
                aka complexity_threshold
            arr
                Channel (i.e. byteplane) matrix, flattened
    */
    SET_THREAD_COORD(row, y);
    int col = (blockIdx.x * blockDim.x) + threadIdx.x;
    
    if (col < w/9 && row < h){
        for (int j=0; j<N_BITPLANES; ++j){
            
            if (gpu_calculate_grid_complexity(row, col, w, arr) >= t){
                extraced_bytes[11*(w*row + col) + 0] = 1;
                for (int i=1; i<11; ++i){
                    extraced_bytes[11*(w*row + col) + i] = 0;
                    for (int k=0; k<8; ++k){
                        extraced_bytes[11*(w*row + col) + i] *= 2;
                        extraced_bytes[11*(w*row + col) + i] |= (arr[w*row + col + k] >> j) & 1;
                    }
                }
                
            }
        }
    }
}



/* Host */

void rgb2cgc(uint32_t w, uint32_t h, uint8_t*& host_img_data){
    int n_grids;
    int n_blocks;
    //cudaOccupancyMaxPotentialBlockSize(&n_grids, &n_blocks, gpu_rgb2cgc, 0, w*h);
    n_grids=1; n_blocks=1024;
    
    uint8_t* arr;
    cudaMalloc(&arr, w*h*sizeof(uint8_t));
    cudaMemcpy(arr, host_img_data, h*w*sizeof(uint8_t), cudaMemcpyHostToDevice);
    
    gpu_rgb2cgc<<<n_grids, n_blocks>>>(w, h, arr);
    cudaDeviceSynchronize();
    
    cudaMemcpy(host_img_data, arr, h*w*sizeof(uint8_t), cudaMemcpyDeviceToHost);
    cudaFree(arr);
}

void extract_bytes(uint8_t t, uint32_t w, uint32_t h, uint32_t n_hztl_grids, uint32_t n_vtcl_grids, uint8_t*& host_img_data, uint8_t*& host_extraced_bytes){
    /*
        Inputs:
            host_extraced_bytes
                Empty array to write to
    */
    int n_grids;
    int n_blocks;
    //cudaOccupancyMaxPotentialBlockSize(&n_grids, &n_blocks, gpu_rgb2cgc, 0, n_hztl_grids*n_vtcl_grids);
    n_grids=1; n_blocks=1024;
    
    uint8_t* arr;
    cudaMalloc(&arr, w*h*sizeof(uint8_t));
    cudaMemcpy(arr, host_img_data, h*w*sizeof(uint8_t), cudaMemcpyHostToDevice);
    
    auto n_extracted_bytes = n_hztl_grids*n_vtcl_grids*11*sizeof(uint8_t);
    uint8_t* extraced_bytes;
    cudaMalloc(&extraced_bytes, n_extracted_bytes);
    
    gpu_extract_bytes<<<n_grids, n_blocks>>>(t, w, h, n_hztl_grids, n_vtcl_grids, arr, extraced_bytes);
    cudaDeviceSynchronize();
    
    cudaFree(arr);
    
    cudaMemcpy(host_extraced_bytes, extraced_bytes, n_extracted_bytes, cudaMemcpyDeviceToHost);
    cudaFree(extraced_bytes);
    
}













































#include <opencv2/core/core.hpp>
#include <png.h>
#include <unistd.h> // for STD(IN|OUT)_FILENO
#include <cstdlib> // for exit()

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
    #define IS_POSIX
#endif

#ifdef DEBUG
    #include <iostream>

    #ifdef IS_POSIX
        #include <execinfo.h> // for printing stack trace
    #endif
#endif

#include "config.h" // for N_BITPLANES, N_CHANNELS


void handler(int sgnl){
  #if (defined (DEBUG)) && defined (IS_POSIX)
    void* arr[10];
    
    size_t size = backtrace(arr, 10);
    
    fprintf(stderr, "E(%d):\n", sgnl);
    backtrace_symbols_fd(arr, size, STDERR_FILENO);
  #endif
    exit(sgnl);
}



class BPCSStreamBuf {
    // Based on excellent post by krzysztoftomaszewski
    // src https://artofcode.wordpress.com/2010/12/12/deriving-from-stdstreambuf/
  public:
    /* Constructors */
    BPCSStreamBuf(const uint8_t min_complexity, int img_n, int n_imgs, char** im_fps):
                // WARNING: img_fps is just argv which needs an index to skip the options
                // Use double pointer rather than array of pointers due to constraints on constructor initialisation
    not_exhausted(true),
    min_complexity(min_complexity), img_n(img_n), img_n_offset(img_n), n_imgs(n_imgs), img_fps(im_fps)
    {}
    
    
    bool not_exhausted;
    int n_extracted_bytes;
    
    uchar* img_data;
    uint8_t* extraction_byte_tensor;
    
    void gets();
    void load_next_img(); // Init
  private:
    const uint8_t min_complexity;
    
    uint8_t channel_n;
    
    uint32_t w;
    uint32_t h;
    uint32_t n_hztl_grids;
    uint32_t n_vtcl_grids;
    
    uint32_t rowbytes;
    
    const int img_n_offset;
    int img_n;
    int n_imgs;
    
    char** img_fps;
    
    cv::Mat im_mat;
    std::vector<cv::Mat> channel_byteplanes;
};

void BPCSStreamBuf::gets(){
    if (this->channel_n == N_CHANNELS){
        if (this->img_n == this->n_imgs){
            this->not_exhausted = false;
            return;
        }
        this->load_next_img();
    }
    
    extract_bytes(this->min_complexity, this->w, this->h, this->n_hztl_grids, this->n_vtcl_grids, this->channel_byteplanes[0].data, this->extraction_byte_tensor);
    
    this->n_extracted_bytes = 0;
    for (int j=0; j<this->n_vtcl_grids; ++j)
        for (int i=0; i<this->n_hztl_grids; ++i)
            if (this->extraction_byte_tensor[11*(this->n_hztl_grids*j + i) + 0] != 0)
                for (int k=1; k<11; ++k)
                    this->extraction_byte_tensor[this->n_extracted_bytes++] = this->extraction_byte_tensor[11*(w*j + i) + k];
                    // Index of LHS is not greater than index of RHS - this is overwriting from the 'left'
    
    ++this->channel_n;
}

void BPCSStreamBuf::load_next_img(){
    if (this->img_n != this->img_n_offset){
        free(this->extraction_byte_tensor);
    }
    
    /* Load PNG file into array */
    FILE* png_file = fopen(this->img_fps[this->img_n], "rb");
    
    uchar png_sig[8];
    
    fread(png_sig, 1, 8, png_file);
    if (!png_check_sig(png_sig, 8)){
        #ifdef DEBUG
        std::cerr << "Bad signature in file `" << this->img_fps[this->img_n] << "`" << std::endl;
        #endif
        handler(60);
    }
    
    auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    if (!png_ptr)
        // Could not allocate memory
        handler(4);
  
    auto png_info_ptr = png_create_info_struct(png_ptr);
    
    if (!png_info_ptr){
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        handler(69);
    }
    
    /* ERROR - incorrect use of incomplete type
    if (setjmp(png_ptr->jmpbuf)){
        png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
        handler(1);
    }
    */
    
    png_init_io(png_ptr, png_file);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, png_info_ptr);
    
  #ifdef TESTS
    int32_t bit_depth;
    int32_t colour_type;
  #endif
    
    png_get_IHDR(
        png_ptr, png_info_ptr, &this->w, &this->h
      #ifdef TESTS
        , &bit_depth, &colour_type
      #else
        , nullptr, nullptr
      #endif
        , NULL, NULL, NULL
    );
    
    this->n_hztl_grids = this->w/9;
    this->n_vtcl_grids = this->h/9;
    
    #ifdef TESTS
        assert(bit_depth == N_BITPLANES);
        if (colour_type != PNG_COLOR_TYPE_RGB){
            handler(61);
        }
    #endif
    
    png_read_update_info(png_ptr, png_info_ptr);
    
    this->rowbytes = png_get_rowbytes(png_ptr, png_info_ptr);
    
    #ifdef TESTS
        assert(png_get_channels(png_ptr, png_info_ptr) == 3);
    #endif
    
    this->img_data = (uchar*)malloc(this->rowbytes*this->h);
    
    uchar* row_ptrs[h];
    for (uint32_t i=0; i<this->h; ++i)
        row_ptrs[i] = this->img_data + i*this->rowbytes;
    
    png_read_image(png_ptr, row_ptrs);
    
    fclose(png_file);
    png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
    
    rgb2cgc(this->w, N_CHANNELS*this->h, row_ptrs[0]);
    //                == this->rowbytes
    
    this->im_mat = cv::Mat(this->h, this->w, CV_8UC3, this->img_data);
    // WARNING: Loaded as RGB rather than OpenCV's default BGR
    
    cv::split(this->im_mat, this->channel_byteplanes);
    
    free(this->img_data);
    
    this->extraction_byte_tensor = (uint8_t*)malloc(this->n_hztl_grids*this->n_vtcl_grids*11*sizeof(uint8_t));
    
    this->channel_n = 0;
    
    ++this->img_n;
}


int main(const int argc, char* argv[]){
    int i = 0;
    
    const uint8_t min_complexity = 50 + (argv[++i][0] -48);
    
    BPCSStreamBuf bpcs_stream(min_complexity, ++i, argc, argv);
    bpcs_stream.load_next_img(); // Init
    
    do {
        bpcs_stream.gets();
        write(STDOUT_FILENO, bpcs_stream.extraction_byte_tensor, bpcs_stream.n_extracted_bytes-1);
    } while (bpcs_stream.not_exhausted);
    free(bpcs_stream.extraction_byte_tensor);
}
