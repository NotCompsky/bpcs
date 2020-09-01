#include "units.h" // for uintN_t typedefs
#include "config.h" // for N_BITPLANES, N_CHANNELS
#define SET_THREAD_COORD(row, x) int row = (blockIdx.y * blockDim.y) + threadIdx.y
#define SET_THREAD_COORDS() SET_THREAD_COORD(row, x); SET_THREAD_COORD(col, y)

#ifdef DEBUG
    #include <cstdio> // for printf
#endif

/* See https://en.wikipedia.org/wiki/CUDA for variious hardware-specific constraints. */


/* Device */

__global__ void gpu_rgb2cgc(
    int n_pixels, uint8_t* arr
  #ifdef DEBUG
    , uint32_t* d_n
  #endif
){
    /*
        Inputs:
            row_ptrs:
                Matrix of 8-bit pixels
    */
    int indx = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (indx < n_pixels){
        // Order is because most images are landscape, therefore - and this is just a totally uneducated guess - I would expect the height condition to fail more often
        arr[indx] ^= (arr[indx] >> 1);
      #ifdef DEBUG
        atomicAdd(d_n, 1);
      #endif
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
inline __device__ void gpu_calculate_grid_complexities(uint8_t bytegrid[81], uint8_t complexities[N_BITPLANES]){
    for (int j=0; j<8; ++j)
        for (int i=0; i<9; ++i)
            for (int k=0; k<N_BITPLANES; ++k)
                complexities[k]  +=  ((bytegrid[9*j + i]  ^  bytegrid[9*(j + 1) + i])  &  (1 << k)) >> k;
    for (int j=0; j<9; ++j)
        for (int i=0; i<8; ++i)
            for (int k=0; k<N_BITPLANES; ++k)
                complexities[k]  +=  ((bytegrid[9*j + i]  ^  bytegrid[9*j + (i + 1)])  &  (1 << k)) >> k;
}

__global__ void gpu_extract_bytes(
    uint8_t t, uint32_t w, uint32_t h, uint32_t n_hztl_grids, uint32_t n_vtcl_grids, uint8_t* arr, uint8_t* extraced_bytes
  #ifdef DEBUG
    , uint32_t* d_ns
  #endif
){
    /*
        Inputs:
            t
                aka complexity_threshold
            arr
                Image data
    */
    int row = (blockIdx.y     * blockDim.y) + threadIdx.y;
    int col = ((blockIdx.x/3) * blockDim.x) + threadIdx.x;
    uint8_t conjugation; // 0 or 1
      #ifdef DEBUG
        d_ns[blockIdx.y*blockDim.x + blockIdx.x] = 1;
        printf(".");
      #endif
    if (row < n_vtcl_grids  &&  col < n_hztl_grids){
        auto channel_n = blockIdx.x % 3;
        uint8_t complexities[N_BITPLANES];
        for (auto i=0; i<N_BITPLANES; ++i)
            complexities[i] = 0;
        
        uint8_t bytegrid[81];
        for (int j=0; j<9; ++j)
            for (int i=0; i<9; ++i)
                bytegrid[9*j + i] = arr[N_CHANNELS*(w*(row + j) + col + i) + channel_n];
        
        gpu_calculate_grid_complexities(bytegrid, complexities);
        
        for (int n_bitplane=0; n_bitplane<N_BITPLANES; ++n_bitplane){
          #ifdef DEBUG
            printf("%d ", complexities[n_bitplane]);
          #endif
            if (complexities[n_bitplane] >= t){
                auto indx = 11 * ((N_BITPLANES * ((n_hztl_grids * row) + col) + n_bitplane) + channel_n * (n_vtcl_grids * n_hztl_grids * N_BITPLANES));
                extraced_bytes[indx] = 1;
                
                conjugation = bytegrid[80];
                
                for (int n_byte=1; n_byte<11; ++n_byte){
                    extraced_bytes[indx + n_byte] = 0;
                    
                    for (int i=0; i<8; ++i){
                        extraced_bytes[indx + n_byte] *= 2;
                        extraced_bytes[indx + n_byte] |= (bytegrid[8*n_byte + i] >> n_bitplane) ^ conjugation;
                    }
                }
                
            }
        }

    }
}



/* Host */

void rgb2cgc(uint32_t n_pixels, uint8_t*& host_img_data){
    int n_grids;
    int n_blocks;
    //cudaOccupancyMaxPotentialBlockSize(&n_grids, &n_blocks, gpu_rgb2cgc, 0, w*h);
    n_grids=n_pixels/1024; n_blocks=1024;
    
  #ifdef DEBUG
    printf("rgb2cgc\t%d grids\t%d threads/grid\n", n_grids, n_blocks); // tmp
    
    uint32_t n_threads_completed = 0;
    uint32_t* d_n;
    cudaMalloc(&d_n, sizeof(uint32_t));
    cudaMemcpy(d_n, &n_threads_completed, sizeof(uint32_t), cudaMemcpyHostToDevice);
  #endif
    
    uint8_t* arr;
    cudaMalloc(&arr, n_pixels);
    cudaMemcpy(arr, host_img_data, n_pixels, cudaMemcpyHostToDevice);
    
    gpu_rgb2cgc<<<n_grids, n_blocks>>>(
        n_pixels, arr
      #ifdef DEBUG
        , d_n
      #endif
    );
    cudaDeviceSynchronize();
    
  #ifdef DEBUG
    cudaMemcpy(&n_threads_completed, d_n, sizeof(uint32_t), cudaMemcpyDeviceToHost);
    printf("n_threads_completed\t%d\n", n_threads_completed);
  #endif
    
    cudaMemcpy(host_img_data, arr, n_pixels, cudaMemcpyDeviceToHost);
    cudaFree(arr);
}

void extract_bytes(uint8_t t, uint32_t w, uint32_t h, uint32_t n_hztl_grids, uint32_t n_vtcl_grids, uint8_t*& host_img_data, uint8_t*& host_extraced_bytes){
    /*
        Inputs:
            host_extraced_bytes
                Empty array to write to
    */
    //cudaOccupancyMaxPotentialBlockSize(&dim_grids, &dim_threads, gpu_rgb2cgc, 0, n_hztl_grids*n_vtcl_grids);
    dim3 dim_grids(N_CHANNELS*(n_hztl_grids/32), n_vtcl_grids/32);
    dim3 dim_threads(32, 32);
    // Memory constrains -> smaller blocks?
    
  #ifdef DEBUG
    printf("extract_bytes\n"); // tmp
    
    auto n_grids = N_CHANNELS*(n_hztl_grids/32) * (n_vtcl_grids/32);
    auto n_threads = 32*32;
    
    uint32_t h_ns[n_grids * n_threads];
    for (auto i=0;  i<n_grids * n_threads;  ++i)
        h_ns[i] = 0;
    
    uint32_t* d_ns;
    cudaMalloc(&d_ns, sizeof(uint32_t) * n_grids * n_threads);
    cudaMemcpy(d_ns, h_ns, sizeof(uint32_t) * n_grids * n_threads, cudaMemcpyHostToDevice);
  #endif
    
    uint8_t* arr;
    cudaMalloc(&arr, w*h*sizeof(uint8_t));
    cudaMemcpy(arr, host_img_data, h*w*sizeof(uint8_t), cudaMemcpyHostToDevice);
    
    auto n_extracted_bytes = N_CHANNELS*n_hztl_grids*n_vtcl_grids*11*sizeof(uint8_t);
    uint8_t* extraced_bytes;
    cudaMalloc((void**)&extraced_bytes, n_extracted_bytes);
    cudaMemset(extraced_bytes, 0, n_extracted_bytes);
    
    gpu_extract_bytes<<<dim_grids, dim_threads>>>(
        t, w, h, n_hztl_grids, n_vtcl_grids, arr, extraced_bytes
      #ifdef DEBUG
        , d_ns
      #endif
    );
    cudaDeviceSynchronize();
    
  #ifdef DEBUG
    uint32_t n_threads_completed = 0;
    
    cudaMemcpy(h_ns, d_ns, sizeof(uint32_t) * n_grids * n_threads, cudaMemcpyDeviceToHost);
    cudaFree(d_ns);
    
    for (auto i=0;  i<n_grids*n_threads;  ++i){
        //printf("n_threads_completed in block %d\t%d\n", i, h_ns[i]);
        n_threads_completed += h_ns[i];
    }
    
    printf("n_threads_completed\t%d\n", n_threads_completed);
  #endif
    
    cudaFree(arr);
    
    cudaMemcpy(host_extraced_bytes, extraced_bytes, n_extracted_bytes, cudaMemcpyDeviceToHost);
    cudaFree(extraced_bytes);
    
}













































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
    min_complexity(min_complexity), img_n(img_n), img_n_offset(img_n), n_imgs(n_imgs), img_fps(im_fps)
    {}
    
    
    int n_extracted_bytes;
    
    uchar* img_data;
    uint8_t* extraction_byte_tensor;
    
    bool gets();
    void load_next_img();
  private:
    const uint8_t min_complexity;
    
    uint32_t w;
    uint32_t h;
    uint32_t n_hztl_grids;
    uint32_t n_vtcl_grids;
    
    const int img_n_offset;
    int img_n;
    int n_imgs;
    
    char** img_fps;
};

bool BPCSStreamBuf::gets(){
    if (this->img_n == this->n_imgs)
        return false;
    
    this->load_next_img();
    
    extract_bytes(this->min_complexity, this->w, this->h, this->n_hztl_grids, this->n_vtcl_grids, this->img_data, this->extraction_byte_tensor);
    
    this->n_extracted_bytes = 0;
    for (int i=0; i<N_CHANNELS*this->n_vtcl_grids*this->n_hztl_grids; ++i)
        if (this->extraction_byte_tensor[11*i + 0] != 0)
            for (int k=1; k<11; ++k)
                this->extraction_byte_tensor[this->n_extracted_bytes++] = this->extraction_byte_tensor[11*i + k];
                // Index of LHS is not greater than index of RHS - this is overwriting from the 'left'
    
  #ifdef DEBUG
    printf("Extracted %d bytes\n", this->n_extracted_bytes);
  #endif
    
    return true;
}

void BPCSStreamBuf::load_next_img(){
    if (this->img_n != this->img_n_offset){
        free(this->img_data);
        free(this->extraction_byte_tensor);
    }
    
  #ifdef DEBUG
    printf("Loading img %d of %d: %s\n", this->img_n, this->n_imgs, this->img_fps[this->img_n]);
  #endif
    
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
        if (bit_depth != N_BITPLANES)
            handler(61);
        if (colour_type != PNG_COLOR_TYPE_RGB){
            handler(61);
        }
    #endif
    
    png_read_update_info(png_ptr, png_info_ptr);
    
    #ifdef TESTS
        if (png_get_channels(png_ptr, png_info_ptr) != 3)
            handler(61);
        if (png_get_rowbytes(png_ptr, png_info_ptr) != N_CHANNELS*this->w)
            handler(61);
    #endif
    
    this->img_data = (uchar*)malloc(N_CHANNELS*this->w*this->h);
    
    uchar* row_ptrs[h];
    for (uint32_t i=0; i<this->h; ++i)
        row_ptrs[i] = this->img_data + i*N_CHANNELS*this->w;
    
    png_read_image(png_ptr, row_ptrs);
    
    fclose(png_file);
    png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
    
    rgb2cgc(N_CHANNELS*this->w*this->h, this->img_data);
    
    this->extraction_byte_tensor = (uint8_t*)malloc(N_CHANNELS * this->n_hztl_grids * this->n_vtcl_grids * 11 * sizeof(uint8_t));
    
    ++this->img_n;
}


int main(const int argc, char* argv[]){
    int i = 0;
    
  #ifdef DEBUG
    bool print_content = true;
    bool ignore_errors = false;
    int verbosity = 3;
    
    while (++i < argc){
        if (argv[i][0] == '-' && argv[i][2] == 0){
            switch(argv[i][1]){
                case 'v': ++verbosity; break;
                case 'q': --verbosity; break;
                case 'Q': print_content=false; break;
                case 'i': ignore_errors=true; break;
                default: --i; goto end_args;
            }
        } else {
            --i;
            goto end_args;
        }
    }
    end_args:
    
    if (verbosity < 0)
        verbosity = 0;
    else if (verbosity > 10)
        verbosity = 10;
  #endif
    
    const uint8_t min_complexity = 50 + (argv[++i][0] -48);
    
    BPCSStreamBuf bpcs_stream(min_complexity, ++i, argc, argv);
    
    while (bpcs_stream.gets()){
      #ifdef DEBUG
        if (print_content)
      #endif
            write(STDOUT_FILENO, bpcs_stream.extraction_byte_tensor, bpcs_stream.n_extracted_bytes);
    }
}