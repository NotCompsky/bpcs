#include <opencv2/core/core.hpp>
#include <png.h>
#include <unistd.h> // for STD(IN|OUT)_FILENO

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
    #define IS_POSIX
#endif

#ifdef DEBUG
    #include <iostream>
    #include <compsky/logger.hpp> // for CompskyLogger

    #ifdef IS_POSIX
        #include <execinfo.h> // for printing stack trace
    #endif
#endif

#ifdef EMBEDDOR
    #include "utils.hpp" // for format_out_fp
#endif

#define N_BITPLANES 8
#define N_CHANNELS 3


typedef cv::Matx<uchar, 9, 9> Matx99uc;
typedef cv::Matx<uchar, 9, 8> Matx98uc;
typedef cv::Matx<uchar, 8, 9> Matx89uc;


#ifdef DEBUG
    uint whichbyte = 0;
    uint_fast64_t gridlimit = 0;
    
    uint_fast16_t n_bins = 10;
    uint_fast8_t n_binchars = 200;
    
    // Fancy loggers
    static CompskyLogger mylog("bpcs", std::cout);
    static CompskyLogger mylog1("bpcs1", std::cout);
    
    
    uint MAX_CONJ_GRIDS = 0;
    uint conj_grids_found = 0;
    
    uint_fast64_t SGETPUTC_MAX = 0;
    uint_fast64_t sgetputc_count = 0;
    uint64_t SGETPUTC_FROM = (uint64_t)~0; // Set all bits to 1, i.e. maximal value
    uint64_t SGETPUTC_TO = (uint64_t)~0;
    
    bool ignore_errors = false;
#endif


void handler(int sgnl){
  #if (defined (DEBUG)) && defined (IS_POSIX)
    void* arr[10];
    
    size_t size = backtrace(arr, 10);
    
    fprintf(stderr, "E(%d):\n", sgnl);
    backtrace_symbols_fd(arr, size, STDERR_FILENO);
  #endif
    exit(sgnl);
}


/*
 * Bitwise operations on OpenCV Matrices
 */
inline void cv_div2(cv::Mat& arr, cv::Mat& dest){
    cv::Mat m;
    m = ((arr & 2) / 2) & (arr & 1);
    dest  = arr/2;
    dest -= m;
}

inline void convert_to_cgc(cv::Mat &arr){
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl(0);
        mylog << "Converted to CGC: arr(sum==" << +cv::sum(arr)[0] << ") -> dest(sum==";
    #endif
    cv::Mat dest;
    cv_div2(arr, dest);
    cv::bitwise_xor(arr, dest, arr);
    #ifdef DEBUG
        mylog << +cv::sum(arr)[0] << ")" << std::endl;
    #endif
}



/*
 * Initialise chequerboard
 */
static const Matx99uc chequerboard{1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};



/*
 * Display statistics
 */

#ifdef DEBUG
void print_histogram(std::vector<uint8_t> &complexities, uint_fast16_t n_bins, uint_fast16_t n_binchars){
    std::sort(std::begin(complexities), std::end(complexities));
    uint_fast64_t len_complexities = complexities.size();
    
    if (len_complexities == 0){
        mylog.set_verbosity(2);
        mylog.set_cl('r');
        mylog << "No complexities to display" << std::endl;
        return;
    }
    
    mylog.set_verbosity(3);
    mylog.set_cl('B');
    mylog << "Complexities Histogram" << '\n';
    mylog.set_cl(0);
    
    uint8_t min = *std::min_element(std::begin(complexities), std::end(complexities));
    uint8_t max = *std::max_element(std::begin(complexities), std::end(complexities));
    mylog << "Total: " << +len_complexities << " between " << +min << ", " << +max << '\n';
    if (min == max){
        mylog.set_verbosity(2);
        mylog.set_cl('r');
        mylog << "No complexity range" << std::endl;
        return;
    }
    uint8_t step = 1; //(max - min) / (float)n_bins;
    mylog << "Bins:  " << +n_bins << " with step " << step << '\n';
    std::vector<uint_fast64_t> bin_totals;
    uint8_t bin_max = step;
    uint_fast64_t bin_total = 0;
    
    uint_fast32_t j;
    
    for (j=0; j<len_complexities; ++j){
        while (complexities[j] > bin_max){
            bin_totals.push_back(bin_total);
            bin_max += step;
            bin_total = 0;
        }
        ++bin_total;
    }
    bin_totals.push_back(bin_total);
    
    uint_fast16_t k;
    for (j=0; j<n_bins; ++j){
        bin_total = n_binchars * bin_totals[j] / len_complexities;
        mylog << j * step << ": " << bin_totals[j] << '\n' << "    ";
        for (k=0; k<bin_total; ++k){
            mylog << "#";
        }
        mylog << '\n';
    }
    
    mylog << n_bins * step << std::endl;
}
#endif














