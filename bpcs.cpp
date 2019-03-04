#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <fstream>
#include <sys/stat.h> // for stat
#include <string> // for std::stoi (stof already defined elsewhere)

namespace sodium {
    #include <sodium.h> // /crypto_secretstream_xchacha20poly1305.h> // libsodium for cryption (-lsodium)
}

#ifdef DEBUG
    #include <compsky/logger.hpp> // for CompskyLogger
#endif



static const std::string NULLSTR = "[NULL]";


#ifdef DEBUG
    uint whichbyte = 0;
    uint_fast64_t gridlimit = 0;
    
    
    static CompskyLogger mylog("bpcs", std::cout);
    static CompskyLogger mylog1("bpcs1", std::cout);
    static std::ostream* os = &std::cout;
    static std::ostream* os1;
    
    
    uint MAX_CONJ_GRIDS = 0;
    uint conj_grids_found = 0;
#endif



inline uint_fast64_t get_charp_len(char* chrp){
    uint_fast64_t i = 0;
    while (*chrp != 0){
        ++i;
        ++chrp;
    }
    return i;
}



std::string format_out_fp(char* out_fmt, char* fp, const bool check_if_lossless){
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
    
    if (check_if_lossless){
        std::string result_ext = result.substr(result.find_last_of(".") + 1);
        if (result_ext == "png"){
        } else {
            #ifdef DEBUG
                mylog.set_verbosity(0);
                mylog.set_cl('r');
                mylog << "Must write to a file in lossless format, not `" << result_ext << "`" << std::endl;
                throw std::runtime_error("Must write to a file in lossless format");
            #else
                abort();
            #endif
        }
    }
    
    return result;
}



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
    for (uint_fast16_t i=0; i<arr.rows*arr.cols; ++i)
        dest.data[i] = (arr.data[i] >> n) & 1;
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl(0);
        mylog << "bitandshift " << +n << ":  arr(sum==" << +cv::sum(arr)[0] << ")  ->  dest(sum==" << +cv::sum(dest)[0] << ")" << std::endl;
    #endif
}

inline void bitshift_up(cv::Mat &arr, uint_fast16_t n){
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl(0);
        mylog << "bitshift_up " << +n << std::endl;
    #endif
    cv::multiply(arr, 1 << n, arr);
}

inline void convert_to_cgc(cv::Mat &arr){
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl(0);
        mylog << "Converted to CGC: arr(sum==" << +cv::sum(arr)[0] << ") -> dest(sum==";
    #endif
    for (uchar* bit = arr.data;  bit != arr.data + (arr.rows*arr.cols);  ++bit)
        *bit = *bit ^ (*bit >> 1);
    #ifdef DEBUG
        mylog << +cv::sum(arr)[0] << ")" << std::endl;
    #endif
}






/*
 * Initialise chequerboards
 */
#ifdef DEBUG
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
#else
// Results in a larger binary size by 152B, but *surely* in slightly less overhead regardless...
uchar chequered_arr_a[64] = {0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0};
uchar chequered_arr_b[64] = {1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1};
// NOTE: These are constants, but there is no OpenCV class for const data
static const cv::Mat chequerboard_a = cv::Mat(8,8, CV_8UC1, chequered_arr_a);
static const cv::Mat chequerboard_b = cv::Mat(8,8, CV_8UC1, chequered_arr_b);
#endif




/*
 * Grid complexity
 */

