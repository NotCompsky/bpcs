std::string format_out_fp(char* out_fmt, char* fp){
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
            switch(*(++it)){
                case '{': result+='{'; break;
                case 'b': result+=basename; it+=8; break;
                case 'd': result+=dir; it+=3; break;
                case 'e': result+=ext; it+=3; break;
                case 'f':
                    switch(*(++it)){
                        case 'p': it+=1; result+=fp; break;
                        default: it+=4; result+=fname; break;
                    }
                    break;
            }
            continue;
        } else {
            result += *it;
        }
    }
    return result;
}
