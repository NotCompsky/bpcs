#include "bpcs.hpp"
#include "errors.hpp"
#include "png.hpp"

#ifdef EMBEDDOR
# include "utils.hpp" // for format_out_fp
# ifdef ONLY_COUNT
#  error "No use being an embeddor if only counting"
# endif
#endif

#include <compsky/macros/likely.hpp>
#include <cstring> // for malloc

/*
* NOTE: While extracting, it is assumed that there is at least one full grid of bytes embedded in the vessel image.
* However, this is not necessarily the case when using bpcs-count, because a use-case is to identify good vessel images before embedding.
* Hence the count must be corrected in case the stream is exhausted
*/


/*
 * Bitwise operations on matrices
 */
inline
constexpr
uint8_t to_cgc(const uint8_t n){
	return n ^ (n / 2);
}


constexpr
unsigned get_grid_complexity(const uchar* grid){
	unsigned sum = 0;
	
	// Complexity of horizontal neighbours
	size_t _indx = 0;
	for (auto j = 0;  j < GRID_H;  ++j){
		for (auto i = 0;  i < GRID_W - 1;  ++i){
			sum += grid[_indx] ^ grid[_indx + 1];
			_indx += 1;
		}
		_indx += 1; // Skip the last column
	}
	
	// Complexity of vertical neighbours
	for (auto i = 0;  i < GRID_W;  ++i){
		size_t _indx = i;
		for (auto j = 0;  j < GRID_H - 1;  ++j){
			sum += grid[_indx] ^ grid[_indx + GRID_W];
			_indx += GRID_W;
		}
	}
    
    return sum;
}


void BPCSStreamBuf::split_channels(){
	// RGBRGBRGBRGB... -> RRRR... GGGG... BBBB...
	for (auto i = 0;  i < this->w * this->h;  ++i){
		for (auto k = 0;  k < N_CHANNELS;  ++k){
			this->channel_byteplanes[k][i] = this->img_data[N_CHANNELS*i + k];
		}
	}
}

void BPCSStreamBuf::merge_channels(){
	// RRRR... GGGG... BBBB... -> RGBRGBRGBRGB...
	for (auto i = 0;  i < this->w * this->h;  ++i){
		for (auto k = 0;  k < N_CHANNELS;  ++k){
			this->img_data[N_CHANNELS*i + k] = this->channel_byteplanes[k][i];
		}
	}
}


void BPCSStreamBuf::convert_to_cgc(uchar* arr){
	for (uint64_t i = 0;  i < this->w * this->h;  ++i)
		arr[i]  =  to_cgc(arr[i]);
}

void BPCSStreamBuf::convert_from_cgc(uchar* arr){
	constexpr
	static
	const uint8_t from_cgc[256] = {0, 1, 3, 2, 7, 6, 4, 5, 15, 14, 12, 13, 8, 9, 11, 10, 31, 30, 28, 29, 24, 25, 27, 26, 16, 17, 19, 18, 23, 22, 20, 21, 63, 62, 60, 61, 56, 57, 59, 58, 48, 49, 51, 50, 55, 54, 52, 53, 32, 33, 35, 34, 39, 38, 36, 37, 47, 46, 44, 45, 40, 41, 43, 42, 127, 126, 124, 125, 120, 121, 123, 122, 112, 113, 115, 114, 119, 118, 116, 117, 96, 97, 99, 98, 103, 102, 100, 101, 111, 110, 108, 109, 104, 105, 107, 106, 64, 65, 67, 66, 71, 70, 68, 69, 79, 78, 76, 77, 72, 73, 75, 74, 95, 94, 92, 93, 88, 89, 91, 90, 80, 81, 83, 82, 87, 86, 84, 85, 255, 254, 252, 253, 248, 249, 251, 250, 240, 241, 243, 242, 247, 246, 244, 245, 224, 225, 227, 226, 231, 230, 228, 229, 239, 238, 236, 237, 232, 233, 235, 234, 192, 193, 195, 194, 199, 198, 196, 197, 207, 206, 204, 205, 200, 201, 203, 202, 223, 222, 220, 221, 216, 217, 219, 218, 208, 209, 211, 210, 215, 214, 212, 213, 128, 129, 131, 130, 135, 134, 132, 133, 143, 142, 140, 141, 136, 137, 139, 138, 159, 158, 156, 157, 152, 153, 155, 154, 144, 145, 147, 146, 151, 150, 148, 149, 191, 190, 188, 189, 184, 185, 187, 186, 176, 177, 179, 178, 183, 182, 180, 181, 160, 161, 163, 162, 167, 166, 164, 165, 175, 174, 172, 173, 168, 169, 171, 170};
	
	for (uint64_t i = 0;  i < this->w * this->h;  ++i)
		arr[i]  =  from_cgc[arr[i]];
}


