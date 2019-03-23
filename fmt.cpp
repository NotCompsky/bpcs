#include <assert.h>
#include <string>
#include <sys/stat.h> // for stat
#include <unistd.h> // for STD(IN|OUT)_FILENO
#include <vector>


typedef uint8_t uchar;


#ifdef DEBUG
    #include <iostream> // for std::cout, std::endl
    #include <compsky/logger.hpp> // for CompskyLogger
    
    // Fancy loggers
    static CompskyLogger mylog("bpcs", std::cout);
#endif


inline uint_fast64_t get_charp_len(char* chrp){
    uint_fast64_t i = 0;
    while (*(chrp++) != 0)
        ++i;
    return i;
}


std::string format_out_fp(char* out_fmt, char* fp){
    // TODO: Plop into seperate file to unify this with that of bpcs.cpp
    // WARNING: Requires absolute paths?
    std::string basename;
    std::string dir = "";
    std::string ext;
    std::string fname;
    std::string result = "";
    
    // Intermediates
    std::string slashblock;
    std::string dotblock;
    bool dot_present = false;
    
    for (char* it=fp; *it; ++it){
        if (*it == '/'){
            dir += slashblock;
            slashblock = "";
            if (dot_present){
                dir += '.' + dotblock;
                dotblock = "";
                dot_present = false;
            }
            continue;
        }
        if (*it == '.'){
            if (dot_present){
                slashblock += '.' + dotblock;
                dotblock = "";
            }
            dot_present = true;
            continue;
        }
        if (dot_present){
            dotblock += *it;
        } else {
            slashblock += *it;
        }
    }
    basename = slashblock;
    ext = dotblock;
    fname = slashblock + "." + dotblock;
    
    for (char* it=out_fmt; *it; ++it){
        if (*it == '{'){
            ++it;
            if (*it == '{'){
                result += '{';
            } else if (*it == 'b'){
                it += 8; // WARNING: Skipping avoids error checking.
                result += basename;
            } else if (*it == 'd'){
                it += 3;
                result += dir;
            } else if (*it == 'e'){
                it += 3;
                result += ext;
            } else if (*it == 'f'){
                ++it;
                if (*it == 'p'){
                    it += 1;
                    result += fp;
                } else {
                    it += 4;
                    result += fname;
                }
            }
            continue;
        } else {
            result += *it;
        }
    }
    return result;
}




int main(const int argc, char *argv[]){
    #ifdef EMBEDDOR
    bool embedding = false;
    std::vector<char*> msg_fps;
    uint64_t bytes_max;
    uint64_t bytes_written = 0;
    #endif
    
    uint8_t n_msg_fps = 0;
    
    char* out_fmt = NULL;
    
    std::string out_fp;
    
    #ifdef DEBUG
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
        #ifdef EMBEDDOR
        if (argv[i][2] == 0 && argv[i][1] == 'N' && argv[i][0] == '-'){
            bytes_max = std::stoi(argv[++i]);
            ++i;
        }
        #endif
        if (argv[i][2] == 0 && argv[i][0] == '-'){
            if (argv[i][1] == 'o'){
                out_fmt = argv[++i];
            }
            #ifdef EMBEDDOR
            else if (argv[i][1] == 'm'){
                embedding = true;
            }
            #endif
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
    
    uint_fast64_t j;
    uint_fast64_t n_msg_bytes;
    
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
            bytes_written += n_msg_bytes;
            
            #ifdef DEBUG
                mylog.set_verbosity(3);
                mylog.set_cl('g');
                mylog << "Reading msg `" << fp << "` (" << +(i+1) << "/" << +n_msg_fps << ") of size " << +n_msg_bytes << std::endl;
            #endif
            write(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
            for (j=0; j<n_msg_bytes; ++j){
                c = (uchar)fp[j];
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
            bytes_written += n_msg_bytes;
            #ifdef DEBUG
                mylog.set_verbosity(5);
                mylog.set_cl(0);
                mylog << "n_msg_bytes (contents): " << +n_msg_bytes << std::endl;
            #endif
            
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
        const uchar eight_zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        write(STDOUT_FILENO, eight_zeros, 8);
        if (bytes_written+8 > bytes_max){
            #ifdef DEBUG
                mylog.set_verbosity(0);
                mylog << "Too many bytes to encode" << std::endl;
            #endif
            abort();
        }
        for (j=bytes_written+8; j<bytes_max; ++j)
            // Pad with zeros
            // It doesn't matter if we add too many zeros - bpcs will ignore excess data
            write(STDOUT_FILENO, eight_zeros, 1);
    } else {
    #endif
        std::string fp_str;
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
                if (out_fmt != NULL){
                    #ifdef DEBUG
                        mylog.set_verbosity(3);
                        mylog << "Writing extracted file to `" << fp_str << "`" << std::endl;
                    #endif
                    #ifdef TESTS
                        assert(fp_str != "");
                    #endif
                    FILE* of = fopen(fp_str.c_str(), "rb");
                    for (j=0; j<n_msg_bytes; ++j){
                        read(STDIN_FILENO, &c, 1);
                        fwrite(&c, 1, 1, of);
                    }
                    fclose(of);
                    #ifndef DEBUG
                        if (verbosity)
                            write(STDOUT_FILENO, fp_str.c_str(), fp_str.size());
                    #endif
                } else {
                    // Stream to anonymous pipe
                    for (j=0; j<n_msg_bytes; ++j){
                        read(STDIN_FILENO, &c, 1);
                        write(STDOUT_FILENO, &c, 1);
                    }
                }
            } else {
                fp_str = "";
                for (j=0; j<n_msg_bytes; ++j){
                    read(STDIN_FILENO, &c, 1);
                    fp_str += c;
                }
                #ifdef DEBUG
                    mylog.set_verbosity(3);
                    mylog.set_cl('g');
                    mylog << "Original fp: " << fp_str << std::endl;
                #endif
                if (out_fmt != NULL){
                    fp_str = format_out_fp(out_fmt, (char*)fp_str.c_str());
                    #ifdef DEBUG
                        mylog.set_verbosity(3);
                        mylog.set_cl('g');
                        mylog << "Formatted fp: " << fp_str << std::endl;
                    #endif
                }
            }
        }
    #ifdef EMBEDDOR
    }
    #endif
}