inline float grid_complexity(
    cv::Mat &grid,
    cv::Mat &xor_adj_mat1, cv::Mat &xor_adj_mat2, cv::Rect &xor_adj_rect1, cv::Rect &xor_adj_rect2, cv::Rect &xor_adj_rect3, cv::Rect &xor_adj_rect4
){
    return (float)xor_adj(grid, xor_adj_mat1, xor_adj_mat2, xor_adj_rect1, xor_adj_rect2, xor_adj_rect3, xor_adj_rect4) / (2*8*7); // (float)(grid_w * (grid_h -1) + grid_h * (grid_w -1));
}








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
    BPCSStreamBuf(const float min_complexity, std::vector<char*>& img_fps, bool emb, char* outfmt):
    embedding(emb), out_fmt(outfmt), min_complexity(min_complexity), img_n(0), x(0), y(0), grids_since_conjgrid(0), img_fps(img_fps)
    {}
    
    
    const bool embedding;
    uchar sgetc();
    
    void sputc(uchar c);
    
    char* out_fmt;
    
    void load_next_img(); // Init
    void save_im(); // End
    
    #ifdef DEBUG
        std::vector<float> complexities;
    #endif
  private:
    int set_next_grid();
    void load_next_bitplane();
    void load_next_channel();
    
    void write_conjugation_map();
    
    inline float get_grid_complexity(cv::Mat&);
    inline void conjugate_grid(cv::Mat&);

    const float min_complexity;
    uint_fast16_t img_n;
    #ifdef DEBUG
        uint_fast64_t n_grids;
        uint_fast64_t n_complex_grids_found;
    #endif
    
    uint_fast64_t x; // the current grid is the (x-1)th grid horizontally and yth grid vertically (NOT the coordinates of the corner of the current grid of the current image)
    uint_fast64_t y;
    
    uint_fast8_t gridbitindx; // Count of bits already read/written, modulo 64 (i.e. the index in the grid we are writing/reading the byte to/from)
    uint_fast8_t grids_since_conjgrid;
    // To reserve the first grid of every 64 complex grids in order to write conjugation map
    // Note that the first bit of this map is for its own conjugation state
    
    uint_fast8_t n_channels;
    uint_fast8_t channel_n;
    uint_fast8_t n_bitplanes;
    uint_fast8_t bitplane_n;
    
    uchar conjugation_map[64];
    
    uchar* conjugation_map_ptr;
    uchar* conjugation_grid_ptr;
    
    uchar* grid_ptr;
    
    const std::vector<char*> img_fps;
    std::vector<cv::Mat> channel_byteplanes;
    
    cv::Mat im_mat;
    
    cv::Mat grid{8, 8, CV_8UC1};
    cv::Mat conjugation_grid{8, 8, CV_8UC1};
    
    cv::Mat dummy_grid_a{8, 8, CV_8UC1};
    cv::Mat dummy_grid_b{8, 8, CV_8UC1};
    
    cv::Mat xor_adj_mat1{8, 7, CV_8UC1};
    cv::Mat xor_adj_mat2{7, 8, CV_8UC1};
    
    cv::Rect xor_adj_rect1{cv::Point(0, 0), cv::Size(7, 8)};
    cv::Rect xor_adj_rect2{cv::Point(1, 0), cv::Size(7, 8)};
    cv::Rect xor_adj_rect3{cv::Point(0, 0), cv::Size(8, 7)};
    cv::Rect xor_adj_rect4{cv::Point(0, 1), cv::Size(8, 7)};
    // cv::Rect, cv::Point, cv::Size are all column-major, i.e. (x, y) or (width, height), but cv::Mat is row-major
    
    cv::Mat bitplane;
    
    cv::Mat bitplanes[32 * 4]; // WARNING: Images rarely have a bit-depth greater than 32, but would ideally be set on per-image basis
};

inline float BPCSStreamBuf::get_grid_complexity(cv::Mat &arr){
    return grid_complexity(arr, this->xor_adj_mat1, this->xor_adj_mat2, this->xor_adj_rect1, this->xor_adj_rect2, this->xor_adj_rect3, this->xor_adj_rect4);
}

