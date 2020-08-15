#include <cstdlib> // for malloc, free
#include <fcntl.h> // for open, O_WRONLY
#include <unistd.h> // for STD(IN|OUT)_FILENO
#include <vector>
#include <cstdio> // for FILE, fopen
#include <inttypes.h>
#include "utils.hpp" // for format_out_fp


#ifdef EMBEDDOR
    #include <sys/stat.h> // for stat
#endif
#ifdef TESTS
    #include <assert.h>
#endif


typedef unsigned char uchar;


inline uint64_t get_charp_len(char* chrp){
    uint64_t i = 0;
    while (*(chrp++) != 0)
        ++i;
    return i;
}


int main(const int argc, char *argv[]){
    #ifdef EMBEDDOR
    bool embedding = false;
    std::vector<char*> msg_fps;
    #endif
    
    uint8_t n_msg_fps = 0;
    
    char* out_fmt = NULL;
    
    int i = 1;
    
    if (argc != 1){
        if (argv[i][2] == 0 && argv[i][0] == '-'){
            switch(argv[i][1]){
                case 'o': out_fmt=argv[++i]; break;
              #ifdef EMBEDDOR
                case 'm': embedding=true; break;
              #endif
            }
        }
        #ifdef EMBEDDOR
        while (++i < argc){
            msg_fps.push_back(argv[i]);
            ++n_msg_fps;
        };
        #endif
    }
    
    uint64_t j;
    uint64_t n_msg_bytes;
    
    uchar c;
    
    #ifdef EMBEDDOR
    char* fp;
    struct stat stat_buf;
    
    if (embedding){
        FILE* msg_file;
        for (i=0; i<n_msg_fps; ++i){
            fp = msg_fps[i];
            // At start, and between embedded messages, is a 64-bit integer telling us the size of the next embedded message
            // The first 32+ bits will almost certainly be 0 - but this will not aid decryption of the rest of the contents (assuming we are using an encryption method that is resistant to known-plaintext attack)
            
            n_msg_bytes = get_charp_len(fp);
            
            write(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
            for (j=0; j<n_msg_bytes; ++j){
                c = (uchar)fp[j];
                write(STDOUT_FILENO, &c, 1);
            }
            
            if (stat(fp, &stat_buf) == -1){
                return 1;
            }
            n_msg_bytes = stat_buf.st_size;
            
            write(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
            msg_file = fopen(fp, "rb");
            for (j=0; j<n_msg_bytes; ++j){
                // WARNING: Assumes there are exactly n_msg_bytes
                c = getc(msg_file);
                write(STDOUT_FILENO, &c, 1);
            }
            fclose(msg_file);
        }
        // After all messages, signal end with signalled size of 0
        const uchar zero[1] = {0};
        for (uint8_t j=0; j<32; ++j)
            write(STDOUT_FILENO, zero, 1);
        // Some encryption methods require blocks of length 16 or 32 bytes, so this ensures that there is at least 8 zero bytes even if a final half-block is cut off.
    } else {
    #endif
        char* fp_str;
        
        FILE* fout = stdout;
        
        for (i=0; true; ++i) {
            read(STDIN_FILENO, (char*)(&n_msg_bytes), 8);
            
            if (n_msg_bytes == 0){
                // Reached end of embedded datas
                return 0;
            }
            
            if (i & 1){
                // First block of data is original file path, second is file contents
              #ifdef TESTS
                if (n_msg_bytes > 1099511627780){
                    // ~2**40
                    free(fp_str);
                    return 1;
                }
              #endif
                if (out_fmt != NULL){
                    #ifdef TESTS
                        assert(fp_str[0] != 0);
                    #endif
                    fflush(fout);
                    fout = fopen(fp_str, "w");
                }
                // Stream to anonymous pipe
                for (j=0; j<n_msg_bytes; ++j){
                    read(STDIN_FILENO, &c, 1);
                    fwrite(&c, 1, 1, fout);
                }
                fclose(fout);
                // Placed here, it ensures that stdout is not fclosed
                free(fp_str);
            } else {
              #ifdef TESTS
                if (n_msg_bytes > 1024){
                    exit(1);
                }
              #endif
                fp_str = (char*)malloc(n_msg_bytes);
                // NOTE: fp_str will be NULL on error
                read(STDIN_FILENO, fp_str, n_msg_bytes);
                if (out_fmt != NULL){
                    format_out_fp(out_fmt, &fp_str, n_msg_bytes);
                }
            }
        }
    #ifdef EMBEDDOR
    }
    #endif
}
