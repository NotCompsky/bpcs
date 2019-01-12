#include <args.hxx>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp> // for calcHist
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


long unsigned int get_fsize(const char* fp){
    struct stat stat_buf;
    int rc = stat(fp, &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void add_msgfile_bits(std::vector<uint_fast8_t> &msg, const char* fp){
    msg.reserve(get_fsize(fp));
    
    std::ifstream msg_file(fp, std::ios::binary);
    
    char C;
    unsigned char c;
    #ifdef DEBUG6
        std::cout << "msg_bytes: ";
    #endif
    while (msg_file.get(C)){
        c = (unsigned char)C;
        #ifdef DEBUG6
            std::cout << +c << "  ";
        #endif
        msg.push_back(c & 1);
        msg.push_back((c & 2) >> 1);
        msg.push_back((c & 4) >> 2);
        msg.push_back((c & 8) >> 3);
        msg.push_back((c & 16) >> 4);
        msg.push_back((c & 32) >> 5);
        msg.push_back((c & 64) >> 6);
        msg.push_back((c & 128) >> 7);
    }
    #ifdef DEBUG6
        std::cout << std::endl;
    #endif
}



cv::Mat bitshift_down(cv::Mat &arr, unsigned int w, unsigned int h){
    #ifdef DEBUG5
        std::cout << "bitshift_down" << std::endl << arr << std::endl << std::endl;
    #endif
    cv::Mat dest = cv::Mat::zeros(w, h, CV_8UC1);
    for (int i=0; i<w; i++)
        for (int j=0; j<h; j++)
            dest.at<uint_fast8_t>(i,j) = arr.at<uint_fast8_t>(i,j) >> 1;
    #ifdef DEBUG6
        std::cout << "bitshift_down" << std::endl << dest << std::endl << std::endl;
    #endif
    return dest;
}

unsigned int xor_adj(cv::Mat &arr, unsigned int w, unsigned int h){
    unsigned int sum = 0;
    for (int j=0; j<h; j++)
        for (int i=1; i<w; i++)
            sum += arr.at<uint_fast8_t>(i,j) ^ arr.at<uint_fast8_t>(i-1,j);
    for (int i=0; i<w; i++)
        for (int j=1; j<h; j++)
            sum += arr.at<uint_fast8_t>(i,j) ^ arr.at<uint_fast8_t>(i,j-1);
    return sum;
}

void bitandshift(cv::Mat &arr, cv::Mat &dest, unsigned int w, unsigned int h, unsigned int n){
    for (int i=0; i<w; i++)
        for (int j=0; j<h; j++)
            dest.at<uint_fast8_t>(i,j) = (arr.at<uint_fast8_t>(i,j) >> n) & 1;
}

#ifdef DEBUG1
void print_cv_arr(const char* name, int i, cv::Mat &arr){
    std::cout << name << i << std::endl << arr << std::endl << std::endl;
}
#endif

void conjugate_grid(cv::Mat &grid){
    // Just specified for verbosity / to remember
    cv::bitwise_xor(grid, 1, grid);
}

int decode_grid(const float min_complexity, cv::Mat &grid, unsigned int grid_w, unsigned int grid_h, std::vector<uint_fast8_t> &msg){
    // Pass reference to msg, else a copy is passed (and changes are not kept)
    #ifdef DEBUG6
        std::cout << "decode_grid(grid, " << grid_w << ", " << grid_h << ", msg)" << std::endl;
    #endif
    
    if (grid.at<uint_fast8_t>(0,0) == 1)
        conjugate_grid(grid);
    
    unsigned int index = 0;
    for (int j=0; j<grid_h; j++)
        for (int i=0; i<grid_w; i++){
            if (index == 0)
                // Do not add conjugation status bit to msg
                continue;
            msg.push_back((bool)grid.at<uint_fast8_t>(i,j));
            index++;
        }
    
    return 1;
}

float grid_complexity(cv::Mat &grid, unsigned int grid_w, unsigned int grid_h){
    return (float)xor_adj(grid, grid_w, grid_h) / (float)(grid_w * (grid_h -1) + grid_h * (grid_w -1));
}

int encode_grid(const float min_complexity, cv::Mat &grid, unsigned int grid_w, unsigned int grid_h, std::vector<uint_fast8_t> &msg){
    // Pass reference to msg, else a copy is passed (and changes are not kept)
    #ifdef DEBUG6
        std::cout << "encode_grid(grid, " << grid_w << ", " << grid_h << ", msg)" << std::endl;
    #endif
    
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
    
    int index = -2;
    
    for (int j=0; j<grid_h; ++j){
        for (int i=0; i<grid_w; ++i){
            ++index;
            if (index == -1)
                continue;
                // First bit (at (0,0)) of grid is reserved for conjugation status
            
            grid.at<uint_fast8_t>(i,j) = msg[index];
        }
    }
    
    msg.erase(std::begin(msg), std::begin(msg) + index + 1);
    
    if (grid_complexity(grid, grid_w, grid_h) < min_complexity){
        conjugate_grid(grid);
        grid.at<uint_fast8_t>(0,0) = 1;
    }
    // In other words, the first bit of each message grid tells us whether it was conjugated
    
    return 1;
}

int iterate_over_bitgrids(std::vector<float> &complexities, cv::Mat &bitplane, float min_complexity, unsigned int n_hztl_grids, unsigned int n_vert_grids, unsigned int bitplane_w, unsigned int bitplane_h, unsigned int grid_w, unsigned int grid_h, std::function<int(const float, cv::Mat&, unsigned int, unsigned int, std::vector<uint_fast8_t>&)> grid_fnct, std::vector<uint_fast8_t> &msg){
    // Pass reference to complexities, else a copy is passed (and changes are not kept)
    // Note that we will be doing millions of operations, and do not mind rounding errors - the important thing here is that we get consistent results. Hence we use float not double
    cv::Mat grid;
    unsigned long int n_grids_used = 0;
    float complexity;
    int grid_fnct_status;
    #ifdef DEBUG3
        unsigned long int msg_size = 0;
        unsigned long int n_grids_so_far = 0;
        unsigned long int n_grids_total  = n_hztl_grids * n_vert_grids;
    #endif
    for (int i=0; i<n_hztl_grids; i++){
        for (int j=0; j<n_vert_grids; j++){
            cv::Rect grid_shape(cv::Point(i*grid_w, j*grid_h), cv::Size(grid_w, grid_h));
            bitplane(grid_shape).copyTo(grid);
            complexity = grid_complexity(grid, grid_w, grid_h);
            
            #ifdef DEBUG1
                complexities.push_back(complexity);
            #endif
            
            if (complexity < min_complexity)
                continue;
            
            grid_fnct_status = grid_fnct(min_complexity, grid, grid_w, grid_h, msg);
            
            if (grid_fnct_status == 0)
                return 0;
            
            n_grids_used++;
        }
        #ifdef DEBUG3
            n_grids_so_far += n_vert_grids;
            if (i % 10 == 0 || n_hztl_grids < 11 || msg_size < 20000){
                msg_size = msg.size();
                std::cout << n_grids_so_far << " of " << n_grids_total << " grids\t" << n_grids_used << " with complexity >= " << min_complexity << "\tmsg size = " << msg_size << std::endl;
            }
        #endif
    }
    return 1;
}

#ifdef DEBUG1
void print_histogram(std::vector<float> &complexities, unsigned int n_bins, unsigned int n_binchars){
    unsigned long int len_complexities;
    float min;
    float max;
    float step;
    float bin_max;
    unsigned int bin_total;
    
    std::sort(std::begin(complexities), std::end(complexities));
    len_complexities = complexities.size();
    if (len_complexities == 0){
        std::cout << "W: len_complexities is 0" << std::endl;
    } else {
        std::cout << "Complexities Histogram" << std::endl;
        min = *std::min_element(std::begin(complexities), std::end(complexities));
        max = *std::max_element(std::begin(complexities), std::end(complexities));
        std::cout << "Total: " << len_complexities << " between " << min << ", " << max << std::endl;
        step = (max - min) / (float)n_bins;
        std::cout << "Bins:  " << +n_bins << " with step " << step << std::endl;
        std::vector<unsigned int> bin_totals;
        bin_max = step;
        bin_total = 0;
        
        for (unsigned long int j=0; j<len_complexities; j++){
            while (complexities[j] > bin_max){
                #ifdef DEBUG5
                    std::cout << "Bin" << bin_totals.size() << ":  " << complexities[j] << " == complexities[" << j << "] > bin_max == " << bin_max << std::endl;
                #endif
                bin_totals.push_back(bin_total);
                bin_max += step;
                bin_total = 0;
            }
            bin_total++;
        }
        bin_totals.push_back(bin_total);
        
        for (int j=0; j<n_bins; j++){
            bin_total = n_binchars * bin_totals[j] / len_complexities;
            std::cout << j * step << ": " << bin_totals[j] << std::endl << "   ";
            for (int k=0; k<bin_total; k++)
                std::cout << "#";
            std::cout << std::endl;
        }
        
        std::cout << n_bins * step << std::endl;
    }
    std::cout << std::endl << std::endl;
}
#endif

int main(const int argc, char *argv[]){
    args::ArgumentParser parser("(En|De)code BPCS", "This goes after the options.");
    args::HelpFlag                      Ahelp           (parser, "help", "Display help", {'h', "help"});
    args::ValueFlag<uint_fast8_t>       An_bits         (parser, "n_bits", "n_bits", {'b', "bits"});
    args::ValueFlag<uint_fast8_t>       An_chns         (parser, "n_chns", "n_chns", {'c', "channels"});
    
    args::ValueFlag<uint_fast8_t>       An_bins         (parser, "n_bins", "Number of histogram bins", {'B', "bins"});
    args::ValueFlag<uint_fast8_t>       An_binchars     (parser, "n_binchars", "Total number of `#` characters printed out in histogram totals", {"binchars"});
    
    args::ValueFlagList<std::string>    Amsg_fps        (parser, "msg_fps", "File path(s) of message file(s) to embed. Sets mode to `encoding`", {'m', "msg"});
    args::Positional<float>             Amin_complexity (parser, "min_complexity", "Minimum bitplane complexity");
    args::Positional<std::string>       Aout_fmt        (parser, "out_fmt", "Format of output file path(s)");
    args::PositionalList<std::string>   Aimg_fps        (parser, "img_fps", "File path(s) of input image file(s)");
    
    try {
        parser.ParseCLI(argc, argv);
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
    
    unsigned int grid_w = 8;
    unsigned int grid_h = 8;
    
    uint_fast8_t n_bits;
    if (An_bits){
        n_bits = args::get(An_bits);
    } else {
        n_bits = 8;
    }
    
    uint_fast8_t n_channels;
    if (An_chns){
        n_channels = args::get(An_chns);
    } else {
        n_channels = 3;
    }
    
    uint_fast8_t n_bins;
    if (An_bins){
        n_bins = args::get(An_bins);
    } else {
        n_bins = 10;
    }
    
    uint_fast8_t n_binchars;
    if (An_binchars){
        n_binchars = args::get(An_binchars);
    } else {
        n_binchars = 200;
    }
    
    std::string mode;
    std::vector<std::string> msg_fps;
    std::function<int(const float, cv::Mat&, unsigned int, unsigned int, std::vector<uint_fast8_t>&)> grid_fnct;
    if (Amsg_fps){
        msg_fps = args::get(Amsg_fps);
        mode = "Encoding";
        grid_fnct = encode_grid;
    } else {
        mode = "Decoding";
        grid_fnct = decode_grid;
    }
    
    cv::Mat im_mat;
    cv::Mat tmparr;
    cv::Mat tmparrorig;
    cv::Mat bitplane;
    
    unsigned int w;
    unsigned int h;
    
    unsigned int n_hztl_grids;
    unsigned int n_vert_grids;
    
    std::vector<std::string>img_fps = args::get(Aimg_fps);
    unsigned int img_fps_len        = img_fps.size();
    
    const float min_complexity = args::get(Amin_complexity);
    
    #ifdef DEBUG1
        std::cout << mode << " " << +img_fps_len << " img inputs, using: Complexity >= " <<  +min_complexity << ", " << +n_channels << " channels, " << +n_bits << " bits" << std::endl;
        // Preceding primitive data type with `+` operator makes it print ASCII character representing that numerical value, rather than the ASCII character of that value
    #endif
    
    std::vector<uint_fast8_t> msg;
    const unsigned int msg_fps_len = msg_fps.size();
    for (int i=0; i<msg_fps_len; i++){
        add_msgfile_bits(msg, msg_fps[i].c_str());
    }
    const unsigned int bits_encoded_per_grid = grid_w * grid_h -1;
    const unsigned int diff = bits_encoded_per_grid - msg.size() % bits_encoded_per_grid;
    for (int i=0; i<diff; i++)
        msg.push_back(i & 1);
        // Alternate the junk bit so that we get a chequered style, a cheap pattern that is likely to result in a high complexity (which is what we desire)
    #ifdef DEBUG1
        std::cout << (msg.size() >> 3) << "B to encode" << std::endl;
    #endif
    
    bool msg_exhausted = false;
    
    for (int i=0; i<img_fps_len; i++){
        im_mat = cv::imread(img_fps[i], CV_LOAD_IMAGE_COLOR);
        // WARNING: OpenCV loads images as BGR, not RGB
        
        w = im_mat.cols;
        h = im_mat.rows;
        
        n_hztl_grids = w / grid_w;
        n_vert_grids = h / grid_h;
        
        bitplane = cv::Mat::zeros(w, h, CV_8UC1);
        // CV_8UC1 - aka CV_8UC(1) - means 1 channel of uint8
        
        std::vector<cv::Mat> channel_planes;
        cv::split(im_mat, channel_planes);
        
        std::vector<cv::Mat> channel_planes_orig;
        cv::split(im_mat.clone(), channel_planes_orig);
        
        std::vector<float> complexities;
        complexities.reserve(n_hztl_grids * n_vert_grids);
        // Speeds up assignments by reserving required memory beforehand
        
        for (int j=0; j<n_channels; j++){
            tmparrorig  = channel_planes_orig[j];
            tmparr      = bitshift_down(channel_planes[j], w, h);
            #ifdef DEBUG5
                print_cv_arr("channel_planes_orig", j, tmparrorig);
            #endif
            #ifdef DEBUG6
                print_cv_arr("bitshifted down    ", j, tmparr);
            #endif
            tmparr      = tmparr ^ tmparrorig;
            #ifdef DEBUG6
                print_cv_arr("XOR'd with orig    ", j, tmparr);
            #endif
            // Happily, bitshifting down makes the first bits of (arr >> 1) are 0 - so the first digit of the CGC'd arr is retained
            for (int k=0; k<n_bits; k++){
                bitandshift(tmparr, bitplane, w, h, k);
                #ifdef DEBUG6
                    print_cv_arr("bitplane", k, bitplane);
                #endif
                // i.e. dest is bitplane
                
                if (iterate_over_bitgrids(complexities, bitplane, min_complexity, n_hztl_grids, n_vert_grids, w, h, grid_w, grid_h, grid_fnct, msg) == 0){
                    msg_exhausted = true;
                    #ifdef DEBUG1
                        std::cout << "Incomplete histogram as exited early - msg was exhausted" << std::endl;
                    #endif
                    goto msg_exhausted;
                }
            }
        }
        
        msg_exhausted:
        #ifdef DEBUG1
            print_histogram(complexities, n_bins, n_binchars);
        #endif
        if (msg_exhausted)
            goto exit;
    }
    exit:
    return 0;
}
