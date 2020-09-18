#pragma once

#include "typedefs.hpp"
#include "png.hpp"

#define CONJUGATION_BIT_INDX (GRID_W * GRID_H - 1)
#define BYTES_PER_GRID ((GRID_W * GRID_H - 1) / 8)


inline
uint8_t a2i_1or2digits(const char* const str){
	uint8_t n = str[0] - '0';
	if (str[1] == 0)
		return n;
	return (n * 10) + str[1] - '0';
}


class BPCSStreamBuf {
  public:
    /* Constructors */
    BPCSStreamBuf(const uint8_t min_complexity, int img_n, int n_imgs, char** im_fps
                // WARNING: img_fps is just argv which needs an index to skip the options
                // Use double pointer rather than array of pointers due to constraints on constructor initialisation
                #ifdef EMBEDDOR
                  , bool emb
                  , char* outfmt
                #endif
                ):
	exhausted(false)
  #ifdef EMBEDDOR
	, embedding(emb)
	, out_fmt(outfmt)
  #endif
	, x(0)
	, y(0)
	, min_complexity(min_complexity)
	, img_n(img_n)
	, img_n_offset(img_n)
	, n_imgs(n_imgs)
	, img_fps(im_fps)
	, img_data_sz(0)
    {}
    
    
	bool exhausted;
    
    #ifdef EMBEDDOR
    const bool embedding;
    char* out_fmt = NULL;
    #endif
    
	void get(uchar* msg_arr);
    
    
    
	uchar* img_data; // Points to a contiguous portion of memory. The first section is as large as 3 sections, and stores the image pixels in RGBRGBRGB fashion (as decoded by LibPNG); the next 3 sections store each channel's byteplane; the last section stores the current bitplane.
	size_t img_data_sz;
	
	uint32_t w;
	uint32_t h;
    
    void load_next_img(); // Init
    
    #ifdef EMBEDDOR
    void put(uchar arr[BYTES_PER_GRID]);
    void save_im(); // End
    #endif
  private:
    int x; // the current grid is the (x-1)th grid horizontally and yth grid vertically (NOT the coordinates of the corner of the current grid of the current image)
    int y;
    
    

    const uint8_t min_complexity;
    
    uint8_t channel_n;
	int n_bitplanes;
    uint8_t bitplane_n;
    
    const int img_n_offset;
    int img_n;
    int n_imgs;
    
	uchar grid[GRID_H * GRID_W];
    
	uchar* bitplane;
    
    #ifdef EMBEDDOR
	uchar* bitplanes[MAX_BITPLANES];
    #endif
    
	uchar* channel_byteplanes[N_CHANNELS];
    
    #ifdef EMBEDDOR
    png_color_16p png_bg;
    #endif
    
    char** img_fps;
    
	void convert_to_cgc(uchar* arr);
	void convert_from_cgc(uchar* arr);
    void set_next_grid();
    void load_next_bitplane();
    void load_next_channel();
	void split_channels();
	void merge_channels();
    
	inline void byteplane_div2(uchar* arr);
	void extract_grid(uchar* arr,  size_t indx);
	void embed_grid(uchar* arr,  size_t indx);
    inline void conjugate_grid();
    
};
