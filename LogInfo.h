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
 *      Function: find all the log statements
 * 
 * 	llvm build command:
 * 	cmake -DLLVM_TARGETS_TO_BUILD="X86,ARM" -DCMAKE_CXX_STANDARD=17 ../
 * 
 * 
 */

#ifndef _LOGINFO_H_
#define _LOGINFO_H_

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
#include <stdlib.h>


#define DEBUG false


extern std::deque<struct LogInfo> LogInfoDeque;
extern unsigned global_log_info_id;
extern std::vector<std::string> common_func_names;


struct LogInfo{
	unsigned LogInfoID;
	std::string LogLoc;
	std::string FilePath;
	unsigned BeginLineNumber;
	unsigned BeginColumnNumber;
	unsigned EndLineNumber;
	unsigned EndColumnNumber;
	std::string StmtType;

	std::vector<std::string> StrTxtVec;
	std::vector<struct VariableInfo> VarInfoVec;
	std::vector<struct FuncInfo> FuncInfoVec;
};



enum VARTYPE { SINGLE_VAR, MEMBER_VAR, ARRAY_VAR, OTHER};

struct VariableInfo{
	bool IsParmVar;
	
	VARTYPE VarType;
	std::string SingleVarName;
	std::string MemberVarName;
	std::string ArrayVarBase;
};

struct FuncInfo{
	std::string FuncName;
	// TODO record all the param info also.
};


class LogStatementASTVisitor : public clang::RecursiveASTVisitor<LogStatementASTVisitor>{
    
private:
    clang::CompilerInstance* CI;
	clang::StringRef InFile;

public:
    explicit LogStatementASTVisitor(clang::CompilerInstance* ci, clang::StringRef infile) : CI(ci), InFile(infile) {}

	
	bool VisitStringLiteral(clang::StringLiteral*);
	bool alreadyVisited(clang::StringLiteral*);
	bool isInSkipDirs(std::string);
	clang::Stmt* getParent(clang::Stmt*);
	bool reformatLogTxt(std::vector<std::string>&, std::string);
	int countWords(std::vector<std::string>);
	bool isNumber(std::string);
	
	struct LogInfo handleReturnStmt(clang::ReturnStmt*, clang::StringLiteral*);
	struct LogInfo handleCallExpr(clang::CallExpr*, clang::StringLiteral*);
	struct LogInfo handleBinaryOperator(clang::BinaryOperator*, clang::StringLiteral*);
	
	clang::Stmt* getBranchCondition(clang::Stmt*, std::vector<struct VariableInfo>&, std::vector<struct FuncInfo>&);
	void analyzeExpr(clang::Expr*, std::vector<struct VariableInfo>&, std::vector<struct FuncInfo>&);
	bool isCommonAPI(std::string);
	
	void traversePrecedent( int, clang::Stmt*, std::vector<struct VariableInfo>&, std::vector<struct FuncInfo>&);
	clang::Stmt* findParent(int&, clang::Stmt*);
	clang::FunctionDecl* findFuncDeclParent(clang::Stmt*);
	void analyzeStmt(clang::Stmt*, std::vector<struct VariableInfo>&, std::vector<struct FuncInfo>&);
	
	void travelChilds(clang::CXXOperatorCallExpr*, std::vector<std::string>&);
	clang::Stmt* travelParents(clang::Stmt*, std::vector<std::string>&);
	void travelCallExpr(clang::CallExpr*, std::vector<std::string>&, bool);
	std::string textualizeExpr(clang::Expr*);
	
	bool isContainLogInfo(struct LogInfo);
	bool roughMatchLogFunc(std::string);
	bool isContainVariableInfo(std::vector<struct VariableInfo>, struct VariableInfo);
	bool isContainFuncInfo(std::vector<struct FuncInfo>, struct FuncInfo);
	std::string expr2str(clang::Expr*);	// get Origin String of an Expr, if it is "", get the Spelling String, if it is also "", finally get Expansion String. (Based on the experiment on Httpd's MemberExpr.)
	clang::Stmt* getIndexChild(clang::Stmt*,int);	// get the index's child of current Stmt.
	bool getChildIndex(clang::Stmt*, clang::Stmt*, int&);
	clang::DeclRefExpr* getComplexExprBase(clang::Stmt*);
	clang::DeclRefExpr* _getComplexExprBase(clang::Expr*);
	clang::Expr* getArrayExprIdx(clang::Expr*);
	bool findExpr(clang::Expr*, std::vector<struct VariableInfo>);	// check if any DeclRefExpr in var_list is in current Expr.



	void tempWriteToFile(std::vector<std::string>, int);
	void writeToFile(std::string);
	
};


class LogStatementASTConsumer : public clang::ASTConsumer{
    
private:
    LogStatementASTVisitor Visitor;
	clang::StringRef InFile;
    
    
public:
    explicit LogStatementASTConsumer(clang::CompilerInstance* CI, clang::StringRef infile) : Visitor(CI, infile), InFile(infile) {}
    
    virtual void HandleTranslationUnit(clang::ASTContext & astContext);
    
};

class LogStatementFrontendAction : public clang::ASTFrontendAction{
public:
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, clang::StringRef InFile) {
        return std::make_unique<LogStatementASTConsumer>(&CI, InFile);   // pass CI pointer to ASTConsumer
    }
};


#endif
