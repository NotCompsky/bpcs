#include <args.hxx>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#ifdef DEBUG7
    #define DEBUG6 DEBUG7
#endif
#ifdef DEBUG6
    #define DEBUG5 DEBUG6
#endif
#ifdef DEBUG5
    #define DEBUG4 DEBUG5
#endif
#ifdef DEBUG4
    #define DEBUG3 DEBUG4
#endif
#ifdef DEBUG3
    #define DEBUG2 DEBUG3
#endif
#ifdef DEBUG2
    #define DEBUG1 DEBUG2
#endif

#include <iostream>
#include <fstream>
#include <sys/stat.h> // for stat
#include <regex> // for std::basic_regex

#include <cstdio> // for std::remove

namespace sodium {
    #include <sodium.h> // /crypto_secretstream_xchacha20poly1305.h> // libsodium for cryption (-lsodium)
}

#include <boost/log/trivial.hpp>




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


void bitandshift(cv::Mat &arr, cv::Mat &dest, uint_fast16_t n){
    for (uint_fast16_t i=0; i!=arr.rows*arr.cols; ++i)
        dest.data[i] = (arr.data[i] >> n) & 1;
    #ifdef DEBUG3
        std::cout << "bitandshift:  arr(sum==" << +cv::sum(arr)[0] << ")  ->  dest(sum==" << +cv::sum(dest)[0] << ")" << std::endl;
    #endif
}

void bitshift_up(cv::Mat &arr, uint_fast16_t n){
    cv::multiply(arr, 1 << n, arr);
}

