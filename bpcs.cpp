#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
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

unsigned int xor_adj(std::vector<uint_fast8_t> arr, unsigned int len){
    unsigned int sum = 0;
    for (int i=1; i<len; i++)
        sum += arr[i-1] ^ arr[i];
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
    std::vector<uint_fast8_t> col_or_row_sum;
    unsigned int complexity_int;
    cv::reduce(grid, col_or_row_sum, 0, cv::REDUCE_SUM, CV_8UC1); // sum rows
    complexity_int  = xor_adj(col_or_row_sum, grid_w);
    
    cv::reduce(grid, col_or_row_sum, 1, cv::REDUCE_SUM, CV_8UC1); // sum columns
    // The summing and consequent bitshifting is just to XOR adjacent bits
    complexity_int += complexity_int  = xor_adj(col_or_row_sum, grid_h);
    
    return (float)complexity_int / (float)(grid_w * grid_h);
}

void iterate_over_bitgrids(cv::Mat bitplane, float min_complexity, unsigned int bitplane_w, unsigned int bitplane_h, unsigned int grid_w, unsigned int grid_h, std::function<void(cv::Mat&)> grid_fnct){
    const unsigned int n_hztl_grids = bitplane_w / grid_w;
    const unsigned int n_vert_grids = bitplane_h / grid_h;
    float complexity;
    // Note that we will be doing millions of operations, and do not mind rounding errors - the important thing here is that we get consistent results
    #ifdef DEBUG1
        std::vector<float> complexities;
    #endif
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
            #ifdef DEBUG1
                complexities.push_back(complexity);
            #endif
            if (complexity < min_complexity)
                continue;
            grid_fnct(grid);
            n_grids_used++;
        }
        #ifdef DEBUG1
            std::cout << n_grids_used << " grids with complexity >= " << min_complexity << std::endl;
        #endif
    }
    #ifdef DEBUG1
        std::cout << "Complexities:" << std::endl;
        unsigned int n = complexities.size();
        for (int i=0; i<n; i++)
            std::cout << "  " << complexities[i] << std::endl;
    #endif
}

int main(const int argc, char *argv[]){
    unsigned int grid_w = 3;
    unsigned int grid_h = 3;
    
    const unsigned int n_bits = 8;
    
    cv::Mat im_mat;
    cv::Mat tmparr;
    cv::Mat tmparrorig;
    cv::Mat bitplane;
    
    unsigned int w;
    unsigned int h;
    
    unsigned int im_bits__length;
    const unsigned int n_channels = 3;
    
    for (int i=2; i<argc; i++){
        im_mat = cv::imread(argv[i], CV_LOAD_IMAGE_COLOR);
        // WARNING: OpenCV loads images as BGR, not RGB
        
        w = im_mat.cols;
        h = im_mat.rows;
        
        bitplane = cv::Mat::zeros(w, h, CV_8UC1);
        
        std::vector<cv::Mat> channel_planes;
        cv::split(im_mat, channel_planes);
        
        std::vector<cv::Mat> channel_planes_orig;
        cv::split(im_mat.clone(), channel_planes_orig);
        
        for (int i=0; i<n_channels; i++){
            tmparrorig  = channel_planes_orig[i];
            tmparr      = bitshift_down(channel_planes[i], w, h);
            #ifdef DEBUG3
                print_cv_arr("channel_planes_orig", i, tmparrorig);
            #endif
            #ifdef DEBUG5
                print_cv_arr("bitshifted down    ", i, tmparr);
            #endif
            tmparr      = tmparr ^ tmparrorig;
            #ifdef DEBUG5
                print_cv_arr("XOR'd with orig    ", i, tmparr);
            #endif
            // Bitshifting down ensures that the first bits of (arr >> 1) are 0 - so the first digit of the CGC'd arr is retained
            for (int j=0; j<n_bits; j++){
                bitandshift(tmparr, bitplane, w, h, j);
                #ifdef DEBUG5
                    print_cv_arr("bitplane", j, bitplane);
                #endif
                iterate_over_bitgrids(bitplane, 0.45, w, h, grid_w, grid_h, decode_grid);
            }
            #ifdef DEBUG3
                std::cout << std::endl << std::endl;
            #endif
        }
    }
    return 0;
}
