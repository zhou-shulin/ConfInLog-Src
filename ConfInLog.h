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
 *      Function: Entry of the program, merge information about log and configuration
 * 
 * 	llvm build command:
 * 	cmake -DLLVM_TARGETS_TO_BUILD="X86,ARM" -DCMAKE_CXX_STANDARD=17 ../
 * 
 * 
 */

#ifndef _LOGCONS_H_
#define _LOGCONS_H_


#include "ConfInfo.h"
#include "LogInfo.h"
#include "WordNinja.h"

// #include "python3.6/Python.h"

#include <regex>
#include <algorithm>
#include <sstream>
#include <map>
#include <tuple>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <unistd.h>	// for c access()


// 0 for LevenshteinDistance
// 1 for SplitWords
#define SIMILARITY_CALC_METHOD	1


#define CONF_NAME_SIMILARITY_THRESHOLD 0.6
#define CONF_RELATED_VAR_SIMILARITY_THRESHOLD 0.6



struct SimilarityRecord{
	int MatchLevel;
	double SimilarityValue;
	unsigned ConfInfoID;
	unsigned LogInfoID;

	// for MatchLevel = 4
	// std::string MatchedFuncName;

	// for MatchLevel = 2 & 1
	std::string MatchedVarName;

	// for MatchLevel = 1
	std::string ConfigRelatedVarName;
};


int collectLogInfo(std::vector<std::string>, clang::tooling::CommonOptionsParser&);
int collectConfInfo(std::vector<std::string>, clang::tooling::CommonOptionsParser&);
bool isPartialMatch(std::string, std::string);
bool isSequentialMatch(struct ConfInfo, std::vector<std::string>);
int Levenshtein_Distance(std::string, std::string);
void str2Lower(std::string& );
void readDict();
void initSynonymAbbreviation();
double calcSimilarity(std::string, std::string);
int isRelated( unsigned, unsigned);
void FilterLogMessages();


void writeSimilarityInfoToFile();
void fine_grained_output();


#endif