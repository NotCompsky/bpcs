#pragma once

#include "typedefs.hpp"
#include <cstring> // for memcpy


inline
void _format_out_fp(char* out_fmt, char* fp, char result[1024], int32_t dir_length, int32_t indx_dot, int32_t i, int32_t fp_length){
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
    
    int32_t j = 0;
    for (char* it=out_fmt;  *it!=0;  ++it){
        if (*it == '{'){
            switch(*(++it)){
                case '{': result[j]='{'; goto count_ch;
                case 'b': memcpy(result + j, fp + dir_length + 1, basename_length);    j+=basename_length; it+=8; break;
                case 'd': memcpy(result + j, fp, dir_length);                          j+=dir_length;      it+=3; break;
                case 'e': memcpy(result + j, fp + indx_dot + 1, ext_length);           j+=ext_length;      it+=3; break;
                case 'f':
                    switch(*(++it)){
                        case 'p': memcpy(result + j, fp, fp_length);                   j+=fp_length;       it+=1; break;
                        default: memcpy(result + j, fp + dir_length +1, fname_length); j+=fname_length;    it+=4; break;
                    }
                    break;
            }
            continue;
        }
        count_ch:
        result[j] = *it;
        ++j;
    }
    
	result[j] = 0;
}

inline
void format_out_fp(char* out_fmt,  char* fp,  char result[1024]){
    // WARNING: Requires absolute paths?
    
	int32_t dir_length = -1; // TODO: Fix this logic
    int32_t indx_dot = 0;
    int32_t i = 0;
    
    for (char* it=fp; true; ++it, ++i){
        switch(*it){
            case '/': dir_length=i; break;
            case '.': indx_dot=i; break;
            case 0: goto forbreak;
        }
    }
    forbreak:
    
    --i;
    
	_format_out_fp(out_fmt, fp, result, dir_length, indx_dot, i, i+1);
}