inline void BPCSStreamBuf::conjugate_grid(cv::Mat &grid){
    cv::bitwise_and(grid, chequerboard_a, this->dummy_grid_a);
    
    cv::bitwise_not(grid, grid);
    cv::bitwise_and(grid, chequerboard_b, this->dummy_grid_b);
    
    cv::bitwise_or(this->dummy_grid_a, this->dummy_grid_b, grid);
    
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl('p');
        mylog << "<*" << +this->grids_since_conjgrid << "(" << +(this->x -8) << ", " << +this->y << ")>" << std::endl;
        mylog << grid << std::endl;
    #endif
}

inline void BPCSStreamBuf::load_next_bitplane(){
    #ifdef TESTS
        assert (!this->embedding);
    #endif
    
    #ifdef DEBUG
        mylog.set_verbosity(4);
        mylog.set_cl('b');
        mylog << "Loading bitplane " << +(this->bitplane_n -1) << " of " << +this->n_bitplanes << " from channel " << +this->channel_n << std::endl;
    #endif
    
    bitandshift(this->channel_byteplanes[this->channel_n], this->bitplane, this->n_bitplanes - ++this->bitplane_n);
    
    #ifdef DEBUG
        mylog.set_verbosity(10);
        mylog.set_cl('g');
        mylog << "\n";
        mylog << this->bitplane;
        mylog << "\n";
    #endif
}

void BPCSStreamBuf::load_next_channel(){
    #ifdef DEBUG
        mylog.set_verbosity(4);
        mylog.set_cl(0);
        mylog << "Loading channel(sum==" << +cv::sum(channel_byteplanes[this->channel_n])[0] << ") " << +(this->channel_n + 1) << " of " << +this->n_channels << std::endl;
    #endif
    convert_to_cgc(this->channel_byteplanes[this->channel_n]);
    this->bitplane_n = 0;
    this->load_next_bitplane();
}

void BPCSStreamBuf::load_next_img(){
    if (this->embedding && this->img_n != 0){
        this->save_im();
    }
    #ifdef DEBUG
        mylog.set_verbosity(3);
        mylog.set_cl('g');
        mylog << "Loading img " << +this->img_n << " of " << +this->img_fps.size() << " `" << this->img_fps[this->img_n] << "`, using: Complexity >= " << +this->min_complexity << std::endl;
    #endif
    this->im_mat = cv::imread(this->img_fps[this->img_n++], CV_LOAD_IMAGE_COLOR);
    #ifdef TESTS
        assert(this->im_mat.isContinuous());
        // Apparently guaranteed to be the case for imread, as it calls create()
    #endif
    cv::split(this->im_mat, this->channel_byteplanes);
    this->n_channels = this->im_mat.channels();
    
    switch(this->im_mat.depth()){
        case CV_8U:
            this->n_bitplanes = 8;
            break;
        case CV_8S:
            this->n_bitplanes = 8;
            break;
        case CV_16U:
            this->n_bitplanes = 16;
            break;
        case CV_16S:
            this->n_bitplanes = 16;
            break;
        case CV_32S:
            this->n_bitplanes = 32;
            break;
        case CV_32F:
            this->n_bitplanes = 32;
            break;
        case CV_64F:
            this->n_bitplanes = 64;
            break;
    }
    
    #ifdef DEBUG
        mylog.set_verbosity(3);
        mylog.set_cl('g');
        mylog << +this->im_mat.cols << "x" << +this->im_mat.rows << '\n';
        mylog << +this->n_channels << " channels" << '\n';
        mylog << "Bit-depth of " << +this->n_bitplanes << '\n';
        mylog << std::endl;
    #endif
    
    #ifdef DEBUG
        this->complexities.clear();
        this->n_complex_grids_found = 0;
    #endif
    
    this->channel_n = 0;
    this->gridbitindx = 0;
    
    if (this->embedding){
        uint_fast8_t k = 0;
        uint_fast8_t i = 0;
        for (uint_fast8_t j=0; j<this->n_channels; ++j){
            convert_to_cgc(this->channel_byteplanes[j]);
            i = 0;
            while (i < this->n_bitplanes){
                this->bitplanes[k] = cv::Mat::zeros(this->im_mat.rows, this->im_mat.cols, CV_8UC1);
                bitandshift(this->channel_byteplanes[j], this->bitplanes[k++], this->n_bitplanes - ++i);
                #ifdef DEBUG
                    mylog.set_verbosity(10);
                    mylog.set_cl('g');
                    mylog << "\n";
                    mylog << this->bitplanes[k-1];
                    mylog << "\n";
                #endif
            }
        }
        this->bitplane = this->bitplanes[0];
        
        this->bitplane_n = 1;
    } else {
        this->bitplane = cv::Mat::zeros(im_mat.rows, im_mat.cols, CV_8UC1); // Need to initialise for bitandshift
        this->load_next_channel();
    }
    // Get conjugation grid
    this->set_next_grid();
    
    if (!this->embedding){
        if (*(this->grid_ptr + 7*(this->bitplane.cols) + 7) != 0)
            // Since this is the conjugation map grid, it contains its own conjugation status in its last bit
            // TODO: Have each conjgrid store the conjugation bit of the next conjgrid
            this->conjugate_grid(this->grid);
        
        this->conjugation_map_ptr = this->conjugation_map; // Reset to first position
        for (uint_fast8_t memi=0; memi<8; ++memi){
            memcpy(this->conjugation_map_ptr, this->grid_ptr, 8);
            this->conjugation_map_ptr += 8;
            this->grid_ptr            += this->bitplane.cols;
        }
        // No need to reset this->grid_ptr - we are not going to write conjugation data
    }
    
    this->conjugation_grid = this->grid;
    this->conjugation_grid_ptr = this->grid_ptr;
    
    this->conjugation_map_ptr = this->conjugation_map;
    
    this->grids_since_conjgrid = 0;
    
    // Get first data grid
    this->set_next_grid();
    
    if (!this->embedding)
        if (*(this->conjugation_map_ptr++))
            this->conjugate_grid(this->grid);
}

