#include <cblas.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <fstream>


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
    
    std::vector<bool> msg = msgfile2bits(argv[1]);
    
    std::cout << "msg: ";
    for (int i=0; i<msg.size(); i++)
        std::cout << msg[i];
    std::cout << std::endl;
    
    cv::Mat im;
    unsigned int w;
    unsigned int h;
    unsigned int entries;
    unsigned int im_bits__length;
    const unsigned int n_channels = 3;
    uint8_t* arr;
    std::vector<bool> im_bits;
    for (int i=2; i<argc; i++){
        im = cv::imread(argv[i], CV_LOAD_IMAGE_COLOR);
        // WARNING: OpenCV loads images as BGR, not RGB
        
        arr = im.data;
        
        w = im.cols;
        h = im.rows;
        entries = w*h*n_channels;
        
        for (int i=0; i<entries; i++){
            im_bits.push_back(arr[i] & 1);
            im_bits.push_back((arr[i] & 2) >> 1);
            im_bits.push_back((arr[i] & 4) >> 2);
            im_bits.push_back((arr[i] & 8) >> 3);
            im_bits.push_back((arr[i] & 16) >> 4);
            im_bits.push_back((arr[i] & 32) >> 5);
            im_bits.push_back((arr[i] & 64) >> 6);
            im_bits.push_back((arr[i] & 128) >> 7);
        }
        
        im_bits__length = entries << 3;
        
    }
    return 0;
}
