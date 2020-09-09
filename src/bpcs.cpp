#include <opencv2/core/core.hpp>
#include <png.h>
#include <unistd.h> // for STD(IN|OUT)_FILENO
#include <array>
#include <compsky/macros/likely.hpp>

#ifdef EMBEDDOR
# include "utils.hpp" // for format_out_fp
# ifdef ONLY_COUNT
#  error "No use being an embeddor if only counting"
# endif
#endif

#define CONJUGATION_BIT_INDX (GRID_W * GRID_H - 1)
#define BYTES_PER_GRID ((GRID_W * GRID_H - 1) / 8)


enum {
	NO_ERROR,
	MISC_ERROR,
	TOO_MUCH_DATA_TO_ENCODE,
	INVALID_PNG_MAGIC_NUMBER,
	OOM,
	CANNOT_CREATE_PNG_READ_STRUCT,
	EMPTY_BITPLANE,
	COULD_NOT_OPEN_PNG_FILE,
	CANNOT_CREATE_PNG_INFO_STRUCT,
	PNG_ERROR_1,
	PNG_ERROR_2,
	PNG_ERROR_3,
	PNG_ERROR_4,
	N_ERRORS
};
#ifdef NO_EXCEPTIONS
# define handler(msg) exit(msg)
#else
# include <stdexcept>
# define handler(msg) throw std::runtime_error(handler_msgs[msg])
constexpr static
const char* const handler_msgs[] = {
	"No error",
	"Misc error",
	"Too much data to encode",
	"Invalid PNG magic number",
	"Out of memory",
	"Cannot create PNG read struct",
	"Empty bitplane",
	"Could not open PNG file",
	"Could not create PNG info struct",
	"PNG error 1",
	"PNG error 2",
	"PNG error 3",
	"PNG error 4"
};
#endif

typedef cv::Matx<uchar, GRID_H,   GRID_W> Matx99uc;
typedef cv::Matx<uchar, GRID_H, GRID_W-1> Matx98uc;
typedef cv::Matx<uchar, GRID_H-1, GRID_W> Matx89uc;


/*
 * Bitwise operations on OpenCV Matrices
 */
inline void cv_div2(cv::Mat& arr, cv::Mat& dest){
    cv::Mat m;
    m = ((arr & 2) / 2) & (arr & 1);
    dest  = arr/2;
    dest -= m;
}

inline
constexpr
uint8_t to_cgc(const uint8_t n){
	return n ^ (n / 2);
}

inline
void convert_to_cgc(cv::Mat &arr){
	for (uint64_t i = 0;  i < (uint64_t)arr.cols * (uint64_t)arr.rows;  ++i)
		arr.data[i]  =  to_cgc(arr.data[i]);
}

inline
void convert_from_cgc(cv::Mat &arr){
	constexpr
	static
	const uint8_t from_cgc[256] = {0, 1, 3, 2, 7, 6, 4, 5, 15, 14, 12, 13, 8, 9, 11, 10, 31, 30, 28, 29, 24, 25, 27, 26, 16, 17, 19, 18, 23, 22, 20, 21, 63, 62, 60, 61, 56, 57, 59, 58, 48, 49, 51, 50, 55, 54, 52, 53, 32, 33, 35, 34, 39, 38, 36, 37, 47, 46, 44, 45, 40, 41, 43, 42, 127, 126, 124, 125, 120, 121, 123, 122, 112, 113, 115, 114, 119, 118, 116, 117, 96, 97, 99, 98, 103, 102, 100, 101, 111, 110, 108, 109, 104, 105, 107, 106, 64, 65, 67, 66, 71, 70, 68, 69, 79, 78, 76, 77, 72, 73, 75, 74, 95, 94, 92, 93, 88, 89, 91, 90, 80, 81, 83, 82, 87, 86, 84, 85, 255, 254, 252, 253, 248, 249, 251, 250, 240, 241, 243, 242, 247, 246, 244, 245, 224, 225, 227, 226, 231, 230, 228, 229, 239, 238, 236, 237, 232, 233, 235, 234, 192, 193, 195, 194, 199, 198, 196, 197, 207, 206, 204, 205, 200, 201, 203, 202, 223, 222, 220, 221, 216, 217, 219, 218, 208, 209, 211, 210, 215, 214, 212, 213, 128, 129, 131, 130, 135, 134, 132, 133, 143, 142, 140, 141, 136, 137, 139, 138, 159, 158, 156, 157, 152, 153, 155, 154, 144, 145, 147, 146, 151, 150, 148, 149, 191, 190, 188, 189, 184, 185, 187, 186, 176, 177, 179, 178, 183, 182, 180, 181, 160, 161, 163, 162, 167, 166, 164, 165, 175, 174, 172, 173, 168, 169, 171, 170};
	
	for (uint64_t i = 0;  i < (uint64_t)arr.cols * (uint64_t)arr.rows;  ++i)
		arr.data[i]  =  from_cgc[arr.data[i]];
}



