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


cv::Mat bitshift_down(cv::Mat arr, unsigned int w, unsigned int h){
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

unsigned int xor_adj(cv::Mat arr, unsigned int w, unsigned int h){
    unsigned int sum = 0;
    for (int j=0; j<h; j++)
        for (int i=1; i<w; i++)
            sum += arr.at<uint_fast8_t>(i,j) ^ arr.at<uint_fast8_t>(i-1,j);
    for (int i=0; i<w; i++)
        for (int j=1; j<h; j++)
            sum += arr.at<uint_fast8_t>(i,j) ^ arr.at<uint_fast8_t>(i,j-1);
    return sum;
}

void bitandshift(cv::Mat arr, cv::Mat dest, unsigned int w, unsigned int h, unsigned int n){
    for (int i=0; i<w; i++)
        for (int j=0; j<h; j++)
            dest.at<uint_fast8_t>(i,j) = (arr.at<uint_fast8_t>(i,j) >> n) & 1;
}

#ifdef DEBUG1
void print_cv_arr(const char* name, int i, cv::Mat arr){
    std::cout << name << i << std::endl << arr << std::endl << std::endl;
}
#endif

void decode_grid(cv::Mat grid){
    #ifdef DEBUG3
        std::cout << "decode_grid" << std::endl << grid << std::endl << std::endl;
    #endif
}

float grid_complexity(cv::Mat grid, unsigned int grid_w, unsigned int grid_h){
    unsigned int complexity_int  = xor_adj(grid, grid_w, grid_h);
    
    return (float)complexity_int / (float)(grid_w * (grid_h -1) + grid_h * (grid_w -1));
}

cv::Mat iterate_over_bitgrids(cv::Mat bitplane, float min_complexity, unsigned int bitplane_w, unsigned int bitplane_h, unsigned int grid_w, unsigned int grid_h, std::function<void(cv::Mat&)> grid_fnct){
    const unsigned int n_hztl_grids = bitplane_w / grid_w;
    const unsigned int n_vert_grids = bitplane_h / grid_h;
    float complexity;
    // Note that we will be doing millions of operations, and do not mind rounding errors - the important thing here is that we get consistent results
    cv::Mat complexities = cv::Mat::zeros(n_hztl_grids, n_vert_grids, CV_32F);
    cv::Mat grid;
    unsigned long int n_grids_used = 0;
    for (int i=0; i<n_hztl_grids; i++){
        #ifdef DEBUG1
            std::cout << "Processing " << i << " out of " << n_hztl_grids << " grids" << std::endl;
        #endif
        for (int j=0; j<n_vert_grids; j++){
            cv::Rect grid_shape(cv::Point(i*grid_w, j*grid_h), cv::Size(grid_w, grid_h));
            bitplane(grid_shape).copyTo(grid);
            complexity = grid_complexity(grid, grid_w, grid_h);
            
            complexities.at<float>(i, j) = complexity;
            
            if (complexity < min_complexity)
                continue;
            grid_fnct(grid);
            n_grids_used++;
        }
        #ifdef DEBUG1
            std::cout << n_grids_used << " grids with complexity >= " << min_complexity << std::endl;
        #endif
    }
    return complexities;
}

int main(const int argc, char *argv[]){
    args::ArgumentParser parser("(En|De)code BPCS", "This goes after the options.");
    args::HelpFlag                      Ahelp           (parser, "help", "Display help", {'h', "help"});
    args::ValueFlag<uint_fast8_t>       An_bits         (parser, "n_bits", "n_bits", {'b', "bits"});
    args::ValueFlag<uint_fast8_t>       An_chns         (parser, "n_chns", "n_chns", {'c', "channels"});
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
    
    unsigned int grid_w = 3;
    unsigned int grid_h = 3;
    
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
    
    std::string mode;
    if (Amsg_fps){
        std::vector<std::string>msg_fps = args::get(Amsg_fps);
        mode = "Encoding";
    } else {
        mode = "Decoding";
    }
    
    cv::Mat im_mat;
    cv::Mat tmparr;
    cv::Mat tmparrorig;
    cv::Mat bitplane;
    
    unsigned int w;
    unsigned int h;
    
    std::vector<std::string>img_fps = args::get(Aimg_fps);
    unsigned int img_fps_len        = img_fps.size();
    
    float min_complexity = args::get(Amin_complexity);
    
    #ifdef DEBUG1
        std::cout << mode << " " << +img_fps_len << " img inputs, using: Complexity >= " <<  +min_complexity << ", " << +n_channels << " channels, " << +n_bits << " bits" << std::endl;
        // Preceding primitive data type with `+` operator makes it print ASCII character representing that numerical value, rather than the ASCII character of that value
    #endif
    
    for (int i=0; i<img_fps_len; i++){
        im_mat = cv::imread(img_fps[i], CV_LOAD_IMAGE_COLOR);
        // WARNING: OpenCV loads images as BGR, not RGB
        
        w = im_mat.cols;
        h = im_mat.rows;
        
        bitplane = cv::Mat::zeros(w, h, CV_8UC1);
        
        std::vector<cv::Mat> channel_planes;
        cv::split(im_mat, channel_planes);
        
        std::vector<cv::Mat> channel_planes_orig;
        cv::split(im_mat.clone(), channel_planes_orig);
        
        std::vector<cv::Mat> complexity_mats;
        
        for (int j=0; j<n_channels; j++){
            tmparrorig  = channel_planes_orig[j];
            tmparr      = bitshift_down(channel_planes[j], w, h);
            #ifdef DEBUG3
                print_cv_arr("channel_planes_orig", j, tmparrorig);
            #endif
            #ifdef DEBUG5
                print_cv_arr("bitshifted down    ", j, tmparr);
            #endif
            tmparr      = tmparr ^ tmparrorig;
            #ifdef DEBUG5
                print_cv_arr("XOR'd with orig    ", j, tmparr);
            #endif
            // Bitshifting down ensures that the first bits of (arr >> 1) are 0 - so the first digit of the CGC'd arr is retained
            for (int k=0; k<n_bits; k++){
                bitandshift(tmparr, bitplane, w, h, k);
                #ifdef DEBUG5
                    print_cv_arr("bitplane", k, bitplane);
                #endif
                complexity_mats.push_back(iterate_over_bitgrids(bitplane, min_complexity, w, h, grid_w, grid_h, decode_grid));
            }
            #ifdef DEBUG3
                std::cout << std::endl << std::endl;
            #endif
        }
        cv::Mat hist;
        const int n_bins = 11;
        float range[] = {0.0, 1.0};
        const float* hist_range = {range};
        // 2nd var is complexity_mats.size()
        cv::calcHist(&complexity_mats[0], 1, 0, cv::Mat(), hist, 1, &n_bins, &hist_range, true, false);
        std::cout << hist << std::endl;
    }
    return 0;
}
