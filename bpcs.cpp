#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <fstream>
#include <sys/stat.h> // for stat
#include <regex> // for std::basic_regex

#include <cstdio> // for std::remove

namespace sodium {
    #include <sodium.h> // /crypto_secretstream_xchacha20poly1305.h> // libsodium for cryption (-lsodium)
}

#ifdef DEBUG
    #include <compsky/logger.hpp> // for CompskyLogger
#endif



static char* NULLCHAR_DYN        = "[NULL]";
static const char* NULLCHAR      = "[NULL]";
static const std::string NULLSTR = "[NULL]";


static const std::regex path_regexp("^((.*)/)?(([^/]+)[.]([^./]+))$");
// Groups are full_match, optional, parent_dir, filename, basename, ext
static const std::regex fmt_fp("[{]fp[}]");
static const std::regex fmt_dir("[{]dir[}]");
static const std::regex fmt_fname("[{]fname[}]");
static const std::regex fmt_basename("[{]basename[}]");
static const std::regex fmt_ext("[{]ext[}]");
// WARNING: Better would be to ignore `{{fp}}` (escape it to a literal `{fp}`) with (^|[^{]), but no personal use doing so.





std::string format_out_fp(std::string out_fmt, std::smatch path_regexp_match){
    return std::regex_replace(std::regex_replace(std::regex_replace(std::regex_replace(std::regex_replace(out_fmt, fmt_fp, (std::string)path_regexp_match[0]), fmt_dir, (std::string)path_regexp_match[2]), fmt_fname, (std::string)path_regexp_match[3]), fmt_basename, (std::string)path_regexp_match[4]), fmt_ext, (std::string)path_regexp_match[4]);
}




static const uint_fast8_t MODE_EMBEDDING = 0;
static const uint_fast8_t MODE_CALC_MAX_CAP = 1;
static const uint_fast8_t MODE_EXTRACT = 2;
static const uint_fast8_t MODE_EDIT = 3;


#ifdef DEBUG
    unsigned int whichbyte = 0;
    uint_fast64_t gridlimit = 0;
    static CompskyLogger mylog("bpcs", std::cout);
    unsigned int MAX_CONJ_GRIDS = 0;
    unsigned int conj_grids_found = 0;
#endif





/*
 * Bitwise operations on OpenCV Matrices
 */

inline uint_fast16_t xor_adj(
    cv::Mat &arr,
    cv::Mat &xor_adj_mat1, cv::Mat &xor_adj_mat2, cv::Rect &xor_adj_rect1, cv::Rect &xor_adj_rect2, cv::Rect &xor_adj_rect3, cv::Rect &xor_adj_rect4
){
    uint_fast16_t sum = 0;
    
    arr(xor_adj_rect1).copyTo(xor_adj_mat1);
    cv::bitwise_xor(arr(xor_adj_rect2), xor_adj_mat1, xor_adj_mat1);
    sum += cv::sum(xor_adj_mat1)[0];
    
    arr(xor_adj_rect3).copyTo(xor_adj_mat2);
    cv::bitwise_xor(arr(xor_adj_rect4), xor_adj_mat2, xor_adj_mat2);
    sum += cv::sum(xor_adj_mat2)[0];
    
    return sum;
}


inline void bitandshift(cv::Mat &arr, cv::Mat &dest, uint_fast16_t n){
    /*
    IIRC, cv::divide does not divide by 2 via bitshifts - it rounds up
    TODO: Maybe fix this by looking at the assembly generated
    
    for (uint_fast8_t i = 0; i<n; ++i)
        cv::divide(arr,  2,  dest);
    
    cv::bitwise_and(dest, 1, dest);
    */
    for (uint_fast16_t i=0; i!=arr.rows*arr.cols; ++i)
        dest.data[i] = (arr.data[i] >> n) & 1;
    #ifdef DEBUG
        mylog.tedium();
        mylog << "bitandshift " << +n << ":  arr(sum==" << +cv::sum(arr)[0] << ")  ->  dest(sum==" << +cv::sum(dest)[0] << ")" << std::endl;
    #endif
}

inline void bitshift_up(cv::Mat &arr, uint_fast16_t n){
    #ifdef DEBUG
        mylog.tedium();
        mylog << "bitshift_up " << +n << std::endl;
    #endif
    cv::multiply(arr, 1 << n, arr);
}

inline void convert_to_cgc(cv::Mat &arr, cv::Mat &dest){
    uint_fast8_t n;
    for (uint_fast64_t i=0; i<arr.rows*arr.cols; ++i){
        n = arr.data[i];
        dest.data[i] = n ^ (n >> 1);
    }
    //cv::bitwise_xor(arr, arr/2, dest);
    #ifdef DEBUG
        mylog.tedium();
        mylog << "Converted to CGC: arr(sum==" << +cv::sum(arr)[0] << ") -> dest(sum==" << +cv::sum(dest)[0] << ")" << std::endl;
    #endif
}