/*
 * Initialise chequerboard
 */
static Matx99uc chequerboard;
void init_chequerboard(){
	// NOTE: Matx template initialisation has changed, soo we have to do this for now.
	for (auto j = 0;  j < GRID_H;  ++j)
		for (auto i = 0;  i < GRID_W;  ++i)
			chequerboard.val[GRID_W*j + i] = 1 ^ ((i & 1) ^ (j & 1));
			// NOTE: chequerboard.val[0] should be 1, so that when the chequerboard is applied to grids, the grid[CONJUGATION_BIT_INDX] == 1 (to mark it as conjugated)
}










class BPCSStreamBuf {
    // Based on excellent post by krzysztoftomaszewski
    // src https://artofcode.wordpress.com/2010/12/12/deriving-from-stdstreambuf/
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
	exhausted(false),
    #ifdef EMBEDDOR
        embedding(emb), out_fmt(outfmt),
    #endif
    x(0), y(0), min_complexity(min_complexity), img_n(img_n), img_n_offset(img_n), n_imgs(n_imgs), img_fps(im_fps)
    
    {}
    
    
	bool exhausted;
    
    #ifdef EMBEDDOR
    const bool embedding;
    char* out_fmt = NULL;
    #endif
    
	void get(uchar* msg_arr);
    
    
    
    uchar* img_data;
    
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
    
    Matx99uc grid{GRID_H, GRID_W, CV_8UC1};
    
    Matx98uc xor_adj_mat1{GRID_H,   GRID_W-1, CV_8UC1};
    Matx89uc xor_adj_mat2{GRID_H-1, GRID_W,   CV_8UC1};
    
    cv::Rect xor_adj_rect1{cv::Point(0, 0), cv::Size(GRID_W-1, GRID_H)};
    cv::Rect xor_adj_rect2{cv::Point(1, 0), cv::Size(GRID_W-1, GRID_H)};
    cv::Rect xor_adj_rect3{cv::Point(0, 0), cv::Size(GRID_W, GRID_H-1)};
    cv::Rect xor_adj_rect4{cv::Point(0, 1), cv::Size(GRID_W, GRID_H-1)};
    
    cv::Mat grid_orig{GRID_H, GRID_W, CV_8UC1};
    
    cv::Mat bitplane;
    
    #ifdef EMBEDDOR
    cv::Mat bitplanes[MAX_BITPLANES];
    #endif
    
    std::vector<cv::Mat> channel_byteplanes;
    
    cv::Mat im_mat;
    
    #ifdef EMBEDDOR
    png_color_16p png_bg;
    #endif
    
    char** img_fps;
    
    void set_next_grid();
    void load_next_bitplane();
    void load_next_channel();
    
    inline uint8_t get_grid_complexity(Matx99uc&);
    inline uint8_t get_grid_complexity(cv::Mat&);
    inline void conjugate_grid();
    
};