void BPCSStreamBuf::write_conjugation_map(){
    float complexity;
    
    #ifdef DEBUG
        mylog.set_verbosity(6);
        mylog.set_cl('p');
        mylog << "Conjgrid orig" << "\n";
        mylog.set_verbosity(6);
        mylog.set_cl(0);
        mylog << this->conjugation_grid << "\n";
    #endif
    
    // NOTE: this->conjugation_map_ptr should at this stage always point at the 63rd element of this->conjugation_grid, as we have either written 63 complex grids, or it has been called by this->save_im() (where it should be set to the 63rd)
    #ifdef TESTS
        assert(*this->conjugation_map_ptr == this->conjugation_map[63]);
    #endif
    this->conjugation_map_ptr = this->conjugation_map;
    for (uint_fast8_t k=0; k<8; ++k){
        memcpy(this->conjugation_grid_ptr, this->conjugation_map_ptr, 8);
        this->conjugation_grid_ptr += this->bitplane.cols;
        this->conjugation_map_ptr += 8;
    }
    
    // this->conjugation_grid_ptr should obviously be at the 65th element, due to the memcpy-ing above
    *(--this->conjugation_grid_ptr) = 0;
    
    complexity = this->get_grid_complexity(this->conjugation_grid);
    
    if (complexity < this->min_complexity){
        this->conjugate_grid(this->conjugation_grid);
        *this->conjugation_grid_ptr = 1;
        if (1 - complexity - 1/57 < this->min_complexity)
            // Maximum difference in complexity from changing first bit is `2 / (2 * 8 * 7)` == 1/57
            // Hence - assuming this->min_complexity<0.5 - the conjugate's complexity is 
            
            if (*(this->conjugation_map_ptr -1) != 0)
                // If it were 0, the XOR of this with the first bit was 1, and had been 0 before.
                // Hence the grid complexity would have been at least its previous value
                
                if (*(this->conjugation_map_ptr -8) != 0)
                    #ifdef DEBUG
                    throw std::runtime_error("Grid complexity fell below minimum value");
                    #else
                    abort();
                    #endif
    }
    
    #ifdef DEBUG
        mylog.set_verbosity(6);
        mylog.set_cl(0);
        mylog << "Written conjugation map" << "\n" << this->conjugation_grid << std::endl;
    #endif
}

