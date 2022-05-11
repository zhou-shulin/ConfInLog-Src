/*
 *      Maintained by 
 * 
 *      _                     _           _ _       
 *     | |                   | |         | (_)      
 *  ___| |__   ___  _   _ ___| |__  _   _| |_ _ __  
 * |_  / '_ \ / _ \| | | / __| '_ \| | | | | | '_ \ 
 *	/ /| | | | (_) | |_| \__ \ | | | |_| | | | | | |
 * /___|_| |_|\___/ \__,_|___/_| |_|\__,_|_|_|_| |_|
 * 
 * 
 *      
 *      Start from 2020.06.10
 * 
 *      Compatible with version llvm-10.0.0
 * 
 *      Function: split string into single words.
 * 
 * 
 */


#ifndef _WORDNINJA_H_
#define _WORDNINJA_H_

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <math.h>
#include <string.h>


class WordNinja
{
public:
    WordNinja();
    ~WordNinja();
    int init(const std::string dictPath);
    int split(const std::string inputText, std::vector<std::string>& result);
    void str2Lower(std::string& );
    void splitWord(std::vector<std::string>&, std::string);
private:
	inline  bool isDigit(const std::string& str);
    std::map<std::string, float> m_dict;
    int m_wordMaxLen;
};

#endif