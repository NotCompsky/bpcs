#include <args.hxx>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#ifdef DEBUG1
    #include <opencv2/imgproc/imgproc.hpp> // for calcHist
#endif
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
#include <functional> // for std::function
#include <sys/stat.h> // for stat
#include <map> // for std::map
#include <regex> // for std::basic_regex

#include <fcntl.h>    // For O_WRONLY
#include <unistd.h>   // For open()
#include <tuple> // for std::tie
#include <cstdio> // for std::remove

#include <cryptopp/cryptlib.h> // for (en|de)cryption

/*
Example usage (here $a = 0.45)
    A:
        PIPE_FP=/tmp/bpcs.pipe
        ./bpcs "$a" /tmp/img.jpg -m /tmp/msg.txt -o '/tmp/bpcs.png'
        (typed-piper "$PIPE_FP")& ./bpcs "$a" /tmp/bpcs.png -o "$PIPE_FP"
    B:
        PIPE_FP=/tmp/bpcs.pipe
        ./bpcs "$a" /tmp/img.jpg -m /tmp/msg.mp4 -o '/tmp/bpcs.png'
        (ffmpeg "$PIPE_FP")& ./bpcs "$a" /tmp/bpcs.png -o "$PIPE_FP"
    
    C:
        # No need for pipe because OpenCV can disply images
        
        ./bpcs "$a" /tmp/img.jpg -m /tmp/msg.jpg -o '/tmp/bpcs.png'
        ./bpcs "$a" /tmp/bpcs.png
    
    D:
        ./bpcs "$a" /tmp/img.png > /tmp/extracted.img.jpg
        Equivalent to -o '/tmp/extracted.img.jpg', except will concatenate all extracted images into the single output.

NOTE: Certain codecs (such as mp4) store metadata at the end of the file. This will cause ffplay to fail to stream the file from a named pipe (as named pipes do not allow seeking). Ensure files are sanitised (moov atom placed at start of file - e.g. for MP4, `ffmpeg -i /path/to/input.mp4 -c copy -movflags +faststart /path/to/output.mp4` - before embedding).
*/


const char* NULLCHAR      = "[NULL]";
const std::string NULLSTR = "[NULL]";


const std::regex path_regexp("^((.*)/)?(([^/]+)[.]([^./]+))$");
// Groups are full_match, optional, parent_dir, filename, basename, ext
const std::regex fmt_fp("[{]fp[}]");
const std::regex fmt_dir("[{]dir[}]");
const std::regex fmt_fname("[{]fname[}]");
const std::regex fmt_basename("[{]basename[}]");
const std::regex fmt_ext("[{]ext[}]");
// WARNING: Better would be to ignore `{{fp}}` (escape it to a literal `{fp}`) with (^|[^{]), but no personal use doing so.