int BPCSStreamBuf::set_next_grid(){
    #ifdef DEBUG
    if (++whichbyte == gridlimit){
        mylog << std::endl;
        throw std::runtime_error("Reached gridlimit");
    }
    mylog.set_verbosity(5);
    mylog.set_cl(0);
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
            this->write_conjugation_map();
            
            this->conjugation_grid = this->grid;
            this->conjugation_grid_ptr = this->grid_ptr;
        } else {
            if (*(this->grid_ptr + 7*(this->bitplane.cols) + 7) != 0)
                // Since this is the conjugation map grid, it contains its own conjugation status in its last bit
                // TODO: Have each conjgrid store the conjugation bit of the next conjgrid
                this->conjugate_grid(this->grid);
            
            this->conjugation_map_ptr = this->conjugation_map;
            for (uint_fast8_t memi=0; memi<8; ++memi){
                memcpy(this->conjugation_map_ptr, this->grid_ptr, 8);
                this->conjugation_map_ptr += 8;
                this->grid_ptr            += this->bitplane.cols;
            }
            // This specific this->grid_ptr is not used beyond this point, so no need to reset
            #ifdef DEBUG
                for (uint_fast8_t k=0; k<63; ++k){
                    mylog.set_verbosity(5);
                    mylog.set_cl(0);
                    mylog << +this->conjugation_map[k];
                }
                mylog << std::endl;
                mylog << "\n" << this->grid;
            #endif
        }
        this->conjugation_map_ptr = this->conjugation_map;
        
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
            
            #ifdef DEBUG
                //this->complexities.push_back(complexity);
            #endif
            
            i += 8;
            
            if (complexity >= this->min_complexity){
                //this->bitplane(grid_shape).copyTo(this->grid);
                this->x = i;
                this->y = j;
                this->grid_ptr = this->bitplane.ptr<uchar>(j) +(i -8);
                #ifdef DEBUG
                    *os1 << +this->x << "\t" << +this->y << std::endl;
                    
                    ++this->n_complex_grids_found;
                    mylog.set_verbosity(7);
                    mylog.set_cl('B');
                    mylog << "Found grid(" << +i << "," << +j << "): " << +complexity << "\n";
                    mylog.set_cl('r');
                    mylog << this->grid << std::endl;
                #endif
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
        mylog.set_verbosity(4);
        mylog.set_cl('b');
        mylog << "Exhausted bitplane" << std::endl;
        mylog.set_verbosity(4);
        mylog.set_cl(0);
        mylog << "embedding: " << +this->embedding << std::endl;
        mylog << "bitplane_n: " << +this->bitplane_n << std::endl;
        mylog << "n_bitplanes: " << +this->n_bitplanes << std::endl;
        mylog << "channel_n: " << +this->channel_n << std::endl;
        mylog << "n_channels: " << +this->n_channels << std::endl;
        mylog << "n_complex_grids_found: " << +n_complex_grids_found << std::endl;
    #endif
    
    if (this->embedding){
        if (this->bitplane_n < this->n_bitplanes * this->n_channels){
            this->bitplane = this->bitplanes[this->bitplane_n++]; // Equivalent to this->load_next_bitplane();
            if (this->bitplane_n > this->n_bitplanes)
                ++this->channel_n;
            goto try_again;
        }
    } else if (this->bitplane_n < this->n_bitplanes){
        this->load_next_bitplane();
        goto try_again;
    } else if (++this->channel_n < this->n_channels){
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
    if (this->grids_since_conjgrid != 0)
        // Because we might have set it to 0 above
        --this->grids_since_conjgrid;
    return this->set_next_grid();
}

uchar BPCSStreamBuf::sgetc(){
    uchar c = 0;
    for (uint_fast8_t i=0; i<8; ++i){
        c |= *(this->grid_ptr++) << i;
    }
    
    if (++this->gridbitindx == 8){
        if (this->set_next_grid())
            #ifdef DEBUG
            throw std::runtime_error("Unexpected end of BPCS stream");
            #else
            abort();
            #endif
        
        if (*(this->conjugation_map_ptr++))
            this->conjugate_grid(this->grid);
        
        this->gridbitindx = 0;
    } else {
        this->grid_ptr += this->bitplane.cols -8;
    }
    
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl('p');
        mylog << "sgetc " << +c << " (";
        if (c == '\n')
            mylog << "\\n";
        else if (c == '\r')
            mylog << "\\r";
        else
            mylog << c;
        mylog << ")  (x,y,bitplane,ch) = " << +this->x << ", " << +this->y << ", " << +this->bitplane_n << ", " << +this->channel_n << std::endl;
    #endif
    return c;
}

void BPCSStreamBuf::sputc(uchar c){
    #ifdef DEBUG
        mylog.set_verbosity(5);
        mylog.set_cl('p');
        mylog << "sputc " << +c << " (" << c << ") at gridbitindx " << +this->gridbitindx << "  (x,y,bitplane) = " << +this->x << ", " << +this->y << ", " << +this->bitplane_n << std::endl;
        mylog.set_verbosity(5);
        mylog.set_cl(0);
        mylog << "sputc " << +c << " bits ";
    #endif
    for (uint_fast8_t i=0; i<8; ++i){
        #ifdef DEBUG
            mylog.set_cl('B');
            mylog << +((c >> i) & 1);
            mylog.set_cl(0);
            mylog << +(*this->grid_ptr);
        #endif
        *(this->grid_ptr++) = c & 1;
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
            this->conjugate_grid(this->grid);
            *this->conjugation_map_ptr = 1;
        } else {
            *this->conjugation_map_ptr = 0;
        }
        ++this->conjugation_map_ptr;
        
        if (this->set_next_grid()){
            #ifdef DEBUG
                print_histogram(this->complexities, 10, 200);
                throw std::runtime_error("Too much data to encode");
            #else
                abort();
            #endif
        }
        
        this->gridbitindx = 0;
    } else {
        this->grid_ptr += this->bitplane.cols -8;
    }
    
    #ifdef DEBUG
        mylog << std::endl;
    #endif
}