inline uint8_t BPCSStreamBuf::get_grid_complexity(Matx99uc &arr){
    uint8_t sum = 0;
    cv::bitwise_xor(arr.get_minor<GRID_H-1,GRID_W>(1,0), arr.get_minor<GRID_H-1,GRID_W>(0,0), this->xor_adj_mat2);
    sum += cv::sum(this->xor_adj_mat2)[0];
    
    cv::bitwise_xor(arr.get_minor<GRID_H,GRID_W-1>(0,1), arr.get_minor<GRID_H,GRID_W-1>(0,0), this->xor_adj_mat1);
    sum += cv::sum(this->xor_adj_mat1)[0];
    
    return sum;
}
inline uint8_t BPCSStreamBuf::get_grid_complexity(cv::Mat &arr){
    uint8_t sum = 0;
    cv::bitwise_xor(arr(this->xor_adj_rect2), arr(this->xor_adj_rect1), this->xor_adj_mat1);
    sum += cv::sum(this->xor_adj_mat1)[0];
    
    cv::bitwise_xor(arr(this->xor_adj_rect4), arr(this->xor_adj_rect3), this->xor_adj_mat2);
    sum += cv::sum(this->xor_adj_mat2)[0];
    
    return sum;
}

inline void BPCSStreamBuf::conjugate_grid(){
    cv::bitwise_xor(this->grid, chequerboard, this->grid);
    
    
}

inline void BPCSStreamBuf::load_next_bitplane(){
    this->bitplane = this->channel_byteplanes[this->channel_n] & 1;
    cv_div2(this->channel_byteplanes[this->channel_n], this->channel_byteplanes[this->channel_n]);
}

void BPCSStreamBuf::load_next_channel(){
    this->bitplane_n = 0;
    this->load_next_bitplane();
}

void BPCSStreamBuf::load_next_img(){
  #ifdef TESTS
    assert(this->img_n != this->n_imgs);
  #endif
    /* Load PNG file into array */
    FILE* png_file = fopen(this->img_fps[this->img_n], "rb");
    
    uchar png_sig[8];
    
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
    
    /* ERROR - incorrect use of incomplete type
    if (setjmp(png_ptr->jmpbuf)){
        png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
        handler(1);
    }
    */
    
    png_init_io(png_ptr, png_file);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, png_info_ptr);
    
    uint32_t w;
    uint32_t h;
  #ifdef TESTS
    int32_t colour_type;
  #endif
    
    png_get_IHDR(
        png_ptr, png_info_ptr, &w, &h, &this->n_bitplanes
      #ifdef TESTS
        , &colour_type
      #else
        , nullptr
      #endif
        , NULL, NULL, NULL
    );
    
    #ifdef TESTS
        assert(this->n_bitplanes <= MAX_BITPLANES);
		assert(colour_type == PNG_COLOR_TYPE_RGB);
    #endif
    
    #ifdef EMBEDDOR
	this->png_bg = nullptr;
    png_get_bKGD(png_ptr, png_info_ptr, &this->png_bg);
    #endif
    
    uint32_t rowbytes;
    
    png_read_update_info(png_ptr, png_info_ptr);
    
    rowbytes = png_get_rowbytes(png_ptr, png_info_ptr);
    
    #ifdef TESTS
        assert(png_get_channels(png_ptr, png_info_ptr) == N_CHANNELS);
    #endif
    
    this->img_data = (uchar*)malloc(rowbytes*h);
    
    uchar* row_ptrs[h];
    for (uint32_t i=0; i<h; ++i)
        row_ptrs[i] = this->img_data + i*rowbytes;
    
    png_read_image(png_ptr, row_ptrs);
    
    fclose(png_file);
    png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
    
    this->im_mat = cv::Mat(h, w, CV_8UC3, this->img_data);
    // WARNING: Loaded as RGB rather than OpenCV's default BGR
    
    convert_to_cgc(this->im_mat);
    
    #ifdef TESTS
        assert(this->im_mat.depth() == CV_8U);
        assert(this->im_mat.channels() == N_CHANNELS);
    #endif
    
    cv::split(this->im_mat, this->channel_byteplanes);
  #ifdef EMBEDDOR
    if (!this->embedding)
  #endif
        free(this->img_data);
    
    
    this->channel_n = 0;
    
    #ifdef EMBEDDOR
    if (this->embedding){
		auto k = 0;
        for (auto j = 0;  j < N_CHANNELS;  ++j){
			for (auto i = 0;  i < this->n_bitplanes;  ++i){
                this->bitplanes[k++] = this->channel_byteplanes[j] & 1;
                cv_div2(this->channel_byteplanes[j], this->channel_byteplanes[j]);
            }
        }
        this->bitplane = this->bitplanes[0];
        this->bitplane_n = 0;
    } else {
    #endif
        this->bitplane = cv::Mat::zeros(im_mat.rows, im_mat.cols, CV_8UC1); // Need to initialise for bitandshift
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
            if (this->grid.val[CONJUGATION_BIT_INDX])
                this->conjugate_grid();
        }
    #ifdef EMBEDDOR
    }
    #endif
    
    ++this->img_n;
}