/*
 * Initialise chequerboards
 */
cv::Mat chequerboard(uint_fast16_t indx, uint_fast16_t w, uint_fast16_t h){
    // indx should be 0 or 1
    cv::Mat arr = cv::Mat(h, w, CV_8UC1);
    for (uint_fast16_t i=0; i<w; ++i)
        for (uint_fast16_t j=0; j<h; ++j)
            arr.at<uint_fast8_t>(j, i) = ((i ^ j) ^ indx) & 1;
    return arr;
}

static const cv::Mat chequerboard_a = chequerboard(0, 8, 8);
static const cv::Mat chequerboard_b = chequerboard(1, 8, 8);




/*
 * Grid complexity
 */

inline float grid_complexity(
    cv::Mat &grid,
    cv::Mat &xor_adj_mat1, cv::Mat &xor_adj_mat2, cv::Rect &xor_adj_rect1, cv::Rect &xor_adj_rect2, cv::Rect &xor_adj_rect3, cv::Rect &xor_adj_rect4
){
    return (float)xor_adj(grid, xor_adj_mat1, xor_adj_mat2, xor_adj_rect1, xor_adj_rect2, xor_adj_rect3, xor_adj_rect4) / (2*8*7); // (float)(grid_w * (grid_h -1) + grid_h * (grid_w -1));
}

void conjugate_grid(cv::Mat &grid, uint_fast8_t n, uint_fast32_t x, uint_fast32_t y){
    #ifdef DEBUG
        mylog.tedium('p');
        mylog << "<*" << +n << "(" << +x << ", " << +y << ")>";
    #endif
    cv::Mat arr1;
    cv::Mat arr2;
    
    cv::bitwise_and(grid, chequerboard_a, arr1);
    
    cv::bitwise_not(grid, grid);
    cv::bitwise_and(grid, chequerboard_b, arr2);
    
    cv::bitwise_or(arr1, arr2, grid);
}








/*
 * Display statistics
 */