class BPCSStreamBuf {
    // Based on excellent post by krzysztoftomaszewski
    // src https://artofcode.wordpress.com/2010/12/12/deriving-from-stdstreambuf/
  public:
    /* Constructors */
    BPCSStreamBuf(const uint8_t min_complexity, int img_n, int n_imgs, char** im_fps
                // WARNING: img_fps is just argv which needs an index to skip the options
                // Use double pointer rather than array of pointers due to constraints on constructor initialisation
                #ifdef EMBEDDOR
                  , bool emb
                  , char* outfmt
                #endif
                ):
    not_exhausted(true),
    #ifdef EMBEDDOR
        embedding(emb), out_fmt(outfmt),
    #endif
    x(0), y(0), min_complexity(min_complexity), img_n(img_n), img_n_offset(img_n), n_imgs(n_imgs), img_fps(im_fps)
    #ifdef DEBUG
        , n_complex_grids_found(0)
    #endif
    {}
    
    
    bool not_exhausted;
    
    #ifdef EMBEDDOR
    const bool embedding;
    char* out_fmt = NULL;
    #endif
    
    std::array<uchar, 10> get();
    
    #ifdef DEBUG
        std::vector<uint8_t> complexities;
    #endif
    
    uchar* img_data;
    
    void load_next_img(); // Init
    
    #ifdef EMBEDDOR
    void put(std::array<uchar, 10>);
    void save_im(); // End
    #endif
  private:
    int x; // the current grid is the (x-1)th grid horizontally and yth grid vertically (NOT the coordinates of the corner of the current grid of the current image)
    int y;
    
    #ifdef DEBUG
        uint_fast64_t n_grids;
        uint_fast64_t n_complex_grids_found;
    #endif

    const uint8_t min_complexity;
    
    uint8_t channel_n;
    uint8_t bitplane_n;
    
    const int img_n_offset;
    int img_n;
    int n_imgs;
    
    std::vector<cv::Mat> channel_byteplanes;
    
    cv::Mat channel;
    
    Matx99uc grid{9, 9, CV_8UC1};
    
    Matx98uc xor_adj_mat1{9, 8, CV_8UC1};
    Matx89uc xor_adj_mat2{8, 9, CV_8UC1};
    
    cv::Rect xor_adj_rect1{cv::Point(0, 0), cv::Size(8, 9)};
    cv::Rect xor_adj_rect2{cv::Point(1, 0), cv::Size(8, 9)};
    cv::Rect xor_adj_rect3{cv::Point(0, 0), cv::Size(9, 8)};
    cv::Rect xor_adj_rect4{cv::Point(0, 1), cv::Size(9, 8)};
    
    cv::Mat grid_orig{9, 9, CV_8UC1};
    
    cv::Mat bitplane;
    
    #ifdef EMBEDDOR
    cv::Mat bitplanes[N_BITPLANES * 4]; // WARNING: Images rarely have a bit-depth greater than 32, but would ideally be set on per-image basis
    #endif
    
    cv::Mat im_mat;
    
    #ifdef EMBEDDOR
    png_color_16p png_bg;
    #endif
    
    char** img_fps;
    
    void set_next_grid();
    void load_next_bitplane();
    void load_next_channel();
    
    inline uint8_t get_grid_complexity(Matx99uc&);
    inline uint8_t get_grid_complexity(cv::Mat&);
    inline void conjugate_grid();
    #ifdef DEBUG
        void print_state();
    #endif
};

