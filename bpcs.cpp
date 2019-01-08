#include <cblas.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <fstream>
#include <Eigen/Core>



cv::Mat bitshift_down(uint8_t* arr, unsigned int w, unsigned int h, unsigned int n){
    unsigned int index = 0;
    for (int i=0; i<w; i++){
        for (int j=0; j<h; j++){
            arr[index] = arr[index] >> n;
            index++;
        }
    }
}


std::vector<bool> msgfile2bits(char* fp){
    std::vector<bool> msg;
    std::ifstream msg_file(fp, std::ios::binary);
    char c;
    std::cout << "msg_bytes: ";
    while (msg_file.get(c)){
        std::cout << c;
        msg.push_back(c & 1);
        msg.push_back((c & 2) >> 1);
        msg.push_back((c & 4) >> 2);
        msg.push_back((c & 8) >> 3);
        msg.push_back((c & 16) >> 4);
        msg.push_back((c & 32) >> 5);
        msg.push_back((c & 64) >> 6);
        msg.push_back((c & 128) >> 7);
    }
    std::cout << std::endl;
    return msg;
}


int main(const int argc, char *argv[]){
    const unsigned int grid_w = 8;
    const unsigned int grid_h = 8;
    const unsigned int grid_size = grid_w * grid_h;
    
    // tmp
    std::vector<bool> msg = msgfile2bits(argv[1]);
    
    std::cout << "msg: ";
    for (int i=0; i<msg.size(); i++)
        std::cout << msg[i];
    std::cout << std::endl;
    
    cv::Mat im_mat;
    typedef Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic> MatrixXb;
    
    unsigned int w;
    unsigned int h;
    
    unsigned int entries;
    unsigned int im_bits__length;
    const unsigned int n_channels = 3;
    std::vector<bool> im_bits;
    for (int i=2; i<argc; i++){
        im_mat = cv::imread(argv[i], CV_LOAD_IMAGE_COLOR);
        // WARNING: OpenCV loads images as BGR, not RGB
        
        w = im_mat.cols;
        h = im_mat.rows;
        
        entries = w*h*n_channels;
        
        uint8_t* im = im_mat.data;
        
        cv::Mat ones = cv::Mat::ones(w, h, CV_8U);
        // CV_8U == 8bit unsigned int
        
        cv::Mat im0;
        cv::bitwise_and(im,                         ones, im0);
        cv::Mat im1;
        cv::bitwise_and(bitshift_down(im, w, h, 1), ones, im1);
        cv::Mat im2;
        cv::bitwise_and(bitshift_down(im, w, h, 2), ones, im1);
        cv::Mat im3;
        cv::bitwise_and(bitshift_down(im, w, h, 3), ones, im1);
        cv::Mat im4;
        cv::bitwise_and(bitshift_down(im, w, h, 4), ones, im1);
        cv::Mat im5;
        cv::bitwise_and(bitshift_down(im, w, h, 5), ones, im1);
        cv::Mat im6;
        cv::bitwise_and(bitshift_down(im, w, h, 6), ones, im1);
        cv::Mat im7;
        cv::bitwise_and(bitshift_down(im, w, h, 7), ones, im1);
    }
    std::cout << im << std::endl;
    return 0;
}
