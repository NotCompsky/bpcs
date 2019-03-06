#include <opencv2/core/core.hpp>
#include <iostream>
#include <fstream>
#include <png.h>
#include <sys/stat.h> // for stat
#include <string> // for std::stoi (stof already defined elsewhere)

namespace sodium {
    #include <sodium.h> // /crypto_secretstream_xchacha20poly1305.h> // libsodium for cryption (-lsodium)
}

#ifdef DEBUG
    #include <compsky/logger.hpp> // for CompskyLogger
#endif


#define N_BITPLANES 8
#define N_CHANNELS 3


typedef cv::Matx<uchar, 8, 8> Matx88uc;
typedef cv::Matx<uchar, 8, 7> Matx87uc;
typedef cv::Matx<uchar, 7, 8> Matx78uc;


static const std::string NULLSTR = "[NULL]";


#ifdef DEBUG
    uint whichbyte = 0;
    uint_fast64_t gridlimit = 0;
    
    
    // Fancy loggers
    static CompskyLogger mylog("bpcs", std::cout);
    static CompskyLogger mylog1("bpcs1", std::cout);
    
    // Plain loggers
    static CompskyLogger os1("", std::cout);
    
    
    uint MAX_CONJ_GRIDS = 0;
    uint conj_grids_found = 0;
    
    uint_fast64_t SGETPUTC_MAX = 0;
    uint_fast64_t sgetputc_count = 0;
#endif



inline uint_fast64_t get_charp_len(char* chrp){
    uint_fast64_t i = 0;
    while (*(chrp++) != 0)
        ++i;
    return i;
}