#ifdef DEBUG
void BPCSStreamBuf::print_state(){
    mylog.set_cl(0);
    mylog << "embedding: " << +this->embedding << std::endl;
    mylog << "bitplane_n: " << +this->bitplane_n << std::endl;
    mylog << "n_bitplanes: " << +N_BITPLANES << std::endl;
    mylog << "channel_n: " << +this->channel_n << std::endl;
    mylog << "n_channels: " << +N_CHANNELS << std::endl;
    mylog << "n_complex_grids_found: " << +n_complex_grids_found << std::endl;
    mylog << "sgetputc_count: " << +sgetputc_count << std::endl;
}
#endif

inline uint8_t BPCSStreamBuf::get_grid_complexity(Matx99uc &arr){
    uint8_t sum = 0;
    cv::bitwise_xor(arr.get_minor<8,9>(1,0), arr.get_minor<8,9>(0,0), this->xor_adj_mat2);
    sum += cv::sum(this->xor_adj_mat2)[0];
    
    cv::bitwise_xor(arr.get_minor<9,8>(0,1), arr.get_minor<9,8>(0,0), this->xor_adj_mat1);
    sum += cv::sum(this->xor_adj_mat1)[0];
    
    return sum;
}
inline uint8_t BPCSStreamBuf::get_grid_complexity(cv::Mat &arr){
    uint8_t sum = 0;
    cv::bitwise_xor(arr(this->xor_adj_rect2), arr(this->xor_adj_rect1), this->xor_adj_mat1);
    sum += cv::sum(this->xor_adj_mat1)[0];
    
    cv::bitwise_xor(arr(this->xor_adj_rect4), arr(this->xor_adj_rect3), this->xor_adj_mat2);
    sum += cv::sum(this->xor_adj_mat2)[0];
    
    return sum;
}

inline void BPCSStreamBuf::conjugate_grid(){
    cv::bitwise_xor(this->grid, chequerboard, this->grid);
    
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl('p');
        mylog << "<*(" << +(this->x -9) << ", " << +this->y << ")>" << std::endl;
        mylog << this->grid << std::endl;
    #endif
}

inline void BPCSStreamBuf::load_next_bitplane(){
    this->bitplane = this->channel & 1;
    cv_div2(this->channel, this->channel);
}

void BPCSStreamBuf::load_next_channel(){
    this->channel = this->channel_byteplanes[this->channel_n];
    this->bitplane_n = 0;
    this->load_next_bitplane();
}