std::string format_out_fp(std::string out_fmt, std::smatch path_regexp_match){
    return std::regex_replace(std::regex_replace(std::regex_replace(std::regex_replace(std::regex_replace(out_fmt, fmt_fp, (std::string)path_regexp_match[0]), fmt_dir, (std::string)path_regexp_match[2]), fmt_fname, (std::string)path_regexp_match[3]), fmt_basename, (std::string)path_regexp_match[4]), fmt_ext, (std::string)path_regexp_match[4]);
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







/*
 * Crop image keeping required number of complex grids
 */
/* Required imports
#include <opencv2/core/core.hpp>
#include <iostream> // for std::cout
*/
void iterate_over_all_rects_from_corner_smaller_than(
    cv::Mat &arr,
    
    uint_fast16_t x, uint_fast16_t y,
    
    uint_fast16_t min_area,
    uint_fast16_t required_min_sum,
    
    uint_fast16_t &w, uint_fast16_t &h
){
    #ifdef DEBUG1
    std::cout << "iterate_over_all_rects_from_corner_smaller_than\n" << arr << "\nx=" << +x << "\ty=" << +y << "\tmin_area=" << +min_area << "\tmin_sum=" << +required_min_sum << "\tw=" << +w << "\th=" << +h << "\n";
    #endif
    
    uint_fast16_t cum_sum;
    
    uint_fast16_t grid_w;
    if (min_area > arr.cols)
        grid_w = arr.cols;
    else
        grid_w = min_area;
    
    int diff = x + grid_w - arr.cols;
    if (diff > 0)
        grid_w -= diff;
    
    uint_fast16_t H;
    uint_fast16_t grid_h;
    
    #ifdef DEBUG1
    std::cout << "grid_w=" << +grid_w << "\n\n";
    #endif
    
    do {
        H = arr.rows - y;
        while (grid_w*H >= min_area)
            --H;
        if (H == 0)
            break;
        
        for (grid_h=1; grid_h<H; ++grid_h){
            cv::Mat grid(grid_h, grid_w, CV_8UC1);
            cv::Rect grid_shape(cv::Point(x, y), cv::Size(grid_w, grid_h));
            arr(grid_shape).copyTo(grid);
            cum_sum = cv::sum(grid)[0];
            #ifdef DEBUG1
            std::cout << grid << std::endl;
            std::cout << +grid_w << "x" << +grid_h << "\t" << +cum_sum;
            #endif
            if (cum_sum >= required_min_sum){
                min_area = grid_w * grid_h;
                w = grid_w;
                h = grid_h;
                #ifdef DEBUG1
                std::cout << "min_area=" << +min_area << "\n\n";
                #endif
                break;
            } else if (H == 1)
                break;
            #ifdef DEBUG1
            std::cout << "\n\n";
            #endif
        }
    } while (--grid_w != 0);
    #ifdef DEBUG1
    std::cout << "\n\n";
    #endif
}

void minimum_area_rectangle_of_sum_gt(cv::Mat &arr, uint_fast16_t &x, uint_fast16_t &y, uint_fast16_t &w, uint_fast16_t &h, uint_fast16_t min_sum){
    #ifdef DEBUG1
    std::cout << "minimum_area_rectangle_of_sum_gt\n" << arr << "\nx=" << +x << "\ty=" << +y << "\tw=" << +w << "\th=" << +h << "\tmin_sum=" << +min_sum << "\n\n";
    #endif
    /*
    Input:
        arr:            2D (w, h) array of uint8_t (values in [0, 8])
                        This is (the array mapping the grids of an image to whether or not the complexity was => min_complexity) summed over each bits
        min_sum:        Minimum sum resulting submatrix must have
                        In reality this is the minimum number of grids of complexity => min_complexity required for encoding a file of a certain size
    Process:
        Calculate minimum submatrix of arr which has 
    Return:
        (corner_x, corner_y), (min_w, min_h)
    */
    
    uint_fast16_t min_area = arr.cols * arr.rows;
    uint_fast16_t min_area_tmp;
    
    for (int j=0; j!=arr.rows; ++j){
        for (int i=0; i!=arr.cols; ++i){
            iterate_over_all_rects_from_corner_smaller_than(arr, i, j, min_area, min_sum, w, h);
            min_area_tmp = w * h;
            if (min_area_tmp < min_area){
                min_area = min_area_tmp;
                x = i;
                y = j;
            }
        }
    }
}










/*
 * Creating bytearrays
 */

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
    for (uint_fast64_t j=0; j<n_bytes; ++j){
        byte = bytes[j];
        if (byte == esc_byte || byte == end_byte){
            add_msg_bits(msg, esc_byte);
        }
        add_msg_bits(msg, byte);
    }
    
    add_msg_bits(msg, end_byte);
    // Signal end of data stream
    
    
    return end_byte;
}

uint_fast8_t add_msgfile_bits(std::vector<uint_fast8_t> &msg, std::string fp){
    #ifdef DEBUG1
        std::cout << "add_msg_bits:  " << fp << std::endl;
    #endif
    
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
        bytes.push_back((uint_fast8_t)c);
    
    return add_bytes_to_msg(msg, bytes);
    // returns the trailing end_byte
}

void add_msgfiles_bits(std::vector<uint_fast8_t> &msg, std::vector<std::string> &msg_fps){
    uint_fast16_t n_fps = msg_fps.size();
    if (n_fps == 0){
        #ifdef DEBUG2
            std::cout << "No vessel images specified" << std::endl;
        #endif
        return;
    }
    uint_fast16_t i = 0;
    uint_fast8_t end_byte;
    do {
        end_byte = add_msgfile_bits(msg, msg_fps[i]);
        // TODO: Read input files one by one, rather than all at once. Reduces memory requirement and possibly disk usage too in cases of debugging and errors.
        if (++i < n_fps)
            add_msg_bits(msg, end_byte+1);
            // Ensure that we do not accidentally signal the end of all embedded data (which would happen if the next embedded byte happened to be same value as end_byte)
        else {
            add_msg_bits(msg, end_byte);
            // Signals the end of the entire stream
            break;
        }
    } while (true);
}








/*
 * Bitwise operations on OpenCV Matrices
 */

cv::Mat bitshifted_down(cv::Mat &arr, uint_fast16_t w, uint_fast16_t h){
    cv::Mat dest = cv::Mat(h, w, CV_8UC1);
    uint_fast16_t i;
    uint_fast16_t j;
    for (j=0; j<h; ++j)
        for (i=0; i<w; ++i)
            dest.at<uint_fast8_t>(j,i) = arr.at<uint_fast8_t>(j,i) >> 1;
    return dest;
}

