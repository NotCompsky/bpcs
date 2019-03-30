#include <cstdlib> // for malloc, free
#include <fcntl.h> // for open, O_WRONLY
#include <unistd.h> // for STD(IN|OUT)_FILENO
#include <vector>


/* Not needed for GCC */
#include <cstdio> // for FILE, fopen
typedef unsigned char uchar;
typedef __uint8_t uint8_t;
typedef __int32_t int32_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;


#include "utils.hpp" // for format_out_fp


#ifdef EMBEDDOR
    #include <sys/stat.h> // for stat
#endif
#ifdef TESTS
    #include <assert.h>
#endif
#ifdef DEBUG
    #include <iostream> // for std::cout, std::endl
    #include <compsky/logger.hpp> // for CompskyLogger
    
    // Fancy loggers
    static CompskyLogger mylog("bpcs", std::cout);
#endif

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
    
    #ifdef DEBUG
        bool print_content = true;
        
        int verbosity = 3;
        
        char* log_fmt = "[%T] ";
    #else
        int verbosity = 0;
    #endif
    int i = 1;
    
    if (argc != 1){
        if (argv[i][2] == 0 && argv[i][1] == 'v' && argv[i][0] == '-'){
            ++verbosity;
            ++i;
        }
        if (argv[i][2] == 0 && argv[i][0] == '-'){
            switch(argv[i][1]){
                case 'o': out_fmt=argv[++i]; break;
              #ifdef EMBEDDOR
                case 'm': embedding=true; break;
              #endif
              #ifdef DEBUG
                case 'Q': print_content=false; break;
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
    #ifdef DEBUG
        if (verbosity < 0)
            verbosity = 0;
        else if (verbosity > 9)
            verbosity = 9;
        mylog.set_level(verbosity);
        
        mylog.set_dt_fmt(log_fmt);
        
        mylog.set_verbosity(4);
        mylog.set_cl('b');
        mylog << "Verbosity " << +verbosity << std::endl;
        
        mylog.set_verbosity(3);
        mylog.set_cl('g');
        if (n_msg_fps != 0)
            mylog << "Embedding";
        else
            mylog << "Extracting";
        
        mylog << std::endl;
    #endif
    
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
            
            #ifdef DEBUG
                mylog.set_verbosity(3);
                mylog.set_cl('g');
                mylog << "Reading msg `" << fp << "` (" << +(i+1) << "/" << +n_msg_fps << ") of size " << +n_msg_bytes << std::endl;
                if (print_content)
            #endif
            write(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
            for (j=0; j<n_msg_bytes; ++j){
                c = (uchar)fp[j];
              #ifdef DEBUG
                if (print_content)
              #endif
                write(STDOUT_FILENO, &c, 1);
            }
            
            if (stat(fp, &stat_buf) == -1){
                #ifdef DEBUG
                    mylog.set_verbosity(0);
                    mylog.set_cl('r');
                    mylog << "No such file:  " << fp << std::endl;
                #endif
                return 1;
            }
            n_msg_bytes = stat_buf.st_size;
            #ifdef DEBUG
                mylog.set_verbosity(5);
                mylog.set_cl(0);
                mylog << "n_msg_bytes (contents): " << +n_msg_bytes << std::endl;
                
                if (print_content)
            #endif
            
            write(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
            msg_file = fopen(fp, "rb");
            for (j=0; j<n_msg_bytes; ++j){
                // WARNING: Assumes there are exactly n_msg_bytes
                c = getc(msg_file);
              #ifdef DEBUG
                if (print_content)
              #endif
                write(STDOUT_FILENO, &c, 1);
            }
            fclose(msg_file);
        }
        // After all messages, signal end with signalled size of 0
        const uchar zero[1] = {0};
        for (uint8_t j=0; j<32; ++j)
          #ifdef DEBUG
            if (print_content)
          #endif
            write(STDOUT_FILENO, zero, 1);
        // Some encryption methods require blocks of length 16 or 32 bytes, so this ensures that there is at least 8 zero bytes even if a final half-block is cut off.
    } else {
    #endif
        char* fp_str;
        int fp_str_length;
        for (i=0; true; ++i) {
            read(STDIN_FILENO, (char*)(&n_msg_bytes), 8);
            #ifdef DEBUG
                mylog.set_verbosity(3);
                mylog.set_cl('g');
                mylog << "n_msg_bytes " << +n_msg_bytes << std::endl;
            #endif
            
            if (n_msg_bytes == 0){
                // Reached end of embedded datas
                #ifdef DEBUG
                    mylog.set_verbosity(0);
                    mylog.set_cl('r');
                    mylog << "n_msg_bytes = 0";
                    mylog.set_cl(0);
                    mylog << std::endl;
                #endif
                return 0;
            }
            
            if (i & 1){
                // First block of data is original file path, second is file contents
              #ifdef TESTS
                if (n_msg_bytes > 1099511627780){
                    // ~2**40
                  #ifdef DEBUG
                    mylog.set_verbosity(0);
                    mylog.set_cl('r');
                    mylog << "File unusually large" << std::endl;
                  #endif
                    free(fp_str);
                    return 1;
                }
              #endif
                if (out_fmt != NULL){
                    #ifdef DEBUG
                        mylog.set_verbosity(3);
                        mylog << "Writing extracted file to `" << fp_str << "`" << std::endl;
                    #endif
                    #ifdef TESTS
                        assert(fp_str[0] != 0);
                    #endif
                    int fd = open(fp_str, O_WRONLY);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                // Stream to anonymous pipe
                for (j=0; j<n_msg_bytes; ++j){
                    read(STDIN_FILENO, &c, 1);
                    #ifdef DEBUG
                    if (print_content)
                    #endif
                    write(STDOUT_FILENO, &c, 1);
                }
                free(fp_str);
            } else {
              #ifdef TESTS
                if (n_msg_bytes > 1024){
                  #ifdef DEBUG
                    mylog.set_verbosity(0);
                    mylog.set_cl('r');
                    mylog << "File path unusually long" << std::endl;
                  #endif
                    exit(1);
                }
              #endif
                fp_str = (char*)malloc(n_msg_bytes);
                // NOTE: fp_str will be NULL on error
                read(STDIN_FILENO, fp_str, n_msg_bytes);
                #ifdef DEBUG
                    char fp_str_terminated[n_msg_bytes+1];
                    memcpy(fp_str_terminated, fp_str, n_msg_bytes);
                    fp_str_terminated[n_msg_bytes] = 0;
                    mylog.set_verbosity(3);
                    mylog.set_cl('g');
                    mylog << "Original fp: " << (char*)fp_str_terminated << std::endl;
                #endif
                if (out_fmt != NULL){
                    fp_str_length = format_out_fp(out_fmt, &fp_str, n_msg_bytes);
                    #ifdef DEBUG
                        mylog.set_verbosity(3);
                        mylog.set_cl('g');
                        mylog << "Formatted fp: " << (char*)fp_str_terminated << std::endl;
                    #endif
                }
            }
        }
    #ifdef EMBEDDOR
    }
    #endif
}