void BPCSStreamBuf::load_next_img(){
    #ifdef DEBUG
        mylog.set_verbosity(3);
        mylog.set_cl('g');
        mylog << "Loading img " << +this->img_n << " of " << +this->n_imgs << " `" << this->img_fps[this->img_n] << "`, using: Complexity >= " << +this->min_complexity << std::endl;
    #endif
    
  #ifdef TESTS
    assert(this->img_n != this->n_imgs);
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
    
    uint32_t w;
    uint32_t h;
  #ifdef TESTS
    int32_t bit_depth;
    int32_t colour_type;
  #endif
    
    png_get_IHDR(
        png_ptr, png_info_ptr, &w, &h
      #ifdef TESTS
        , &bit_depth, &colour_type
      #else
        , nullptr, nullptr
      #endif
        , NULL, NULL, NULL
    );
    
    #ifdef TESTS
        assert(bit_depth == N_BITPLANES);
        if (colour_type != PNG_COLOR_TYPE_RGB){
            #ifdef DEBUG
            mylog.set_verbosity(0);
            mylog << "colour_type: " << +colour_type << " != " << +PNG_COLOR_TYPE_RGB << std::endl;
            mylog << "These are bits - probably 2 for COLOR, 4 for ALPHA, i.e. 6 for 4 channel image" << std::endl;
            #endif
            handler(61);
        }
    #endif
    
    #ifdef EMBEDDOR
    png_get_bKGD(png_ptr, png_info_ptr, &this->png_bg);
    #endif
    
    uint32_t rowbytes;
    
    png_read_update_info(png_ptr, png_info_ptr);
    
    rowbytes = png_get_rowbytes(png_ptr, png_info_ptr);
    
    #ifdef TESTS
        assert(png_get_channels(png_ptr, png_info_ptr) == 3);
    #endif
    
    this->img_data = (uchar*)malloc(rowbytes*h);
    
    uchar* row_ptrs[h];
    for (uint32_t i=0; i<h; ++i)
        row_ptrs[i] = this->img_data + i*rowbytes;
    
    png_read_image(png_ptr, row_ptrs);
    
    fclose(png_file);
    png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
    
    this->im_mat = cv::Mat(h, w, CV_8UC3, this->img_data);
    // WARNING: Loaded as RGB rather than OpenCV's default BGR
    
    convert_to_cgc(this->im_mat);
    
    #ifdef TESTS
        assert(this->im_mat.depth() == CV_8U);
        assert(this->im_mat.channels() == 3);
    #endif
    
    cv::split(this->im_mat, this->channel_byteplanes);
  #ifdef EMBEDDOR
    if (!this->embedding)
        free(this->img_data);
  #endif
    
    #ifdef DEBUG
        mylog.set_verbosity(4);
        this->print_state();
    #endif
    
    this->channel_n = 0;
    
    #ifdef EMBEDDOR
    if (this->embedding){
        uint_fast8_t k = 0;
        uint_fast8_t i = 0;
        for (uint_fast8_t j=0; j<N_CHANNELS; ++j){
            i = 0;
            while (i < N_BITPLANES){
                this->bitplanes[k++] = this->channel_byteplanes[j] & 1;
                cv_div2(this->channel_byteplanes[j], this->channel_byteplanes[j]);
                ++i;
            }
        }
        this->bitplane = this->bitplanes[0];
        
        this->bitplane_n = 0;
    } else {
    #endif
        this->bitplane = cv::Mat::zeros(im_mat.rows, im_mat.cols, CV_8UC1); // Need to initialise for bitandshift
        this->load_next_channel();
    #ifdef EMBEDDOR
    }
    #endif
    
    // Get first data grid
    this->set_next_grid();
    
    #ifdef EMBEDDOR
    if (!this->embedding){
    #endif
        if (this->img_n == this->img_n_offset){
            // If false, this function is being called from within get()
            
            if (this->grid.val[80])
                this->conjugate_grid();
        }
    #ifdef EMBEDDOR
    }
    #endif
    
    ++this->img_n;
}

void BPCSStreamBuf::set_next_grid(){
    #ifdef DEBUG
    if (++whichbyte == gridlimit){
        mylog << std::endl;
        throw std::runtime_error("Reached gridlimit");
    }
    #endif
    
    uint8_t complexity;
    int i = this->x;
    for (int j=this->y;  j <= this->im_mat.rows -9;  j+=9, i=0){
        while (i <= this->im_mat.cols -9){
            cv::Rect grid_shape(cv::Point(i, j), cv::Size(9, 9));
            
            this->grid_orig = this->bitplane(grid_shape);
            
            complexity = this->get_grid_complexity(this->grid_orig);
            
            #ifdef DEBUG
                this->complexities.push_back(complexity);
            #endif
            
            i += 9;
            
            if (complexity >= this->min_complexity){
                this->grid_orig = this->bitplane(grid_shape);
                this->grid_orig.copyTo(this->grid);
                this->x = i;
                this->y = j;
                #ifdef DEBUG
                    ++this->n_complex_grids_found;
                    mylog.set_verbosity(7);
                    mylog.set_cl('B');
                    mylog << "Found grid(" << +i << "," << +j << "): " << +complexity << "\n";
                    mylog.set_cl('r');
                    mylog << this->grid << std::endl;
                #endif
                return;
            }
        }
    }
    
    // If we are here, we have exhausted the bitplane
    
    this->x = 0;
    this->y = 0;
    
    #ifdef DEBUG
        mylog.set_verbosity(4);
        mylog.set_cl('b');
        mylog << "Exhausted bitplane" << std::endl;
        this->print_state();
    #endif
    
    ++this->bitplane_n;
    #ifdef EMBEDDOR
    if (this->embedding){
        if (this->bitplane_n < N_BITPLANES * N_CHANNELS){
            this->bitplane = this->bitplanes[this->bitplane_n];
            goto try_again;
        }
    } else
    #endif
    if (this->bitplane_n < N_BITPLANES){
        this->load_next_bitplane();
        goto try_again;
    } else if (++this->channel_n < N_CHANNELS){
        this->load_next_channel();
        goto try_again;
    }
    
    // If we are here, we have exhausted the image
    if (img_n < this->n_imgs){
#ifdef EMBEDDOR
        if (this->embedding)
            this->save_im();
#endif
        this->load_next_img();
        return;
    }
    
    // If we are here, we have exhausted all images!
    // This is not necessarily alarming - this termination is used rather than returning status values for each get() call.
    #ifdef DEBUG
        print_histogram(this->complexities, n_bins, n_binchars);
        mylog.set_verbosity(0);
        mylog << "Exhausted all vessel images" << std::endl;
    #endif
    #ifdef EMBEDDOR
    if (this->embedding){
        handler(255);
    }
    #endif
    not_exhausted = false;
    return;
    
    try_again:
    this->set_next_grid();
}