inline void BPCSStreamBuf::byteplane_div2(uchar* arr){
	for (auto i = 0;  i < this->w * this->h;  ++i)
		arr[i] /= 2;
}

void BPCSStreamBuf::extract_grid(uchar* arr,  size_t indx){
	uchar* grid_itr = this->grid;
	for (auto j = 0;  j < GRID_H;  ++j){
		memcpy(grid_itr,  arr + indx,  GRID_W);
		indx += this->w;
		grid_itr += GRID_W;
	}
}

void BPCSStreamBuf::embed_grid(uchar* arr,  size_t indx){
	uchar* grid_itr = this->grid;
	for (auto j = 0;  j < GRID_H;  ++j){
		memcpy(arr + indx,  grid_itr,  GRID_W);
		indx += this->w;
		grid_itr += GRID_W;
	}
}

inline void BPCSStreamBuf::conjugate_grid(){
	for (auto j = 0;  j < GRID_H;  ++j)
		for (auto i = 0;  i < GRID_W;  ++i)
			this->grid[GRID_W*j + i] ^= 1 ^ ((i & 1) ^ (j & 1));
			// NOTE: chequerboard.val[0] should be 1, so that when the chequerboard is applied to grids, the grid[CONJUGATION_BIT_INDX] == 1 (to mark it as conjugated)
}

inline void BPCSStreamBuf::load_next_bitplane(){
	for (auto i = 0;  i < this->w * this->h;  ++i)
		this->bitplane[i] = this->channel_byteplanes[this->channel_n][i] & 1;
	this->byteplane_div2(this->channel_byteplanes[this->channel_n]);
}

void BPCSStreamBuf::load_next_channel(){
    this->bitplane_n = 0;
    this->load_next_bitplane();
}

void BPCSStreamBuf::load_next_img(){
	if(unlikely(this->img_n == this->n_imgs))
		handler(TOO_MUCH_DATA_TO_ENCODE);
    /* Load PNG file into array */
  #ifdef CHITTY_CHATTY
	fprintf(stderr,  "Loading image: %s\n",  this->img_fps[this->img_n]);
  #endif
	png::read(
		  this->img_fps[this->img_n]
		, this->n_imgs
		, this->img_data
		, this->img_data_sz
		, this->w
		, this->h
		, this->n_bitplanes
	  #ifdef EMBEDDOR
		, this->png_bg
	  #endif
	);
	const auto img_width_by_height = this->w * this->h;
	{
		uchar* itr = this->img_data + (N_CHANNELS * img_width_by_height);
		for (auto i = 0;  i < N_CHANNELS;  ++i){
			this->channel_byteplanes[i] = itr;
			itr += img_width_by_height;
		}
		this->bitplane = itr;
	}
    
	this->convert_to_cgc(this->img_data);
	this->split_channels();
  #ifdef EMBEDDOR
    if (!this->embedding)
  #endif
    
    
    this->channel_n = 0;
    
    #ifdef EMBEDDOR
    if (this->embedding){
		auto k = 0;
        for (auto j = 0;  j < N_CHANNELS;  ++j){
			for (auto i = 0;  i < this->n_bitplanes;  ++i){
				this->bitplanes[k] = (uchar*)malloc(this->w * this->h);
				if (unlikely(this->bitplanes[k] == nullptr))
					handler(OOM);
				for (auto _i = 0;  _i < this->w * this->h;  ++_i)
					this->bitplanes[k][_i] = this->channel_byteplanes[j][_i] & 1;
				++k;
				this->byteplane_div2(this->channel_byteplanes[j]);
            }
        }
        this->bitplane = this->bitplanes[0];
        this->bitplane_n = 0;
    } else {
    #endif
		this->bitplane = (uchar*)malloc(this->w * this->h);
        this->load_next_channel();
    #ifdef EMBEDDOR
    }
    #endif
    
    // Get first data grid
    this->set_next_grid();
    
    #ifdef EMBEDDOR
    if (!this->embedding){
    #endif
        if (this->img_n == this->img_n_offset){
            // If false, this function is being called from within get()
            if (this->grid[CONJUGATION_BIT_INDX])
                this->conjugate_grid();
        }
    #ifdef EMBEDDOR
    }
    #endif
}

