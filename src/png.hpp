#pragma once

#include "errors.hpp"
#include <compsky/macros/likely.hpp>
#include "typedefs.hpp"
#ifdef USE_LIBSPNG
# include <spng.h>
#else
# include <png.h>
#endif


inline
void set_img_data_sz(uchar*& img_data, size_t& img_data_sz, const uint32_t img_width_by_height, const int n_imgs){
	if (img_data_sz == 0){
		img_data_sz = (N_CHANNELS + N_CHANNELS + 1) * img_width_by_height;
		if (n_imgs != 1)
			img_data_sz *= 2;
		img_data = (uchar*)malloc(img_data_sz);
	} else if (img_data_sz < img_width_by_height * N_CHANNELS){
		img_data_sz = (N_CHANNELS + N_CHANNELS + 1) * img_width_by_height;
		img_data = (uchar*)realloc(img_data,  2 * img_data_sz);
	  #ifdef TESTS
		if (unlikely(img_data == nullptr))
			handler(OOM);
	  #endif
	}
}


namespace png {

/*
#ifdef USE_LIBSPNG
# ifdef EMBEDDOR
#  error "libspng cannot yet write PNG images"
# endif
inline
void read(
	  const char* const fp
	, const int n_imgs
	, uchar*& img_data
	, size_t& img_data_sz
	, unsigned& w
	, unsigned& h
	, int& n_bitplanes
){
	spng_ctx* const ctx = spng_ctx_new(0);
	spng_set_png_buffer(ctx, buf, buf_size);
	spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &out_size);
	spng_decode_image(ctx, out, out_size, SPNG_FMT_RGBA8, 0);
	spng_ctx_free(ctx);
	
	struct spng_ihdr ihdr;
	r = spng_get_ihdr(ctx, &ihdr);
	set_img_data_sz(img_data,  img_data_sz,  w * h,  n_imgs);
	// https://github.com/randy408/libspng/blob/master/examples/example.c
}
#endif


#else*/
inline
void read(
	  const char* const fp
	, const int n_imgs
	, uchar*& img_data
	, size_t& img_data_sz
	, unsigned& w
	, unsigned& h
	, int& n_bitplanes
#ifdef EMBEDDOR
	, png_color_16p& png_bg
#endif
){
	FILE* png_file = fopen(fp, "rb");
	static uchar png_sig[8];
	
    fread(png_sig, 1, 8, png_file);
    if (!png_check_sig(png_sig, 8)){
		handler(INVALID_PNG_MAGIC_NUMBER);
    }
    
    auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        // Could not allocate memory
		handler(OOM);
  
    auto png_info_ptr = png_create_info_struct(png_ptr);
    if (!png_info_ptr){
        png_destroy_read_struct(&png_ptr, NULL, NULL);
		handler(CANNOT_CREATE_PNG_READ_STRUCT);
    }
    
    png_init_io(png_ptr, png_file);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, png_info_ptr);
	
  #ifdef TESTS
    int32_t colour_type;
  #endif
    
    png_get_IHDR(
        png_ptr, png_info_ptr, &w, &h, &n_bitplanes
      #ifdef TESTS
        , &colour_type
      #else
        , nullptr
      #endif
        , NULL, NULL, NULL
    );
	
    #ifdef TESTS
		if (unlikely(n_bitplanes > MAX_BITPLANES))
			handler(TOO_MANY_BITPLANES);
		if (unlikely(colour_type != PNG_COLOR_TYPE_RGB))
			handler(IMAGE_IS_NOT_RGB);
    #endif
    
    #ifdef EMBEDDOR
	png_bg = nullptr;
	png_get_bKGD(png_ptr, png_info_ptr, &png_bg);
    #endif
    
    uint32_t rowbytes;
    
    png_read_update_info(png_ptr, png_info_ptr);
    
    rowbytes = png_get_rowbytes(png_ptr, png_info_ptr);
    
    #ifdef TESTS
		if (unlikely(png_get_channels(png_ptr, png_info_ptr) != N_CHANNELS))
			handler(WRONG_NUMBER_OF_CHANNELS);
    #endif
	
	set_img_data_sz(img_data,  img_data_sz,  w * h,  n_imgs);
	
	uchar* row_ptrs[h];
    for (uint32_t i=0; i<h; ++i)
        row_ptrs[i] = img_data + i*rowbytes;
    
    png_read_image(png_ptr, row_ptrs);
    
    fclose(png_file);
    png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
}


#ifdef EMBEDDOR
inline
void write(
	  const char* const out_fp
	, png_color_16p& png_bg
	, const uchar* const img_data
	, const uint32_t w
	, const uint32_t h
	, const int n_bitplanes
){
	FILE* png_file = fopen(out_fp, "wb");
    auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    #ifdef TESTS
    if (!png_file){
		handler(COULD_NOT_OPEN_PNG_FILE);
    }
    if (!png_ptr){
		handler(OOM);
    }
    #endif
    
    auto png_info_ptr = png_create_info_struct(png_ptr);
    
    if (!png_info_ptr){
		handler(CANNOT_CREATE_PNG_INFO_STRUCT);
    }
    
    if (setjmp(png_jmpbuf(png_ptr))){
		handler(PNG_ERROR_1);
    }
    
    png_init_io(png_ptr, png_file);
    
    if (setjmp(png_jmpbuf(png_ptr))){
		handler(PNG_ERROR_2);
    }
    
	if (png_bg != nullptr)
		png_set_bKGD(png_ptr, png_info_ptr, png_bg);
    
	png_set_IHDR(png_ptr, png_info_ptr, w, h, n_bitplanes, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    
    png_write_info(png_ptr, png_info_ptr);
    
    if (setjmp(png_jmpbuf(png_ptr))){
		handler(PNG_ERROR_3);
    }
    
	const uchar* row_ptrs[h];
    for (uint32_t i=0; i<h; ++i)
        row_ptrs[i] = img_data + i*N_CHANNELS*w;
    
    png_write_image(png_ptr, const_cast<unsigned char**>(row_ptrs));
    
    if (setjmp(png_jmpbuf(png_ptr))){
		handler(PNG_ERROR_4);
    }
    
    png_write_end(png_ptr, NULL);
}
#endif


}; // namespace png