std::array<uchar, 10> BPCSStreamBuf::get(){
    std::array<uchar, 10> out;
    for (uint_fast8_t j=0; j<10; ++j){
        out[j] = 0;
        for (uint_fast8_t i=0; i<8; ++i){
            out[j] |= this->grid.val[8*j +i] << i;
        }
    }
    
    #ifdef DEBUG
        sgetputc_count += 10;
        if (sgetputc_count >= SGETPUTC_TO)
            handler(0);
        else if (sgetputc_count >= SGETPUTC_FROM)
            mylog.set_level(10);
    #endif
    
    this->set_next_grid();
    
    if (this->grid.val[80] != 0)
        this->conjugate_grid();
    
    return out;
}

#ifdef EMBEDDOR
void BPCSStreamBuf::put(std::array<uchar, 10> in){
    for (uint_fast8_t j=0; j<10; ++j)
        for (uint_fast8_t i=0; i<8; ++i){
            this->grid.val[8*j +i] = in[j] & 1;
            in[j] = in[j] >> 1;
        }
    
    this->grid.val[80] = 0;
    
    #ifdef DEBUG
        sgetputc_count += 10;
        if (sgetputc_count >= SGETPUTC_TO)
            handler(0);
        else if (sgetputc_count >= SGETPUTC_FROM)
            mylog.set_level(10);
        mylog.set_verbosity(8);
        mylog.set_cl('B');
        mylog << "Last grid (pre-conj)" << "\n";
        mylog.set_cl(0);
        mylog << this->grid << std::endl;
    #endif
    if (this->get_grid_complexity(this->grid) < this->min_complexity)
        this->conjugate_grid();
    
    cv::Mat(this->grid).copyTo(this->grid_orig);
    this->set_next_grid();
}

