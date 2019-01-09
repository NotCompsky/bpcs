#include <cblas.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <fstream>
#include <Eigen/Core>
#include <opencv2/core/eigen.hpp>



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
    
    cv::Mat img;
    typedef Eigen::Matrix<uint8_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> MatrixXuint;
    typedef Eigen::Stride<Eigen::Dynamic, 3> ThreeChStride;
    // Limits us to images with 3 channels
    typedef Eigen::Map<MatrixXuint, Eigen::Unaligned, ThreeChStride> CV2EigenMap;
    
    unsigned int w;
    unsigned int h;
    
    unsigned int entries;
    unsigned int im_bits__length;
    const unsigned int n_channels = 3;
    std::vector<bool> im_bits;
    for (int i=2; i<argc; i++){
        img = cv::imread(argv[i], CV_LOAD_IMAGE_COLOR);
        // WARNING: OpenCV loads images as BGR, not RGB
        
        w = img.cols;
        h = img.rows;
        
        ThreeChStride ch_stride(w*3, 3);
        
        CV2EigenMap B((img.data) + 0, w, h, ch_stride);
        CV2EigenMap G((img.data) + 1, w, h, ch_stride);
        CV2EigenMap R((img.data) + 2, w, h, ch_stride);
        
        std::cout << R << std::endl << std::endl;
        std::cout << G << std::endl << std::endl;
        std::cout << B << std::endl << std::endl;
    }
    return 0;
}