#ifdef DEBUG
void print_histogram(std::vector<float> &complexities, uint_fast16_t n_bins, uint_fast16_t n_binchars){
    std::sort(std::begin(complexities), std::end(complexities));
    uint_fast64_t len_complexities = complexities.size();
    
    if (len_complexities == 0){
        mylog.warn();
        mylog << "No complexities to display" << std::endl;
        return;
    }
    
    mylog.info('B');
    mylog << "Complexities Histogram" << '\n';
    mylog.info();
    
    float min = *std::min_element(std::begin(complexities), std::end(complexities));
    float max = *std::max_element(std::begin(complexities), std::end(complexities));
    mylog << "Total: " << +len_complexities << " between " << +min << ", " << +max << '\n';
    if (min == max){
        mylog.warn();
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














class BPCSStreamBuf { //: public std::streambuf {
    // Based on excellent post by krzysztoftomaszewski
    // src https://artofcode.wordpress.com/2010/12/12/deriving-from-stdstreambuf/
  public:
    /* Constructors */
    BPCSStreamBuf(const float min_complexity, const std::vector<std::string> img_fps):
    min_complexity(min_complexity), img_fps(img_fps), img_n(0), gridbitindx(64), grids_since_conjgrid(63)
    {}
    
    
    unsigned char sgetc();
    void sputc(unsigned char c);
    
    void load_next_img(); // Init
    void end();
    
    bool embedding;
    std::string out_fmt;
  private:
    int set_next_grid();
    void load_next_bitplane();
    void load_next_channel();
    
    void save_im();
    
    void write_conjugation_map();
    
    inline float get_grid_complexity(cv::Mat&);

    const float min_complexity;
    const std::vector<std::string> img_fps;
    uint_fast16_t img_n;
    #ifdef DEBUG
        uint_fast64_t n_grids;
    #endif
    
    uint_fast8_t grids_since_conjgrid;
    // To reserve the first grid of every 64 complex grids in order to write conjugation map
    // Note that the first bit of this map is for its own conjugation state
    
    uint_fast64_t x; // the current grid is the (x-1)th grid horizontally and yth grid vertically (NOT the coordinates of the corner of the current grid of the current image)
    uint_fast64_t y;
    
    uint_fast8_t gridbitindx; // Count of bits already read/written, modulo 64 (i.e. the index in the grid we are writing/reading the byte to/from)
    
    cv::Mat im_mat;
    std::vector<cv::Mat> channel_byteplanes;
    uint_fast8_t n_channels;
    uint_fast8_t channel_n;
    uint_fast8_t n_bitplanes;
    
    unsigned char conjugation_map[63];
    cv::Mat conjugation_grid{8, 8, CV_8UC1};
    
    #ifdef DEBUG
        std::vector<float> complexities;
    #endif
    
    cv::Mat grid{8, 8, CV_8UC1};
    cv::Mat xor_adj_mat1{8, 7, CV_8UC1};
    cv::Mat xor_adj_mat2{7, 8, CV_8UC1};
    cv::Rect xor_adj_rect1{cv::Point(0, 0), cv::Size(7, 8)};
    cv::Rect xor_adj_rect2{cv::Point(1, 0), cv::Size(7, 8)};
    cv::Rect xor_adj_rect3{cv::Point(0, 0), cv::Size(8, 7)};
    cv::Rect xor_adj_rect4{cv::Point(0, 1), cv::Size(8, 7)};
    // cv::Rect, cv::Point, cv::Size are all column-major, i.e. (x, y) or (width, height), but cv::Mat is row-major
    
    uint_fast8_t bitplane_n;
    uint_fast8_t conjgrid_bitplane_n;
    cv::Mat bitplane;
    cv::Mat prev_bitplane;
    cv::Mat unXORed_bitplane;
    
    cv::Mat byteplane;
    cv::Mat XORed_byteplane;
    
    cv::Mat bitplanes[32 * 4]; // WARNING: Images rarely have a bit-depth greater than 32, but would ideally be set on per-image basis
    
    std::smatch path_regexp_match;
    
    bool past_first_grid;
};

inline float BPCSStreamBuf::get_grid_complexity(cv::Mat &arr){
    return grid_complexity(arr, this->xor_adj_mat1, this->xor_adj_mat2, this->xor_adj_rect1, this->xor_adj_rect2, this->xor_adj_rect3, this->xor_adj_rect4);
}

inline void BPCSStreamBuf::load_next_bitplane(){
    #ifdef TESTS
        if (this->embedding)
            throw std::runtime_error("load_next_bitplane should not be used for embedding");
    #endif
    bitandshift(this->XORed_byteplane, this->bitplane, this->n_bitplanes - ++this->bitplane_n);
    
    #ifdef DEBUG
        mylog.dbg();
        mylog << "Loaded bitplane " << +this->bitplane_n << " of " << +this->n_bitplanes << std::endl;
    #endif
}

void BPCSStreamBuf::load_next_channel(){
    #ifdef DEBUG
        mylog.dbg();
        mylog << "Loading channel(sum==" << +cv::sum(channel_byteplanes[this->channel_n])[0] << ") " << +(this->channel_n + 1) << " of " << +this->n_channels << std::endl;
    #endif
    convert_to_cgc(this->channel_byteplanes[this->channel_n], this->XORed_byteplane);
    ++this->channel_n;
    this->bitplane_n = 0;
    this->load_next_bitplane();
}

void BPCSStreamBuf::load_next_img(){
    if (this->embedding && this->img_n != 0){
        this->save_im();
    }
    #ifdef DEBUG
        mylog.info();
        mylog << "Loading img " << +this->img_n << " of " << +this->img_fps.size() << " `" << this->img_fps[this->img_n] << "`, using: Complexity >= " << +this->min_complexity << std::endl;
    #endif
    this->im_mat = cv::imread(this->img_fps[this->img_n++], CV_LOAD_IMAGE_COLOR);
    cv::split(this->im_mat, this->channel_byteplanes);
    this->n_channels = this->im_mat.channels();
    
    switch(this->im_mat.depth()){
        case CV_8UC1:
            this->n_bitplanes = 8;
            break;
    }
    
    #ifdef DEBUG
        mylog.info();
        mylog << +this->im_mat.cols << "x" << +this->im_mat.rows << '\n';
        mylog << +this->n_channels << " channels" << '\n';
        mylog << "Bit-depth of " << +this->n_bitplanes << '\n';
        mylog << std::endl;
    #endif
    
    #ifdef DEBUG
        this->complexities.clear();
    #endif
    
    this->XORed_byteplane = cv::Mat::zeros(im_mat.rows, im_mat.cols, CV_8UC1);
    
    this->channel_n = 0;
    if (this->embedding){
        uint_fast8_t k = 0;
        uint_fast8_t i = 0;
        for (uint_fast8_t j=0; j<this->n_channels; ++j){
            convert_to_cgc(this->channel_byteplanes[j], this->XORed_byteplane);
            i = 0;
            while (i < this->n_bitplanes){
                this->bitplanes[k] = cv::Mat::zeros(this->im_mat.rows, this->im_mat.cols, CV_8UC1);
                bitandshift(this->XORed_byteplane, this->bitplanes[k++], this->n_bitplanes - ++i);
            }
        }
        this->bitplane = this->bitplanes[0];
    } else {
        convert_to_cgc(this->channel_byteplanes[this->channel_n], this->XORed_byteplane);
        this->bitplane = cv::Mat::zeros(im_mat.rows, im_mat.cols, CV_8UC1); // Need to initialise for bitandshift
        this->load_next_channel();
    }
    this->x = 0;
    this->y = 0;
    this->past_first_grid = false; // Only care about this if this->embedding
}

void BPCSStreamBuf::write_conjugation_map(){
    float complexity;
    
    #ifdef DEBUG
        mylog.tedium('p');
        mylog << "Conjgrid orig" << "\n";
        mylog.tedium();
        mylog << this->conjugation_grid << "\n";
    #endif
    
        for (uint_fast8_t k=0; k<63; ++k){
            #ifdef TESTS
                if (this->conjugation_map[k] != 0 && this->conjugation_map[k] != 1){
                    mylog.crit();
                    mylog << "Non-bit this->conjugation_map[" << +k << "]\n";
                    for (uint_fast8_t i=0; i<63; ++i)
                        mylog << +this->conjugation_map[i] << " ";
                    mylog << std::endl;
                    throw std::runtime_error("Non-bit this->conjugation_map");
                }
            #endif
            #ifdef DEBUG
                mylog << +this->conjugation_map[k];
            #endif
            this->conjugation_grid.data[k] = this->conjugation_map[k];
        }
        
        this->conjugation_grid.data[63] = 0;
        
        complexity = this->get_grid_complexity(this->conjugation_grid);
        
        #ifdef DEBUG
            mylog.tedium('p');
            mylog << "Conjgrid before conjugation" << "\n";
            mylog.tedium();
            mylog << this->conjugation_grid << "\n";
        #endif
        
        if (complexity < this->min_complexity){
            conjugate_grid(this->conjugation_grid, this->grids_since_conjgrid, this->x-64, this->y);
            this->conjugation_grid.data[63] = 1;
            if (1 - complexity - 1/57 < this->min_complexity)
                // Maximum difference in complexity from changing first bit is `2 / (2 * 8 * 7)` == 1/57
                // Hence - assuming this->min_complexity<0.5 - the conjugate's complexity is 
                
                if (this->conjugation_grid.data[62] != 0)
                    // If it were 0, the XOR of this with the first bit was 1, and had been 0 before.
                    // Hence the grid complexity would have been at least its previous value
                    
                    if (this->conjugation_grid.data[55] != 0)
                        #ifdef DEBUG
                        throw std::runtime_error("Grid complexity fell below minimum value");
                        #else
                        abort();
                        #endif
        }
    #ifdef TESTS
        for (uint_fast8_t i=0; i<64; ++i)
            if (this->conjugation_grid.data[i] != 0 && this->conjugation_grid.data[i] != 1){
                #ifdef DEBUG
                    mylog.crit();
                    mylog << "Non-bit in conjugation map" << "\n" << this->conjugation_grid << std::endl;
                #endif
                throw std::runtime_error("Non-bit in conjugation map");
            }
    #endif
    #ifdef DEBUG
        mylog.tedium();
        mylog << "Written conjugation map" << "\n" << this->conjugation_grid << std::endl;
    #endif
}

#ifdef DEBUG
void print_grid(cv::Mat& grid, uint_fast32_t x, uint_fast32_t y){
    unsigned char c;
    uint_fast8_t n;
    mylog.dbg('B');
    mylog << "print_grid (" << +x << ", " << +y << ")" << '\n';
    for (uint_fast8_t j=0; j<8; ++j){
        c = 0;
        mylog.tedium();
        for (uint_fast8_t i=0; i<8; ++i){
            n =grid.at<uint_fast8_t>(j, i);
            c |= (n << i);
            mylog << +n;
        }
        mylog.dbg();
        mylog << " ";
        mylog << c << '\n';
    }
    mylog << std::endl;
}
#endif

int BPCSStreamBuf::set_next_grid(){
    #ifdef DEBUG
    if (++whichbyte == gridlimit){
        mylog << std::endl;
        throw std::runtime_error("Reached gridlimit");
    }
    mylog.tedium();
    mylog << "grids_since_conjgrid " << +this->grids_since_conjgrid << std::endl; // tmp
    #endif
    if (++this->grids_since_conjgrid == 64){ 
        // First grid in every 64 is reserved for conjugation map
        // The next grid starts the next series of 64 complex grids, and should therefore be reserved to contain its conjugation map
        // The old such grid must have the conjugation map emptied into it
        
        #ifdef DEBUG
            if (++conj_grids_found == MAX_CONJ_GRIDS)
                throw std::runtime_error("Found maximum number of conj grids");
        #endif
        
        if (this->set_next_grid())
            // Ran out of grids
            return 1;
        
        if (this->embedding){
            if (this->past_first_grid)
                this->write_conjugation_map();
            else
                this->past_first_grid = true;
            
            this->conjugation_grid = this->grid;
            this->conjgrid_bitplane_n = this->bitplane_n;
        } else {
            if (this->grid.data[63] != 0)
                conjugate_grid(this->grid, this->grids_since_conjgrid, this->x, this->y);
            
            for (uint_fast8_t k=0; k<63; ++k){
                this->conjugation_map[k] = this->grid.data[k];
                #ifdef DEBUG
                    mylog.tedium();
                    mylog << +this->conjugation_map[k];
                #endif
            }
            #ifdef DEBUG
                mylog << std::endl;
                mylog << "\n" << this->grid;
            #endif
        }
        
        #ifdef DEBUG
            mylog << std::endl;
        #endif
        
        this->grids_since_conjgrid = 0;
    }
    
    float complexity;
    uint_fast32_t i = this->x;
    uint_fast32_t j = this->y;
    while (j <= this->im_mat.rows -8){
        while (i <= this->im_mat.cols -8){
            cv::Rect grid_shape(cv::Point(i, j), cv::Size(8, 8));
            
            this->grid = this->bitplane(grid_shape);
            complexity = this->get_grid_complexity(this->grid);
            
            // TODO: Look into removing this unnecessary copy
            //complexity = this->get_grid_complexity(this->bitplane(grid_shape));
            
            #ifdef DEBUG
                mylog.tedium();
                mylog << "complexity " << +complexity << std::endl; // tmp
                this->complexities.push_back(complexity);
            #endif
            
            i += 8;
            
            if (complexity >= this->min_complexity){
                //this->bitplane(grid_shape).copyTo(this->grid);
                this->x = i;
                this->y = j;
                return 0;
            }
        }
        i = 0;
        j += 8;
    }
    
    // If we are here, we have exhausted the bitplane
    
    this->x = 0;
    this->y = 0;
    
    #ifdef DEBUG
        mylog.dbg();
        mylog << "Exhausted bitplane" << std::endl;
        mylog.tedium();
        mylog << "embedding: " << +this->embedding << std::endl;
        mylog << "bitplane_n: " << +this->bitplane_n << std::endl;
        mylog << "n_bitplanes: " << +this->n_bitplanes << std::endl;
        mylog << "channel_n: " << +this->channel_n << std::endl;
        mylog << "n_channels: " << +this->n_channels << std::endl;
    #endif
    
    if (this->embedding && this->bitplane_n < this->n_bitplanes * this->n_channels){
        this->bitplane = this->bitplanes[this->bitplane_n++]; // Equivalent to this->load_next_bitplane();
        ++this->channel_n;
        goto try_again;
    } else if (this->bitplane_n < this->n_bitplanes){
        this->load_next_bitplane();
        goto try_again;
    } else if (this->channel_n < this->n_channels){
        this->load_next_channel();
        goto try_again;
    }
    
    // If we are here, we have exhausted the image
    if (img_n < img_fps.size()){
        this->load_next_img();
        goto try_again;
    }
    
    // If we are here, we have exhausted all images!
    return 1;
    
    try_again:
    --this->grids_since_conjgrid;
    return this->set_next_grid();
}

unsigned char BPCSStreamBuf::sgetc(){
    if (this->gridbitindx == 64){
        if (this->set_next_grid())
            #ifdef DEBUG
            throw std::runtime_error("Unexpected end of BPCS stream");
            #else
            abort();
            #endif
            //return std::char_traits<char>::eof;
        
        if (this->conjugation_map[this->grids_since_conjgrid])
            conjugate_grid(this->grid, this->grids_since_conjgrid, this->x, this->y);
        
        #ifdef DEBUG
            mylog.tedium('B');
            mylog << "Read grid" << "\n";
            mylog.tedium();
            mylog << this->grid << std::endl;
        #endif
        
        this->gridbitindx = 0;
    }
    unsigned char c = 0;
    for (uint_fast8_t i=0; i<8; ++i){
        #ifdef DEBUG
            mylog.tedium();
            mylog << +this->grid.data[this->gridbitindx];
        #endif
        #ifdef TESTS
            if (this->grid.data[this->gridbitindx] != 0 && this->grid.data[this->gridbitindx] != 1){
                mylog.crit();
                mylog << "Non-bit in grid(" << +this->x << ", " << +this->y << ")" << "\n" << this->grid << std::endl;
                throw std::runtime_error("Non-bit in grid");
            }
        #endif
        c |= this->grid.data[this->gridbitindx++] << i;
    }
    #ifdef DEBUG
        mylog.tedium();
        mylog << "sgetc " << +c;
    #endif
    return c;
}

void BPCSStreamBuf::sputc(unsigned char c){
    #ifdef DEBUG
        mylog.tedium();
        mylog << "sputc " << +c << std::endl; // tmp
    #endif
    if (this->gridbitindx == 64){
        if (this->past_first_grid){
            #ifdef DEBUG
                mylog.tedium('B');
                mylog << "Last grid (pre-conj)" << "\n";
                mylog.tedium();
                mylog << this->grid << std::endl;
            #endif
            if (this->get_grid_complexity(this->grid) < this->min_complexity){
                conjugate_grid(this->grid, this->grids_since_conjgrid, this->x, this->y);
                this->conjugation_map[this->grids_since_conjgrid] = 1;
            } else {
                this->conjugation_map[this->grids_since_conjgrid] = 0;
            }
        } 
        if (this->set_next_grid()){
            #ifdef DEBUG
                print_histogram(this->complexities, 10, 200);
                throw std::runtime_error("Too much data to encode");
            #else
                abort();
            #endif
        }
        
        this->gridbitindx = 0;
    }
    
    for (uint_fast8_t i=0; i<8; ++i){
        #ifdef DEBUG
            mylog.tedium();
            mylog << +((c >> i) & 1);
        #endif
        this->grid.at<uint_fast8_t>(this->gridbitindx >> 3, i) = (c >> i) & 1;
        ++this->gridbitindx;
        //this->grid.data[this->gridbitindx++] = (c >> i) & 1;
    }
}

void BPCSStreamBuf::save_im(){
    // Called either when we've exhausted this->im_mat's last channel, or by this->end()
    #ifdef TESTS
        assert(this->embedding);
    #endif
    this->write_conjugation_map();
    
    uint_fast8_t k = 0;
    
    #ifdef DEBUG
        mylog.dbg();
        mylog << "UnXORing " << +this->n_channels << " channels of depth " << +this->n_bitplanes << std::endl;
    #endif
    uint_fast8_t j;
    for (uint_fast8_t i=0; i < this->n_channels; ++i){
        // First bitplane of each channel is unchanged by conversion to CGC
        this->channel_byteplanes[i] = this->bitplanes[k++].clone();
        bitshift_up(this->channel_byteplanes[i], this->n_bitplanes -1);
        
        j = 1;
        while (j < this->n_bitplanes){
            cv::bitwise_xor(this->bitplanes[k], this->bitplanes[k-1], this->bitplanes[k]);
            
            this->bitplane = this->bitplanes[k].clone();
            bitshift_up(this->bitplane, this->n_bitplanes - ++j);
            cv::bitwise_or(this->channel_byteplanes[i], this->bitplane, this->channel_byteplanes[i]);
            
            ++k;
        }
    }
    
    std::regex_search(this->img_fps[this->img_n -1], this->path_regexp_match, path_regexp);
    std::string out_fp = format_out_fp(this->out_fmt, this->path_regexp_match);
    cv::merge(this->channel_byteplanes, this->im_mat);
    #ifdef DEBUG
        mylog.info();
        mylog << "Saving to  `" << out_fp << "`" << std::endl;
    #endif
    
    std::regex_search(out_fp, this->path_regexp_match, path_regexp);
    
    if (path_regexp_match[5] != "png"){
        #ifdef DEBUG
            mylog.crit();
            mylog << "Must be encoded with lossless format, not `" << path_regexp_match[5] << "`" << std::endl;
            throw std::invalid_argument(path_regexp_match[5]);
        #else
            abort();
        #endif
    }
    
    cv::imwrite(out_fp, this->im_mat);
}

inline void BPCSStreamBuf::end(){
    #ifdef DEBUG
        mylog.dbg();
        mylog << "end() called" << std::endl;
        print_histogram(this->complexities, 10, 200);
    #endif
    assert(this->embedding);
    this->save_im();
}







uint_fast64_t get_fsize(char* fp){
    struct stat stat_buf;
    if (stat(fp, &stat_buf) == -1){
        #ifdef DEBUG
            mylog.crit();
            mylog << "No such file:  " << fp << std::endl;
            throw std::runtime_error("No such file");
        #else
            abort();
        #endif
    }
    return stat_buf.st_size;
}











int main(const int argc, char *argv[]){
    uint_fast8_t verbosity = 3;
    std::string arg;
    char* nextarg;
    std::vector<char*> nextlist;
    
    bool to_disk = false;
    bool to_display = false;
    
    std::string out_fmt = NULLSTR;
    std::string out_fp;
    
    uint_fast8_t n_msg_fps = 0;
    std::vector<char*> msg_fps;
    
    uint_fast8_t mode;
    
    char* named_pipe_in = NULLCHAR_DYN;
    
    #ifdef DEBUG
        char log_fmt = "[%T] ";
        
        uint_fast16_t n_bins = 10;
        uint_fast8_t n_binchars = 200;
    #endif
    uint_fast64_t i = 0;
    do {
        // Find opts, until reach arg that does not begin with '-'
        arg = argv[++i];
        if (arg[0] != '-')
            break;
        
        /* Bool flags */
        
        if (arg == "-v"){
            ++verbosity;
            continue;
        }
        if (arg == "-q"){
            --verbosity;
            continue;
        }
        
        /* Value flags */
        
        nextarg = argv[++i];
        
        #ifdef DEBUG
        if (arg == "--log-fmt"){
            // Format of information prepended to each log line. Examples: `[%T] `, `[%F %T] `
            log_fmt = nextarg;
            continue;
        }
        
        // Histogram args
        if (arg == "--bins"){
            // Number of histogram bins
            n_bins = nextarg;
            continue;
        }
        if (arg == "--binchars"){
            // Total number of `#` characters printed out in histogram totals
            n_binchars = nextarg;
            continue;
        }
        
        // Debugging
        if (arg == "--gridlimit"){
            // Quit after having moved through `gridlimit` grids
            gridlimit = nextarg;
            continue;
        }
        if (arg == "--conjlimit"){
            // Limit of conjugation grids
            MAX_CONJ_GRIDS = nextarg;
            continue;
        }
        #endif
        
        if (arg == "-o" || arg == "--out"){
            // Format of output file path(s) - substitutions being fp, dir, fname, basename, ext. Sets mode to `extracting` if msg_fps not supplied.
            out_fmt = nextarg;
            continue;
        }
        
        if (arg == "-d" || arg == "--disk"){
            // Write to disk (as opposed to psuedofile)
            to_disk = true;
        }
        
        if (arg == "-D" || arg == "--display"){
            // Display embedded images through OpenCV::imshow (rather than pipe)
            to_display = true;
        }
        
        if (arg == "-p" || arg == "--pipe"){
            // Path of named input pipe to listen to after having piped extracted message to to output pipe. Sets mode to `editing`
            named_pipe_in = nextarg;
            mode = MODE_EDIT;
            continue;
        }
        
        /* List flags */
        
        nextlist.clear();
        while (nextarg[0] != '-'){
            nextlist.push_back(nextarg);
            nextarg = argv[++i];
        }
        
        if (arg == "-m" || arg == "--msg"){
            // File path(s) of message file(s) to embed. Sets mode to `embedding`
            ++n_msg_fps;
            mode = MODE_EMBEDDING;
            msg_fps = nextlist;
        }
    } while (true);
    
    
    
    #ifdef DEBUG
        if (verbosity < 0)
            verbosity = 0;
        else if (verbosity > 5)
            verbosity = 5;
        mylog.setLevel(verbosity);
        
        mylog.set_dt_fmt(log_fmt);
        
        mylog.dbg();
        mylog << "Verbosity " << +verbosity << std::endl;
    #endif
    
    
    
    if (out_fmt == NULLSTR){
        if (n_msg_fps != 0){
            #ifdef DEBUG
                throw std::runtime_error("Must specify --out-fmt if embedding --msg files");
            #else
                return 1;
            #endif
        }
        #ifdef DEBUG
            mylog << " to display";
        #endif
    } else {
        #ifdef DEBUG
            if (n_msg_fps == 0){
                mylog << " to ";
                if (to_disk)
                    mylog << "file";
                else
                    mylog << "display";
            }
        #endif
    }
    #ifdef DEBUG
        mylog << std::endl;
    #endif
    
    if (mode == 0){
        mode = MODE_EXTRACT;
    }
    
    
    const float min_complexity = std::stof(argv[++i]);
    // Minimum bitplane complexity
    if (min_complexity > 0.5){
        #ifdef DEBUG
            mylog.crit();
            mylog << "Current implementation requires maximum minimum complexity of 0.5" << std::endl;
        #endif
        return 1;
    }
    
    std::vector<std::string> img_fps;
    // File path(s) of input image file(s)
    do {
        img_fps.push_back(argv[++i]);
    } while (i < argc);
    
    
    
    
    if (sodium::sodium_init() == -1) {
        #ifdef DEBUG
            mylog.crit();
            mylog << "libsodium init fail" << std::endl;
        #endif
        return 1;
    }
    
    #ifdef DEBUG
        mylog.info();
        switch(mode){
            case MODE_EMBEDDING:     mylog << "Embedding"; break;
            case MODE_CALC_MAX_CAP:  mylog << "Calculating max cap"; break;
            case MODE_EDIT:          mylog << "Editing"; break;
            case MODE_EXTRACT:       mylog << "Extracting"; break;
        }
    #endif
    
    std::smatch path_regexp_match;
    
    BPCSStreamBuf bpcs_stream(min_complexity, img_fps);
    bpcs_stream.embedding = (mode == MODE_EMBEDDING);
    bpcs_stream.out_fmt = out_fmt;
    bpcs_stream.load_next_img(); // Init
    
    uint_fast64_t j;
    char* fp;
    uint_fast64_t n_msg_bytes;
    if (mode == MODE_EMBEDDING){
        FILE* msg_file;
        for (i=0; i<n_msg_fps; ++i){
            fp = msg_fps[i];
            #ifdef DEBUG
                mylog.info();
                mylog << "Reading msg `" << fp << "`" << std::endl;
            #endif
            
            // At start, and between embedded messages, is a 64-bit integer telling us the size of the next embedded message
            // TODO: XOR this value with 64-bit integer, else the fact that the first 32+ bits will almost certainly be 0 will greatly help unwanted decryption
            
            // TODO: bpcs_stream << encryption << msg_file
            
            n_msg_bytes = sizeof(fp) -1;
            for (j=0; j<8; ++j)
                bpcs_stream.sputc((n_msg_bytes >> (8*j)) & 255);
            //bpcs_stream.write(n_msg_bytes, 8);
            for (j=0; j<n_msg_bytes; ++j)
                bpcs_stream.sputc((unsigned char)fp[j]); // WARNING: Accessing char* by indices - how behaves for non-byte characters?
            //bpcs_stream.write(fp, n_msg_bytes);
            
            n_msg_bytes = get_fsize(fp);
            for (j=0; j<8; ++j)
                bpcs_stream.sputc((n_msg_bytes >> (8*j)) & 255);
            //bpcs_stream.write(n_msg_bytes, 8);
            msg_file = fopen(fp, "r");
            for (j=0; j<n_msg_bytes; ++j)
                // WARNING: Assumes there are exactly n_msg_bytes
                bpcs_stream.sputc(getc(msg_file));
            //bpcs_stream.write(msg_file, n_msg_bytes);
            fclose(msg_file);
        }
        // After all messages, signal end with signalled size of 0
        // TODO: XOR, as above
        for (j=0; j<8; ++j)
            bpcs_stream.sputc(0);
        bpcs_stream.end();
    } else {
        std::string fp_str;
        i = 0;
        unsigned char INTTOREMOVE = 0;
        do {
            n_msg_bytes = 0;
            for (j=0; j<8; ++j){
                INTTOREMOVE = bpcs_stream.sgetc();
                n_msg_bytes |= (INTTOREMOVE << (8*j));
            }
            #ifdef DEBUG
                mylog.info();
                mylog << "n_msg_bytes " << +n_msg_bytes << std::endl;
            #endif
            
            #ifdef TESTS
                if (n_msg_bytes == 0){
                    mylog.crit();
                    mylog << "n_msg_bytes = 0" << std::endl;
                    //return 1;
                    n_msg_bytes = 100;
                }
            #endif
            
            if (i & 1){
                // First block of data is original file path, second is file contents
                if (mode == MODE_EXTRACT && out_fmt != NULLSTR){
                    std::ofstream of(fp);
                    for (j=0; j<n_msg_bytes; ++j)
                        of.put(bpcs_stream.sgetc());
                    of.close();
                } else if (mode == MODE_EXTRACT){
                    unsigned char data[n_msg_bytes];
                    for (j=0; j<n_msg_bytes; ++j)
                        data[j] = bpcs_stream.sgetc();
                    cv::Mat rawdata(1, n_msg_bytes, CV_8UC1, data);
                    cv::Mat decoded_img = cv::imdecode(rawdata, CV_LOAD_IMAGE_UNCHANGED);
                    if (decoded_img.data == NULL){
                        #ifdef DEBUG
                            mylog.crit();
                            mylog << "No image data loaded from " << +n_msg_bytes << "B data stream claiming to originate from file `" << fp << "`" << std::endl;
                            throw std::invalid_argument("No image data loaded");
                        #else
                            abort();
                        #endif
                    }
                    #ifdef DEBUG
                        mylog.info();
                        mylog << "Displaying image" << std::endl;
                    #endif
                    cv::imshow(fp, decoded_img);
                    cv::waitKey(0);
                } else {
                    for (j=0; j<n_msg_bytes; ++j){
                    }
                }
            } else {
                for (j=0; j<n_msg_bytes; ++j){
                    fp += bpcs_stream.sgetc();
                }
                #ifdef DEBUG
                    mylog.info();
                    mylog << "Original fp: " << fp << std::endl;
                #endif
                fp_str = fp;
                std::regex_search(fp_str, path_regexp_match, path_regexp);
                fp = (char*)format_out_fp(out_fmt, path_regexp_match).c_str();
                #ifdef DEBUG
                    mylog.info();
                    mylog << "Formatted fp: " << fp << std::endl;
                #endif
            }
            
            ++i;
        } while (n_msg_bytes != 0);
    }
}