void BPCSStreamBuf::set_next_grid(){
    uint8_t complexity;
    int i = this->x;
    for (int j=this->y;  j <= this->im_mat.rows - GRID_H;  j+=GRID_H, i=0){
        while (i <= this->im_mat.cols - GRID_W){
            cv::Rect grid_shape(cv::Point(i, j), cv::Size(GRID_W, GRID_H));
            
            this->grid_orig = this->bitplane(grid_shape);
            
            complexity = this->get_grid_complexity(this->grid_orig);
            
            i += GRID_W;
            
            if (complexity >= this->min_complexity){
                this->grid_orig = this->bitplane(grid_shape);
                this->grid_orig.copyTo(this->grid);
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
    if (img_n < this->n_imgs){
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
			msg_arr[j] |= this->grid.val[8*j +i] << i;
        }
    }
    
    this->set_next_grid();
    
    if (this->grid.val[CONJUGATION_BIT_INDX] != 0)
        this->conjugate_grid();
}

#ifdef EMBEDDOR
void BPCSStreamBuf::put(uchar* in){
    for (uint_fast8_t j=0; j<BYTES_PER_GRID; ++j){
        for (uint_fast8_t i=0; i<8; ++i){
            this->grid.val[8*j +i] = in[j] & 1;
            in[j] = in[j] >> 1;
        }
	}
    
    this->grid.val[CONJUGATION_BIT_INDX] = 0;
    
    if (this->get_grid_complexity(this->grid) < this->min_complexity)
        this->conjugate_grid();
    
    cv::Mat(this->grid).copyTo(this->grid_orig);
    this->set_next_grid();
}

void BPCSStreamBuf::save_im(){
    uint_fast8_t k = N_CHANNELS * this->n_bitplanes;
    uint_fast8_t i = N_CHANNELS -1;
    
    do {
        // First bitplane (i.e. most significant bit) of each channel is unchanged by conversion to CGC
        this->channel_byteplanes[i] = this->bitplanes[--k].clone();
		auto j = this->n_bitplanes - 1;
        this->channel_byteplanes[i] *= (1 << j--);
        do {
            --k;
            #ifdef TESTS
                if (this->bitplanes[k].cols == 0){
                    std::cerr << "bitplane " << +k << " is empty" << std::endl;
					handler(EMPTY_BITPLANE);
                }
            #endif
            
            this->bitplane = this->bitplanes[k].clone();
            this->bitplane *= (1 << j);
            cv::bitwise_or(this->channel_byteplanes[i], this->bitplane, this->channel_byteplanes[i]);
        } while (j-- != 0);
    } while (i-- != 0);
    
	static char formated_out_fp[1024];
	format_out_fp(this->out_fmt, this->img_fps[this->img_n -1], formated_out_fp);
    cv::merge(this->channel_byteplanes, this->im_mat);
	convert_from_cgc(this->im_mat);
    
    
	FILE* png_file = fopen(formated_out_fp, "wb");
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
    
	if (this->png_bg != nullptr)
		png_set_bKGD(png_ptr, png_info_ptr, this->png_bg);
    
    png_set_IHDR(png_ptr, png_info_ptr, this->im_mat.cols, this->im_mat.rows, this->n_bitplanes, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    
    png_write_info(png_ptr, png_info_ptr);
    
    if (setjmp(png_jmpbuf(png_ptr))){
		handler(PNG_ERROR_3);
    }
    
    uchar* row_ptrs[this->im_mat.rows];
    for (uint32_t i=0; i<this->im_mat.rows; ++i)
        row_ptrs[i] = this->img_data + i*3*this->im_mat.cols;
    
    png_write_image(png_ptr, row_ptrs);
    
    if (setjmp(png_jmpbuf(png_ptr))){
		handler(PNG_ERROR_4);
    }
    
    png_write_end(png_ptr, NULL);
    
    free(this->img_data);
}
#endif


uint8_t a2i_1or2digits(const char* const str){
	uint8_t n = str[0] - '0';
	if (str[1] == 0)
		return n;
	return (n * 10) + str[1] - '0';
}


int main(const int argc, char* argv[]){
    int i = 0;
	
	init_chequerboard();
	
#ifdef EMBEDDOR
    const bool embedding = (argv[1][0] == '-' && argv[1][1] == 'o' && argv[1][2] == 0);
    
    char* out_fmt;
    
    if (embedding){
        ++i;
        out_fmt = argv[++i];
    } else
        out_fmt = NULL;
#endif
	
#ifdef ONLY_COUNT
	uint64_t count = 0;
#endif
    
    
	const uint8_t min_complexity = 50 + a2i_1or2digits(argv[++i]);
    //assert(50 <= min_complexity && min_complexity <= 56);
    
    BPCSStreamBuf bpcs_stream(min_complexity, ++i, argc, argv
                              #ifdef EMBEDDOR
                              , embedding
                              , out_fmt
                              #endif
                              );
    bpcs_stream.load_next_img(); // Init
	
	uchar io_buf[((1024 * 64) / BYTES_PER_GRID) * BYTES_PER_GRID]; // Ensure it is divisible by BYTES_PER_GRID
	uchar* io_buf_itr = io_buf;
    
#ifdef EMBEDDOR
  if (!embedding){
#endif
    do {
		bpcs_stream.get(io_buf_itr);
		io_buf_itr += BYTES_PER_GRID;
#ifdef ONLY_COUNT
		count += BYTES_PER_GRID;
#else
		if (unlikely((io_buf_itr == io_buf + sizeof(io_buf)) or (bpcs_stream.exhausted))){
			if (unlikely(write(STDOUT_FILENO, io_buf, sizeof(io_buf)) != sizeof(io_buf)))
				break;
			io_buf_itr = io_buf;
		}
#endif
    } while (not bpcs_stream.exhausted);
	//free(bpcs_stream.img_data); // Causes segfault // TODO: Investigate
#ifdef EMBEDDOR
  // if (!embedding){
  //     ...
  } else {
	while (likely(read(STDIN_FILENO, io_buf, BYTES_PER_GRID) == BYTES_PER_GRID)){
		// TODO: Optimise
        // read() returns the number of bytes written
		bpcs_stream.put(io_buf);
	}
	bpcs_stream.put(io_buf);
    bpcs_stream.save_im();
  }
#endif
#ifdef ONLY_COUNT
	printf("%lu\n", count);
#endif
	return 0;
}