void BPCSStreamBuf::save_im(){
    // Called either when we've exhausted this->im_mat's last channel, or when we've reached the end of embedding
    #ifdef TESTS
        assert(this->embedding);
    #endif
    this->conjugation_map_ptr += 63 -this->grids_since_conjgrid;
    this->write_conjugation_map();
    
    #ifdef DEBUG
        mylog.set_verbosity(4);
        mylog.set_cl('b');
        mylog << "UnXORing " << +this->n_channels << " channels of depth " << +this->n_bitplanes << std::endl;
    #endif
    uint_fast8_t j;
    uint_fast8_t k = 0;
    for (uint_fast8_t i=0; i < this->n_channels; ++i){
        // First bitplane (i.e. most significant bit) of each channel is unchanged by conversion to CGC
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
    
    std::string out_fp = format_out_fp(this->out_fmt, this->img_fps[this->img_n -1], true);
    cv::merge(this->channel_byteplanes, this->im_mat);
    #ifdef DEBUG
        mylog.set_verbosity(3);
        mylog.set_cl('g');
        mylog << "Saving to  `" << out_fp << "`" << std::endl;
    #endif
    
    cv::imwrite(out_fp, this->im_mat);
}











int main(const int argc, char *argv[]){
    bool to_disk = false;
    bool to_display = false;
    bool embedding = false;
    bool extracting = false;
    // bool editing = false; // Use named_pipe_in[0] != 0
    
    uint_fast8_t n_msg_fps = 0;
    
    char* out_fmt = "";
    char* named_pipe_in = "";
    
    std::string out_fp;
    
    std::vector<char*> msg_fps;
    
    #ifdef DEBUG
        int verbosity = 3;
        
        mylog1.set_level(0);
        
        char* log_fmt = "[%T] ";
        
        uint_fast16_t n_bins = 10;
        uint_fast8_t n_binchars = 200;
    #else
        bool verbose = false;
    #endif
    uint_fast64_t i = 0;
    
    
    /* Argument parser */
    char* arg;
    char* nextarg;
    std::vector<char*> nextlist;
    char second_character;
    do {
        // Find opts, until reach arg that does not begin with '-'
        arg = argv[++i];
        if (arg[0] != '-')
            break;
        
        second_character = arg[1];
        
        /* Bool flags */
        
        switch(second_character){
            case 'd': to_disk = true; goto continue_argloop;
            // --to-disk
            // Write to disk (as opposed to psuedofile)
            case 'D': to_display = true; goto continue_argloop;
            // --to-display
            // Display embedded images through OpenCV::imshow (rather than pipe)
            #ifdef DEBUG
            case 'v': ++verbosity; goto continue_argloop;
            case 'q': --verbosity; goto continue_argloop;
            case '-':
                switch(arg[2]){
                    case 'o':
                        // --os1
                        // outstream - not formatted CompskyLogger
                        os1 = os; goto continue_argloop;
                    default:
                        goto invalid_argument;
                }
            #else
            case 'v': verbose = true; goto continue_argloop;
            // --verbose
            // If writing to disk, print output filepath
            #endif
        }
        
        /* Value flags */
        
        nextarg = argv[++i];
        
        #ifdef DEBUG
        if (second_character == '-'){
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
                    // --gridlimit
                    // Quit after having moved through `gridlimit` grids
                    gridlimit = std::stoi(nextarg);
                    goto continue_argloop;
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
        }
        #endif
        
        switch(second_character){
            case 'o': out_fmt = nextarg; goto continue_argloop;
            // --out-fmt
            // Format of output file path(s) - substitutions being fp, dir, fname, basename, ext. Sets mode to `extracting` if msg_fps not supplied.
            case 'p': named_pipe_in = nextarg; goto continue_argloop;
            // Path of named input pipe to listen to after having piped extracted message to to output pipe. Sets mode to `editing`
            case '-': break;
            case 'm': break;
            default: goto invalid_argument;
        }
        
        /* List flags */
        
        nextlist.clear();
        while (nextarg[0] != '-'){
            nextlist.push_back(nextarg);
            nextarg = argv[++i];
        }
        --i;
        
        if (second_character == 'm'){
            // File path(s) of message file(s) to embed. Sets mode to `embedding`
            ++n_msg_fps;
            embedding = true;
            msg_fps = nextlist;
            continue;
        }
        
        invalid_argument:
        #ifdef DEBUG
            printf("Invalid argument: %s", arg);
            throw std::runtime_error("Invalid argument");
        #else
            abort();
        #endif
        
        continue_argloop:
        continue;
    } while (true);
    
    
    
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
    if (out_fmt[0] == 0){
        if (n_msg_fps != 0){
            #ifdef DEBUG
                throw std::runtime_error("Must specify --out-fmt if embedding --msg files");
            #else
                return 1;
            #endif
        }
        #ifdef DEBUG
            mylog << "display";
        #endif
    }
    #ifdef DEBUG
        else {
            if (n_msg_fps == 0){
                if (to_disk)
                    mylog << "file";
                else
                    mylog << "display";
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
    do {
        img_fps.push_back(argv[i++]);
        #ifdef DEBUG
            mylog << argv[i -1] << std::endl;
        #endif
    } while (i < argc);
    
    
    
    
    if (sodium::sodium_init() == -1) {
        #ifdef DEBUG
            mylog.set_verbosity(0);
            mylog.set_cl('r');
            mylog << "libsodium init fail" << std::endl;
        #endif
        return 1;
    }
    
    BPCSStreamBuf bpcs_stream(min_complexity, img_fps, embedding, out_fmt);
    bpcs_stream.load_next_img(); // Init
    
    uint_fast64_t j;
    char* fp;
    uint_fast64_t n_msg_bytes;
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
                mylog << "Reading msg `" << fp << "`" << std::endl;
                
                mylog.set_verbosity(5);
                mylog.set_cl(0);
                mylog << "n_msg_bytes (filepath): " << +n_msg_bytes << std::endl;
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
                    throw std::runtime_error("No such file");
                #else
                    return 1;
                #endif
            }
            n_msg_bytes = stat_buf.st_size;
            #ifdef DEBUG
                mylog.set_verbosity(5);
                mylog.set_cl(0);
                mylog << "n_msg_bytes (contents): " << +n_msg_bytes << std::endl;
            #endif
            
            for (j=0; j<8; ++j)
                bpcs_stream.sputc((n_msg_bytes >> (8*j)) & 255);
            msg_file = fopen(fp, "r");
            for (j=0; j<n_msg_bytes; ++j)
                // WARNING: Assumes there are exactly n_msg_bytes
                bpcs_stream.sputc(getc(msg_file));
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
        std::string fp_str;
        i = 0;
        do {
            n_msg_bytes = 0;
            for (j=0; j<8; ++j){
                uchar c = bpcs_stream.sgetc();
                n_msg_bytes |= (c << (8*j));
                #ifdef DEBUG
                    mylog.set_verbosity(5);
                    mylog.set_cl(0);
                    mylog << "n_msg_bytes byte: " << +c << " (" << c << "), total: " << +n_msg_bytes << std::endl;
                #endif
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
                if (to_disk){
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
                } else if (extracting){
                    uchar data[n_msg_bytes];
                    for (j=0; j<n_msg_bytes; ++j)
                        data[j] = bpcs_stream.sgetc();
                    cv::Mat rawdata(1, n_msg_bytes, CV_8UC1, data);
                    cv::Mat decoded_img = cv::imdecode(rawdata, CV_LOAD_IMAGE_UNCHANGED);
                    if (decoded_img.data == NULL){
                        #ifdef DEBUG
                            mylog.set_verbosity(0);
                            mylog.set_cl('r');
                            mylog << "No image data loaded from " << +n_msg_bytes << "B data stream claiming to originate from file `" << fp_str << "`" << std::endl;
                            throw std::invalid_argument("No image data loaded");
                        #else
                            return 1;
                        #endif
                    }
                    #ifdef DEBUG
                        mylog.set_verbosity(3);
                        mylog.set_cl('g');
                        mylog << "Displaying image" << std::endl;
                    #endif
                    cv::imshow(fp_str, decoded_img);
                    cv::waitKey(0);
                } else {
                    #ifdef DEBUG
                        mylog.set_verbosity(5);
                        mylog.set_cl(0);
                    #endif
                    for (j=0; j<n_msg_bytes; ++j){
                        #ifdef DEBUG
                            mylog <<
                        #endif
                        bpcs_stream.sgetc();
                    }
                    #ifdef DEBUG
                        mylog << std::endl;
                    #endif
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
                fp_str = format_out_fp(out_fmt, (char*)fp_str.c_str(), false);
                #ifdef DEBUG
                    mylog.set_verbosity(3);
                    mylog.set_cl('g');
                    mylog << "Formatted fp: " << fp_str << std::endl;
                #endif
            }
            
            ++i;
        } while (true);
    }
}
