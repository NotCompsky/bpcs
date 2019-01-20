#include <args.hxx>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp> // for calcHist
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
#ifdef DEBUG1
    #include <iostream>
#endif
#include <fstream>
#include <functional> // for std::function
#include <sys/stat.h> // for stat
#include <map> // for std::map
#include <regex> // for std::basic_regex
#include <libavcodec/avcodec.h>
extern "C" {
    #include <libavutil/avutil.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
}


const std::regex path_regexp("^((.*)/)?(([^/]+)[.]([^./]+))$");
// Groups are full_match, optional, parent_dir, filename, basename, ext
const std::regex fmt_fp("[{]fp[}]");
const std::regex fmt_dir("[{]dir[}]");
const std::regex fmt_fname("[{]fname[}]");
const std::regex fmt_basename("[{]basename[}]");
const std::regex fmt_ext("[{]ext[}]");
// WARNING: Better would be to ignore `{{fp}}` (escape it to a literal `{fp}`) with (^|[^{]), but no personal use doing so.


#ifdef DEBUG5
void print_vector(std::string name, std::vector<uint_fast8_t> &v, uint_fast64_t from, uint_fast64_t to){
    std::cout << name << ":  ";
    for (uint_fast64_t i=from; i!=to; ++i)
        std::cout << v[i];
    std::cout << std::endl;
}
#endif

std::string format_out_fp(std::string out_fmt, std::smatch path_regexp_match){
    return std::regex_replace(std::regex_replace(std::regex_replace(std::regex_replace(std::regex_replace(out_fmt, fmt_fp, (std::string)path_regexp_match[0]), fmt_dir, (std::string)path_regexp_match[2]), fmt_fname, (std::string)path_regexp_match[3]), fmt_basename, (std::string)path_regexp_match[4]), fmt_ext, (std::string)path_regexp_match[4]);
}


