#include <cblas.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <fstream>
#include <Eigen/Core>


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
    
    cv::Mat im;
    typedef Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic> MatrixXb;
    
    unsigned int entries;
    unsigned int im_bits__length;
    const unsigned int n_channels = 3;
    std::vector<bool> im_bits;
    for (int i=2; i<argc; i++){
        im = cv::imread(argv[i], CV_LOAD_IMAGE_COLOR);
        // WARNING: OpenCV loads images as BGR, not RGB
        
        const unsigned int w = im.cols;
        const unsigned int h = im.rows;
        
        entries = w*h*n_channels;
        
        for (int i=0; i<entries; i++){
            im_bits.push_back(im.data[i] & 1);
            im_bits.push_back((im.data[i] & 2) >> 1);
            im_bits.push_back((im.data[i] & 4) >> 2);
            im_bits.push_back((im.data[i] & 8) >> 3);
            im_bits.push_back((im.data[i] & 16) >> 4);
            im_bits.push_back((im.data[i] & 32) >> 5);
            im_bits.push_back((im.data[i] & 64) >> 6);
            im_bits.push_back((im.data[i] & 128) >> 7);
        }
        
        im_bits__length = entries << 3;
        
        MatrixXb arr(h, w);
        
        std::cout << arr << std::endl;
    }
    return 0;
}