void convert_to_cgc(cv::Mat &arr, cv::Mat &dest){
    cv::bitwise_xor(arr, arr/2, dest);
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

cv::Mat chequerboard_a = chequerboard(0, 8, 8);
cv::Mat chequerboard_b = chequerboard(1, 8, 8);




/*
 * Grid complexity
 */

inline float grid_complexity(
    cv::Mat &grid,
    cv::Mat &xor_adj_mat1, cv::Mat &xor_adj_mat2, cv::Rect &xor_adj_rect1, cv::Rect &xor_adj_rect2, cv::Rect &xor_adj_rect3, cv::Rect &xor_adj_rect4
){
    return (float)xor_adj(grid, xor_adj_mat1, xor_adj_mat2, xor_adj_rect1, xor_adj_rect2, xor_adj_rect3, xor_adj_rect4) / (2*8*7); // (float)(grid_w * (grid_h -1) + grid_h * (grid_w -1));
}

void conjugate_grid(cv::Mat &grid){
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

#ifdef DEBUG1
void print_histogram(std::vector<float> &complexities, uint_fast16_t n_bins, uint_fast16_t n_binchars){
    std::sort(std::begin(complexities), std::end(complexities));
    uint_fast64_t len_complexities = complexities.size();
    
    std::cout << std::endl;
    
    if (len_complexities == 0){
        std::cout << "No complexities to display" << std::endl;
        return;
    }
    
    std::cout << "Complexities Histogram" << std::endl;
    float min = *std::min_element(std::begin(complexities), std::end(complexities));
    float max = *std::max_element(std::begin(complexities), std::end(complexities));
    std::cout << "Total: " << +len_complexities << " between " << +min << ", " << +max << std::endl;
    if (min == max)
        return;
    float step = (max - min) / (float)n_bins;
    std::cout << "Bins:  " << +n_bins << " with step " << step << std::endl;
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
        std::cout << j * step << ": " << bin_totals[j] << std::endl << "   ";
        for (k=0; k<bin_total; ++k){
            std::cout << "#";
        }
        std::cout << std::endl;
    }
    
    std::cout << n_bins * step << std::endl;
    std::cout << std::endl << std::endl;
}
#endif














class BPCSStreamBuf { //: public std::streambuf {
    // Based on excellent post by krzysztoftomaszewski
    // src https://artofcode.wordpress.com/2010/12/12/deriving-from-stdstreambuf/
  public:
    /* Constructors */
    BPCSStreamBuf(const float min_complexity, const std::vector<std::string> img_fps):
    min_complexity(min_complexity), img_fps(img_fps), img_n(0), byteindx(0), grids_since_conjgrid(63)
    {}
    
    
    char sgetc();
    char sputc(char c);
    
    void load_next_img(); // Init
    void end();
    
    bool embedding;
    std::string out_fmt;
  private:
    int set_next_grid();
    void unxor_bitplane();
    void load_next_bitplane();
    void load_next_channel();
    void commit_channel();
    
    void save_im();
    
    inline float get_grid_complexity(cv::Mat&);

    const float min_complexity;
    const std::vector<std::string> img_fps;
    uint_fast16_t img_n;
    uint_fast64_t n_grids;
    
    uint_fast8_t grids_since_conjgrid;
    // To reserve the first grid of every 64 complex grids in order to write conjugation map
    // Note that the first bit of this map is for its own conjugation state
    
    uint_fast64_t x; // the current grid is the (x-1)th grid horizontally and yth grid vertically (NOT the coordinates of the corner of the current grid of the current image)
    uint_fast64_t y;
    
    uint_fast64_t conjgrid_x;
    uint_fast64_t conjgrid_y;
    
    uint_fast8_t byteindx; // Count of bytes already read/written, modulo 8 (i.e. which row in the grid we are writing/reading the byte to/from)
    
    cv::Mat im_mat;
    std::vector<cv::Mat> channel_byteplanes;
    uint_fast8_t n_channels;
    uint_fast8_t channel_n;
    uint_fast8_t n_bitplanes;
    
    unsigned char conjugation_map[64];
    
    #ifdef DEBUG1
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
    cv::Mat bitplane;
    cv::Mat prev_bitplane;
    cv::Mat unXORed_bitplane;
    
    cv::Mat byteplane;
    cv::Mat XORed_byteplane;
    
    std::smatch path_regexp_match;
};

inline float BPCSStreamBuf::get_grid_complexity(cv::Mat &arr){
    return grid_complexity(arr, this->xor_adj_mat1, this->xor_adj_mat2, this->xor_adj_rect1, this->xor_adj_rect2, this->xor_adj_rect3, this->xor_adj_rect4);
}

inline void BPCSStreamBuf::unxor_bitplane(){
    #ifdef DEBUG3
        std::cout << "UnXORing bitplane " << +this->bitplane_n << " of " << +this->n_bitplanes << std::endl;
    #endif
    if (this->bitplane_n == 1)
        // Remember - value of this->bitplane_n is the index of the NEXT bitplane
        this->unXORed_bitplane = this->bitplane.clone();
    else
        // Revert conversion of non-first planes to CGC
        cv::bitwise_xor(this->bitplane, this->prev_bitplane, this->unXORed_bitplane);
    
    if (this->bitplane_n != this->n_bitplanes)
        this->prev_bitplane = this->unXORed_bitplane.clone();
        // WARNING: MUST BE DEEP COPY!
    
    bitshift_up(this->unXORed_bitplane, this->n_bitplanes - this->bitplane_n -1);
    
    #ifdef DEBUG4
        std::cout << "Adding unXORed bitplane(sum==" << +cv::sum(this->bitplane)[0] << ") to channel " << +this->byteplane.cols << "x" << +this->byteplane.rows << " byteplane" << std::endl;
    #endif
    
    cv::bitwise_or(this->byteplane, this->unXORed_bitplane, this->byteplane);
}

inline void BPCSStreamBuf::load_next_bitplane(){
    if (this->embedding && this->bitplane_n != 0)
        this->unxor_bitplane();
    bitandshift(this->XORed_byteplane, this->bitplane, this->n_bitplanes - ++this->bitplane_n);
    #ifdef DEBUG3
        std::cout << "Loaded bitplane " << +this->bitplane_n << " of " << +this->n_bitplanes << std::endl;
    #endif
    this->x = 0;
    this->y = 0;
}

inline void BPCSStreamBuf::commit_channel(){
    #ifdef DEBUG3
        std::cout << "Committing changes to channel(sum==" << +cv::sum(this->byteplane)[0] << ") " << +(this->channel_n -1) << std::endl;
    #endif
    if (this->embedding && this->bitplane_n != 0){
        // this->bitplane_n refers to the index of the NEXT bitplane to load - a value of 0 can only mean it has yet to be initialised
        // Commit changes to last bitplane
        while (this->bitplane_n++ < this->n_bitplanes)
            this->unxor_bitplane();
    }
    this->channel_byteplanes[this->channel_n -1] = this->byteplane; // WARNING: clone?
}

inline void BPCSStreamBuf::load_next_channel(){
    #ifdef DEBUG3
        std::cout << "Loading channel(sum==" << +cv::sum(channel_byteplanes[this->channel_n])[0] << ") " << +(this->channel_n + 1) << " of " << +this->n_channels << std::endl;
    #endif
    if (this->embedding && this->channel_n != 0){
        // this->channel_n refers to the index of the NEXT channel to load - a value of 0 can only mean it has yet to be initialised
        // Commit changes to last channel, then init next channel byteplane
        this->commit_channel();
        this->byteplane = cv::Mat::zeros(this->im_mat.rows, im_mat.cols, CV_8UC1);
    }
    convert_to_cgc(this->channel_byteplanes[this->channel_n++], this->XORed_byteplane);
    this->bitplane_n = 0;
    this->load_next_bitplane();
}

void BPCSStreamBuf::load_next_img(){
    if (this->embedding && this->img_n != 0){
        this->commit_channel();
        this->save_im();
    }
    #ifdef DEBUG2
        std::cout << "Loading img " << +(this->img_n + 1) << " of " << +this->img_fps.size() << " `" << this->img_fps[this->img_n] << "`, using: Complexity >= " <<  +this->min_complexity << std::endl;
    #endif
    this->im_mat = cv::imread(this->img_fps[this->img_n++], CV_LOAD_IMAGE_COLOR);
    cv::split(this->im_mat, this->channel_byteplanes);
    this->n_channels = channel_byteplanes.size();
    
    switch(this->im_mat.depth()){
        case CV_8UC1:
            this->n_bitplanes = 8;
            break;
    }
    
    #ifdef DEBUG2
        std::cout << +this->im_mat.cols << "x" << +this->im_mat.rows << std::endl;
        std::cout << +this->n_channels << " channels" << std::endl;
        std::cout << "Bit-depth of " << +this->n_bitplanes << std::endl;
        std::cout << std::endl;
    #endif
    
    #ifdef DEBUG1
        this->complexities.clear();
    #endif
    
    this->channel_n = 0;
    this->bitplane = cv::Mat::zeros(im_mat.rows, im_mat.cols, CV_8UC1); // Need to initialise for bitandshift
    this->load_next_channel();
    if (this->embedding)
        this->byteplane = cv::Mat::zeros(this->im_mat.rows, im_mat.cols, CV_8UC1);
}

int BPCSStreamBuf::set_next_grid(){
    if (++this->grids_since_conjgrid == 64){
        // First grid in every 64 is reserved for conjugation map
        // The next grid starts the next series of 64 complex grids, and should therefore be reserved to contain its conjugation map
        // The old such grid must have the conjugation map emptied into it
        
        for (uint_fast8_t k=1; k<64; ++k)
            this->grid.data[k] = conjugation_map[k];
        
        this->grid.data[0] = 0;
        
        if (this->get_grid_complexity(this->grid) < this->min_complexity){
            conjugate_grid(this->grid);
            this->grid.data[0] = 1;
            if (this->get_grid_complexity(this->grid) < this->min_complexity){
                throw std::runtime_error("Grid complexity fell below minimum value");
            }
        }
        
        this->set_next_grid();
        this->conjgrid_x = this->x;
        this->conjgrid_y = this->y;
        this->grids_since_conjgrid = 0;
    }
    float complexity;
    uint_fast32_t i = this->x;
    uint_fast32_t j = this->y;
    while (j <= this->im_mat.rows -8){
        while (i <= this->im_mat.cols -8){
            cv::Rect grid_shape(cv::Point(i, j), cv::Size(8, 8));
            
            this->bitplane(grid_shape).copyTo(this->grid);
            complexity = this->get_grid_complexity(this->grid);
            
            // TODO: Look into removing this unnecessary copy
            //complexity = this->get_grid_complexity(this->bitplane(grid_shape));
            
            #ifdef DEBUG1
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
    if (this->bitplane_n < this->n_bitplanes){
        this->load_next_bitplane();
        goto try_again;
    }
    
    // If we are here, we have exhausted the byteplane
    if (++this->channel_n < this->n_channels){
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
    return this->set_next_grid();
}



char BPCSStreamBuf::sgetc(){
    if (this->byteindx == 8){
        if (set_next_grid())
            throw std::runtime_error("Unexpected end of BPCS stream");
            //return std::char_traits<char>::eof;
        this->byteindx = 0;
    }
    char c = 0;
    for (uint_fast8_t i=0; i<8; ++i)
        c |= (this->grid.at<uint_fast8_t>(this->byteindx, i) << i);
    ++this->byteindx;
    return c;
}

char BPCSStreamBuf::sputc(char c){
    if (this->byteindx == 8){
        if (this->get_grid_complexity(this->grid) < this->min_complexity){
            // Commit grid
            conjugate_grid(this->grid);
            this->conjugation_map[this->grids_since_conjgrid] = 1;
        } else {
            this->conjugation_map[this->grids_since_conjgrid] = 0;
        }
        
        if (this->set_next_grid())
            throw std::runtime_error("Too much data to encode");
            //return std::char_traits<char>::eof;
        this->byteindx = 0;
    }
    for (uint_fast8_t i=0; i<8; ++i)
        this->grid.at<uint_fast8_t>(this->byteindx, i) = c >> i;
    ++this->byteindx;
    return 0;
}

void BPCSStreamBuf::save_im(){
    // Called either when we've exhausted this->im_mat's last channel, or by this->end()
    #ifdef TESTS
        assert(this->embedding);
    #endif
    this->commit_channel();
    std::regex_search(this->img_fps[this->img_n -1], this->path_regexp_match, path_regexp);
    std::string out_fp = format_out_fp(this->out_fmt, this->path_regexp_match);
    cv::merge(this->channel_byteplanes, this->im_mat);
    #ifdef DEBUG1
        std::cout << "Saving to  `" << out_fp << "`" << std::endl;
    #endif
    
    std::regex_search(out_fp, this->path_regexp_match, path_regexp);
    
    if (path_regexp_match[5] != "png"){
        #ifdef DEBUG1
            std::cerr << "Must be encoded with lossless format, not `" << path_regexp_match[5] << "`" << std::endl;
        #endif
        throw std::runtime_error("");
    }
    
    cv::imwrite(out_fp, this->im_mat);
}

void BPCSStreamBuf::end(){
    #ifdef DEBUG1
        print_histogram(complexities, 10, 200);
    #endif
    assert(this->embedding);
    this->unxor_bitplane();
    this->save_im();
}







uint_fast64_t get_fsize(const char* fp){
    struct stat stat_buf;
    if (stat(fp, &stat_buf) == -1){
        #ifdef DEBUG1
            std::cout << "No such file:  " << fp << std::endl;
        #endif
        throw std::runtime_error("");
    }
    return stat_buf.st_size;
}











int main(const int argc, char *argv[]){
    args::ArgumentParser parser("(En|De)code BPCS", "This goes after the options.");
    args::HelpFlag                      Ahelp           (parser, "help", "Display help", {'h', "help"});
    
    #ifdef DEBUG1
    // Histogram args
    args::ValueFlag<uint_fast8_t>       An_bins         (parser, "n_bins", "Number of histogram bins", {'B', "bins"});
    args::ValueFlag<uint_fast8_t>       An_binchars     (parser, "n_binchars", "Total number of `#` characters printed out in histogram totals", {"binchars"});
    #endif
    
    args::ValueFlag<std::string>        Aout_fmt        (parser, "out_fmt", "Format of output file path(s) - substitutions being fp, dir, fname, basename, ext. Sets mode to `extracting` if msg_fps not supplied.", {'o', "out"});
    args::Flag                          Ato_disk        (parser, "to_disk", "Write to disk (as opposed to psuedofile)", {'d', "to-disk"});
    args::Flag                          Adisplay_img    (parser, "display", "Display embedded images through OpenCV::imshow (rather than pipe)", {'D', "display"});
    
    args::ValueFlag<std::string>        Apipe_in        (parser, "pipe_in", "Path of named input pipe to listen to after having piped extracted message to to output pipe. Sets mode to `editing`", {'i', "pipe-in"});
    
    args::ValueFlagList<std::string>    Amsg_fps        (parser, "msg_fps", "File path(s) of message file(s) to embed. Sets mode to `embedding`", {'m', "msg"});
    
    // Encoding variables
    args::Flag                          Amin            (parser, "min", "Minimise image (i.e. crop image to the minimum rectangle holding the required number of complex grids", {"min"});
    
    args::Positional<float>             Amin_complexity (parser, "min_complexity", "Minimum bitplane complexity");
    args::PositionalList<std::string>   Aimg_fps        (parser, "img_fps", "File path(s) of input image file(s)");
    
    #ifdef DEBUG1
    try {
    #endif
        parser.ParseCLI(argc, argv);
    #ifdef DEBUG1
    } catch (const args::Completion& e) {
        std::cout << e.what();
        return 0;
    } catch (const args::Help&) {
        std::cout << parser;
        return 0;
    } catch (const args::ParseError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    #endif
    
    if (Amin && !Amsg_fps){
        #ifdef DEBUG1
            std::cerr << parser;
        #endif
        return 1;
    }
    
    if (sodium::sodium_init() == -1) {
        #ifdef DEBUG1
            std::cerr << "libsodium init fail" << std::endl;
        #endif
        return 1;
    }
    
    chequerboard_a = chequerboard(0, 8, 8);
    chequerboard_b = chequerboard(1, 8, 8);
    
    #ifdef DEBUG1
        uint_fast16_t n_bins;
        uint_fast8_t n_binchars;
        
        if (An_bins)
            n_bins = args::get(An_bins);
        else
            n_bins = 10;
        
        if (An_binchars)
            n_binchars = args::get(An_binchars);
        else
            n_binchars = 200;
    #endif
    
    uint_fast8_t mode;

    std::vector<std::string> msg_fps;
    const char* named_pipe_in;
    if (Amsg_fps){
        msg_fps = args::get(Amsg_fps);
        mode = MODE_EMBEDDING;
    //} else if (Amax_cap){
    //    mode = MODE_CALC_MAX_CAP;
    } else if (Apipe_in){
        mode = MODE_EDIT;
        named_pipe_in = args::get(Apipe_in).c_str();
    } else {
        mode = MODE_EXTRACT;
    }
    
    #ifdef DEBUG1
        switch(mode){
            case MODE_EMBEDDING:     std::cout << "Embedding"; break;
            case MODE_CALC_MAX_CAP:  std::cout << "Calculating max cap"; break;
            case MODE_EDIT:          std::cout << "Editing"; break;
            case MODE_EXTRACT:       std::cout << "Extracting"; break;
        }
    #endif
    
    std::string out_fmt;
    std::string out_fp;
    if (Aout_fmt){
        out_fmt = args::get(Aout_fmt);
        #ifdef DEBUG1
            if (!Amsg_fps){
                std::cout << " to ";
                if (Ato_disk)
                    std::cout << "file";
                else
                    std::cout << "display";
            }
        #endif
    } else {
        if (Amsg_fps){
            #ifdef DEBUG1
                std::cerr << "Must specify out_fmt when embedding" << std::endl;
            #endif
            return 1;
        }
        #ifdef DEBUG1
            else
                std::cout << " to display";
        #endif
        out_fmt = NULLCHAR;
    }
    #ifdef DEBUG1
        std::cout << std::endl;
    #endif
    
    std::smatch path_regexp_match;
    
    const float min_complexity = args::get(Amin_complexity);
    if (min_complexity > 0.5){
        #ifdef DEBUG1
            std::cerr << "Current implementation requires maximum minimum complexity of 0.5" << std::endl;
        #endif
        return 1;
    }
    
    BPCSStreamBuf bpcs_stream(min_complexity, args::get(Aimg_fps));
    bpcs_stream.embedding = (mode == MODE_EMBEDDING);
    bpcs_stream.out_fmt = out_fmt;
    bpcs_stream.load_next_img(); // Init
    
    uint_fast64_t i, j;
    std::string fp;
    uint_fast64_t n_msg_bytes;
    if (mode == MODE_EMBEDDING){
        FILE* msg_file;
        for (i=0; i<msg_fps.size(); ++i){
            fp = msg_fps[i];
            #ifdef DEBUG2
                std::cout << "Reading msg `" << fp << "`" << std::endl;
            #endif
            
            // At start, and between embedded messages, is a 64-bit integer telling us the size of the next embedded message
            // TODO: XOR this value with 64-bit integer, else the fact that the first 32+ bits will almost certainly be 0 will greatly help unwanted decryption
            
            // TODO: bpcs_stream << encryption << msg_file
            
            n_msg_bytes = sizeof(fp);
            for (j=0; j<8; ++j)
                bpcs_stream.sputc(n_msg_bytes >> j);
            //bpcs_stream.write(n_msg_bytes, 8);
            for (j=0; j<n_msg_bytes; ++j)
                bpcs_stream.sputc((char)fp[j] >> j);
            //bpcs_stream.write(fp, n_msg_bytes);
            
            n_msg_bytes = get_fsize(fp.c_str());
            for (j=0; j<8; ++j)
                bpcs_stream.sputc(n_msg_bytes >> j);
            //bpcs_stream.write(n_msg_bytes, 8);
            msg_file = fopen(fp.c_str(), "r");
            for (j=0; j<n_msg_bytes; ++j)
                // WARNING: Assumes there are exactly n_msg_bytes
                bpcs_stream.sputc(getc(msg_file) >> j);
            //bpcs_stream.write(msg_file, n_msg_bytes);
            fclose(msg_file);
        }
        // After all messages, signal end with signalled size of 0
        // TODO: XOR, as above
        for (j=0; j<8; ++j)
            bpcs_stream.sputc(0);
        bpcs_stream.end();
    } else {
        i = 0;
        do {
            n_msg_bytes = 0;
            for (j=0; j<8; ++j){
                n_msg_bytes |= (bpcs_stream.sgetc() << j);
            }
            
            if (i & 1){
                // First block of data is original file path, second is file contents
                if (mode == MODE_EXTRACT && Aout_fmt){
                    std::ofstream of(fp.c_str());
                    for (j=0; j<n_msg_bytes; ++j)
                        of.put(bpcs_stream.sgetc());
                    of.close();
                } else {
                    for (j=0; j<n_msg_bytes; ++j){
                    }
                }
            } else {
                for (j=0; j<n_msg_bytes; ++j){
                    fp += bpcs_stream.sgetc();
                }
                std::regex_search(fp, path_regexp_match, path_regexp);
                fp = format_out_fp(out_fmt, path_regexp_match);
            }
            
            ++i;
        } while (n_msg_bytes != 0);
    }
}