long unsigned int get_fsize(const char* fp){
    struct stat stat_buf;
    int rc = stat(fp, &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void add_msg_bits(std::vector<uint_fast8_t> &msg, uint_fast8_t c){
    msg.push_back(c & 1);
    msg.push_back((c & 2) >> 1);
    msg.push_back((c & 4) >> 2);
    msg.push_back((c & 8) >> 3);
    msg.push_back((c & 16) >> 4);
    msg.push_back((c & 32) >> 5);
    msg.push_back((c & 64) >> 6);
    msg.push_back((c & 128) >> 7);
}

uint_fast8_t add_bytes_to_msg(std::vector<uint_fast8_t> &msg, std::vector<uint_fast8_t> bytes){
    std::map<uint_fast8_t, uint_fast32_t> byte_freq;
    
    for (auto &vi : bytes)
        // access arbitrary elements in bytes vector - we don't care about the ordering
        ++byte_freq[vi];
    
    uint_fast32_t min1 = 4294967296; // 2**32 - 1
    uint_fast32_t min2 = 4294967296;
    
    uint_fast8_t esc_byte;
    uint_fast8_t end_byte;
    
    uint_fast32_t freq;
    
    uint_fast8_t i=0;
    do {
        freq = byte_freq[i];
        if (freq < min2){
            if (freq < min1){
                min1 = freq;
                esc_byte = i;
            } else {
                min2 = freq;
                end_byte = i;
            }
        }
    } while (++i != 0);
    
    #ifdef DEBUG4
        std::cout << "esc_byte " << +esc_byte << ", end_byte " << +end_byte << std::endl;
    #endif
    
    // Signal what the end and escape byte values are
    add_msg_bits(msg, esc_byte);
    add_msg_bits(msg, end_byte);
    
    uint_fast64_t n_bytes = bytes.size();
    uint_fast8_t byte;
    for (int j=0; j<n_bytes; ++j){
        byte = bytes[j];
        if (byte == esc_byte || byte == end_byte){
            add_msg_bits(msg, esc_byte);
            #ifdef DEBUG7
                std::cout << "[" << +j << "] escaped byte  ==  " << +byte << std::endl;
            #endif
        }
        add_msg_bits(msg, byte);
    }
    
    add_msg_bits(msg, end_byte);
    // Signal end of data stream
    
    
    return end_byte;
}

uint_fast8_t add_msgfile_bits(std::vector<uint_fast8_t> &msg, std::string fp){
    std::vector<uint_fast8_t> fp_as_vector(fp.begin(), fp.end());
    uint_fast8_t end_byte;

    end_byte = add_bytes_to_msg(msg, fp_as_vector);
    
    add_msg_bits(msg, end_byte+1);
    // Add junk bit to ensure that there aren't two consecutive `end_byte`s (which would signal end of all data streams)
    
    
    const char* fp_c_str = fp.c_str();
    // TODO: Improve performance via something like mmap
    uint_fast64_t fsize = get_fsize(fp_c_str);
    msg.reserve(fsize << 3);
    
    std::vector<uint_fast8_t> bytes;
    bytes.reserve(fsize);
    
    std::ifstream msg_file(fp_c_str, std::ios::binary);
    
    char c;
    while (msg_file.get(c))
        bytes.push_back((unsigned char)c);
    
    return add_bytes_to_msg(msg, bytes);
    // returns the trailing end_byte
}

cv::Mat bitshifted_down(cv::Mat &arr, unsigned int w, unsigned int h){
    cv::Mat dest = cv::Mat(h, w, CV_8UC1);
    unsigned int i;
    unsigned int j;
    for (j=0; j<h; ++j)
        for (i=0; i<w; ++i)
            dest.at<uint_fast8_t>(j,i) = arr.at<uint_fast8_t>(j,i) >> 1;
    return dest;
}

unsigned int xor_adj(cv::Mat &arr, unsigned int w, unsigned int h){
    unsigned int sum = 0;
    
    for (int j=0; j<h; ++j)
        for (int i=1; i<w; ++i)
            sum += arr.at<uint_fast8_t>(j,i) ^ arr.at<uint_fast8_t>(j,i-1);
    for (int i=0; i<w; ++i)
        for (int j=1; j<h; ++j)
            sum += arr.at<uint_fast8_t>(j,i) ^ arr.at<uint_fast8_t>(j-1,i);
    
    return sum;
}

void bitandshift(cv::Mat &arr, cv::Mat &dest, unsigned int w, unsigned int h, unsigned int n){
    for (int i=0; i<w; ++i)
        for (int j=0; j<h; ++j)
            dest.at<uint_fast8_t>(j,i) = (arr.at<uint_fast8_t>(j,i) >> n) & 1;
}

void bitshift_up(cv::Mat &arr, unsigned int w, unsigned int h, unsigned int n){
    for (int i=0; i<w; ++i)
        for (int j=0; j<h; ++j)
            arr.at<uint_fast8_t>(j,i) = arr.at<uint_fast8_t>(j,i) << n;
}

#ifdef DEBUG1
void print_cv_arr(const char* name, int i, cv::Mat &arr){
    std::cout << name << i << std::endl << arr << std::endl << std::endl;
}
#endif

cv::Mat chequerboard(unsigned int indx, unsigned int w, unsigned int h){
    // indx should be 0 or 1
    cv::Mat arr = cv::Mat(h, w, CV_8UC1);
    for (int i=0; i<w; ++i)
        for (int j=0; j<h; ++j){
            arr.at<uint_fast8_t>(j, i) = ((i ^ j) ^ indx) & 1;
        }
    return arr;
}

cv::Mat chequerboard_a;
cv::Mat chequerboard_b;

void conjugate_grid(cv::Mat &grid){
    cv::Mat arr1;
    cv::Mat arr2;
    
    cv::bitwise_and(grid, chequerboard_a, arr1);
    
    cv::bitwise_not(grid, grid);
    cv::bitwise_and(grid, chequerboard_b, arr2);
    
    cv::bitwise_or(arr1, arr2, grid);
}

int decode_grid(cv::Mat &bitplane, cv::Rect &grid_shape, const float min_complexity, cv::Mat &grid, unsigned int grid_w, unsigned int grid_h, std::vector<uint_fast8_t> &msg){
    // Pass reference to msg, else a copy is passed (and changes are not kept)
    
    if (grid.at<uint_fast8_t>(0,0) == 1)
        conjugate_grid(grid);
    
    bool not_encountered_first_el = true;
    for (int j=0; j<grid_h; ++j)
        for (int i=0; i<grid_w; ++i){
            if (not_encountered_first_el){
                // Do not add conjugation status bit to msg
                not_encountered_first_el = false;
                continue;
            }
            msg.push_back(grid.at<uint_fast8_t>(j, i));
        }
    
    return 1;
}

float grid_complexity(cv::Mat &grid, unsigned int grid_w, unsigned int grid_h){
    #ifdef TESTS
        float value = 
    #else
        return 
    #endif
    (float)xor_adj(grid, grid_w, grid_h) / (float)(grid_w * (grid_h -1) + grid_h * (grid_w -1));
    #ifdef TESTS
        if (value < 0 || value > 1){
            #ifdef DEBUG1
                std::cerr << "Complexity of `" << +value << "` out of bounds for " << +grid_w << "x" << +grid_h << " grid" << std::endl;
                std::cout << grid << std::endl;
            #endif
            throw std::runtime_error("");
        }
        return value;
    #endif
}

int encode_grid(cv::Mat &bitplane, cv::Rect &grid_shape, const float min_complexity, cv::Mat &grid, unsigned int grid_w, unsigned int grid_h, std::vector<uint_fast8_t> &msg){
    // Pass reference to msg, else a copy is passed (and changes are not kept)
    
    long unsigned msg_size = msg.size();
    
    if (msg_size == 0)
        // We've successfully embedded the entirety of the message bits into the image grids
        // Note that we can only guarantee that the msg will either entirely fit or have 0 length if we ensure its length is divisible by grid_w*grid_h earlier, after initialising it.
        return 0;
    
    /*
    if (msg.size() < grid_h * grid_w){
        // SHOULD NOT HAPPEN - should ensure right number of bits from the beginning
        std::cerr << "E: Not enough msg bits: " << msg.size() << " < " << grid_h * grid_w << std::endl;
        return 1;
    }
    If the above condition is met, ill reult in stack error
    */
    
    bool not_encountered_first_el = true;
    int index = -1;
    
    for (int j=0; j<grid_h; ++j){
        for (int i=0; i<grid_w; ++i){
            if (not_encountered_first_el){
                not_encountered_first_el = false;
                continue;
                // First bit (at (0,0)) of grid is reserved for conjugation status
            }
            grid.at<uint_fast8_t>(j, i) = msg[++index];
        }
    }
    
    msg.erase(std::begin(msg), std::begin(msg) + index + 1);
    
    float old_complexity = grid_complexity(grid, grid_w, grid_h);
    float new_complexity;
    bool to_conjugate = true;
    
    if (old_complexity >= min_complexity){
        grid.at<uint_fast8_t>(0,0) = 0;
        new_complexity = grid_complexity(grid, grid_w, grid_h);
        if (new_complexity >= min_complexity)
            to_conjugate = false;
    }
    
    if (to_conjugate){
        conjugate_grid(grid);
        grid.at<uint_fast8_t>(0,0) = 1;
        new_complexity = grid_complexity(grid, grid_w, grid_h);
        if (new_complexity < min_complexity){
            #ifdef DEBUG1
                std::cout << "Grid complexity fell below minimum value; from " << old_complexity << " to " << new_complexity << std::endl;
            #endif
            throw std::runtime_error("");
        }
    }
    
    grid.copyTo(bitplane(grid_shape));
    
    // In other words, the first bit of each message grid tells us whether it was conjugated
    
    return 1;
}

#ifdef DEBUG3
void iterate_over_bitgrids__msg(const char* msg, uint_fast32_t n_grids_so_far, uint_fast32_t n_grids_used, uint_fast32_t n_grids_total, float min_complexity, uint_fast64_t msg_size){
    std::cout << msg << ", at state:  " << n_grids_so_far << " of " << n_grids_total << " grids\t" << n_grids_used << " with complexity >= " << min_complexity;
    if (msg_size != 0)
        std::cout << "\tmsg size " << msg_size;
    std::cout << std::endl;
}
#endif

int iterate_over_bitgrids(std::vector<float> &complexities, cv::Mat &bitplane, float min_complexity, unsigned int n_hztl_grids, unsigned int n_vert_grids, unsigned int bitplane_w, unsigned int bitplane_h, unsigned int grid_w, unsigned int grid_h, std::function<int(cv::Mat&, cv::Rect&, const float, cv::Mat&, unsigned int, unsigned int, std::vector<uint_fast8_t>&)> grid_fnct, std::vector<uint_fast8_t> &msg){
    // Pass reference to complexities, else a copy is passed (and changes are not kept)
    // Note that we will be doing millions of operations, and do not mind rounding errors - the important thing here is that we get consistent results. Hence we use float not double
    cv::Mat grid;
    uint_fast32_t n_grids_used = 0;
    float complexity;
    int grid_fnct_status;
    #ifdef DEBUG3
        uint_fast64_t msg_size = 0;
        uint_fast32_t n_grids_so_far = 0;
        uint_fast32_t n_grids_total  = n_hztl_grids * n_vert_grids;
    #endif
    for (int i=0; i<n_hztl_grids; ++i){
        for (int j=0; j<n_vert_grids; ++j){
            cv::Rect grid_shape(cv::Point(i*grid_w, j*grid_h), cv::Size(grid_w, grid_h));
            // cv::Rect, cv::Point, cv::Size are all column-major, i.e. (x, y) or (width, height)
            bitplane(grid_shape).copyTo(grid);
            complexity = grid_complexity(grid, grid_w, grid_h);
            
            #ifdef DEBUG1
                complexities.push_back(complexity);
            #endif
            
            if (complexity < min_complexity)
                continue;
            
            grid_fnct_status = grid_fnct(bitplane, grid_shape, min_complexity, grid, grid_w, grid_h, msg);
            
            if (grid_fnct_status == 0){
                #ifdef DEBUG3
                    iterate_over_bitgrids__msg("msg exhausted", n_grids_so_far, n_grids_used, n_grids_total, min_complexity, 0);
                #endif
                return 0;
            }
            
            ++n_grids_used;
        }
        #ifdef DEBUG3
            n_grids_so_far += n_vert_grids;
        #endif
        #ifdef DEBUG4
            if (i % 10 == 0 || n_hztl_grids < 11 || msg_size < 20000){
                msg_size = msg.size();
                iterate_over_bitgrids__msg("debug", n_grids_so_far, n_grids_used, n_grids_total, min_complexity, msg_size);
            }
        #endif
    }
    #ifdef DEBUG3
        iterate_over_bitgrids__msg("bitplane exhausted", n_grids_so_far, n_grids_used, n_grids_total, min_complexity, msg.size());
    #endif
    return 1;
}

#ifdef DEBUG1
void print_histogram(std::vector<float> &complexities, unsigned int n_bins, unsigned int n_binchars){
    uint_fast32_t len_complexities;
    float min;
    float max;
    float step;
    float bin_max;
    unsigned int bin_total;
    
    std::sort(std::begin(complexities), std::end(complexities));
    len_complexities = complexities.size();
    
    std::cout << std::endl;
    
    assert(("len_complexities must be positive", len_complexities != 0));
    
    std::cout << "Complexities Histogram" << std::endl;
    min = *std::min_element(std::begin(complexities), std::end(complexities));
    max = *std::max_element(std::begin(complexities), std::end(complexities));
    std::cout << "Total: " << len_complexities << " between " << min << ", " << max << std::endl;
    step = (max - min) / (float)n_bins;
    std::cout << "Bins:  " << +n_bins << " with step " << step << std::endl;
    std::vector<unsigned int> bin_totals;
    bin_max = step;
    bin_total = 0;
    
    for (uint_fast32_t j=0; j<len_complexities; ++j){
        while (complexities[j] > bin_max){
            bin_totals.push_back(bin_total);
            bin_max += step;
            bin_total = 0;
        }
        ++bin_total;
    }
    bin_totals.push_back(bin_total);
    
    for (int j=0; j<n_bins; ++j){
        bin_total = n_binchars * bin_totals[j] / len_complexities;
        std::cout << j * step << ": " << bin_totals[j] << std::endl << "   ";
        for (int k=0; k<bin_total; ++k)
            std::cout << "#";
        std::cout << std::endl;
    }
    
    std::cout << n_bins * step << std::endl;
    std::cout << std::endl << std::endl;
}
#endif

void convert_to_cgc(cv::Mat &arr, unsigned int w, unsigned int h, cv::Mat &dest){
    cv::bitwise_xor(arr, bitshifted_down(arr, w, h), dest);
}

uint_fast8_t get_byte_from(std::vector<uint_fast8_t> &msg, uint_fast64_t indx, const char* name){
    uint_fast8_t byte = 0;
    uint_fast64_t endx = indx + 8;
    uint_fast8_t shift = 0;
    for (uint_fast64_t j=indx; j!=endx; ++j){
        byte |= msg[j] << shift++;
    }
    #ifdef DEBUG5
        if (name != "")
            std::cout << "[" << +(indx >> 3) << "] " << name << "  ==  " << +byte << std::endl;
    #endif
    return byte;
}


void vector2file(uint_fast8_t *extracted_msg_pointer, uint_fast64_t n_bytes, std::string name, std::string fp){
    std::ofstream outfile;
    #ifdef DEBUG7
        std::cout << "Written " << name << " to " << fp << std::endl;
    #endif
    outfile.open(fp, std::ios::out | std::ios::binary);
    outfile.write((char*)extracted_msg_pointer, n_bytes);
    outfile.close();
}


int main(const int argc, char *argv[]){
    args::ArgumentParser parser("(En|De)code BPCS", "This goes after the options.");
    args::HelpFlag                      Ahelp           (parser, "help", "Display help", {'h', "help"});
    
    // Image format args
    // TODO: automatically detect these settings per image file
    args::ValueFlag<uint_fast8_t>       An_bits         (parser, "n_bits", "n_bits", {'b', "bits"});
    
    #ifdef DEBUG1
    // Histogram args
    args::ValueFlag<uint_fast8_t>       An_bins         (parser, "n_bins", "Number of histogram bins", {'B', "bins"});
    args::ValueFlag<uint_fast8_t>       An_binchars     (parser, "n_binchars", "Total number of `#` characters printed out in histogram totals", {"binchars"});
    #endif
    
    // Debugging args
    args::Flag                          Amsg_empty      (parser, "msg_empty", "Embed a msg of 0 bytes", {"msg-empty"});
    
    args::ValueFlag<std::string>        Aout_fmt        (parser, "out_fmt", "Format of output file path(s) - substitutions being fp, dir, fname, basename, ext. Sets mode to `extracting` if msg_fps not supplied.", {'o', "out"});
    
    args::ValueFlagList<std::string>    Amsg_fps        (parser, "msg_fps", "File path(s) of message file(s) to embed. Sets mode to `embedding`", {'m', "msg"});
    
    // Encoding variables
    args::ValueFlag<uint_fast16_t>      Agrid_w         (parser, "grid_w", "Width of individual complexity grids", {'w', "gridw"});
    args::ValueFlag<uint_fast16_t>      Agrid_h         (parser, "grid_h", "Height of individual complexity grids", {'h', "gridh"});
    
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
    
    uint_fast16_t grid_w;
    if (Agrid_w)
        grid_w = args::get(Agrid_w);
    else
        grid_w = 8;
    
    uint_fast16_t grid_h;
    if (Agrid_h)
        grid_h = args::get(Agrid_h);
    else
        grid_h = 8;
    
    chequerboard_a = chequerboard(0, grid_w, grid_h);
    chequerboard_b = chequerboard(1, grid_w, grid_h);
    
    uint_fast8_t n_bits;
    if (An_bits)
        n_bits = args::get(An_bits);
    else
        n_bits = 8;
    
    uint_fast8_t n_channels;
    
    #ifdef DEBUG1
    uint_fast8_t n_bins;
    if (An_bins)
        n_bins = args::get(An_bins);
    else
        n_bins = 10;
    
    uint_fast8_t n_binchars;
    if (An_binchars)
        n_binchars = args::get(An_binchars);
    else
        n_binchars = 200;
    #endif
    
    std::string out_fmt;
    std::string out_fp;
    bool save_extracted;
    if (Aout_fmt){
        out_fmt = args::get(Aout_fmt);
        save_extracted = true;
    } else {
        save_extracted = false;
        out_fmt = "%(fp)s";
    }
    
    std::string mode;
    std::vector<std::string> msg_fps;
    std::function<int(cv::Mat&, cv::Rect&, const float, cv::Mat&, unsigned int, unsigned int, std::vector<uint_fast8_t>&)> grid_fnct;
    
    bool encoding;
    if (Amsg_empty){
        encoding = true;
        mode = "Encoding blank";
        grid_fnct = encode_grid;
    } else if (Amsg_fps){
        encoding = true;
        msg_fps = args::get(Amsg_fps);
        mode = "Encoding";
        grid_fnct = encode_grid;
    } else {
        encoding = false;
        mode = "Decoding";
        grid_fnct = decode_grid;
    }
    
    cv::Mat im_mat;
    cv::Mat XORed_byteplane;
    cv::Mat bitplane;
    cv::Mat prev_bitplane;
    
    unsigned int w;
    unsigned int h;
    
    unsigned int n_hztl_grids;
    unsigned int n_vert_grids;
    
    std::vector<std::string>img_fps = args::get(Aimg_fps);
    unsigned int img_fps_len        = img_fps.size();
    
    const float min_complexity = args::get(Amin_complexity);
    if (min_complexity > 0.5){
        #ifdef DEBUG1
            std::cerr << "Current implementation requires maximum minimum complexity of 0.5" << std::endl;
        #endif
        throw std::runtime_error("");
    }
    
    #ifdef DEBUG1
        std::cout << mode << " " << +img_fps_len << " img inputs, using: Complexity >= " <<  +min_complexity << ", " << +n_channels << " channels, " << +n_bits << " bits" << std::endl;
        // Preceding primitive data type with `+` operator makes it print ASCII character representing that numerical value, rather than the ASCII character of that value
    #endif
    
    std::vector<uint_fast8_t> msg;
    
    int i;
    
    uint_fast8_t escape_byte;
    uint_fast8_t end_byte;
    
    if (encoding){
        // i.e. if mode==encoding
        unsigned int msg_fps_len = msg_fps.size();
        
        if (Amsg_fps){
            i = 0;
            do {
                end_byte = add_msgfile_bits(msg, msg_fps[i]);
                // TODO: Read input files one by one, rather than all at once. Reduces memory requirement and possibly disk usage too in cases of debugging and errors.
                if (++i < msg_fps_len)
                    add_msg_bits(msg, end_byte+1);
                    // Ensure that we do not accidentally signal the end of all embedded data (which would happen if the next embedded byte happened to be same value as end_byte)
                else {
                    add_msg_bits(msg, end_byte);
                    // Signals the end of the entire stream
                    break;
                }
            } while (true);
        }
        // else we are encoding a msg of 0 bytes
        
        const unsigned int bits_encoded_per_grid = grid_w * grid_h -1;
        uint_fast64_t msg_size = msg.size();
        
        
        int diff = (int)msg_size % bits_encoded_per_grid;
        if (diff != 0){
            diff = bits_encoded_per_grid - diff;
            
            i = 0;
            do {
                msg.push_back(i & 1);
                // Alternate the junk bit so that we get a chequered style, a cheap pattern that is likely to result in a high complexity (which is what we desire)
            } while (++i != diff);
        }
        
        #ifdef DEBUG1
            std::cout << msg_size << "b to encode" << std::endl;
        #endif
        #ifdef DEBUG7
            std::vector<uint_fast8_t> msg_as_bytes;
            msg_as_bytes.reserve(msg_size >> 3);
        #endif
        #ifdef DEBUG4
            uint_fast8_t byte = 0;
            uint_fast8_t shift = 0;
            for (i=0; i<msg_size; ++i){
                byte |= msg[i] << shift++;
                if (shift == 8){
                    #ifdef DEBUG7
                        msg_as_bytes.push_back(byte);
                    #endif
                    byte = 0;
                    shift = 0;
                }
            }
            std::cout << std::endl;
            #ifdef DEBUG7
                uint_fast8_t* msg_pointer = &msg_as_bytes[0];
                vector2file(msg_pointer, msg_size >> 3, "msg_as_bytes", "/tmp/bpcs.encoded.msg.txt");
            #endif
            if (shift != 0){
                std::cout << "Encoded a non-byte number of bits: " << +shift << " extra bits" << std::endl;
                throw std::invalid_argument("");
            }
        #endif
    }
    
    bool msg_was_exhausted = false;
    
    int j;
    
    int iterate_over_bitgrids__result;
    
    std::smatch path_regexp_match;
    
    std::string ext;
    
    std::string fp;
    
    for (i=0; i<img_fps_len; ++i){
        fp = img_fps[i];
        im_mat = cv::imread(fp, CV_LOAD_IMAGE_COLOR);
        // WARNING: OpenCV loads images as BGR, not RGB
        
        if (im_mat.data == NULL){
            #ifdef DEBUG1
                std::cerr << "Cannot load image data from:  " << fp << std::endl;
            #endif
            throw std::invalid_argument(fp);
        }
        
        n_channels = im_mat.channels();
        
        w = im_mat.cols;
        h = im_mat.rows;
        
        // NOTE: OpenCV treats matrices in row-major order - i.e. accessed as (row_id, column_id). For images, this is (y, x).
        
        #ifdef DEBUG1
            std::cout << +w << "x" << +h << "\t" << +n_channels << " channels" << std::endl;
        #endif
        
        n_hztl_grids = w / grid_w;
        n_vert_grids = h / grid_h;
        
        bitplane = cv::Mat(h, w, CV_8UC1);
        // CV_8UC1 - aka CV_8UC(1) - means 1 channel of uint8
        
        std::vector<cv::Mat> channel_byteplanes;
        cv::split(im_mat.clone(), channel_byteplanes);
        
        std::vector<float> complexities;
        complexities.reserve(n_hztl_grids * n_vert_grids);
        // Speeds up assignments by reserving required memory beforehand
        
        for (j=0; j<n_channels; ++j){
            convert_to_cgc(channel_byteplanes[j], w, h, XORed_byteplane);
            // Bitshifting up ensures that the last bits of (arr >> 1) are 0 - so the last digit of this CGC'd (or XOR'd-ish) arr is retained
            
            cv::Mat byteplane = cv::Mat::zeros(h, w, CV_8UC1);
            
            for (uint_fast8_t k=0; k<n_bits; ++k){
                bitandshift(XORed_byteplane, bitplane, w, h, n_bits-k);
                if (!msg_was_exhausted){
                    iterate_over_bitgrids__result = iterate_over_bitgrids(complexities, bitplane, min_complexity, n_hztl_grids, n_vert_grids, w, h, grid_w, grid_h, grid_fnct, msg);
                    
                    if (iterate_over_bitgrids__result == 0)
                        msg_was_exhausted = true;
                }
                if (encoding){
                    cv::Mat unXORed_bitplane;
                    // TODO: We probably don't need to define new matrices here... but using src as the dest matrix seemed to cause issues (returned 0)
                    
                    if (k == 0)
                        unXORed_bitplane = bitplane.clone();
                    else
                        // Revert conversion of non-first planes to CGC
                        cv::bitwise_xor(bitplane, prev_bitplane, unXORed_bitplane);
                    
                    prev_bitplane = unXORed_bitplane.clone();
                    // WARNING: MUST BE DEEP COPY!
                    
                    bitshift_up(unXORed_bitplane, w, h, n_bits-k);
                    
                    cv::bitwise_or(byteplane, unXORed_bitplane, byteplane);
                }
            }
            if (encoding)
                // i.e. if (mode == "Encoding")
                channel_byteplanes[j] = byteplane;
            if (msg_was_exhausted)
                goto msg_exhausted;
        }
        
        msg_exhausted:
        
        if (encoding){
            // i.e. if (mode == "Encoding")
            std::regex_search(fp, path_regexp_match, path_regexp);
            out_fp = format_out_fp(out_fmt, path_regexp_match);
            
            cv::merge(channel_byteplanes, im_mat);
            #ifdef DEBUG1
                std::cout << "Saving to:  " << out_fp << std::endl;
            #endif
            
            std::regex_search(out_fp, path_regexp_match, path_regexp);
            
            std::string out_ext = path_regexp_match[5];
            
            if (out_ext != "png"){
                #ifdef DEBUG1
                    std::cerr << "Must be encoded with lossless format, not `" << out_ext << "`" << std::endl;
                #endif
                return 1;
            }
            
            cv::imwrite(out_fp, im_mat);
        } else {
            uint_fast64_t n_msg_bits = msg.size();
            uint_fast8_t shift = 0;
            uint_fast8_t byte = 0;
            uint_fast8_t either_endbyte_or_junk;
            #ifdef DEBUG1
                std::cout << "Extracted message" << std::endl;
            #endif
            escape_byte = get_byte_from(msg, 0, "esc_byte");
            end_byte = get_byte_from(msg, 8, "end_byte");
            std::vector<uint_fast8_t> extracted_msg;
            extracted_msg.reserve(n_msg_bits >> 3);
            std::vector<uint_fast64_t> stream_starts = {0};
            // NOTE: Also indicates index of the start of all data streams - which should be 3 bytes along from the end of the last data stream. Also indicates the index of the end of all embedded data - given as 3 bytes along from the end of the final data stream. Also does not contain `0` (or -2, which it would be to be consistent with the above) as it is implied that embedded streams begin from 0.
            uint_fast8_t* extracted_msg_pointer;
            j = 16;
            uint_fast64_t index = 0;
            do {
                byte |= msg[j++] << shift++;
                if (shift == 8){
                    if (byte == escape_byte) {
                        extracted_msg.push_back(get_byte_from(msg, j, ""));
                        #ifdef DEBUG7
                            byte = get_byte_from(msg, j, "escaped byte");
                            if (byte != escape_byte && byte != end_byte){
                                std::cerr << "[" << +j << "] Byte `" << +byte << "` was escaped, but is neither the esc_byte `" << +escape_byte << "` nor end_byte `" << +end_byte << "`." << std::endl;
                                return 1;
                            }
                        #endif
                        j += 8;
                    } else if (byte == end_byte) {
                        either_endbyte_or_junk = get_byte_from(msg, j, "end_byte or junk");
                        #ifdef DEBUG3
                            std::cout << "[" << +j << "] End of an embedded data stream" << std::endl;
                            #ifdef DEBUG5
                                if (stream_starts.size() & 1)
                                    // Only print first (metadata) of every pair
                                    print_vector("Extracted stream", extracted_msg, stream_starts[stream_starts.size()-1], index);
                            #endif
                        #endif
                        stream_starts.push_back(index);
                        if (either_endbyte_or_junk == end_byte){
                            #ifdef DEBUG3
                                std::cout << "[" << +j << "] End of embedded data" << std::endl;
                            #endif
                            goto print_extracted_msg;
                        } else if (either_endbyte_or_junk != end_byte+1){
                            #ifdef DEBUG1
                                std::cerr << "[" << +j << "] Unescaped end_byte (" << +end_byte << ") was followed by a byte (" << +either_endbyte_or_junk << ") that was neither another end_byte (signalling end of embedded data) nor junk byte (end_byte + 1 == " << +(end_byte +1) << ")" << std::endl;
                            #endif
                            goto goto_print_histogram;
                        }
                        j += 8;
                        // Discard the previously calculated byte
                        escape_byte = get_byte_from(msg, j, "esc_byte");
                        j += 8;
                        end_byte    = get_byte_from(msg, j, "end_byte");
                        j += 8;
                    } else {
                        extracted_msg.push_back(byte);
                        // Should be no need to set n_consecutive_ends = 0, as an unescaped end_byte should never appear in the data stream
                    }
                    shift = 0;
                    byte = 0;
                    ++index;
                }
            } while (true);
            
            print_extracted_msg:
            int n_extracted_msgs = stream_starts.size() -1;
            #ifdef DEBUG3
                std::cout << "[" << +j << "] " << +n_extracted_msgs << " data streams extracted" << std::endl;
            #endif
            #ifdef DEBUG7
                std::cout << "Data stream starts: ";
                for (j=0; j!=n_extracted_msgs+1; ++j)
                    std::cout << +stream_starts[j] << " ";
                std::cout << std::endl;
            #endif
            uint_fast64_t n_extracted_msg_bytes;
            
            uint_fast64_t k;
            
            uint_fast64_t next_index;
            
            for (j=0; j<n_extracted_msgs; ++j){
                index = stream_starts[j];
                next_index = stream_starts[j+1];
                n_extracted_msg_bytes = next_index - index;
                if ((j & 1) == 0){
                    // This stream is the file metadata (filename)
                    fp = "";
                    for (k=index; k!=next_index; k++)
                        fp += extracted_msg[k];
                    
                    if (!std::regex_search(fp, path_regexp_match, path_regexp)){
                        // This also assigns captured groups to path_regexp_match
                        #ifdef DEBUG1
                            std::cerr << "Filename did not match regexp:  " << fp << std::endl;
                        #endif
                        throw std::invalid_argument(fp);
                    }
                    
                    out_fp = format_out_fp(out_fmt, path_regexp_match);
                    ext = (std::string)path_regexp_match[5];
                } else {
                    // This stream is the file contents
                    
                    if (save_extracted){
                        #ifdef DEBUG1
                            std::cout << "Saving to: " << out_fp << std::endl;
                        #endif
                            
                    } else {
                        #ifdef DEBUG1
                            std::cout << "Reading `" << ext << "` data stream originating from: " << fp << std::endl;
                        #endif
                        
                        extracted_msg_pointer = &extracted_msg[index];
                        
                        if (ext == "jpg" || ext == "png" || ext == "jpeg" || ext == "bmp"){
                            cv::Mat rawdata(1, n_extracted_msg_bytes, CV_8UC1, extracted_msg_pointer);
                            cv::Mat decoded_img = cv::imdecode(rawdata, CV_LOAD_IMAGE_UNCHANGED);
                            if (decoded_img.data == NULL){
                                #ifdef DEBUG1
                                    std::cerr << "No image data loaded from " << n_extracted_msg_bytes << "B data stream claiming to originate from file `" << fp << "`. Saved extracted bytes to `/tmp/bpcs.msg.txt` for debugging." << std::endl;
                                #endif
                                vector2file(extracted_msg_pointer, n_extracted_msg_bytes, "extracted_msg", "/tmp/bpcs.msg.txt");
                                throw std::invalid_argument(fp);
                            }
                            #ifdef DEBUG3
                                std::cout << "Displaying image" << std::endl;
                            #endif
                            cv::imshow(fp, decoded_img);
                            cv::waitKey(0);
                        }
                    }
                }
            }
        }
        
        goto_print_histogram:
        #ifdef DEBUG1
            print_histogram(complexities, n_bins, n_binchars);
        #endif
        if (msg_was_exhausted)
            goto exit;
    }
    exit:
    return 0;
}