std::string format_out_fp(char* out_fmt, char* fp){
    // WARNING: Requires absolute paths?
    std::string basename;
    std::string dir = "";
    std::string ext;
    std::string fname;
    std::string result = "";
    
    // Intermediates
    std::string slashblock;
    std::string dotblock;
    bool dot_present = false;
    
    for (char* it=fp; *it; ++it){
        if (*it == '/'){
            dir += slashblock;
            slashblock = "";
            if (dot_present){
                dir += '.' + dotblock;
                dotblock = "";
                dot_present = false;
            }
            continue;
        }
        if (*it == '.'){
            if (dot_present){
                slashblock += '.' + dotblock;
                dotblock = "";
            }
            dot_present = true;
            continue;
        }
        if (dot_present){
            dotblock += *it;
        } else {
            slashblock += *it;
        }
    }
    basename = slashblock;
    ext = dotblock;
    fname = slashblock + "." + dotblock;
    
    for (char* it=out_fmt; *it; ++it){
        if (*it == '{'){
            ++it;
            if (*it == '{'){
                result += '{';
            } else if (*it == 'b'){
                it += 8; // WARNING: Skipping avoids error checking.
                result += basename;
            } else if (*it == 'd'){
                it += 3;
                result += dir;
            } else if (*it == 'e'){
                it += 3;
                result += ext;
            } else if (*it == 'f'){
                ++it;
                if (*it == 'p'){
                    it += 1;
                    result += fp;
                } else {
                    it += 4;
                    result += fname;
                }
            }
            continue;
        } else {
            result += *it;
        }
    }
    return result;
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
static const Matx88uc chequerboard{1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1};



/*
 * Display statistics
 */

#ifdef DEBUG
void print_histogram(std::vector<float> &complexities, uint_fast16_t n_bins, uint_fast16_t n_binchars){
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
    
    float min = *std::min_element(std::begin(complexities), std::end(complexities));
    float max = *std::max_element(std::begin(complexities), std::end(complexities));
    mylog << "Total: " << +len_complexities << " between " << +min << ", " << +max << '\n';
    if (min == max){
        mylog.set_verbosity(2);
        mylog.set_cl('r');
        mylog << "No complexity range" << std::endl;
        return;
    }
    float step = (max - min) / (float)n_bins;
    mylog << "Bins:  " << +n_bins << " with step " << step << '\n';
    std::vector<uint_fast64_t> bin_totals;
    float bin_max = step;
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
    BPCSStreamBuf(const float min_complexity, std::vector<char*>& img_fps,
                #ifdef EMBEDDOR
                  bool emb,
                #endif
                  char* outfmt):
    #ifdef EMBEDDOR
        embedding(emb), out_fmt(outfmt),
    #endif
    x(0), y(0), min_complexity(min_complexity), img_n(0), img_fps(img_fps)
    {}
    
    
    #ifdef EMBEDDOR
    const bool embedding;
    char* out_fmt = NULL;
    #endif
    
    uchar sgetc();
    
    #ifdef DEBUG
        std::vector<float> complexities;
    #endif
    
    void load_next_img(); // Init
    
    #ifdef EMBEDDOR
    void sputc(uchar c);
    void save_im(); // End
    #endif
  private:
    uint64_t x; // the current grid is the (x-1)th grid horizontally and yth grid vertically (NOT the coordinates of the corner of the current grid of the current image)
    uint64_t y;
    
    #ifdef DEBUG
        uint_fast64_t n_grids;
        uint_fast64_t n_complex_grids_found;
    #endif

    const float min_complexity;
    
    uint8_t gridbitindx; // Count of bits already read/written, modulo 64 (i.e. the index in the grid we are writing/reading the byte to/from)
    uint8_t conjmap_indx;
    // To reserve the first grid of every 64 complex grids in order to write conjugation map
    // Note that the first bit of this map is for its own conjugation state
    
    uint8_t channel_n;
    uint8_t bitplane_n;
    
    uint16_t img_n;
    
    const std::vector<char*> img_fps;
    
    #ifdef EMBEDDOR
    std::vector<cv::Mat> channel_byteplanes;
    #endif
    cv::Mat channel;
    
    Matx88uc grid{8, 8, CV_8UC1};
    Matx88uc conjgrid{8, 8, CV_8UC1};
    
    Matx87uc xor_adj_mat1{8, 7, CV_8UC1};
    Matx78uc xor_adj_mat2{7, 8, CV_8UC1};
    
    cv::Rect xor_adj_rect1{cv::Point(0, 0), cv::Size(7, 8)};
    cv::Rect xor_adj_rect2{cv::Point(1, 0), cv::Size(7, 8)};
    cv::Rect xor_adj_rect3{cv::Point(0, 0), cv::Size(8, 7)};
    cv::Rect xor_adj_rect4{cv::Point(0, 1), cv::Size(8, 7)};
    
    cv::Mat grid_orig{8, 8, CV_8UC1};
    
    #ifdef EMBEDDOR
    cv::Mat conjgrid_orig{8, 8, CV_8UC1};
    #endif
    
    cv::Mat bitplane;
    
    #ifdef EMBEDDOR
    cv::Mat bitplanes[32 * 4]; // WARNING: Images rarely have a bit-depth greater than 32, but would ideally be set on per-image basis
    #endif
    
    uchar* img_data;
    cv::Mat im_mat;
    
    #ifdef EMBEDDOR
    png_color_16p png_bg;
    #endif
    
    void set_next_grid();
    void load_next_bitplane();
    void load_next_channel();
    
    #ifdef EMBEDDOR
    void write_conjugation_map();
    #ifdef TESTS
    void unset_conjmap();
    void assert_conjmap_set();
    #endif
    #endif
    
    inline float get_grid_complexity(Matx88uc&);
    inline float get_grid_complexity(cv::Mat&);
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

inline float BPCSStreamBuf::get_grid_complexity(Matx88uc &arr){
    float sum = 0;
    cv::bitwise_xor(arr.get_minor<7,8>(1,0), arr.get_minor<7,8>(0,0), this->xor_adj_mat2);
    sum += cv::sum(this->xor_adj_mat2)[0];
    
    cv::bitwise_xor(arr.get_minor<8,7>(0,1), arr.get_minor<8,7>(0,0), this->xor_adj_mat1);
    sum += cv::sum(this->xor_adj_mat1)[0];
    
    return sum / (2*8*7);
}
inline float BPCSStreamBuf::get_grid_complexity(cv::Mat &arr){
    float sum = 0;
    cv::bitwise_xor(arr(this->xor_adj_rect2), arr(this->xor_adj_rect1), this->xor_adj_mat1);
    sum += cv::sum(this->xor_adj_mat1)[0];
    
    cv::bitwise_xor(arr(this->xor_adj_rect4), arr(this->xor_adj_rect3), this->xor_adj_mat2);
    sum += cv::sum(this->xor_adj_mat2)[0];
    
    return sum / (2*8*7);
}

inline void BPCSStreamBuf::conjugate_grid(){
    cv::bitwise_xor(this->grid, chequerboard, this->grid);
    
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl('p');
        mylog << "<*" << +this->conjmap_indx << "(" << +(this->x -8) << ", " << +this->y << ")>" << std::endl;
        mylog << this->grid << std::endl;
    #endif
}

inline void BPCSStreamBuf::load_next_bitplane(){
    this->bitplane = this->channel & 1;
    cv_div2(this->channel, this->channel);
}

void BPCSStreamBuf::load_next_channel(){
    cv::extractChannel(this->im_mat, this->channel, this->channel_n);
    convert_to_cgc(this->channel);
    this->bitplane_n = 0;
    this->load_next_bitplane();
}

void BPCSStreamBuf::load_next_img(){
    #ifdef EMBEDDOR
    if (this->embedding && this->img_n != 0)
        this->save_im();
    #endif
    #ifdef DEBUG
        mylog.set_verbosity(3);
        mylog.set_cl('g');
        mylog << "Loading img " << +this->img_n << " of " << +this->img_fps.size() << " `" << this->img_fps[this->img_n] << "`, using: Complexity >= " << +this->min_complexity << std::endl;
    #endif
    
    
    
    /* Load PNG file into array */
    FILE* png_file = fopen(this->img_fps[this->img_n], "rb");
    
    uchar png_sig[8];
    
    fread(png_sig, 1, 8, png_file);
    if (!png_check_sig(png_sig, 8)){
        #ifdef DEBUG
        std::cerr << "Bad signature in file `" << this->img_fps[this->img_n] << "`" << std::endl;
        #endif
        abort();
    }
    
    auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    if (!png_ptr)
        // Could not allocate memory
        abort();
  
    auto png_info_ptr = png_create_info_struct(png_ptr);
    
    if (!png_info_ptr){
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        abort();
    }
    
    /* ERROR - incorrect use of incomplete type
    if (setjmp(png_ptr->jmpbuf)){
        png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
        abort();
    }
    */
    
    png_init_io(png_ptr, png_file);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, png_info_ptr);
    
    uint32_t w;
    uint32_t h;
    int32_t bit_depth;
    int32_t colour_type;
    
    png_get_IHDR(png_ptr, png_info_ptr, &w, &h, &bit_depth, &colour_type, NULL, NULL, NULL);
    
    #ifdef TESTS
        assert(bit_depth == N_BITPLANES);
        assert(colour_type == PNG_COLOR_TYPE_RGB);
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
    
    if ((this->img_data = (uchar*)malloc(rowbytes*h)) == NULL){
        png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
        abort();
    }
    
    uchar* row_ptrs[h];
    for (uint32_t i=0; i<h; ++i)
        row_ptrs[i] = this->img_data + i*rowbytes;
    
    png_read_image(png_ptr, row_ptrs);
    
    this->im_mat = cv::Mat(h, w, CV_8UC3, this->img_data);
    // WARNING: Loaded as RGB rather than OpenCV's default BGR
    
    #ifdef TESTS
        assert(this->im_mat.depth() == CV_8U);
        assert(this->im_mat.channels() == 3);
    #endif
    
    #ifdef EMBEDDOR
    cv::split(this->im_mat, this->channel_byteplanes);
    #endif
    
    #ifdef DEBUG
        mylog.set_verbosity(4);
        this->print_state();
        this->complexities.clear();
        this->n_complex_grids_found = 0;
    #endif
    
    this->channel_n = 0;
    this->gridbitindx = 0;
    
    #ifdef EMBEDDOR
    if (this->embedding){
        uint_fast8_t k = 0;
        uint_fast8_t i = 0;
        for (uint_fast8_t j=0; j<N_CHANNELS; ++j){
            convert_to_cgc(this->channel_byteplanes[j]);
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
    
    #ifdef TESTS
        this->unset_conjmap();
    #endif
    #endif
    
    this->conjmap_indx = 0;
    
    // Get conjugation grid
    this->set_next_grid();
    
    #ifdef EMBEDDOR
    if (this->embedding){
        this->conjgrid_orig = this->grid_orig;
    } else {
    #endif
        if (this->grid.val[63] != 0)
            // Since this is the conjugation map grid, it contains its own conjugation status in its last bit
            // TODO: Have each conjgrid store the conjugation bit of the next conjgrid
            this->conjugate_grid();
        
        memcpy(this->conjgrid.val, this->grid.val, 64);
    #ifdef EMBEDDOR
    }
    #endif
    
    // Get first data grid
    this->set_next_grid();
    
    #ifdef TESTS
    assert(this->conjmap_indx == 0);
    #endif
    
    #ifdef EMBEDDOR
    if (!this->embedding){
    #endif
        if (this->img_n == 0){
            // If false, this function is being called from within sgetc()
            
            if (this->conjgrid.val[0])
                this->conjugate_grid();
            
            this->conjmap_indx = 1;
            // Must be 1 for extracting, and 0 for embedding
            // It denotes the index of the *next* complex grid when extracting, but the index of the *current* grid when embedding.
        }
    #ifdef EMBEDDOR
    }
    #endif
    
    ++this->img_n;
}


#ifdef EMBEDDOR
#ifdef TESTS
void BPCSStreamBuf::unset_conjmap(){
    for (uint8_t i=0; i<64; ++i)
        this->conjgrid.val[i] = 2;
}
void BPCSStreamBuf::assert_conjmap_set(){
    for (uint8_t i=0; i<63; ++i){
        if (this->conjgrid.val[i] == 2){
            std::cerr << "conjmap[" << +i << "] was not set\n" << this->conjgrid << std::endl;
            goto abort_w_info;
        }
    }
    if (this->conjgrid.val[63] != 2){
        std::cerr << "conjmap[63] was set\n" << this->conjgrid << std::endl;
        goto abort_w_info;
    }
    
    return;
    
    abort_w_info:
    
    #ifdef DEBUG
    this->print_state();
    #endif
    
    abort();
}
#endif
#endif


#ifdef EMBEDDOR
void BPCSStreamBuf::write_conjugation_map(){
    float complexity;
    
    #ifdef TESTS
        this->assert_conjmap_set();
    #endif
    
    #ifdef DEBUG
        mylog.set_verbosity(6);
        mylog.set_cl('p');
        mylog << "Conjgrid orig" << "\n";
        mylog.set_verbosity(6);
        mylog.set_cl(0);
        mylog << this->conjgrid << "\n";
    #endif
    
    this->conjgrid.val[63] = 0;
    
    complexity = this->get_grid_complexity(this->conjgrid);
    
    if (complexity < this->min_complexity)
        cv::bitwise_xor(this->conjgrid, chequerboard, this->conjgrid); // i.e. conjugate
    
    cv::Mat(this->conjgrid).copyTo(this->conjgrid_orig);
    
    #ifdef TESTS
        this->unset_conjmap();
        assert(cv::sum(this->conjgrid_orig)[0] < 65);
    #endif
    
    #ifdef DEBUG
        mylog.set_verbosity(6);
        mylog.set_cl(0);
        mylog << "Written conjugation map" << "\n" << this->conjgrid_orig << std::endl;
    #endif
}
#endif

void BPCSStreamBuf::set_next_grid(){
    #ifdef DEBUG
    if (++whichbyte == gridlimit){
        mylog << std::endl;
        throw std::runtime_error("Reached gridlimit");
    }
    mylog.set_verbosity(5);
    mylog.set_cl(0);
    mylog << "conjmap_indx " << +this->conjmap_indx << std::endl; // tmp
    #endif
    if (this->conjmap_indx == 63){ 
        // First grid in every 64 is reserved for conjugation map
        // The next grid starts the next series of 64 complex grids, and should therefore be reserved to contain its conjugation map
        // The old such grid must have the conjugation map emptied into it
        
        #ifdef DEBUG
            if (++conj_grids_found == MAX_CONJ_GRIDS)
                throw std::runtime_error("Found maximum number of conj grids");
        #endif
        
        this->conjmap_indx = 0;
        
        this->set_next_grid();
        
        #ifdef EMBEDDOR
        if (this->embedding){
            this->write_conjugation_map();
            this->conjgrid_orig = this->grid_orig;
        } else {
        #endif
            if (this->grid.val[63] != 0)
                // Since this is the conjugation map grid, it contains its own conjugation status in its last bit
                // TODO: Have each conjgrid store the conjugation bit of the next conjgrid
                this->conjugate_grid();
            
            memcpy(this->conjgrid.val, this->grid.val, 64);
            
            #ifdef DEBUG
                for (uint_fast8_t k=0; k<63; ++k){
                    mylog.set_verbosity(5);
                    mylog.set_cl(0);
                    mylog << +this->conjgrid.val[k];
                }
                mylog << std::endl;
                mylog << "\n" << this->grid << std::endl;
            #endif
        #ifdef EMBEDDOR
        }
        #endif
    }
    
    float complexity;
    int32_t i = this->x;
    for (int32_t j=this->y;  j <= this->im_mat.rows -8;  j+=8, i=0){
        while (i <= this->im_mat.cols -8){
            cv::Rect grid_shape(cv::Point(i, j), cv::Size(8, 8));
            
            this->grid_orig = this->bitplane(grid_shape);
            
            complexity = this->get_grid_complexity(this->grid_orig);
            
            #ifdef DEBUG
                this->complexities.push_back(complexity);
            #endif
            
            i += 8;
            
            if (complexity >= this->min_complexity){
                this->grid_orig = this->bitplane(grid_shape);
                this->grid_orig.copyTo(this->grid);
                this->x = i;
                this->y = j;
                #ifdef DEBUG
                    os1 << +this->x << "\t" << +this->y << std::endl;
                    
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
    if (img_n < img_fps.size()){
        this->load_next_img();
        this->conjmap_indx = 0; // i.e. decrement
        return;
    }
    
    // If we are here, we have exhausted all images!
    #ifdef DEBUG
        print_histogram(this->complexities, 10, 200);
        this->print_state();
        std::cerr << "Ran out of complex grids (either too much data to embed, or missing files when extracting" << std::endl;
    #endif
    abort();
    
    try_again:
    this->set_next_grid();
}

uchar BPCSStreamBuf::sgetc(){
    uchar c = 0;
    for (uint_fast8_t i=0; i<8; ++i){
        c |= this->grid.val[8*this->gridbitindx +i] << i;
    }
    
    if (++this->gridbitindx == 8){
        this->set_next_grid();
        
        if (this->conjgrid.val[this->conjmap_indx++])
            this->conjugate_grid();
        
        this->gridbitindx = 0;
    }
    
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl('p');
        mylog << "sgetc #" << +(++sgetputc_count) << ": " << +c << " (";
        if (sgetputc_count == SGETPUTC_MAX){
            mylog.set_verbosity(0);
            mylog << "Reached SGETPUTC_MAX " << +SGETPUTC_MAX << std::endl;
            abort();
        } else if (sgetputc_count == SGETPUTC_MAX -120){
            mylog.set_level(10);
        }
        if (c == '\n')
            mylog << "\\n";
        else if (c == '\r')
            mylog << "\\r";
        else
            mylog << c;
        mylog << ") at (gridbitindx, conjmap_indx) " << +this->gridbitindx << ", " << +this->conjmap_indx << "\t(x,y,bitplane,ch) = " << +this->x << ", " << +this->y << ", " << +this->bitplane_n << ", " << +this->channel_n << std::endl;
    #endif
    return c;
}

#ifdef EMBEDDOR
void BPCSStreamBuf::sputc(uchar c){
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl('p');
        mylog << "sputc #" << +(++sgetputc_count) << ": " << +c << " (" << c << ") at (gridbitindx, conjmap_indx) " << +this->gridbitindx << ", " << +this->conjmap_indx << "\t(x,y,bitplane) = " << +this->x << ", " << +this->y << ", " << +this->bitplane_n << std::endl;
        if (sgetputc_count == SGETPUTC_MAX){
            mylog.set_verbosity(0);
            mylog << "Reached SGETPUTC_MAX " << +SGETPUTC_MAX << std::endl;
            abort();
        } else if (sgetputc_count == SGETPUTC_MAX -120){
            mylog.set_level(10);
        }
        mylog.set_verbosity(5);
        mylog.set_cl(0);
        mylog << "sputc " << +c << " bits ";
    #endif
    for (uint_fast8_t i=0; i<8; ++i){
        #ifdef DEBUG
            mylog.set_cl('B');
            mylog << +(c & 1);
        #endif
        this->grid.val[8*this->gridbitindx +i] = c & 1;
        c = c >> 1;
    }
    
    if (++this->gridbitindx == 8){
        #ifdef DEBUG
            mylog.set_verbosity(8);
            mylog.set_cl('B');
            mylog << "Last grid (pre-conj)" << "\n";
            mylog.set_cl(0);
            mylog << this->grid << std::endl;
        #endif
        if (this->get_grid_complexity(this->grid) < this->min_complexity){
            this->conjugate_grid();
            this->conjgrid.val[this->conjmap_indx] = 1;
        } else {
            this->conjgrid.val[this->conjmap_indx] = 0;
        }
        ++this->conjmap_indx;
        
        cv::Mat(this->grid).copyTo(this->grid_orig);
        this->set_next_grid();
        
        this->gridbitindx = 0;
    }
}

void BPCSStreamBuf::save_im(){
    // Called either when we've exhausted this->im_mat's last channel, or when we've reached the end of embedding
    // Ensure last grid is saved
    // TODO: Value of char inserted by sputc would ideally be randomised
    while ((this->gridbitindx != 8) && (this->gridbitindx != 0))
        this->sputc(0);
    
    for (uint8_t i=this->conjmap_indx; i<63; ++i)
        // This will only occur when reached the end of all data being encoded
        this->conjgrid.val[i] = 0;
    
    this->write_conjugation_map();
    
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
                if (!this->bitplanes[k].cols){
                    std::cerr << "bitplane " << +k << " is empty" << std::endl;
                    abort();
                }
            #endif
            cv::bitwise_xor(this->bitplanes[k], this->bitplanes[k+1], this->bitplanes[k]);
            
            this->bitplane = this->bitplanes[k].clone();
            this->bitplane *= (1 << j);
            cv::bitwise_or(this->channel_byteplanes[i], this->bitplane, this->channel_byteplanes[i]);
        } while (j-- != 0);
    } while (i-- != 0);
    
    std::string out_fp = format_out_fp(this->out_fmt, this->img_fps[this->img_n -1]);
    cv::merge(this->channel_byteplanes, this->im_mat);
    #ifdef DEBUG
        mylog.set_verbosity(3);
        mylog.set_cl('g');
        mylog << "Saving to  `" << out_fp << "`" << std::endl;
    #endif
    
    
    
    FILE* png_file = fopen(out_fp.c_str(), "wb");
    auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    #ifdef TESTS
    if (!png_file){
        std::cerr << "Cannot write to file" << std::endl;
        abort();
    }
    if (!png_ptr){
        std::cerr << "png_create_write_struct failed" << std::endl;
        abort();
    }
    #endif
    
    auto png_info_ptr = png_create_info_struct(png_ptr);
    
    if (!png_info_ptr){
        std::cerr << "png_create_info_struct failed" << std::endl;
        abort();
    }
    
    if (setjmp(png_jmpbuf(png_ptr))){
        std::cerr << "png_init_io failed" << std::endl;
        abort();
    }
    
    png_init_io(png_ptr, png_file);
    
    if (setjmp(png_jmpbuf(png_ptr))){
        std::cerr << "png_set_IHDR failed" << std::endl;
        abort();
    }
    
    png_set_bKGD(png_ptr, png_info_ptr, this->png_bg);
    
    png_set_IHDR(png_ptr, png_info_ptr, this->im_mat.cols, this->im_mat.rows, N_BITPLANES, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    
    png_write_info(png_ptr, png_info_ptr);
    
    if (setjmp(png_jmpbuf(png_ptr))){
        std::cerr << "png_write_image failed" << std::endl;
        abort();
    }
    
    uchar* row_ptrs[this->im_mat.rows];
    for (uint32_t i=0; i<this->im_mat.rows; ++i)
        row_ptrs[i] = this->img_data + i*3*this->im_mat.cols;
    
    png_write_image(png_ptr, row_ptrs);
    
    if (setjmp(png_jmpbuf(png_ptr))){
        std::cerr << "png_write_end failed" << std::endl;
        abort();
    }
    
    png_write_end(png_ptr, NULL);
}
#endif









int main(const int argc, char *argv[]){
    #ifdef EMBEDDOR
    bool embedding = false;
    std::vector<char*> msg_fps;
    #endif
    bool extracting = false;
    // bool editing = false; // Use named_pipe_in != NULL
    
    uint_fast8_t n_msg_fps = 0;
    
    char* out_fmt = NULL;
    char* named_pipe_in = NULL;
    
    std::string out_fp;
    
    #ifdef DEBUG
        int verbosity = 3;
        
        mylog1.set_level(0);
        
        os1.set_level(0);
        os1.set_verbosity(1);
        os1.set_dt_fmt("");
        
        char* log_fmt = "[%T] ";
        
        uint_fast16_t n_bins = 10;
        uint_fast8_t n_binchars = 200;
    #else
        bool verbose = false;
    #endif
    int i = 0;
    
    
    /* Argument parser */
    char* arg;
    char* nextarg;
    #ifdef EMBEDDOR
    std::vector<char*> nextlist;
    #endif
    char second_character;
    while (true) {
        // Find opts, until reach arg that does not begin with '-'
        arg = argv[++i];
        if (arg[0] != '-')
            break;
        
        second_character = arg[1];
        
        /* Bool flags */
        
        switch(second_character){
            #ifdef DEBUG
            case 'v': ++verbosity; goto continue_argloop;
            case 'q': --verbosity; goto continue_argloop;
            case '-':
                switch(arg[2]){
                    case 'o':
                        // --os1
                        // outstream - not formatted CompskyLogger
                        os1.set_verbosity(0); goto continue_argloop;
                    default:
                        break;
                }
            #else
            case 'v': verbose = true; goto continue_argloop;
            // --verbose
            // If writing to disk, print output filepath
            #endif
        }
        
        /* Value flags */
        
        nextarg = argv[++i];
        
        switch(second_character){
            case 'o': out_fmt = nextarg; goto continue_argloop;
            // --out-fmt
            // Format of output file path(s) - substitutions being fp, dir, fname, basename, ext. Sets mode to `extracting` if msg_fps not supplied.
            case 'p': named_pipe_in = nextarg; goto continue_argloop;
            // Path of named input pipe to listen to after having piped extracted message to to output pipe. Sets mode to `editing`
            #ifdef DEBUG
            case '-':
                switch(arg[2]){
                    case 'b':
                        switch(arg[3]){
                            case 'i':
                                switch(arg[4]){
                                    case 'n':
                                        switch(arg[5]){
                                            case 'c':
                                                // --binchars
                                                // Total number of `#` characters printed out in histogram totals
                                                n_binchars = std::stoi(nextarg);
                                                goto continue_argloop;
                                            case 's':
                                                // --bins
                                                // Number of histogram bins
                                                n_bins = std::stoi(nextarg);
                                                goto continue_argloop;
                                            default:
                                                goto invalid_argument;
                                        }
                                    default: goto invalid_argument;
                                }
                            case 'y':
                                // --bytelimit
                                // Exit after encountering SGETPUTC_MAX bytes
                                SGETPUTC_MAX = std::stoi(nextarg);
                                goto continue_argloop;
                            default: goto invalid_argument;
                        }
                    case 'c':
                        switch(arg[3]){
                            case 'o':
                                // --conjlimit
                                // Limit of conjugation grids
                                MAX_CONJ_GRIDS = std::stoi(nextarg);
                                goto continue_argloop;
                            default: goto invalid_argument;
                        }
                    case 'g':
                        switch(arg[3]){
                            case 'r':
                                // --gridlimit
                                // Quit after having moved through `gridlimit` grids
                                gridlimit = std::stoi(nextarg);
                                goto continue_argloop;
                            default: goto invalid_argument;
                        }
                    case 'l':
                        switch(arg[3]){
                            case 'o':
                                switch(arg[4]){
                                    case 'g':
                                        switch(arg[5]){
                                            case '-':
                                                // --log-fmt
                                                // Format of information prepended to each log line. Examples: `[%T] `, `[%F %T] `
                                                log_fmt = nextarg;
                                                goto continue_argloop;
                                            case '1':
                                                mylog1.set_level(std::stoi(nextarg)); goto continue_argloop;
                                            default:
                                                goto invalid_argument;
                                        }
                                    default:
                                        goto invalid_argument;
                                }
                            default:
                                goto invalid_argument;
                        }
                    default: goto invalid_argument;
                }
            #endif
            #ifdef EMBEDDOR
            case 'm': break;
            #endif
            case '-':
                switch(arg[2]){
                    case 0: goto stop_parsing_opts;
                    default: goto invalid_argument;
                }
            default: goto invalid_argument;
        }
        
        /* List flags */
        #ifdef EMBEDDOR
        nextlist.clear();
        while (nextarg[0] != '-'){
            nextlist.push_back(nextarg);
            nextarg = argv[++i];
        }
        --i;
        
        if (second_character == 'm'){
            // File path(s) of message file(s) to embed. Sets mode to `embedding`
            n_msg_fps = nextlist.size();
            embedding = true;
            msg_fps = nextlist;
            continue;
        }
        #endif
        
        invalid_argument:
        #ifdef DEBUG
            std::cerr << "Invalid argument: " << arg << std::endl;
            return 1;
        #else
            abort();
        #endif
        
        continue_argloop:
        continue;
    }
    
    stop_parsing_opts:
    
    #ifdef DEBUG
        if (verbosity < 0)
            verbosity = 0;
        else if (verbosity > 9)
            verbosity = 9;
        mylog.set_level(verbosity);
        
        mylog1.set_level(verbosity);
        
        mylog.set_dt_fmt(log_fmt);
        mylog1.set_dt_fmt(log_fmt);
        
        mylog.set_verbosity(4);
        mylog.set_cl('b');
        mylog << "Verbosity " << +verbosity << std::endl;
        
        mylog.set_verbosity(3);
        mylog.set_cl('g');
        if (n_msg_fps != 0)
            mylog << "Embedding";
        else
            mylog << "Extracting to ";
    #endif
    if (out_fmt == NULL){
        if (n_msg_fps != 0){
            #ifdef DEBUG
                std::cerr << "Must specify --out-fmt if embedding --msg files" << std::endl;
            #endif
            return 1;
        }
        #ifdef DEBUG
            mylog << "display";
        #endif
    }
    #ifdef DEBUG
        else {
            if (n_msg_fps == 0){
                mylog << "file";
            }
        }
        mylog << std::endl;
        
        mylog.set_verbosity(5);
        mylog.set_cl(0);
        mylog << "min_complexity: " << +argv[i] << std::endl;
    #endif
    const float min_complexity = std::stof(argv[i++]);
    // Minimum bitplane complexity
    if (min_complexity > 0.5){
        #ifdef DEBUG
            mylog.set_verbosity(0);
            mylog.set_cl('r');
            mylog << "Current implementation requires maximum minimum complexity of 0.5" << std::endl;
        #endif
        return 1;
    }
    
    std::vector<char*> img_fps;
    // File path(s) of input image file(s)
    #ifdef DEBUG
        mylog.set_verbosity(5);
    #endif
    while (i < argc) {
        img_fps.push_back(argv[i++]);
        #ifdef DEBUG
            mylog << argv[i -1] << std::endl;
        #endif
    };
    
    
    
    
    if (sodium::sodium_init() == -1) {
        #ifdef DEBUG
            mylog.set_verbosity(0);
            mylog.set_cl('r');
            mylog << "libsodium init fail" << std::endl;
        #endif
        return 1;
    }
    
    #ifdef DEBUG
        mylog.set_verbosity(4);
        mylog << "sizeof(BPCSStreamBuf) == " << +sizeof(BPCSStreamBuf) << std::endl;
    #else
        #ifdef RELEASE_STATS
            std::cout << "sizeof(BPCSStreamBuf) == " << +sizeof(BPCSStreamBuf) << std::endl;
        #endif
    #endif
    
    BPCSStreamBuf bpcs_stream(min_complexity, img_fps,
                              #ifdef EMBEDDOR
                              embedding,
                              #endif
                              out_fmt);
    bpcs_stream.load_next_img(); // Init
    
    uint_fast64_t j;
    uint_fast64_t n_msg_bytes;
    
    #ifdef EMBEDDOR
    char* fp;
    struct stat stat_buf;
    
    if (embedding){
        FILE* msg_file;
        for (i=0; i<n_msg_fps; ++i){
            fp = msg_fps[i];
            // At start, and between embedded messages, is a 64-bit integer telling us the size of the next embedded message
            // The first 32+ bits will almost certainly be 0 - but this will not aid decryption of the rest of the contents (assuming we are using an encryption method that is resistant to known-plaintext attack)
            
            n_msg_bytes = get_charp_len(fp);
            
            #ifdef DEBUG
                mylog.set_verbosity(3);
                mylog.set_cl('g');
                mylog << "Reading msg `" << fp << "` (" << +(i+1) << "/" << +n_msg_fps << ") of size " << +n_msg_bytes << std::endl;
            #endif
            for (j=0; j<8; ++j)
                bpcs_stream.sputc((n_msg_bytes >> (8*j)) & 255);
            for (j=0; j<n_msg_bytes; ++j)
                bpcs_stream.sputc((uchar)fp[j]);
            
            if (stat(fp, &stat_buf) == -1){
                #ifdef DEBUG
                    mylog.set_verbosity(0);
                    mylog.set_cl('r');
                    mylog << "No such file:  " << fp << std::endl;
                #endif
                return 1;
            }
            n_msg_bytes = stat_buf.st_size;
            #ifdef DEBUG
                mylog.set_verbosity(5);
                mylog.set_cl(0);
                mylog << "n_msg_bytes (contents): " << +n_msg_bytes << std::endl;
            #endif
            
            for (j=0; j<8; ++j)
                bpcs_stream.sputc((n_msg_bytes >> (8*j)) & 255);
            msg_file = fopen(fp, "rb");
            for (j=0; j<n_msg_bytes; ++j){
                // WARNING: Assumes there are exactly n_msg_bytes
                bpcs_stream.sputc(getc(msg_file));
                #ifdef DEBUG
                    if (whichbyte > gridlimit -10){
                        mylog.set_verbosity(5);
                        mylog.set_cl(0);
                        mylog << "byte #" << +j << std::endl;
                    }
                #endif
            }
            fclose(msg_file);
        }
        // After all messages, signal end with signalled size of 0
        for (j=0; j<8; ++j)
            bpcs_stream.sputc(0);
        #ifdef DEBUG
            print_histogram(bpcs_stream.complexities, 10, 200);
        #endif
        bpcs_stream.save_im();
    } else {
    #endif
        std::string fp_str;
        for (i=0; true; ++i) {
            n_msg_bytes = 0;
            for (j=0; j<8; ++j){
                uchar c = bpcs_stream.sgetc();
                n_msg_bytes |= (c << (8*j));
            }
            #ifdef DEBUG
                mylog.set_verbosity(3);
                mylog.set_cl('g');
                mylog << "n_msg_bytes " << +n_msg_bytes << std::endl;
            #endif
            
            if (n_msg_bytes == 0){
                // Reached end of embedded datas
                #ifdef DEBUG
                    mylog.set_verbosity(0);
                    mylog.set_cl('r');
                    mylog << "n_msg_bytes = 0";
                    mylog.set_cl(0);
                    mylog << std::endl;
                #endif
                return 0;
            }
            
            if (i & 1){
                // First block of data is original file path, second is file contents
                if (out_fmt != NULL){
                    #ifdef DEBUG
                        mylog.set_verbosity(3);
                        mylog << "Writing extracted file to `" << fp_str << "`" << std::endl;
                    #endif
                    #ifdef TESTS
                        assert(fp_str != "");
                    #endif
                    std::ofstream of(fp_str);
                    for (j=0; j<n_msg_bytes; ++j)
                        of.put(bpcs_stream.sgetc());
                    of.close();
                    #ifndef DEBUG
                        if (verbose)
                            std::cout << fp_str << std::endl;
                    #endif
                } else {
                    // Stream to anonymous pipe
                    for (j=0; j<n_msg_bytes; ++j)
                        std::cout << bpcs_stream.sgetc();
                }
            } else {
                fp_str = "";
                for (j=0; j<n_msg_bytes; ++j){
                    fp_str += bpcs_stream.sgetc();
                }
                #ifdef DEBUG
                    mylog.set_verbosity(3);
                    mylog.set_cl('g');
                    mylog << "Original fp: " << fp_str << std::endl;
                #endif
                if (out_fmt != NULL){
                    fp_str = format_out_fp(out_fmt, (char*)fp_str.c_str());
                    #ifdef DEBUG
                        mylog.set_verbosity(3);
                        mylog.set_cl('g');
                        mylog << "Formatted fp: " << fp_str << std::endl;
                    #endif
                }
            }
        }
    #ifdef EMBEDDOR
    }
    #endif
}
