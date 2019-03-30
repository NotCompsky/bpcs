#include <cstring> // for memcpy
#ifdef TESTS
    #include <assert.h>
    #include <iostream>
#endif

inline int32_t _format_out_fp(char* out_fmt, char** fp, int32_t dir_length, int32_t indx_dot, int32_t i, int32_t fp_length){
    int32_t basename_length;
    int32_t ext_length = i - indx_dot;
    int32_t fname_length = i - dir_length;
    
    if (indx_dot > dir_length){
        // If last '.' appears after last '/'
        basename_length = indx_dot - dir_length - 1;
    } else {
        basename_length = i - dir_length - 1;
        ext_length = 0;
    }
    
    int32_t result_length = 0;
    
    for (char* it=out_fmt; *it!=0; ++it){
        if (*it == '{'){
            switch(*(++it)){
                case '{': goto append_ch;
                case 'b':         result_length+=basename_length;   it+=8; break;
                case 'd':         result_length+=dir_length;        it+=3; break;
                case 'e':         result_length+=ext_length;        it+=3; break;
                case 'f':
                    switch(*(++it)){
                        case 'p': result_length+=fp_length;         it+=1; break;
                        default:  result_length+=fname_length;      it+=4; break;
                    }
                    break;
            }
#ifdef TESTS
            assert(*it == '}');
#endif
            continue;
        }
        append_ch:
        result_length += 1;
    }
    ++result_length; // terminating 0
    
#ifdef DEBUG
    std::cout << "result_length:\t" << +result_length << std::endl;
    char basename[basename_length+1];
    memcpy(basename, *fp + dir_length + 1, basename_length);
    basename[basename_length] = 0;
    std::cout << "basename_length:\t" << +basename_length << "\t" << basename << std::endl;
    char dir[dir_length+1];
    memcpy(dir, *fp, dir_length);
    dir[dir_length] = 0;
    std::cout << "dir_length:\t" << +dir_length << "\t" << dir << std::endl;
    char ext[ext_length+1];
    memcpy(ext, *fp + indx_dot + 1, ext_length);
    ext[ext_length] = 0;
    std::cout << "ext_length:\t" << +ext_length << "\t" << ext << std::endl;
    char fname[fname_length+1];
    memcpy(fname, *fp + dir_length +1, fname_length);
    fname[fname_length] = 0;
    std::cout << "fname_length:\t" << +fname_length << "\t" << fname << std::endl;
    char effpee[fp_length+1];
    memcpy(effpee, *fp, fp_length);
    effpee[fp_length] = 0;
    std::cout << "fp_length:\t" << +fp_length << "\t" << effpee << std::endl;
#endif
    
    char* result = (char*)malloc(result_length);
    
    int32_t j = 0;
    for (char* it=out_fmt;  *it!=0;  ++it){
        if (*it == '{'){
            switch(*(++it)){
                case '{': result[j]='{'; goto count_ch;
                case 'b': memcpy(result + j, *fp + dir_length + 1, basename_length);    j+=basename_length; it+=8; break;
                case 'd': memcpy(result + j, *fp, dir_length);                          j+=dir_length;      it+=3; break;
                case 'e': memcpy(result + j, *fp + indx_dot + 1, ext_length);           j+=ext_length;      it+=3; break;
                case 'f':
                    switch(*(++it)){
                        case 'p': memcpy(result + j, *fp, fp_length);                   j+=fp_length;       it+=1; break;
                        default: memcpy(result + j, *fp + dir_length +1, fname_length); j+=fname_length;    it+=4; break;
                    }
                    break;
            }
            continue;
        }
        count_ch:
        result[j] = *it;
        ++j;
    }
    
    result[result_length - 1] = 0;
    
    *fp = result;
    
    return result_length;
}

int32_t format_out_fp(char* out_fmt, char** fp){
    // WARNING: Requires absolute paths?
    
    int32_t dir_length = 0;
    int32_t indx_dot = 0;
    int32_t i = 0;
    
    for (char* it=*fp; true; ++it, ++i){
        switch(*it){
            case '/': dir_length=i; break;
            case '.': indx_dot=i; break;
            case 0: goto forbreak;
        }
    }
    forbreak:
    
    --i;
    
    return _format_out_fp(out_fmt, &(*fp), dir_length, indx_dot, i, i+1);
}

int32_t format_out_fp(char* out_fmt, char** fp, uint64_t fp_length){
    // WARNING: Requires absolute paths?
    
    int32_t dir_length = 0;
    int32_t indx_dot = 0;
    int32_t i = 0; // eventual value will be fp_length - 1
    
    for (char* it=*fp; i<fp_length; ++i){
        switch(*(it++)){
            case '/': dir_length=i; break;
            case '.': indx_dot=i; break;
        }
    }
    --i;
    
    return _format_out_fp(out_fmt, &(*fp), dir_length, indx_dot, i, fp_length);
}
