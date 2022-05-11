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
 *      Start from 2020.09.14
 * 
 *      Compatible with version llvm-10.0.0
 * 
 *      Function: find related information of Config Option
 * 
 * 	llvm build command:
 * 	cmake -DLLVM_TARGETS_TO_BUILD="X86,ARM" -DCMAKE_CXX_STANDARD=17 ../
 * 
 * 
 */
#ifndef _CONFINFO_H_
#define _CONFINFO_H_

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Decl.h"
#include "clang/AST/ASTContext.h"
#include "clang/Analysis/CFG.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/CommandLine.h"



#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include <list>
#include <algorithm>	// to use transform to tolower
#include <regex>
#include <any>

#include "LogInfo.h"


extern std::deque<struct ConfInfo> ConfInfoDeque;


struct RelatedItem{
	std::string ItemName;
	std::string ItemType;	// Function, Var, MEMBER_VAR, STR_CONSTANT, 
};

struct ConfInfo{
	std::string ConfigName;
	// std::vector<std::string> Words;	// the splitted word of ConfigName
	// TODO other related information and record form.
	std::vector<struct RelatedItem> RelatedItemVec;
	
	std::deque< std::pair< int, struct LogInfo> > RelatedLogInfoDeque;
};



class ConfInfoASTVisitor : public clang::RecursiveASTVisitor<ConfInfoASTVisitor>{
    
private:
    clang::CompilerInstance* CI;
	clang::StringRef InFile;

public:
    explicit ConfInfoASTVisitor(clang::CompilerInstance* ci, clang::StringRef infile) : CI(ci), InFile(infile) {}

	
	bool VisitStringLiteral(clang::StringLiteral*);
	int isConfig(clang::StringLiteral*);
	void addToRelatedItemVec(int, std::vector<struct RelatedItem>);
	clang::Stmt* getParent(clang::Stmt*);
	bool getChildIndex(clang::Stmt*, clang::Stmt*, int&);
	int _getChildIndex(clang::Stmt*, clang::Stmt*, int&);
	void travelRelatedItem(clang::Stmt*, int, std::vector<struct RelatedItem>&);
	void handleStmt(clang::Stmt*, std::vector<struct RelatedItem>&);
	bool isCommonAPI(std::string);
	std::string member2str(clang::MemberExpr*);

	std::string toLower(std::string);

	
};

class ConfInfoASTConsumer : public clang::ASTConsumer{
    
private:
    ConfInfoASTVisitor Visitor;
	clang::StringRef InFile;
    
    
public:
    explicit ConfInfoASTConsumer(clang::CompilerInstance* CI, clang::StringRef infile) : Visitor(CI, infile), InFile(infile) {}
    
    virtual void HandleTranslationUnit(clang::ASTContext & astContext);
    
};

class ConfInfoFrontendAction : public clang::ASTFrontendAction{
public:
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, clang::StringRef InFile) {
        return std::make_unique<ConfInfoASTConsumer>(&CI, InFile);   // pass CI pointer to ASTConsumer
    }
};


#endif