uint_fast16_t xor_adj(
    cv::Mat &arr, uint_fast16_t w, uint_fast16_t h,
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

void bitandshift(cv::Mat &arr, cv::Mat &dest, uint_fast16_t w, uint_fast16_t h, uint_fast16_t n){
    for (uint_fast16_t i=0; i<w; ++i)
        for (uint_fast16_t j=0; j<h; ++j)
            dest.at<uint_fast8_t>(j,i) = (arr.at<uint_fast8_t>(j,i) >> n) & 1;
}

void bitshift_up(cv::Mat &arr, uint_fast16_t w, uint_fast16_t h, uint_fast16_t n){
    for (uint_fast16_t i=0; i<w; ++i)
        for (uint_fast16_t j=0; j<h; ++j)
            arr.at<uint_fast8_t>(j,i) = arr.at<uint_fast8_t>(j,i) << n;
}

void convert_to_cgc(cv::Mat &arr, uint_fast16_t w, uint_fast16_t h, cv::Mat &dest){
    cv::bitwise_xor(arr, bitshifted_down(arr, w, h), dest);
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

cv::Mat chequerboard_a;
cv::Mat chequerboard_b;




/*
 * Grid complexity
 */

float grid_complexity(
    cv::Mat &grid, uint_fast16_t grid_w, uint_fast16_t grid_h,
    cv::Mat &xor_adj_mat1, cv::Mat &xor_adj_mat2, cv::Rect &xor_adj_rect1, cv::Rect &xor_adj_rect2, cv::Rect &xor_adj_rect3, cv::Rect &xor_adj_rect4
){
    return (float)xor_adj(grid, grid_w, grid_h, xor_adj_mat1, xor_adj_mat2, xor_adj_rect1, xor_adj_rect2, xor_adj_rect3, xor_adj_rect4) / (float)(grid_w * (grid_h -1) + grid_h * (grid_w -1));
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
 * Act on complex grids
 */

void grid_dummy(
    cv::Mat &bitplane, cv::Rect &grid_shape, const float min_complexity, cv::Mat &grid, uint_fast16_t grid_w, uint_fast16_t grid_h, std::vector<uint_fast8_t> &msg, uint_fast64_t msg_size, uint_fast16_t encodable_bits_per_grid,
    cv::Mat &xor_adj_mat1, cv::Mat &xor_adj_mat2, cv::Rect &xor_adj_rect1, cv::Rect &xor_adj_rect2, cv::Rect &xor_adj_rect3, cv::Rect &xor_adj_rect4
){
}

void grid_extract(
    cv::Mat &bitplane, cv::Rect &grid_shape, const float min_complexity, cv::Mat &grid, uint_fast16_t grid_w, uint_fast16_t grid_h, std::vector<uint_fast8_t> &msg, uint_fast64_t msg_size, uint_fast16_t encodable_bits_per_grid,
    cv::Mat &xor_adj_mat1, cv::Mat &xor_adj_mat2, cv::Rect &xor_adj_rect1, cv::Rect &xor_adj_rect2, cv::Rect &xor_adj_rect3, cv::Rect &xor_adj_rect4
){
    msg.reserve(encodable_bits_per_grid);
    
    if (grid.data[encodable_bits_per_grid] == 1)
        conjugate_grid(grid);
    
    std::copy_n(grid.data, encodable_bits_per_grid, std::back_inserter(msg));
}

void grid_embed(
    cv::Mat &bitplane, cv::Rect &grid_shape, const float min_complexity, cv::Mat &grid, uint_fast16_t grid_w, uint_fast16_t grid_h, std::vector<uint_fast8_t> &msg, uint_fast64_t msg_size, uint_fast16_t encodable_bits_per_grid,
    cv::Mat &xor_adj_mat1, cv::Mat &xor_adj_mat2, cv::Rect &xor_adj_rect1, cv::Rect &xor_adj_rect2, cv::Rect &xor_adj_rect3, cv::Rect &xor_adj_rect4
){
    // `msg_size` here is used as the offset - i.e. the size of the msg that has already been embedded
    
    uint_fast16_t encoded_bits = grid_w * grid_h -1;
    
    for (uint_fast16_t i=0; i<encoded_bits; ++i){
        grid.data[i] = msg[msg_size++];
    }
    
    float old_complexity = grid_complexity(grid, grid_w, grid_h, xor_adj_mat1, xor_adj_mat2, xor_adj_rect1, xor_adj_rect2, xor_adj_rect3, xor_adj_rect4);
    float new_complexity;
    bool to_conjugate = true;
    
    if (old_complexity >= min_complexity){
        grid.data[encoded_bits] = 0;
        new_complexity = grid_complexity(grid, grid_w, grid_h, xor_adj_mat1, xor_adj_mat2, xor_adj_rect1, xor_adj_rect2, xor_adj_rect3, xor_adj_rect4);
        if (new_complexity >= min_complexity)
            to_conjugate = false;
    }
    
    if (to_conjugate){
        conjugate_grid(grid);
        grid.data[encoded_bits] = 1;
        new_complexity = grid_complexity(grid, grid_w, grid_h, xor_adj_mat1, xor_adj_mat2, xor_adj_rect1, xor_adj_rect2, xor_adj_rect3, xor_adj_rect4);
        if (new_complexity < min_complexity){
            #ifdef DEBUG1
                std::cout << "Grid complexity fell below minimum value; from " << old_complexity << " to " << new_complexity << std::endl;
            #endif
            throw std::runtime_error("");
        }
    }
    
    grid.copyTo(bitplane(grid_shape));
    
    // In other words, the first bit of each message grid tells us whether it was conjugated
}











/*
 * Display statistics
 */

#ifdef DEBUG1
void print_histogram(std::vector<float> &complexities, uint_fast16_t n_bins, uint_fast16_t n_binchars){
    uint_fast32_t len_complexities;
    float min;
    float max;
    float step;
    float bin_max;
    uint_fast16_t bin_total;
    
    std::sort(std::begin(complexities), std::end(complexities));
    len_complexities = complexities.size();
    
    std::cout << std::endl;
    
    if (len_complexities == 0){
        std::cout << "No complexities to display" << std::endl;
        return;
    }
    
    std::cout << "Complexities Histogram" << std::endl;
    min = *std::min_element(std::begin(complexities), std::end(complexities));
    max = *std::max_element(std::begin(complexities), std::end(complexities));
    std::cout << "Total: " << len_complexities << " between " << min << ", " << max << std::endl;
    step = (max - min) / (float)n_bins;
    std::cout << "Bins:  " << +n_bins << " with step " << step << std::endl;
    std::vector<uint_fast16_t> bin_totals;
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
    
    uint_fast16_t k;
    for (uint_fast16_t j=0; j<n_bins; ++j){
        bin_total = n_binchars * bin_totals[j] / len_complexities;
        std::cout << j * step << ": " << bin_totals[j] << std::endl << "   ";
        for (k=0; k<bin_total; ++k)
            std::cout << "#";
        std::cout << std::endl;
    }
    
    std::cout << n_bins * step << std::endl;
    std::cout << std::endl << std::endl;
}
#endif















/*
 * Iterate over complex grids
 */

#ifdef DEBUG3
void iterate_over_bitgrids__msg(const char* msg, uint_fast32_t n_grids_so_far, uint_fast32_t n_grids_used, uint_fast32_t n_grids_total, float min_complexity, uint_fast64_t msg_size){
    std::cout << msg << ", at state:  " << n_grids_so_far << " of " << n_grids_total << " grids\t" << n_grids_used << " with complexity >= " << min_complexity;
    if (msg_size != 0)
        std::cout << "\tmsg size " << msg_size;
    std::cout << std::endl;
}
#endif

int iterate_over_bitgrids(uint_fast64_t &msg_size, bool minimise_img, cv::Mat &count_complex_grids, std::vector<float> &complexities, cv::Mat &bitplane, float min_complexity, uint_fast16_t n_hztl_grids, uint_fast16_t n_vert_grids, uint_fast16_t bitplane_w, uint_fast16_t bitplane_h, uint_fast16_t grid_w, uint_fast16_t grid_h, std::function<void(cv::Mat&, cv::Rect&, const float, cv::Mat&, uint_fast16_t, uint_fast16_t, std::vector<uint_fast8_t>&, uint_fast64_t, uint_fast64_t, cv::Mat&, cv::Mat&, cv::Rect&, cv::Rect&, cv::Rect&, cv::Rect&)> grid_fnct, std::vector<uint_fast8_t> &msg){
    // Pass reference to complexities, else a copy is passed (and changes are not kept)
    // Note that we will be doing millions of operations, and do not mind rounding errors - the important thing here is that we get consistent results. Hence we use float not double
    cv::Mat grid(grid_h, grid_w, CV_8UC1);
    // cv::Rect, cv::Point, cv::Size are all column-major, i.e. (x, y) or (width, height)
    cv::Mat xor_adj_mat1(grid_h,   grid_w-1, CV_8UC1);
    cv::Mat xor_adj_mat2(grid_h-1, grid_w,   CV_8UC1);
    cv::Rect xor_adj_rect1(cv::Point(0, 0), cv::Size(grid_w-1, grid_h));
    cv::Rect xor_adj_rect2(cv::Point(1, 0), cv::Size(grid_w-1, grid_h));
    cv::Rect xor_adj_rect3(cv::Point(0, 0), cv::Size(grid_w,   grid_h-1));
    cv::Rect xor_adj_rect4(cv::Point(0, 1), cv::Size(grid_w,   grid_h-1));
    float complexity;
    uint_fast64_t msg_size_orig = msg.size();
    uint_fast16_t encodable_bits_per_grid = grid_w * grid_h -1;
    #ifdef DEBUG4
        uint_fast32_t n_grids_used = 0;
        uint_fast32_t n_grids_so_far = 0;
        uint_fast32_t n_grids_total  = n_hztl_grids * n_vert_grids;
        std::cout << "msg_size before embedding:  " << +msg_size << " ~ " << +msg_size_orig << std::endl;
    #endif
    for (uint_fast16_t i=0; i<n_hztl_grids; ++i){
        for (uint_fast16_t j=0; j<n_vert_grids; ++j){
            cv::Rect grid_shape(cv::Point(i*grid_w, j*grid_h), cv::Size(grid_w, grid_h));
            bitplane(grid_shape).copyTo(grid);
            complexity = grid_complexity(grid, grid_w, grid_h, xor_adj_mat1, xor_adj_mat2, xor_adj_rect1, xor_adj_rect2, xor_adj_rect3, xor_adj_rect4);
            
            #ifdef DEBUG1
                complexities.push_back(complexity);
            #endif
            
            if (complexity < min_complexity)
                continue;
            
            if (minimise_img)
                ++count_complex_grids.at<uint_fast8_t>(j, i);
            
            grid_fnct(
                bitplane, grid_shape, min_complexity, grid, grid_w, grid_h, msg, msg_size, encodable_bits_per_grid,
                xor_adj_mat1, xor_adj_mat2, xor_adj_rect1, xor_adj_rect2, xor_adj_rect3, xor_adj_rect4
            );
            
            msg_size += encodable_bits_per_grid;
            
            ++n_grids_used;
        }
        #ifdef DEBUG4
            n_grids_so_far += n_vert_grids;
        #endif
        #ifdef DEBUG6
            if (i % 10 == 0 || n_hztl_grids < 11 || msg_size < 20000){
                iterate_over_bitgrids__msg("debug", n_grids_so_far, n_grids_used, n_grids_total, min_complexity, msg_size);
            }
        #endif
    }
    #ifdef DEBUG4
        iterate_over_bitgrids__msg("bitplane exhausted", n_grids_so_far, n_grids_used, n_grids_total, min_complexity, msg_size);
    #endif
    return 1;
}

int iterate_over_all_bitgrids(
    uint_fast64_t &msg_size,
    
    cv::Mat &im_mat,
    std::vector<uint_fast8_t> &msg,
    
    std::vector<float> &complexities,
    
    bool minimise_img,
    bool encoding,
    std::function<void(cv::Mat&, cv::Rect&, const float, cv::Mat&, uint_fast16_t, uint_fast16_t, std::vector<uint_fast8_t>&, uint_fast64_t, uint_fast64_t, cv::Mat&, cv::Mat&, cv::Rect&, cv::Rect&, cv::Rect&, cv::Rect&)> grid_fnct,
    
    uint_fast8_t grid_w,
    uint_fast8_t grid_h,
    float min_complexity,
    
    #ifdef DEBUG1
        uint_fast16_t n_bins,
        uint_fast16_t n_binchars,
    #endif
    
    std::vector<cv::Mat> &channel_byteplanes
){
    
    cv::Mat XORed_byteplane;
    cv::Mat prev_bitplane;
    
    uint_fast8_t n_channels = im_mat.channels();
    
    uint_fast16_t w = im_mat.cols;
    uint_fast16_t h = im_mat.rows;
    
    // NOTE: OpenCV treats matrices in row-major order - i.e. accessed as (row_id, column_id). For images, this is (y, x).
    
    uint_fast16_t n_hztl_grids = w / grid_w;
    uint_fast16_t n_vert_grids = h / grid_h;
    
    cv::Mat count_complex_grids = cv::Mat::zeros(n_vert_grids, n_hztl_grids, CV_8UC1);
    
    cv::Mat bitplane = cv::Mat(h, w, CV_8UC1);
    // CV_8UC1 - aka CV_8UC(1) - means 1 channel of uint8
    
    complexities.reserve(n_hztl_grids * n_vert_grids);
    // Speeds up assignments by reserving required memory beforehand
    
    uint_fast8_t n_bits;
    switch(im_mat.depth()){
        case CV_8UC1:
            n_bits = 8;
            break;
    }
    
    #ifdef DEBUG3
        std::cout << +w << "x" << +h << "\t" << +n_channels << " channels\t" << +n_bits << " depth" << std::endl;
    #endif
    
    bool msg_was_exhausted = false;
    
    for (uint_fast8_t j=0; j<n_channels; ++j){
        convert_to_cgc(channel_byteplanes[j], w, h, XORed_byteplane);
        // Bitshifting up ensures that the last bits of (arr >> 1) are 0 - so the last digit of this CGC'd (or XOR'd-ish) arr is retained
        
        cv::Mat byteplane = cv::Mat::zeros(h, w, CV_8UC1);
        
        for (uint_fast8_t k=0; k<n_bits; ++k){
            bitandshift(XORed_byteplane, bitplane, w, h, n_bits-k);
            if (!msg_was_exhausted){
                if (iterate_over_bitgrids(msg_size, minimise_img, count_complex_grids, complexities, bitplane, min_complexity, n_hztl_grids, n_vert_grids, w, h, grid_w, grid_h, grid_fnct, msg) == 0)
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
    }
    
    if (minimise_img){
        #ifdef DEBUG3
            std::cout << count_complex_grids << "\n\n";
        #endif
        uint_fast16_t x = 0;
        uint_fast16_t y = 0;
        uint_fast16_t w, h;
        uint_fast64_t min_req_complex_grids = 100; // msg.size() / (grid_w * grid_h);
        minimum_area_rectangle_of_sum_gt(count_complex_grids, x, y, w, h, min_req_complex_grids);
        #ifdef DEBUG2
            printf("Minimum rectangle with >= %d encodable grids is cv::Rect((%d, %d), (%d, %d))\n", min_req_complex_grids, x, y, w, h);
            std::cout << "(" << +x << ", " << +y << ")\t(" << +w << ", " << +h << ")\n";
        #endif
    }
}






/*
 * Bitarrays
 */

uint_fast8_t get_byte_from(std::vector<uint_fast8_t> &msg, uint_fast64_t indx, const char* name){
    uint_fast8_t byte = 0;
    uint_fast64_t endx = indx + 8;
    uint_fast8_t shift = 0;
    for (uint_fast64_t j=indx; j!=endx; ++j){
        byte |= msg[j] << shift++;
    }
    #ifdef DEBUG5
        if (name != NULLCHAR)
            std::cout << "[" << +(indx >> 3) << "] " << name << "  ==  " << +byte << std::endl;
    #endif
    return byte;
}




/*
 * Bytearrays
 */

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
    
    #ifdef DEBUG1
    // Histogram args
    args::ValueFlag<uint_fast8_t>       An_bins         (parser, "n_bins", "Number of histogram bins", {'B', "bins"});
    args::ValueFlag<uint_fast8_t>       An_binchars     (parser, "n_binchars", "Total number of `#` characters printed out in histogram totals", {"binchars"});
    #endif
    
    // Debugging args
    args::Flag                          Amax_cap        (parser, "max_cap", "Calculate the maximum capacity of vessel image files", {"max-cap"});
    
    args::ValueFlag<std::string>        Aout_fmt        (parser, "out_fmt", "Format of output file path(s) - substitutions being fp, dir, fname, basename, ext. Sets mode to `extracting` if msg_fps not supplied.", {'o', "out"});
    args::Flag                          Ato_disk        (parser, "to_disk", "Write to disk (as opposed to psuedofile)", {'d', "to-disk"});
    args::Flag                          Adisplay_img    (parser, "display", "Display embedded images through OpenCV::imshow (rather than pipe)", {'D', "display"});
    
    args::ValueFlag<std::string>        Apipe_in        (parser, "pipe_in", "Path of named input pipe to listen to after having piped extracted message to to output pipe. Sets mode to `editing`", {'i', "pipe-in"});
    
    args::ValueFlagList<std::string>    Amsg_fps        (parser, "msg_fps", "File path(s) of message file(s) to embed. Sets mode to `embedding`", {'m', "msg"});
    
    // Encoding variables
    args::Flag                          Amin            (parser, "min", "Minimise image (i.e. crop image to the minimum rectangle holding the required number of complex grids", {"min"});
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
    
    if (Amin && !Amsg_fps){
        #ifdef DEBUG1
            std::cerr << parser;
        #endif
        return 1;
    }
    
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
        
        std::string mode;
    #endif
    std::vector<std::string> msg_fps;
    std::function<void(cv::Mat&, cv::Rect&, const float, cv::Mat&, uint_fast16_t, uint_fast16_t, std::vector<uint_fast8_t>&, uint_fast64_t, uint_fast64_t, cv::Mat&, cv::Mat&, cv::Rect&, cv::Rect&, cv::Rect&, cv::Rect&)> grid_fnct;
    
    const char* named_pipe_in;
    if (Amsg_fps){
        msg_fps = args::get(Amsg_fps);
        #ifdef DEBUG1
            mode = "Encoding";
        #endif
        grid_fnct = grid_embed;
    } else if (Amax_cap){
        #ifdef DEBUG1
            mode = "Calculating max capacity";
        #endif
        grid_fnct = grid_dummy;
    } else if (Apipe_in){
        #ifdef DEBUG1
            mode = "Editing";
        #endif
        named_pipe_in = args::get(Apipe_in).c_str();
        grid_fnct = grid_extract;
    } else {
        #ifdef DEBUG1
            mode = "Decoding";
        #endif
        grid_fnct = grid_extract;
    }
    
    std::string out_fmt;
    std::string out_fp;
    if (Aout_fmt){
        out_fmt = args::get(Aout_fmt);
        #ifdef DEBUG1
            mode += " to ";
            if (Ato_disk)
                mode += "file";
            else
                mode += "display";
        #endif
    } else {
        if (Amsg_fps){
            #ifdef DEBUG1
                std::cerr << "Must specify out_fmt when embedding" << std::endl;
            #endif
            return 1;
        }
        out_fmt = NULLCHAR;
        #ifdef DEBUG1
            mode += " to display";
        #endif
    }
    
    cv::Mat im_mat;
    
    std::vector<std::string>img_fps = args::get(Aimg_fps);
    uint_fast16_t img_fps_len        = img_fps.size();
    
    const float min_complexity = args::get(Amin_complexity);
    if (min_complexity > 0.5){
        #ifdef DEBUG1
            std::cerr << "Current implementation requires maximum minimum complexity of 0.5" << std::endl;
        #endif
        throw std::runtime_error("");
    }
    
    #ifdef DEBUG1
        std::cout << mode << ": " << +img_fps_len << " img inputs, using: Complexity >= " <<  +min_complexity << std::endl;
        // Preceding primitive data type with `+` operator makes it print ASCII character representing that numerical value, rather than the ASCII character of that value
    #endif
    
    std::vector<uint_fast8_t> msg;
    
    uint_fast16_t i;
    
    uint_fast8_t esc_byte;
    uint_fast8_t end_byte;
    
    uint_fast64_t msg_size;
    
    if (Amsg_fps){
        add_msgfiles_bits(msg, msg_fps);
        
        const int bits_encoded_per_grid = grid_w * grid_h -1;
        msg_size = msg.size();
        
        int diff = (int)msg_size % bits_encoded_per_grid;
        if (diff != 0){
            diff = bits_encoded_per_grid - diff;
            
            i = 0;
            do {
                msg.push_back(i & 1);
                // Alternate the junk bit so that we get a chequered style, a cheap pattern that is likely to result in a high complexity (which is what we desire)
            } while (++i != diff);
        }
        
        #ifdef DEBUG2
            std::cout << msg_size << "b to encode" << std::endl;
        #endif
    }
    
    uint_fast16_t j;
    
    std::smatch path_regexp_match;
    
    std::string ext;
    std::string fp;
    
    std::vector<float> complexities;
    
    uint_fast64_t n_bits_processed = 0;
    
    for (i=0; i<img_fps_len; ++i){
        fp = img_fps[i];
        im_mat = cv::imread(fp, CV_LOAD_IMAGE_COLOR);
        // WARNING: OpenCV loads images as BGR, not RGB
        
        if (im_mat.data == NULL){
            #ifdef DEBUG1
                std::cerr << "Cannot load image data from:  " << fp << std::endl;
            #endif
            return 1;
        }
        
        std::vector<cv::Mat> channel_byteplanes;
        cv::split(im_mat, channel_byteplanes);
        
        if (Amsg_fps){
        if (Amin){
            #ifdef DEBUG2
                std::cout << "Minimising img" << std::endl;
            #endif
        }
            std::vector<float> complexities_tmp;
            uint_fast64_t n_bits_preprocessed = 0;
            iterate_over_all_bitgrids(
                n_bits_preprocessed, im_mat, msg, complexities_tmp, (Amin), false, grid_dummy, grid_w, grid_h, min_complexity,
                #ifdef DEBUG1
                    n_bins, n_binchars,
                #endif
                channel_byteplanes
            );
            int64_t diff = (int64_t)n_bits_preprocessed - (int64_t)msg_size;
            #ifdef DEBUG2
                std::cout << "#b that can be embedded in img - #b to be embedded  ==  " << +diff << std::endl;
            #endif
            // We only need to run this once - if we are already running it to minimise the vessel image, do not repeat for crypt.
            
            // We must encrypt all bits in complex grids, as encryption depends on the size of the message, and we will not know the exact size of the embedded message when decrypting
            if (diff > 0){
                // Fill with junk bits
                for (int64_t l=0; l<diff; ++l)
                    msg.push_back(l & 1);
                msg_size = n_bits_preprocessed;
            }
        }
        
        iterate_over_all_bitgrids(
            n_bits_processed, im_mat, msg, complexities, false, (Amsg_fps), grid_fnct, grid_w, grid_h, min_complexity,
            #ifdef DEBUG1
                n_bins, n_binchars,
            #endif
            channel_byteplanes
        );
        
        if (Amsg_fps){
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
        }
    }
    
    if (!Amsg_fps && !Amax_cap){
            uint_fast64_t n_msg_bits = msg.size();
            uint_fast8_t shift = 0;
            uint_fast8_t byte = 0;
            uint_fast8_t either_endbyte_or_junk;
            if (n_msg_bits < 32){
                #ifdef DEBUG1
                    std::cerr << "Failed to extract complete message; only " << +n_msg_bits << " bits were extracted" << std::endl;
                #endif
                return 1;
            }
            #ifdef DEBUG1
                std::cout << "Extracted message" << std::endl;
            #endif
            esc_byte = get_byte_from(msg, 0, "esc_byte");
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
                    if (byte == esc_byte) {
                        extracted_msg.push_back(get_byte_from(msg, j, NULLCHAR));
                        j += 8;
                    } else if (byte == end_byte) {
                        either_endbyte_or_junk = get_byte_from(msg, j, "end_byte or junk");
                        stream_starts.push_back(index);
                        if (either_endbyte_or_junk == end_byte){
                            goto print_extracted_msg;
                        } else if (either_endbyte_or_junk != end_byte+1){
                            return 0;
                        }
                        j += 8;
                        // Discard the previously calculated byte
                        esc_byte = get_byte_from(msg, j, "esc_byte");
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
            uint_fast16_t n_extracted_msgs = stream_starts.size() -1;
            
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
                        throw std::invalid_argument("");
                    }
                    
                    out_fp = format_out_fp(out_fmt, path_regexp_match);
                    ext = (std::string)path_regexp_match[5];
                } else {
                    // This stream is the file contents
                    
                    extracted_msg_pointer = &extracted_msg[index];
                    
                    if (Ato_disk){
                        #ifdef DEBUG1
                            std::cout << "Saving to: " << out_fp << std::endl;
                        #endif
                        vector2file(extracted_msg_pointer, n_extracted_msg_bytes, "extracted_msg", out_fp);
                    } else {
                        #ifdef DEBUG1
                            std::cout << "Reading `" << ext << "` data stream originating from: " << fp << std::endl;
                        #endif
                        
                        if (out_fmt == NULLSTR && Adisplay_img && (ext == "jpg" || ext == "png" || ext == "jpeg" || ext == "bmp")){
                            cv::Mat rawdata(1, n_extracted_msg_bytes, CV_8UC1, extracted_msg_pointer);
                            cv::Mat decoded_img = cv::imdecode(rawdata, CV_LOAD_IMAGE_UNCHANGED);
                            if (decoded_img.data == NULL){
                                #ifdef DEBUG1
                                    std::cerr << "No image data loaded from " << n_extracted_msg_bytes << "B data stream claiming to originate from file `" << fp << "`. Saved extracted bytes to `/tmp/bpcs.msg.txt` for debugging." << std::endl;
                                #endif
                                throw std::invalid_argument("");
                            }
                            #ifdef DEBUG3
                                std::cout << "Displaying image" << std::endl;
                            #endif
                            cv::imshow(fp, decoded_img);
                            cv::waitKey(0);
                        } else {
                            struct stat buffer;
                            
                            if (out_fmt == NULLSTR){
                                // Pipe to unnamed pipe
                                for (k=0; k<n_extracted_msg_bytes; ++k)
                                    std::cout << extracted_msg[k];
                            } else {
                                // Pipe to named pipe
                                const char* named_pipe_fp = out_fp.c_str();
                                
                                if (stat(named_pipe_fp, &buffer) != 0){
                                    // Ensure named_pipe exists, else create
                                    if (mkfifo(named_pipe_fp, 0666) == -1){
                                        // WARNING: mkfifo is POSIX-specific
                                        #ifdef DEBUG1
                                            std::cerr << "Failed to create named pipe `" << named_pipe_fp << "`" << std::endl;
                                        #endif
                                        throw std::runtime_error("");
                                    }
                                }
                                
                                #ifdef DEBUG3
                                    printf("Piping to `%s`\n", named_pipe_fp);
                                #endif
                                
                                int fd = open(named_pipe_fp, O_WRONLY);
                                write(fd, (char*)extracted_msg_pointer, n_extracted_msg_bytes);
                                close(fd);
                                
                                std::remove(named_pipe_fp);
                            }
                            
                            if (Apipe_in){
                                // E.g. editing txt - we'd send it to our edited NoFrillsTextEditor `typed-piper`, which reads the input stream from named_pipe, and - when saved - pipes the edited file to `/tmp/NoFrillsTextEditor.named_pipe`
                                #ifdef DEBUG2
                                    std::cout << "Reading edited file from:  " << named_pipe_in << std::endl;
                                #endif
                                
                                if (stat(named_pipe_in, &buffer) != 0){
                                    // Ensure named_pipe exists, else create
                                    if (mkfifo(named_pipe_in, 0666) == -1){
                                        // WARNING: mkfifo is POSIX-specific
                                        #ifdef DEBUG1
                                            std::cerr << "Failed to create named pipe `" << named_pipe_in << "`" << std::endl;
                                        #endif
                                        throw std::runtime_error("");
                                    }
                                }
                                
                                FILE* named_pipe_inf = fopen(named_pipe_in, "r");
                                char c;
                                while ((c=getc(named_pipe_inf)) != EOF){
                                    // TODO: Input this data to a bitarray vector, and BPCS embed into image. This will require the re-ordering of the majority of this code.
                                    #ifdef DEBUG4
                                        std::cout << c;
                                    #endif
                                }
                                fclose(named_pipe_inf);
                                std::remove(named_pipe_in);
                            }
                        }
                    }
                }
            }
    }
    #ifdef DEBUG1
        print_histogram(complexities, n_bins, n_binchars);
    #endif
    return 0;
}
