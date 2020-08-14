#include <cstring> // for strlen
#include <iostream> // for std::endl
#include <stdio.h>
#include <time.h>
#include <string>
#include <stdexcept>
#include <ostream> // for std::stringstream
#include "colours.hpp"

#ifndef __COMPSKY_LOGGER
#define __COMPSKY_LOGGER


const char* char2cl(const char c){
    switch(c){
        case 0: return compsky::cl::CL_END;
        
        case 'p': return compsky::cl::CL_PURPLE;
        case 'c': return compsky::cl::CL_CYAN;
        //case 'C': return CL_DCYAN;
        case 'r': return compsky::cl::CL_RED;
        case 'B': return compsky::cl::CL_BOLD;
        case 'U': return compsky::cl::CL_UNDERLINED;
        case 'w': return compsky::cl::CL_WHITE;
        case 'b': return compsky::cl::CL_BLUE;
        case 'g': return compsky::cl::CL_GREEN;
        case 'o': return compsky::cl::CL_ORANGE;
        //case 'G': return CL_GREY;
        default: std::cerr << "No colour mapped to " << c << std::endl; return compsky::cl::CL_PURPLE;
    }
}

char* get_datestr(const char* fmt){
    time_t t = time(NULL);
    char* date = (char*)malloc(100);
    strftime(date, 100, fmt, localtime(&t));
    return date;
}



class CompskyLogger {
  private:
    bool to_add_newline;
    unsigned int dt_fmt_len;
    unsigned int verbosity;
    unsigned int line_verbosity;
    const char* dt_fmt;
    char* cl;
    std::string name;
    std::ostream& ostream;
  public:
    /* Constructors */
    CompskyLogger(std::string name, std::ostream& ostream):
    to_add_newline(true), dt_fmt("[%F %T] "), name(name), ostream(ostream)
    {}
    
    
    typedef std::ostream&  (*ManipFn)(std::ostream&);
    
    
    template<class Obj2Print>
    CompskyLogger& operator<<(const Obj2Print& obj2print){
        if (this->line_verbosity > this->verbosity)
            return *this;
        
        if (this->to_add_newline)
            this->add_newline();
        
        ostream << obj2print;
        
        return *this;
    }
    
    CompskyLogger& operator<<(char c){
        if (this->line_verbosity > this->verbosity)
            return *this;
        
        if (c == '\n'){
            ostream << "\n";
            for (uint_fast8_t i=0; i<this->dt_fmt_len; ++i)
                ostream << " ";
            return *this;
        }
        
        if (this->to_add_newline)
            this->add_newline();
        
        ostream << c;
        
        return *this;
    }
    
    CompskyLogger& operator<<(ManipFn signal){
        if (this->line_verbosity > this->verbosity)
            return *this;
        
        // signal is e.g. endl or flush
        signal(ostream);
        
        if (signal == static_cast<ManipFn>(std::endl))
            this->to_add_newline = true;
        
        return *this;
    }
    
    void set_dt_fmt(const char* fmt){
        this->dt_fmt = fmt;
        this->dt_fmt_len = strlen(get_datestr(fmt));
    }
    
    /*void setLevel(unsigned int);*/
    
    void set_level(uint_fast8_t n){
        this->verbosity = n;
    }
    
    void add_newline();
    
    // Set verbosity for next input
    void set_verbosity(uint_fast8_t n){
        this->line_verbosity = n;
    }
    void set_cl(char c){
        if (this->line_verbosity > this->verbosity)
            ostream << char2cl(c);
    }
    /*
    void err(char c){
        this->line_verbosity = 1;
        ostream << char2cl(c);
    }
    void warn(char c){
        this->line_verbosity = 2;
        ostream << char2cl(c);
    }
    void info(char c){
        this->line_verbosity = 3;
        ostream << char2cl(c);
    }
    void dbg(char c){
        this->line_verbosity = 4;
        ostream << char2cl(c);
    }
    void tedium(char c){
        this->line_verbosity = 5;
        ostream << char2cl(c);
    }
    
    // Set verbosity for next input AND insert newline
    void ncrit(){
        this->line_verbosity = 0;
        ostream << compsky::cl::CL_CRIT;
        this->add_newline();
    }
    void nerr(){
        this->line_verbosity = 1;
        ostream << compsky::cl::CL_ERR;
        this->add_newline();
    }
    void nwarn(){
        this->line_verbosity = 2;
        ostream << compsky::cl::CL_WARN;
        this->add_newline();
    }
    void ninfo(){
        this->line_verbosity = 3;
        ostream << compsky::cl::CL_INFO;
        this->add_newline();
    }
    void ndbg(){
        this->line_verbosity = 4;
        ostream << compsky::cl::CL_DBG;
        this->add_newline();
    }
    void ntedium(){
        this->line_verbosity = 5;
        ostream << compsky::cl::CL_TEDIUM;
        this->add_newline();
    }
    */
};

/*CompskyLogger& operator<<(const Obj2Print& obj2print){
    this->ostream << this->cl << obj2print;
    return *this;
}

CompskyLogger& operator<<(std::ios_base signal){
    // signal is e.g. endl
    signal(this->ostream);
    
    if (signal == std::endl || signal == std::flush)
        this->flush();
    
    return *this;
}*/
/*
inline void CompskyLogger::setLevel(unsigned int n){
    
    //    0 Criticals
    //   1 Errors
    //    2 Warnings
    //    3 Infos
    //    4 Debugs
    //    5 Tediums
    
    switch(n){
        case 0: this->cl = const_cast<char*>(compsky::cl::CL_CRIT); break;
        case 1: this->cl = const_cast<char*>(compsky::cl::CL_ERR);  break;
        case 2: this->cl = const_cast<char*>(compsky::cl::CL_WARN); break;
        case 3: this->cl = const_cast<char*>(compsky::cl::CL_INFO); break;
        case 4: this->cl = const_cast<char*>(compsky::cl::CL_DBG);  break;
        case 5: this->cl = const_cast<char*>(compsky::cl::CL_TEDIUM); break;
        default: ostream << 0 << +n << " is an invalid verbosity level - valid are ints in [0,5]" << std::endl;
        abort();
    }
    this->verbosity = n;
}
*/

inline void CompskyLogger::add_newline(){
    ostream << "[" << this->name << "] " << get_datestr(this->dt_fmt);
    this->to_add_newline = false;
}
#endif


/*
NOTE
    `log << "foobar \n foobar" << std::endl;` results in 1 call to add_newline():
        [YYY-MM-DD HH:MM:SS] foobar
         foobar
    `log << "foobar " << std::endl << " foobar" << std::endl;` results in 2 calls
        [YYY-MM-DD HH:MM:SS] foobar
        [YYY-MM-DD HH:MM:SS] foobar
    `log << "foobar " << "\n" << " foobar" << std::endl;` results in 1 call:
        [YYY-MM-DD HH:MM:SS] foobar
     foobar
    `log << "foobar " << '\n' << " foobar" << std::endl;` results in 1 call:
        [YYY-MM-DD HH:MM:SS] foobar
                             foobar
*/


/*
Example usage
int main(){
    CompskyLogger log(std::cout);
    log.setLevel(5);
    log.warn();
    log << "Three == " << +3 << "\t99 == " << +99 << "\n" << "second line" << std::endl;
    log.info();
    log << "Information here";
    log.dbg();
    log << "Debugation here";
    log.tedium();
    log << "Tedium!" << std::endl;
    log << "This was a new line" << "\n";
    log << "This was but not std::endl" << std::endl;
    log.ncrit();
    log << "CRITICAL!!" << std::endl;
}
*/