void BPCSStreamBuf::save_im(){
    #ifdef DEBUG
        mylog.set_verbosity(4);
        mylog.set_cl('b');
        mylog << "UnXORing " << +N_CHANNELS << " channels of depth " << +N_BITPLANES << std::endl;
    #endif
    uint_fast8_t j;
    uint_fast8_t k = N_CHANNELS * N_BITPLANES;
    uint_fast8_t i = N_CHANNELS -1;
    
    do {
        // First bitplane (i.e. most significant bit) of each channel is unchanged by conversion to CGC
        this->channel_byteplanes[i] = this->bitplanes[--k].clone();
        j = N_BITPLANES -1;
        this->channel_byteplanes[i] *= (1 << j--);
        do {
            --k;
            #ifdef TESTS
                if (this->bitplanes[k].cols == 0){
                    std::cerr << "bitplane " << +k << " is empty" << std::endl;
                    handler(10);
                }
            #endif
            
            this->bitplane = this->bitplanes[k].clone();
            this->bitplane *= (1 << j);
            cv::bitwise_or(this->channel_byteplanes[i], this->bitplane, this->channel_byteplanes[i]);
        } while (j-- != 0);
    } while (i-- != 0);
    
    format_out_fp(this->out_fmt, &this->img_fps[this->img_n -1]);
    cv::merge(this->channel_byteplanes, this->im_mat);
    convert_to_cgc(this->im_mat);
    #ifdef DEBUG
        mylog.set_verbosity(3);
        mylog.set_cl('g');
        mylog << "Saving to  `" << this->img_fps[this->img_n -1] << "`" << std::endl;
    #endif
    
    
    
    FILE* png_file = fopen(this->img_fps[this->img_n -1], "wb");
    auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    #ifdef TESTS
    if (!png_file){
        handler(5);
    }
    if (!png_ptr){
        handler(66);
    }
    #endif
    
    auto png_info_ptr = png_create_info_struct(png_ptr);
    
    if (!png_info_ptr){
        handler(67);
    }
    
    if (setjmp(png_jmpbuf(png_ptr))){
        handler(68);
    }
    
    png_init_io(png_ptr, png_file);
    
    if (setjmp(png_jmpbuf(png_ptr))){
        handler(69);
    }
    
    png_set_bKGD(png_ptr, png_info_ptr, this->png_bg);
    
    png_set_IHDR(png_ptr, png_info_ptr, this->im_mat.cols, this->im_mat.rows, N_BITPLANES, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    
    png_write_info(png_ptr, png_info_ptr);
    
    if (setjmp(png_jmpbuf(png_ptr))){
        handler(64);
    }
    
    uchar* row_ptrs[this->im_mat.rows];
    for (uint32_t i=0; i<this->im_mat.rows; ++i)
        row_ptrs[i] = this->img_data + i*3*this->im_mat.cols;
    
    png_write_image(png_ptr, row_ptrs);
    
    if (setjmp(png_jmpbuf(png_ptr))){
        handler(65);
    }
    
    png_write_end(png_ptr, NULL);
    
    free(this->img_fps[this->img_n -1]);
    free(this->img_data);
}
#endif


int main(const int argc, char* argv[]){
    int i = 0;
#ifdef EMBEDDOR
    const bool embedding = (argv[1][0] == '-' && argv[1][1] == 'o' && argv[1][2] == 0);
    
    char* out_fmt;
    
    if (embedding){
        ++i;
        out_fmt = argv[++i];
    } else
        out_fmt = NULL;
#endif
    
    #ifdef DEBUG
        bool print_content = true;
        int verbosity = 3;
        
        while (++i < argc){
            if (argv[i][0] == '-' && argv[i][2] == 0){
                switch(argv[i][1]){
                    case 'f': SGETPUTC_FROM=std::stoi(argv[++i]); break;
                    case 't': SGETPUTC_TO=std::stoi(argv[++i]); break;
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
        
        mylog.set_level(verbosity);
    #endif
    
    const uint8_t min_complexity = 50 + (argv[++i][0] -48);
    #ifdef DEBUG
    if (!(50 <= min_complexity && min_complexity <= 56)){
        mylog.set_verbosity(0);
        mylog << "E: invalid min_complexity" << std::endl;
        mylog << "min_complexity:\t" << +min_complexity << std::endl;
        mylog.set_verbosity(1);
        mylog << "i:\t" << +i << std::endl;
        mylog << "argv[i]:\t" << argv[i] << std::endl;
    }
    #elif defined(TESTS)
    assert(50 <= min_complexity && min_complexity <= 56);
    #endif
    
    BPCSStreamBuf bpcs_stream(min_complexity, ++i, argc, argv
                              #ifdef EMBEDDOR
                              , embedding
                              , out_fmt
                              #endif
                              );
    bpcs_stream.load_next_img(); // Init
    
    std::array<uchar, 10> arr;
    // Using std::array rather than C-style array to allow direct copying
    // arr is the same size as a pointer (8 bytes), so perhaps copying directly is more performative.
#ifdef EMBEDDOR
  if (!embedding){
#endif
    do {
        arr = bpcs_stream.get();
        #ifdef DEBUG
            if (print_content)
        #endif
                write(STDOUT_FILENO, arr.data(), 10);
    } while (bpcs_stream.not_exhausted);
    free(bpcs_stream.img_data);
#ifdef EMBEDDOR
  // if (!embedding){
  //     ...
  } else {
    read(STDIN_FILENO, arr.data(), 10);
    do {
        // read() returns the number of bytes written
        bpcs_stream.put(arr);
    } while (read(STDIN_FILENO, arr.data(), 10) == 10);
    bpcs_stream.put(arr);
    bpcs_stream.save_im();
  }
#endif
}
