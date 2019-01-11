#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <fstream>
#include <functional> // for std::function


void bitshift_down(cv::Mat arr, unsigned int w, unsigned int h){
    for (int i=0; i<w; i++){
        for (int j=0; j<h; j++){
            arr.at<uint_fast8_t>(i,j) = arr.at<uint_fast8_t>(i,j) >> 1;
        }
    }
}

void bitandshift(cv::Mat arr, cv::Mat dest, unsigned int w, unsigned int h, unsigned int n){
    for (int i=0; i<w; i++)
        for (int j=0; j<h; j++)
            dest.at<uint_fast8_t>(i,j) = (arr.at<uint_fast8_t>(i,j) >> n) & 1;
}

void print_cv_arr(const char* name, int i, cv::Mat arr){
    std::cout << name << i << std::endl << arr << std::endl << std::endl;
}

void decode_grid(cv::Mat grid){
    std::cout << "decode_grid" << std::endl << grid << std::endl << std::endl;
}

float grid_complexity(cv::Mat grid, unsigned int grid_w, unsigned int grid_h){
    return 0.5;
}

void iterate_over_bitgrids(cv::Mat bitplane, float min_complexity, unsigned int bitplane_w, unsigned int bitplane_h, unsigned int grid_w, unsigned int grid_h, std::function<void(cv::Mat&)> grid_fnct){
    const unsigned int n_hztl_grids = bitplane_w / grid_w;
    const unsigned int n_vert_grids = bitplane_h / grid_h;
    float complexity;
    // Note that we will be doing millions of operations, and do not mind rounding errors - the important thing here is that we get consistent results
    cv::Mat grid;
    unsigned long int n_grids_used = 0;
    for (int i=0; i<n_hztl_grids; i++){
        std::cout << "Processing " << i << " out of " << n_hztl_grids << " grids" << std::endl;
        for (int j=0; j<n_vert_grids; j++){
            cv::Rect grid_shape(cv::Point(i*grid_w, j*grid_h), cv::Size(grid_w, grid_h));
            bitplane(grid_shape).copyTo(grid);
            complexity = grid_complexity(grid, grid_w, grid_h);
            if (complexity < min_complexity)
                continue;
            grid_fnct(grid);
            n_grids_used++;
        }
        std::cout << n_grids_used << " grids with complexity >= " << min_complexity << std::endl;
    }
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
            tmparr      = channel_planes[i];
            tmparrorig  = channel_planes_orig[i];
            bitshift_down(tmparr, w, h);
            print_cv_arr("channel_planes_orig", i, tmparrorig);
            print_cv_arr("bitshifted down    ", i, tmparr);
            tmparr      = tmparr ^ tmparrorig;
            print_cv_arr("XOR'd with orig    ", i, tmparr);
            // Bitshifting down ensures that the first bits of (arr >> 1) are 0 - so the first digit of the CGC'd arr is retained
            for (int j=0; j<n_bits; j++){
                bitandshift(tmparr, bitplane, w, h, j);
                print_cv_arr("bitplane", j, bitplane);
                iterate_over_bitgrids(bitplane, 0.45, w, h, grid_w, grid_h, decode_grid);
            }
            std::cout << std::endl << std::endl;
        }
    }
    return 0;
}