void BPCSStreamBuf::set_next_grid(){
    int i = this->x;
    for (int j=this->y;  j <= this->h - GRID_H;  j+=GRID_H, i=0){
        while (i <= this->w - GRID_W){
			this->extract_grid(this->bitplane, i + j * this->w); // For cache locality, copy the grid - which is fragmented - to a compact small array
			const unsigned complexity = get_grid_complexity(this->grid);
            
            i += GRID_W;
            
            if (complexity >= this->min_complexity){
                this->x = i;
                this->y = j;
                
                return;
            }
        }
    }
    
    // If we are here, we have exhausted the bitplane
    
    this->x = 0;
    this->y = 0;
    
    ++this->bitplane_n;
    #ifdef EMBEDDOR
    if (this->embedding){
        if (this->bitplane_n < this->n_bitplanes * N_CHANNELS){
            this->bitplane = this->bitplanes[this->bitplane_n];
            goto try_again;
        }
    } else
    #endif
    if (this->bitplane_n < this->n_bitplanes){
        this->load_next_bitplane();
        goto try_again;
    } else if (++this->channel_n < N_CHANNELS){
        this->load_next_channel();
        goto try_again;
    }
    
    // If we are here, we have exhausted the image
    if (++this->img_n < this->n_imgs){
#ifdef EMBEDDOR
        if (this->embedding)
            this->save_im();
#endif
        this->load_next_img();
        return;
    }
    
    // If we are here, we have exhausted all images!
    // This is not necessarily alarming - this termination is used rather than returning status values for each get() call.
    
    #ifdef EMBEDDOR
    if (this->embedding){
		handler(TOO_MUCH_DATA_TO_ENCODE);
    }
    #endif
	exhausted = true;
    return;
    
    try_again:
    this->set_next_grid();
}

void BPCSStreamBuf::get(uchar* msg_arr){
    for (uint_fast8_t j=0; j<BYTES_PER_GRID; ++j){
		msg_arr[j] = 0;
        for (uint_fast8_t i=0; i<8; ++i){
			msg_arr[j] |= this->grid[8*j +i] << i;
        }
    }
    
    this->set_next_grid();
    
    if (this->grid[CONJUGATION_BIT_INDX] != 0)
        this->conjugate_grid();
}

#ifdef EMBEDDOR
void BPCSStreamBuf::put(uchar* in){
    for (uint_fast8_t j=0; j<BYTES_PER_GRID; ++j){
        for (uint_fast8_t i=0; i<8; ++i){
            this->grid[8*j +i] = in[j] & 1;
            in[j] = in[j] >> 1;
        }
	}
    
    this->grid[CONJUGATION_BIT_INDX] = 0;
    
    if (get_grid_complexity(this->grid) < this->min_complexity)
        this->conjugate_grid();
    
	this->embed_grid(this->bitplane,  (this->x - GRID_W) + this->y * this->w);
    this->set_next_grid();
}

void BPCSStreamBuf::save_im(){
    uint_fast8_t k = N_CHANNELS * this->n_bitplanes;
    uint_fast8_t i = N_CHANNELS -1;
    
    do {
        // First bitplane (i.e. most significant bit) of each channel is unchanged by conversion to CGC
		--k;
		auto j = this->n_bitplanes - 1;
		for (auto _i = 0;  _i < this->w * this->h;  ++_i)
			this->channel_byteplanes[i][_i] = this->bitplanes[k][_i] << j;
		--j;
        do {
            --k;
			for (auto _i = 0;  _i < this->w * this->h;  ++_i)
				this->channel_byteplanes[i][_i] |= this->bitplanes[k][_i] << j;
        } while (j-- != 0);
    } while (i-- != 0);
    
	static char formated_out_fp[MAX_FILE_PATH_LEN];
	format_out_fp(this->out_fmt, this->img_fps[this->img_n], formated_out_fp);
	this->merge_channels();
	convert_from_cgc(this->img_data);
	
	png::write(formated_out_fp, this->png_bg, this->img_data, this->w, this->h, this->n_bitplanes);
}
#endif
