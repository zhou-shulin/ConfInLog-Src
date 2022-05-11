#include "LogInfo.h"


std::deque<struct LogInfo> LogInfoDeque;	// All Log Info List in source code.


std::vector<std::string> AlreadyVisitedLogStrVec;


std::vector<std::string> log_related_word = 
{	 "log", "error", "err", "die",	"fail",	"hint", "alert", "message", 
	"assert", "trace", "print", "write", "report", "record", "crit", "strcat",
	"fatal", "dump", "msg", "put", "out", "warn", "debug", "emerg" 
}; 

unsigned global_log_info_id = 0;

std::vector<std::string> skip_dirs = {"/mysql-5.7.29/extra/", 
									  "/mysql-5.7.29/mysql-test/"};

std::vector<std::string> common_func_names = 
{ 	"strcmp", "strncmp", "strcasecmp", "strncasecmp", "strcasecmp_l", 
	"strncasecmp_l",  "printf", "fprintf", "sprintf", "snprintf", 
	"asprintf", "dprintf", "vprintf", "vfprintf", "vsprintf", 
	"vsnprintf", "vasprintf", "vdprintf", "stpcpy", "stpncpy", "strcspn",
	"strcpy", "strncpy", "getenv", "putenv", "setenv", "unsetenv",	// general lib api
	"ngx_strcasecmp",	// nginx
	"pg_strcasecmp", 	// postgresql
	"apr_table_get", "apr_table_addn", "apr_table_setn",		// httpd
	"buffer_is_equal_string", "array_get_element_klen", "log_error_write",
	"buffer_eq_icase_ssn", "buffer_is_equal_caseless_string",	// lighttpd
	"addReplyBulkCString", 	// redis
	"msg_fatal", "msg_warn", "msg_info", "match_list_init",	// postfix
	"perror", "__printf_chk", "__fprintf_chk", "opt_match"	// openssh
}; 


void LogStatementASTConsumer::HandleTranslationUnit(clang::ASTContext& astContext){
	// llvm::outs()<<"Now analyze "<<InFile.str()<<"\n";
    Visitor.TraverseDecl(astContext.getTranslationUnitDecl());
}


bool LogStatementASTVisitor::VisitStringLiteral(clang::StringLiteral* strliteral){
	if( ! strliteral)
		return true;

	// llvm::outs()<<"Filtered String: "<<strliteral->getBeginLoc().printToString(CI->getSourceManager())<<"\n";
	if( strliteral->getBeginLoc().printToString(CI->getSourceManager()).substr(0,5) == "/usr/" )
		return true;
	
	if( isInSkipDirs(strliteral->getBeginLoc().printToString(CI->getSourceManager())) )
		return true;

	// judge whether it is already visited based on SourceLocation.
	std::string str_loc_txt = strliteral->getBeginLoc().printToString(CI->getSourceManager());
	std::vector<std::string>::iterator ite = std::find(AlreadyVisitedLogStrVec.begin(), AlreadyVisitedLogStrVec.end(), str_loc_txt);
	if( ite != AlreadyVisitedLogStrVec.end())
		return true;
	AlreadyVisitedLogStrVec.push_back(str_loc_txt);

	if( DEBUG )
	{
		llvm::outs()<<str_loc_txt<<"\n";
		strliteral->dumpColor();
	}

	clang::Stmt* ancestor = NULL;
	ancestor = getParent(strliteral);
	if( ! ancestor){
		if( DEBUG )
			llvm::outs()<<"NO ancestor found\n";
		return true;
	}
	
	if( DEBUG ){
		llvm::outs()<<"Current ancestor is: "<<std::string(ancestor->getStmtClassName())<<"\n";
		ancestor->dumpColor();
	}

	struct LogInfo log_info;
	
	if( clang::ReturnStmt* returnstmt = llvm::dyn_cast<clang::ReturnStmt>(ancestor) ){
		log_info = handleReturnStmt(returnstmt, strliteral);
	}
	else if(llvm::isa<clang::DeclStmt>(ancestor) ){
		// // Tips : strings/apr_strings.c:434:24 :  StringLiteral 0x55daf36bb928 'const char [7]' "KMGTPE"
		// // seams nothing since no log will write this way at present.
		// handleDeclStmt(declstmt, strliteral, str_txt_vec, var_vec, func_vec);
	}
	else if(clang::CallExpr* callexpr = llvm::dyn_cast<clang::CallExpr>(ancestor) ){
		log_info = handleCallExpr(callexpr, strliteral);
	}
	else if(clang::BinaryOperator* binop = llvm::dyn_cast<clang::BinaryOperator>(ancestor)){
		if(binop->getOpcode() == clang::BO_Assign || binop->getOpcode() == clang::BO_AddAssign){
			log_info = handleBinaryOperator(binop, strliteral);
		}
	}
	
	if( log_info.StrTxtVec.size() == 0 )
		return true;

	if( DEBUG ){
		llvm::outs()<<"var_vec size : "<< log_info.VarInfoVec.size()<<"\n";
		llvm::outs()<<"func_vec size : "<< log_info.FuncInfoVec.size()<<"\n";
	}

	// TODO re-evaluate the StrTxtVec in log_info to judge whether it is a real log statement.
	// tempWriteToFile(log_info.StrTxtVec, 0);
	if( ! reformatLogTxt(log_info.StrTxtVec, log_info.StmtType) )
		return true;

	if( this->isContainLogInfo(log_info) == false){
		log_info.LogInfoID = global_log_info_id;
		global_log_info_id++;
		LogInfoDeque.push_back(log_info);
	}
	
	return true;
}

bool LogStatementASTVisitor::isInSkipDirs(std::string path)
{
	for(unsigned i=0; i<skip_dirs.size(); i++){
		if(path.find(skip_dirs[i]) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}


clang::Stmt* LogStatementASTVisitor::getParent(clang::Stmt* stmt){
	if(! stmt)
		return NULL;
	
	const auto& parents = CI->getASTContext().getParents(*stmt);
	if(parents.empty()){
        llvm::errs() << "Can not find parent\n";
        return NULL;
    }
    clang::Stmt* aStmt = const_cast<clang::Stmt*>(parents[0].get<clang::Stmt>());
    if(aStmt){
		
		if( llvm::isa<clang::ParenExpr>(aStmt) ||
			llvm::isa<clang::ImplicitCastExpr>(aStmt) ||
			llvm::isa<clang::ExplicitCastExpr>(aStmt) ) {
			
			// llvm::outs()<<"skipped ancestor type is "<<std::string(aStmt->getStmtClassName())<<"\n";
			return this->getParent(aStmt);
		}
		return aStmt;
	}
	
	clang::Decl* aDecl = const_cast<clang::Decl*>(parents[0].get<clang::Decl>());
	if( aDecl){
		if(llvm::isa<clang::FunctionDecl>(aDecl))
			return NULL;
		
		const auto& p = CI->getASTContext().getParents(*aDecl);
		if(p.empty()){
			llvm::errs() << "Can not find parent\n";
			return NULL;
		}
		clang::Stmt* aaStmt = const_cast<clang::Stmt*>(p[0].get<clang::Stmt>());
		if(aaStmt){
			
			if( llvm::isa<clang::ParenExpr>(aaStmt) ||
				llvm::isa<clang::ImplicitCastExpr>(aaStmt) ||
				llvm::isa<clang::ExplicitCastExpr>(aaStmt) ) {
				
				// llvm::outs()<<"skipped ancestor type is "<<std::string(aaStmt->getStmtClassName())<<"\n";
				return this->getParent(aaStmt);
			}
			return aaStmt;
		}
	}
	
    return NULL;
}

bool LogStatementASTVisitor::isNumber(std::string str){
	for(unsigned i=0; i<str.size(); i++)
	{
		int tmp = (int)str[i];
		if( isdigit(tmp) )
			continue;
		else
			return false;
	}
	return true;
}

int LogStatementASTVisitor::countWords(std::vector<std::string> str_txt_vec)
{
	int word_count = 0;
	for(unsigned i=0; i<str_txt_vec.size(); i++)
	{
		if( str_txt_vec[i] == "VARIABLE" || isNumber(str_txt_vec[i]))
			continue;
		
		std::string str = str_txt_vec[i];
		std::vector<std::string> words;
		std::string::size_type pos;
		std::string pattern = " ";
		str += pattern;//扩展字符串以方便操作
		unsigned size = str.size();
		for (unsigned i = 0; i < size; i++)
		{
			pos = str.find(pattern, i);
			if (pos < size)
			{
				std::string s = str.substr(i, pos - i);
				if( s.length() > 0)
					words.push_back(s);
				i = pos + pattern.size() - 1;
			}
		}
		word_count += words.size();
	}

	return word_count;
}

bool LogStatementASTVisitor::reformatLogTxt(std::vector<std::string>& origin_str_txt_vec, std::string StmtType){
	std::string long_str;

	if( countWords(origin_str_txt_vec) <= 3 )
		return false;

	// for(unsigned i=0; i<origin_str_txt_vec.size(); i++)
	// {
	// 	llvm::outs()<<i<<": "<<origin_str_txt_vec[i]<<"\n";
	// }

	if( StmtType == "CALL" || StmtType == "DUALCALL" )
	{
		// llvm::outs()<<"in CALL/DUALCALL\n";
		std::vector<std::string> str_txt_vec;
		unsigned i=0;
		bool flag_placeholder_exist = false;
		while( i < origin_str_txt_vec.size() )
		{
			if( origin_str_txt_vec[i].find("%") == std::string::npos )
			{
				i++;
				continue;
			}
			flag_placeholder_exist = true;

			std::string str = origin_str_txt_vec[i];
			std::regex specifier_reg("%[hljztL\\d]?[diuoxXfFeEgGaAcspn]{1}");
			std::smatch sm;
			while( std::regex_search(str, sm, specifier_reg))
			{
				std::string mstr = sm[0];
				std::string::size_type pos = str.find(mstr);
				i++;
				if(i >= origin_str_txt_vec.size())
					break;
				if( mstr == "%d" && isNumber(origin_str_txt_vec[i]) )
				{
					// llvm::outs()<<"TEST: "<<mstr<<"  "<<origin_str_txt_vec[i]<<"\n";
					std::string int_value = origin_str_txt_vec[i];
					str = str.replace(pos, mstr.length(), int_value);
				}
				else
					str = str.replace(pos, mstr.length(), origin_str_txt_vec[i]);
			}
			while( str.find("\n") != std::string::npos ){
				str = str.replace(str.find("\n"), 1, " ");
			}
			str_txt_vec.push_back(str);
			i++;
		}
		if( flag_placeholder_exist )
		{
			origin_str_txt_vec.assign(str_txt_vec.begin(), str_txt_vec.end());
		}
		for(unsigned i=0; i<origin_str_txt_vec.size(); i++){
			long_str = long_str + " " + origin_str_txt_vec[i];
		}
	}
	else{
		// llvm::outs()<<"in else CALL/DUALCALL\n";
		for(unsigned i=0; i<origin_str_txt_vec.size(); i++){
			long_str = long_str + " " + origin_str_txt_vec[i];
		}
	}
	// llvm::outs()<<"long_str("<<long_str.length()<<"): "<<long_str<<"\n";

	if( long_str.length() > 500)
		return false;
	std::regex alpha_reg("\\w+ \\w+");
	std::smatch sm;
	if( ! std::regex_search(long_str, sm, alpha_reg) ){
		// llvm::outs()<<"CHECK long_str: "<<long_str<<"\n";
		// str_txt_vec.clear();
		return false;
	}
	for(unsigned i=0; i<origin_str_txt_vec.size(); i++){
		while( origin_str_txt_vec[i].find("\n") != std::string::npos ){
			origin_str_txt_vec[i] = origin_str_txt_vec[i].replace(origin_str_txt_vec[i].find("\n"), 1, " ");
		}
	}

	return true;
}




LogInfo LogStatementASTVisitor::handleReturnStmt(clang::ReturnStmt* returnstmt, clang::StringLiteral* strliteral){
	struct LogInfo log_info;
	if( ! returnstmt)
		return log_info;
	
	// std::string log_txt = strliteral->getString().str();
	std::string log_txt = strliteral->getBytes().str();
	if( log_txt.find(" ") == std::string::npos || log_txt.length() < 6 )	// return a single word, could not be a log
		return log_info;

	std::vector<std::string> str_txt_vec;
	std::vector<struct VariableInfo> var_vec;
	std::vector<struct FuncInfo> func_vec;
	
	str_txt_vec.push_back(log_txt);
	clang::FullSourceLoc begin_loc = CI->getASTContext().getFullLoc(returnstmt->getBeginLoc());
	clang::FullSourceLoc end_loc = CI->getASTContext().getFullLoc(returnstmt->getEndLoc());
	log_info.LogLoc = returnstmt->getBeginLoc().printToString(CI->getSourceManager());
	log_info.FilePath =  CI->getSourceManager().getFilename(begin_loc.getExpansionLoc());
	log_info.BeginLineNumber = begin_loc.getExpansionLineNumber();
	log_info.BeginColumnNumber = begin_loc.getExpansionColumnNumber();
	log_info.EndLineNumber = end_loc.getExpansionLineNumber();
	log_info.EndColumnNumber = end_loc.getExpansionColumnNumber();
	log_info.StrTxtVec.assign(str_txt_vec.begin(), str_txt_vec.end());
	log_info.StmtType = "RETURN";
	
	clang::Stmt* branchstmt = getBranchCondition(returnstmt, var_vec, func_vec);
	if(var_vec.size() == 0 && ! branchstmt){
		if( DEBUG )
			llvm::outs()<<"No trace meaning of current ReturnStmt\n";
		return log_info;
	}
	
	if( branchstmt)
		this->traversePrecedent( -1, branchstmt, var_vec, func_vec);
	else
		this->traversePrecedent( -1, returnstmt, var_vec, func_vec);
	
	log_info.VarInfoVec.assign(var_vec.begin(), var_vec.end());
	log_info.FuncInfoVec.assign(func_vec.begin(), func_vec.end());

	return log_info;
}


LogInfo LogStatementASTVisitor::handleCallExpr(clang::CallExpr* callexpr, clang::StringLiteral* strliteral){
	struct LogInfo log_info;
	if( ! callexpr)
		return log_info;
	
	std::vector<std::string> str_txt_vec;
	std::vector<struct VariableInfo> var_vec;
	std::vector<struct FuncInfo> func_vec;
	clang::FullSourceLoc begin_loc;
	clang::FullSourceLoc end_loc;
	clang::CallExpr* target_callexpr;
	std::string stmt_type;

	std::vector<std::string> used_log_func;
	used_log_func.clear();
	
	// For CXXOperatorCallExpr, should handle separately, since it may nest a lot.
	if(clang::CXXOperatorCallExpr* cxxop = llvm::dyn_cast<clang::CXXOperatorCallExpr>(callexpr)){
		clang::Expr* callee_expr = cxxop->getCallee();
		callee_expr = callee_expr->IgnoreImpCasts()->IgnoreImplicit()->IgnoreParens();
		
		if( llvm::isa<clang::DeclRefExpr>(callee_expr) ){
			clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(callee_expr);
			if( dre->getNameInfo().getAsString() == "operator<<"){
				stmt_type = "CXXOP<<";
				travelChilds(cxxop, str_txt_vec);
				clang::Stmt* outest_cxxop = travelParents(cxxop, str_txt_vec);
				begin_loc = CI->getASTContext().getFullLoc(outest_cxxop->getBeginLoc());
				end_loc = CI->getASTContext().getFullLoc(outest_cxxop->getEndLoc());
				
				if(llvm::isa<clang::CallExpr>(outest_cxxop)){
					target_callexpr = llvm::dyn_cast<clang::CallExpr>(outest_cxxop);
					used_log_func.push_back(this->expr2str(target_callexpr->getCallee()));
				}
				else{
					if( DEBUG )
					{
						llvm::outs()<<"Direct transform CXXOP to CallExpr FAILED\n";
					}
					return log_info;
				}
			}
		}
	}
	else{ // treat as general CallExpr
		// Analyze the callexpr, summarize all the text in the callexpr,
		std::string func_name = this->expr2str(callexpr->getCallee());
		if( ! roughMatchLogFunc(func_name) )
			return log_info;

		stmt_type = "CALL";
		used_log_func.push_back(func_name);
		travelCallExpr(callexpr, str_txt_vec, true);
		begin_loc = CI->getASTContext().getFullLoc(callexpr->getBeginLoc());
		end_loc = CI->getASTContext().getFullLoc(callexpr->getEndLoc());
		
		// Find the ancestor of this callexpr, also, traverse its parent to judge whether is a callexpr, and whether it has any stringliteral
		clang::Stmt* ce_ancestor = this->getParent(callexpr);
		bool matched_dualcall_mode = false;
		if( ce_ancestor ){
			if(clang::CallExpr* p_callexpr = llvm::dyn_cast<clang::CallExpr>(ce_ancestor)){
				bool met_original_callexpr = false;
				
				for(unsigned i=0; i<p_callexpr->getNumArgs(); i++){
					clang::Expr* arg = p_callexpr->getArg(i);
					arg = arg->IgnoreImpCasts();
					arg = arg->IgnoreImplicit();
					arg = arg->IgnoreParens();
					if(clang::CallExpr* sib_callexpr = llvm::dyn_cast<clang::CallExpr>(arg)){
						if(sib_callexpr == callexpr){
							met_original_callexpr = true;
							continue;
						}
						bool flag_exist_str_txt = false;
						for(unsigned j=0; j<sib_callexpr->getNumArgs(); j++){
							clang::Expr* sib_arg = sib_callexpr->getArg(j);
							sib_arg = sib_arg->IgnoreImpCasts();
							sib_arg = sib_arg->IgnoreImplicit();
							sib_arg = sib_arg->IgnoreParens();
							if(llvm::isa<clang::StringLiteral>(sib_arg)){
								flag_exist_str_txt = true;
								break;
							}
						}
						if(flag_exist_str_txt == true){
							matched_dualcall_mode = true;
							travelCallExpr(sib_callexpr, str_txt_vec, met_original_callexpr);
							used_log_func.push_back(this->expr2str(sib_callexpr->getCallee()));
						}
					}
				}
				begin_loc = CI->getASTContext().getFullLoc(p_callexpr->getBeginLoc());
				end_loc = CI->getASTContext().getFullLoc(p_callexpr->getEndLoc());
			}
		}
		
		if( ce_ancestor && llvm::isa<clang::CallExpr>(ce_ancestor) && matched_dualcall_mode == true ){
			clang::CallExpr* p_callexpr = llvm::dyn_cast<clang::CallExpr>(ce_ancestor);
			target_callexpr = p_callexpr;
			used_log_func.push_back(this->expr2str(target_callexpr->getCallee()));
			stmt_type = "DUALCALL";
		}
		else{
			target_callexpr = callexpr;
		}
	}
	
	// llvm::outs()<<"str_txt_vec:\n";
	// for(auto ite=str_txt_vec.begin(); ite!=str_txt_vec.end(); ite++){
	// 	llvm::outs()<<(*ite)<<" ";
	// }
	// llvm::outs()<<"\n";


	log_info.LogLoc = target_callexpr->getBeginLoc().printToString(CI->getSourceManager());
	log_info.FilePath =  CI->getSourceManager().getFilename(begin_loc.getExpansionLoc());
	log_info.BeginLineNumber = begin_loc.getExpansionLineNumber();
	log_info.BeginColumnNumber = begin_loc.getExpansionColumnNumber();
	log_info.EndLineNumber = end_loc.getExpansionLineNumber();
	log_info.EndColumnNumber = end_loc.getExpansionColumnNumber();
	log_info.StrTxtVec.assign(str_txt_vec.begin(), str_txt_vec.end());
	log_info.StmtType = stmt_type;
	
	this->analyzeExpr(target_callexpr, var_vec, func_vec);
	
	clang::Stmt* branchstmt = getBranchCondition(target_callexpr, var_vec, func_vec);
	if( var_vec.size() == 0 && ! branchstmt){
		if( DEBUG )
			llvm::outs()<<"No trace meaning of current CallExpr\n";
		return log_info;
	}
	
	if( branchstmt)
		this->traversePrecedent( -1, branchstmt, var_vec, func_vec);
	else
		this->traversePrecedent( -1, target_callexpr, var_vec, func_vec);

	for(auto ite=func_vec.begin(); ite!=func_vec.end(); ){
		if( std::find(used_log_func.begin(), used_log_func.end(), ite->FuncName) != used_log_func.end()){
			ite = func_vec.erase(ite);
		}
		else{
			ite++;
		}
	}

	log_info.VarInfoVec.assign(var_vec.begin(), var_vec.end());
	log_info.FuncInfoVec.assign(func_vec.begin(), func_vec.end());
	
	return log_info;
}


LogInfo LogStatementASTVisitor::handleBinaryOperator(clang::BinaryOperator* binop, clang::StringLiteral* strliteral){
	struct LogInfo log_info;
	if( ! binop)
		return log_info;
	
	//  Filter the situation in:
	/*
	 * #define MC_GET "get " 
	 */
	// same should used in ReturnStmt.
	// std::string log_txt = strliteral->getString().str();
	std::string log_txt = strliteral->getBytes().str();
	if( log_txt.find(" ") == std::string::npos || log_txt.length() < 6)	// assign to a single word, could not be a log
		return log_info;
	
	std::vector<std::string> str_txt_vec;
	std::vector<struct VariableInfo> var_vec;
	std::vector<struct FuncInfo> func_vec;

	if(binop->getOpcode() == clang::BO_Assign){
		str_txt_vec.push_back(log_txt);
		this->analyzeExpr(binop->getLHS(), var_vec, func_vec);
	}
	else if(binop->getOpcode() == clang::BO_Add || binop->getOpcode() == clang::BO_AddAssign){
		if( DEBUG ){
			llvm::outs()<<"Unexpected situation in BinaryOperator\n";
			llvm::outs()<<binop->getBeginLoc().printToString(CI->getSourceManager())<<"\n";
			binop->dumpColor();
		}
		return log_info;
	}

	clang::FullSourceLoc begin_loc = CI->getASTContext().getFullLoc(binop->getBeginLoc());
	clang::FullSourceLoc end_loc = CI->getASTContext().getFullLoc(binop->getEndLoc());
	log_info.LogLoc = binop->getBeginLoc().printToString(CI->getSourceManager());
	log_info.FilePath =  CI->getSourceManager().getFilename(begin_loc.getExpansionLoc());
	log_info.BeginLineNumber = begin_loc.getExpansionLineNumber();
	log_info.BeginColumnNumber = begin_loc.getExpansionColumnNumber();
	log_info.EndLineNumber = end_loc.getExpansionLineNumber();
	log_info.EndColumnNumber = end_loc.getExpansionColumnNumber();
	log_info.StrTxtVec.assign(str_txt_vec.begin(), str_txt_vec.end());
	log_info.StmtType = "BINOP";
	
	clang::Stmt* branchstmt = getBranchCondition(binop, var_vec, func_vec);
	if(var_vec.size() == 0 || ! branchstmt){
		if( DEBUG )
			llvm::outs()<<"No trace meaning of current BinaryOperator\n";
		return log_info;
	}
	
	if( branchstmt)
		this->traversePrecedent( -1, branchstmt, var_vec, func_vec);
	else
		this->traversePrecedent( -1, binop, var_vec, func_vec);
	
	log_info.VarInfoVec.assign(var_vec.begin(), var_vec.end());
	log_info.FuncInfoVec.assign(func_vec.begin(), func_vec.end());

	return log_info;
}


bool LogStatementASTVisitor::isContainLogInfo(struct LogInfo log_info){
	for(unsigned i=0; i<LogInfoDeque.size(); i++){
		if( LogInfoDeque[i].BeginLineNumber == log_info.BeginLineNumber &&
			LogInfoDeque[i].BeginColumnNumber == log_info.BeginColumnNumber &&
			LogInfoDeque[i].EndLineNumber == log_info.EndLineNumber &&
			LogInfoDeque[i].EndColumnNumber == log_info.EndColumnNumber )
		{
			if( DEBUG )
				llvm::outs()<<"FOUND A DUPLICATE LOG_INFO\n";
			return true;
		}
	}
	return false;
}


bool LogStatementASTVisitor::roughMatchLogFunc(std::string func_name){
	for(unsigned i=0; i<log_related_word.size(); i++){
		if(func_name.find(log_related_word[i]) != std::string::npos)
			return true;
	}
	return false;
}


clang::Stmt* LogStatementASTVisitor::getBranchCondition(clang::Stmt* stmt, std::vector<struct VariableInfo>& var_vec, std::vector<struct FuncInfo>& func_vec){
	clang::Stmt* parent = this->getParent(stmt);
	if( ! parent)
		return NULL;
	
	if(clang::IfStmt* ifstmt = llvm::dyn_cast<clang::IfStmt>(parent)){
		clang::Expr* condition = ifstmt->getCond();
		if( DEBUG ){
			llvm::outs()<<"If Condition in getBranchCondition():\n";
			condition->dumpColor();
		}
		this->analyzeExpr(condition, var_vec, func_vec);
		return ifstmt;
	}
	else if(clang::SwitchStmt* switchstmt = llvm::dyn_cast<clang::SwitchStmt>(parent)){
		clang::Expr* condition = switchstmt->getCond();
		if( DEBUG ){
			llvm::outs()<<"Switch Condition in getBranchCondition():\n";
			condition->dumpColor();
		}
		this->analyzeExpr(condition, var_vec, func_vec);
		return switchstmt;
	}
	else if(llvm::isa<clang::CompoundStmt>(parent) ||
		llvm::isa<clang::CaseStmt>(parent) ||
		llvm::isa<clang::DefaultStmt>(parent) ){
		return this->getBranchCondition(parent, var_vec, func_vec);
	}
	
	return NULL;
}


bool LogStatementASTVisitor::isCommonAPI(std::string func_name){
	for(auto ite=common_func_names.begin(); ite!=common_func_names.end(); ite++){
		if( (*ite) == func_name)
			return true;
	}
	return false;
}

void LogStatementASTVisitor::analyzeExpr(clang::Expr* expr, std::vector<struct VariableInfo>& var_vec, std::vector<struct FuncInfo>& func_vec){
	if( ! expr)
		return;
	
	expr = expr->IgnoreImpCasts();
	expr = expr->IgnoreImplicit();
	expr = expr->IgnoreParens();
	
// 	llvm::outs()<<"in analyzeExpr() Expr Type: "<<std::string(expr->getStmtClassName())<<"\n";
	
	if(clang::BinaryOperator * binop = llvm::dyn_cast<clang::BinaryOperator>(expr)){
		clang::Expr* lhs = binop->getLHS();
		clang::Expr* rhs = binop->getRHS();
		this->analyzeExpr(lhs, var_vec, func_vec);
		this->analyzeExpr(rhs, var_vec, func_vec);
	}
	else if(clang::UnaryOperator* unaop = llvm::dyn_cast<clang::UnaryOperator>(expr)){
		clang::Expr* op = unaop->getSubExpr();
		this->analyzeExpr(op, var_vec, func_vec);
	}
	else if(clang::CastExpr* castexpr = llvm::dyn_cast<clang::CastExpr>(expr)){
		clang::Expr* subexpr = castexpr->getSubExpr();
		this->analyzeExpr(subexpr, var_vec, func_vec);
	}
	else if(clang::MemberExpr* member = llvm::dyn_cast<clang::MemberExpr>(expr)){
		struct VariableInfo var_info;
		var_info.VarType = MEMBER_VAR;
		var_info.MemberVarName = this->expr2str(member);
		var_info.IsParmVar = false;
		clang::DeclRefExpr* memberbase = this->getComplexExprBase(member);
		if( memberbase && std::string(memberbase->getDecl()->getDeclKindName()) == std::string("ParmVar"))
			var_info.IsParmVar = true;
		
		if( ! this->isContainVariableInfo(var_vec, var_info) )
			var_vec.push_back(var_info);
	}
	else if(clang::ArraySubscriptExpr* arrayexpr = llvm::dyn_cast<clang::ArraySubscriptExpr>(expr)){
		struct VariableInfo var_info;
		var_info.VarType = ARRAY_VAR;
		var_info.IsParmVar = false;
		clang::DeclRefExpr* arraybase = this->getComplexExprBase(arrayexpr);
		if( arraybase ){
			if( std::string(arraybase->getDecl()->getDeclKindName()) == std::string("ParmVar"))
				var_info.IsParmVar = true;
			var_info.ArrayVarBase = arraybase->getNameInfo().getName().getAsString();
		}

		if( ! this->isContainVariableInfo(var_vec, var_info) )
			var_vec.push_back(var_info);

		clang::Expr* arrayindex = arrayexpr->getIdx();
		this->analyzeExpr(arrayindex, var_vec, func_vec);
	}
	else if(clang::CallExpr* callexpr = llvm::dyn_cast<clang::CallExpr>(expr)){
		struct FuncInfo func_info;
		func_info.FuncName = this->expr2str(callexpr->getCallee());
		if( ! isCommonAPI(func_info.FuncName) &&  ! isContainFuncInfo(func_vec, func_info))
		// if( ! isContainFuncInfo(func_vec, func_info) )
			func_vec.push_back(func_info);
		
		for(unsigned i=0; i<callexpr->getNumArgs(); i++){
			clang::Expr* arg = callexpr->getArg(i);
			this->analyzeExpr(arg, var_vec, func_vec);
		}
	}
	else if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(expr)){
		if( std::string(dre->getDecl()->getDeclKindName()).find("Var") == std::string::npos)
			return;
		struct VariableInfo var_info;
		var_info.VarType = SINGLE_VAR;
		var_info.SingleVarName = dre->getNameInfo().getAsString();
		var_info.IsParmVar = (dre->getDecl()->getDeclKindName() == std::string("ParmVar")) ? true : false;

		if( ! this->isContainVariableInfo(var_vec, var_info) )
			var_vec.push_back(var_info);
	}
	else if(clang::StmtExpr* sexpr = llvm::dyn_cast<clang::StmtExpr>(expr)){
		clang::Stmt* compoundstmt = sexpr->getSubStmt();
		this->analyzeStmt(compoundstmt, var_vec, func_vec);
	}
	else if(llvm::isa<clang::UnaryExprOrTypeTraitExpr>(expr)){
		// for sizeof/alignof, nothing todo
	}
	else if(clang::ConditionalOperator* condop = llvm::dyn_cast<clang::ConditionalOperator>(expr)){
		clang::Expr* true_expr = condop->getTrueExpr();
		clang::Expr* false_expr = condop->getFalseExpr();
		this->analyzeExpr(true_expr, var_vec, func_vec);
		this->analyzeExpr(false_expr, var_vec, func_vec);
	}
	else{
		if( DEBUG){
			llvm::outs()<<"in analyzeExpr(), Current unhandled Expr Node Type: "<<std::string(expr->getStmtClassName())<<"\n";
		// expr->dumpColor();
		// llvm::outs()<<"\n\n\n";
		}
	}
}


void LogStatementASTVisitor::traversePrecedent(int index, clang::Stmt* current_parent, std::vector<struct VariableInfo>& var_vec, std::vector<struct FuncInfo>& func_vec){
	
	if( var_vec.size() == 0)
		return;
	
	// llvm::outs()<<"var_vec size : "<< var_vec.size()<<"\n";
	// llvm::outs()<<"func_vec size : "<< func_vec.size()<<"\n";
	// llvm::outs()<<"index value : "<< index<<"\n";
	
	if( index < 0){
		int cur_index = -1;
		// llvm::outs()<<"before enter findParent()\n";
		if( ! current_parent){
			if( DEBUG ) llvm::outs()<<"gotcha!!!\n";
		}
		clang::Stmt* ancestor = this->findParent(cur_index, current_parent);
		// llvm::outs()<<"after enter findParent()\n";
		if( ancestor != NULL){
			if( DEBUG){
				// llvm::outs()<<"find parent: current child is in the "<<cur_index<<" th child of this parent\n";
				// llvm::outs()<<ancestor->getBeginLoc().printToString(CI->getSourceManager())<<"\n";
				// ancestor->dumpColor();
			}
			this->traversePrecedent(cur_index-1, ancestor, var_vec, func_vec);
		}
		else{
			if( DEBUG )
				llvm::outs()<<"reach top of FunctionDecl\n";

			clang::FunctionDecl* funcdecl = findFuncDeclParent(current_parent);
			if( ! funcdecl){
				if( DEBUG )
					llvm::outs()<<"this FunctionDecl is NULL!!!\n";
				return;
			}
			for(unsigned i=0; i<var_vec.size(); i++){
				if( var_vec[i].IsParmVar ){
					struct FuncInfo func_info;
					func_info.FuncName = funcdecl->getNameInfo().getName().getAsString();
					if( ! isContainFuncInfo(func_vec, func_info))
						func_vec.push_back(func_info);
					break;
				}
			}
		}
	}
	else{
		// traverse the index's child of current_parent, then recursively traverse the index-1's child of current_parent.
		if( DEBUG )
			llvm::outs()<<"traverse the "<<index<<" child of current parent\n";
		clang::Stmt* stmt = this->getIndexChild(current_parent, index);
		
		// Recursively visit this Stmt to find variable and functions.
		analyzeStmt(stmt, var_vec, func_vec);
		
		this->traversePrecedent(index-1, current_parent, var_vec, func_vec);
	}
}


void LogStatementASTVisitor::analyzeStmt(clang::Stmt* stmt, std::vector<struct VariableInfo>& var_vec, std::vector<struct FuncInfo>& func_vec){
	if( ! stmt)
		return ;
	
	if(clang::BinaryOperator* binop = llvm::dyn_cast<clang::BinaryOperator>(stmt)){
		if( binop->getOpcode() != clang::BO_Assign && binop->getOpcode() !=clang::BO_AddAssign)
			return;
		clang::Expr* lhs = binop->getLHS();
		clang::Expr* rhs = binop->getRHS();
		if( this->findExpr(lhs, var_vec))	// for a=b+c, if a is in var_vec, then b and c should be add in var_vec
			this->analyzeExpr(rhs, var_vec, func_vec);
	}
	else if(clang::CallExpr* callexpr = llvm::dyn_cast<clang::CallExpr>(stmt)){
		for(unsigned i=0; i<callexpr->getNumArgs(); i++){
			clang::Expr* arg = callexpr->getArg(i);
			if( this->findExpr(arg, var_vec)){
				struct FuncInfo func_info;
				func_info.FuncName = this->expr2str(callexpr->getCallee());
				if( ! this->isContainFuncInfo(func_vec, func_info) )
					func_vec.push_back(func_info);
				break;
			}
		}
	}
	else{
		// llvm::outs()<<"Unhandled situation in analyzeStmt(). Current Stmt Node Type: "<<std::string(stmt->getStmtClassName())<<"\n";
		// stmt->dumpColor();
		for(clang::Stmt::child_iterator ite=stmt->child_begin(), e=stmt->child_end(); ite!=e; ite++){
			clang::Stmt* child = *ite;
			this->analyzeStmt(child, var_vec, func_vec);
		}
	}
}


void LogStatementASTVisitor::travelChilds(clang::CXXOperatorCallExpr* cxxop, std::vector<std::string>& str_txt_vec){
	for(unsigned i=0; i<cxxop->getNumArgs(); i++){
		clang::Expr* arg = cxxop->getArg(i);
		arg = arg->IgnoreImpCasts();
		arg = arg->IgnoreImplicit();
		arg = arg->IgnoreParens();
		
		if(clang::CXXOperatorCallExpr* inner_cxxop = llvm::dyn_cast<clang::CXXOperatorCallExpr>(arg)){
			this->travelChilds(inner_cxxop, str_txt_vec);
		}
		else if(clang::StringLiteral* strlit = llvm::dyn_cast<clang::StringLiteral>(arg)){
			// str_txt_vec.push_back(strlit->getString().str());
			str_txt_vec.push_back(strlit->getBytes().str());
		}
		else if(clang::IntegerLiteral* intlit = llvm::dyn_cast<clang::IntegerLiteral>(arg)){
			str_txt_vec.push_back(intlit->getValue().toString( 10, true));
			// str_txt_vec.push_back( "$_[INT(" + intlit->getValue().toString( 10, true) +")INT]_$" );
		}
		else if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(arg)){
			if( dre->getDecl()->getType().getAsString().find("ostream") != std::string::npos)
				continue;
			str_txt_vec.push_back("VARIABLE");
			// str_txt_vec.push_back("$_[VAR(" + dre->getNameInfo().getAsString() + ")VAR]_$");
		}
		else{	// TODO consider categorise CallExpr and others known Expr Type.
			if( DEBUG ){
				llvm::outs()<<"Unrecognized variable in CXXOperatorCallExpr travelChilds() : "<< std::string(arg->getStmtClassName()) << " \n";
			}
			str_txt_vec.push_back("VARIABLE");
			// str_txt_vec.push_back("$_[VAR(" + this->expr2str(arg) + ")VAR]_$");
			// arg->dumpColor();
		}
	}
	// llvm::outs()<<"in travelChilds(), str_txt_vec is : \n";
	// for(auto ite=str_txt_vec.begin(); ite!=str_txt_vec.end(); ite++){
	// 	llvm::outs()<<(*ite)<<" ";
	// }
	// llvm::outs()<<"\n";
}


// the param "clang::Stmt* cxxop" should always be "clang::CXXOperatorCallExpr* cxxop" type.
clang::Stmt* LogStatementASTVisitor::travelParents(clang::Stmt* cxxop, std::vector<std::string>& str_txt_vec){
	const auto& parents = CI->getASTContext().getParents(*cxxop);
    
    if(parents.empty()){
        llvm::errs() << "Can not find parent of CXXOperatorCallExpr\n";
        return NULL;
    }
    
	clang::Stmt* aStmt = const_cast<clang::Stmt*>(parents[0].get<clang::Stmt>());
    if(aStmt){
    	if( DEBUG){
			llvm::outs()<<"In travelParents() parent type: "<<std::string(aStmt->getStmtClassName())<<"\n";
		}
		if(clang::CXXOperatorCallExpr* outer_cxxop = llvm::dyn_cast<clang::CXXOperatorCallExpr>(aStmt)){
			for(unsigned i=0; i<outer_cxxop->getNumArgs(); i++){
				clang::Expr* arg = outer_cxxop->getArg(i);
				arg = arg->IgnoreImpCasts();
				arg = arg->IgnoreImplicit();
				arg = arg->IgnoreParens();
				if(llvm::isa<clang::CXXOperatorCallExpr>(arg))
					continue;
				else if(clang::StringLiteral* strlit = llvm::dyn_cast<clang::StringLiteral>(arg)){
					// str_txt_vec.push_back(strlit->getString().str());
					str_txt_vec.push_back(strlit->getBytes().str());
				}
				else if(clang::IntegerLiteral* intlit = llvm::dyn_cast<clang::IntegerLiteral>(arg)){
					str_txt_vec.push_back(intlit->getValue().toString( 10, true));
					// str_txt_vec.push_back( "$_[INT(" + intlit->getValue().toString( 10, true) +")INT]_$" );
				}
				else if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(arg)){
					if( dre->getDecl()->getType().getAsString().find("ostream") == std::string::npos){
					str_txt_vec.push_back("VARIABLE");
						// str_txt_vec.push_back("$_[VAR(" + dre->getNameInfo().getAsString() + ")VAR]_$");
					}
				}
				else{	// TODO consider categorise CallExpr and others known Expr Type.
					if (DEBUG) {
						llvm::outs()<<"Unrecognized variable in CXXOperatorCallExpr travelParents() : "<< std::string(arg->getStmtClassName()) << " \n";
					}
					str_txt_vec.push_back("VARIABLE");
					// str_txt_vec.push_back("$_[VAR((" + this->expr2str(arg) + ")VAR]_$");
					// arg->dumpColor();
				}
			}
			
			return this->travelParents(outer_cxxop, str_txt_vec);
		}
		else{
			// seems nothing to do.
			if (DEBUG) {
				llvm::outs()<<"Not CXXOperatorCallExpr anymore, return.\n";
				// cxxop->dumpColor();
			}
			return cxxop;
		}
	}
	return NULL;
}


void LogStatementASTVisitor::travelCallExpr(clang::CallExpr* callexpr, std::vector<std::string>& str_txt_vec, bool insert_in_tail){
	
	std::vector<std::string>::iterator it;
	it = str_txt_vec.begin();
	
	for(unsigned i=0; i<callexpr->getNumArgs(); i++){
		clang::Expr* arg = callexpr->getArg(i);
		
		std::string tmp_txt = textualizeExpr(arg);
	
		if(tmp_txt.length() == 0)
			continue;

		if( insert_in_tail == true)
			str_txt_vec.push_back(tmp_txt);
		else{	// insert in head
			it = str_txt_vec.insert(it, tmp_txt);
			it++;
		}
	}
}


std::string LogStatementASTVisitor::textualizeExpr(clang::Expr* expr){
	if( ! expr)
		return "";

	expr = expr->IgnoreImpCasts();
	expr = expr->IgnoreImplicit();
	expr = expr->IgnoreParens();

	if(clang::StringLiteral* strlit = llvm::dyn_cast<clang::StringLiteral>(expr)){
		// return strlit->getString().str();
		return strlit->getBytes().str();
	}
	else if(clang::IntegerLiteral* intlit = llvm::dyn_cast<clang::IntegerLiteral>(expr)){
		return std::string(intlit->getValue().toString( 10, true));
		// return std::string("$_[INT(" + intlit->getValue().toString( 10, true) +")INT]_$");
	}
	else if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(expr)){
		return "VARIABLE";
		// return std::string("$_[VAR(" + dre->getNameInfo().getAsString() + ")VAR]_$");
	}
	else if(clang::MemberExpr* member = llvm::dyn_cast<clang::MemberExpr>(expr)){
		return "VARIABLE";
		// return std::string("$_[VAR(" + this->expr2str(member) + ")VAR]_$");
	}
	else if(clang::CastExpr* castexpr = llvm::dyn_cast<clang::CastExpr>(expr)){
		return this->textualizeExpr(castexpr->getSubExpr());
	}
	else{	// TODO consider categorise CallExpr and others known Expr Type.
		if (DEBUG) {
			llvm::outs()<<"Unrecognized variable in travelCallExpr() : "<< std::string(expr->getStmtClassName()) << " \n";
		}
		return "VARIABLE";
		// return std::string("$_[VAR(" + this->expr2str(expr) + ")VAR]_$");
		// arg->dumpColor();
	}
}


bool LogStatementASTVisitor::isContainVariableInfo(std::vector<struct VariableInfo> var_vec, struct VariableInfo var_info){
	for(unsigned i=0; i<var_vec.size(); i++){
		if( var_info.VarType == var_vec[i].VarType){
			if( (var_info.VarType == SINGLE_VAR && var_info.SingleVarName == var_vec[i].SingleVarName) ||
				(var_info.VarType == MEMBER_VAR && var_info.MemberVarName == var_vec[i].MemberVarName) ||
				(var_info.VarType == ARRAY_VAR && var_info.ArrayVarBase == var_vec[i].ArrayVarBase) ){
				return true;
			}
		}
	}
	return false;
}


bool LogStatementASTVisitor::isContainFuncInfo(std::vector<struct FuncInfo> func_vec, struct FuncInfo func_info){
	for(unsigned i=0; i<func_vec.size(); i++){
		if(func_vec[i].FuncName == func_info.FuncName)
			return true;
	}
	return false;
}


std::string LogStatementASTVisitor::expr2str(clang::Expr* expr){
	if( ! expr )
		return "";

	clang::FullSourceLoc begin_loc = CI->getASTContext().getFullLoc(expr->getBeginLoc());
	clang::FullSourceLoc end_loc = CI->getASTContext().getFullLoc(expr->getEndLoc());
	if( begin_loc.isInvalid() || end_loc.isInvalid())
		return "";
	clang::SourceRange sr1(begin_loc, end_loc);
	std::string str_txt = clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(sr1), CI->getSourceManager(), clang::LangOptions(), 0);
	if( str_txt.length() == 0){
		clang::SourceRange sr2(begin_loc.getSpellingLoc(), end_loc.getSpellingLoc());
		str_txt = clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(sr2), CI->getSourceManager(), clang::LangOptions(), 0);
		if( str_txt.length() == 0){
			clang::SourceRange sr3(begin_loc.getExpansionLoc(), end_loc.getExpansionLoc());
			str_txt = clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(sr3), CI->getSourceManager(), clang::LangOptions(), 0);
		}
	}
	if( str_txt.length() > 512 )
		return "";
	return str_txt;
}


clang::Stmt* LogStatementASTVisitor::getIndexChild(clang::Stmt* parrent, int index){
	clang::Stmt* child = NULL;
	int cnt = 0;
	for(clang::Stmt::child_iterator ite=parrent->child_begin(), e=parrent->child_end(); ite!=e; ite++){
		if( cnt == index){
			child = *ite;
			break;
		}
		cnt++;
	}
	return child;
}


bool LogStatementASTVisitor::getChildIndex(clang::Stmt* parent, clang::Stmt* cur_child, int& cur_index){
	int index = 0;

	for(clang::Stmt::child_iterator ite=parent->child_begin(), e=parent->child_end(); ite!=e; ite++){
		clang::Stmt* child = *ite;
		if( ! child){
			continue;
		}

// 		llvm::outs()<<"index = "<<index<<"\n";
		if( child == cur_child || this->getChildIndex(child, cur_child, cur_index) == true ){
			cur_index = index;
			return true;
		}
		index++;
	}
	return false;
}


clang::DeclRefExpr* LogStatementASTVisitor::getComplexExprBase(clang::Stmt* stmt){
	if( ! stmt)
		return NULL;

	for(auto ite=stmt->child_begin(), e=stmt->child_end(); ite!=e; ite++){
		clang::Stmt* child = *ite;
		if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(child)){
			return dre;
		}
		return this->getComplexExprBase(child);
	}
	return NULL;
}


clang::DeclRefExpr* LogStatementASTVisitor::_getComplexExprBase(clang::Expr* expr){
	if( ! expr)
		return NULL;
	
	expr = expr->IgnoreImplicit()->IgnoreParenCasts();
	
	if(clang::MemberExpr* member = llvm::dyn_cast<clang::MemberExpr>(expr)){
	
		clang::Expr* base = member->getBase()->IgnoreImplicit()->IgnoreParenCasts();
		if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(base)){
			return dre;
		}
		else{
			return this->_getComplexExprBase(base);
		}
	}
	else if(clang::ArraySubscriptExpr* arrayexpr = llvm::dyn_cast<clang::ArraySubscriptExpr>(expr)){
		clang::Expr* base = arrayexpr->getBase()->IgnoreImplicit()->IgnoreParenCasts();
		if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(base)){
			return dre;
		}
		else{
			return this->_getComplexExprBase(base);
		}
	}
	else if(clang::UnaryOperator* unaop = llvm::dyn_cast<clang::UnaryOperator>(expr)){
		return this->_getComplexExprBase(unaop->getSubExpr());
	}
	else if(clang::CastExpr* castexpr = llvm::dyn_cast<clang::CastExpr>(expr)){
		return this->_getComplexExprBase(castexpr->getSubExpr());
	}
	else if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(expr)){
		return dre;
	}
	return NULL;
}


clang::Expr* LogStatementASTVisitor::getArrayExprIdx(clang::Expr* expr){
	if( ! expr)
		return NULL;
	
	expr = expr->IgnoreImplicit()->IgnoreParenCasts();
	
	if(clang::ArraySubscriptExpr* arrayexpr = llvm::dyn_cast<clang::ArraySubscriptExpr>(expr)){
		clang::Expr* idxexpr = arrayexpr->getIdx();
		return this->getArrayExprIdx(idxexpr);
	}
	else if(clang::UnaryOperator* unaop = llvm::dyn_cast<clang::UnaryOperator>(expr)){
		clang::Expr* operand = unaop->getSubExpr();
		return this->getArrayExprIdx(operand);
	}
	else if(clang::CastExpr* castexpr = llvm::dyn_cast<clang::CastExpr>(expr)){
		clang::Expr* subexpr = castexpr->getSubExpr();
		return this->getArrayExprIdx(subexpr);
	}
	else if(llvm::isa<clang::DeclRefExpr>(expr)){
		return expr;
	}
	else{
		if( DEBUG){
			llvm::outs()<<"Unhandle expr type in getArrayExprIdx() : "<<std::string(expr->getStmtClassName())<<"\n";
		}
		if( expr->child_begin() != expr->child_end() ){
			if( DEBUG )
				llvm::outs()<<"it has a child, just visit it\n";
			return this->getArrayExprIdx(llvm::dyn_cast<clang::Expr>( *(expr->child_begin()) ));
		}
		else{
			if( DEBUG )
				llvm::outs()<<"it is leaf node, return it\n";
			return expr;
		}
	}
}


clang::Stmt* LogStatementASTVisitor::findParent(int& cur_index, clang::Stmt* cur_child){
	if(cur_index >= 0){
		if( DEBUG )
			llvm::outs()<<"should NOT find parrent of cur_child when cur_index >= 0\n";
		return NULL;
	}

	const auto& parents = CI->getASTContext().getParents(*cur_child);
    
    if(parents.empty()){
        llvm::errs() << "Can not find parent\n";
        return NULL;
    }
    
	clang::Stmt* aStmt = const_cast<clang::Stmt*>(parents[0].get<clang::Stmt>());
    if(aStmt){
		// ExplicitCastExpr: e.g. CStyleCastExpr ( (const char*)str ) 
		if( llvm::isa<clang::ParenExpr>(aStmt) ||
			llvm::isa<clang::CastExpr>(aStmt) ) {
			
			return this->findParent(cur_index, aStmt);
		}
		
		// locate the index of cur_child in its parent
		this->getChildIndex(aStmt, cur_child, cur_index);
		if (DEBUG){
			llvm::outs()<<"Return value of getChildIndex(): "<<cur_index<<"\n";
		}
		return aStmt;
	}
	
	clang::Decl* aDecl = const_cast<clang::Decl*>(parents[0].get<clang::Decl>());
	if( aDecl){
		if(llvm::isa<clang::FunctionDecl>(aDecl)){
			return NULL;
		}
		
		const auto& p = CI->getASTContext().getParents(*aDecl);
		if(p.empty()){
			if( DEBUG )
				llvm::outs() << "Can not find parent\n";
			return NULL;
		}
		clang::Stmt* aaStmt = const_cast<clang::Stmt*>(p[0].get<clang::Stmt>());
		if(aaStmt){
			
			if( llvm::isa<clang::ParenExpr>(aaStmt) ||
				llvm::isa<clang::CastExpr>(aaStmt) ) {
				
				// llvm::outs()<<"skipped ancestor type is "<<std::string(aaStmt->getStmtClassName())<<"\n";
				return this->getParent(aaStmt);
			}
			
			cur_index = -1;
			this->getChildIndex(aaStmt, cur_child, cur_index);
			return aaStmt;
		}
	}
	
    return NULL;	
}


clang::FunctionDecl* LogStatementASTVisitor::findFuncDeclParent(clang::Stmt* current_parent){
	const auto& parents = CI->getASTContext().getParents(*current_parent);
    
    if(parents.empty()){
        llvm::errs() << "Can not find parent\n";
        return NULL;
    }
    
	clang::FunctionDecl* fDecl = const_cast<clang::FunctionDecl*>(parents[0].get<clang::FunctionDecl>());
    if(fDecl){
		return fDecl;
	}
	clang::Stmt* aStmt = const_cast<clang::Stmt*>(parents[0].get<clang::Stmt>());
	if( aStmt){
		return this->findFuncDeclParent(aStmt);
	}
	return NULL;
}


bool LogStatementASTVisitor::findExpr(clang::Expr* expr, std::vector<struct VariableInfo> var_vec){
	if( ! expr)
		return false;
	
	expr = expr->IgnoreImpCasts();
	expr = expr->IgnoreImplicit();
	expr = expr->IgnoreParens();
	
	if(clang::BinaryOperator * binop = llvm::dyn_cast<clang::BinaryOperator>(expr)){
		clang::Expr* lhs = binop->getLHS();
		clang::Expr* rhs = binop->getRHS();
		return this->findExpr(lhs, var_vec) || this->findExpr(rhs, var_vec);
	}
	else if(clang::UnaryOperator* unaop = llvm::dyn_cast<clang::UnaryOperator>(expr)){
		clang::Expr* operand = unaop->getSubExpr();
		return this->findExpr(operand, var_vec);
	}
	else if(clang::MemberExpr* member = llvm::dyn_cast<clang::MemberExpr>(expr)){
		std::string member_var_name = this->expr2str(member);
		for(unsigned i=0; i<var_vec.size(); i++){
			if(var_vec[i].VarType == MEMBER_VAR && var_vec[i].MemberVarName == member_var_name){
				return true;
			}
		}
	}
	else if(clang::ArraySubscriptExpr* arrayexpr = llvm::dyn_cast<clang::ArraySubscriptExpr>(expr)){
		clang::DeclRefExpr* arraybase = this->getComplexExprBase(arrayexpr);
		if( arraybase ){
			std::string array_base_name = arraybase->getNameInfo().getName().getAsString();
			for(unsigned i=0; i<var_vec.size(); i++){
				if(var_vec[i].VarType == ARRAY_VAR && var_vec[i].ArrayVarBase == array_base_name){
					return true;
				}
			}
		}
		
		clang::Expr* indexexpr = arrayexpr->getIdx();
		return this->findExpr(indexexpr, var_vec);
	}
	else if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(expr)){
		std::string var_name = dre->getNameInfo().getName().getAsString();
		for(unsigned i=0; i<var_vec.size(); i++){
			if(var_vec[i].VarType == SINGLE_VAR && var_vec[i].SingleVarName == var_name){
				return true;
			}
		}
	}
	else if(clang::CallExpr* callexpr = llvm::dyn_cast<clang::CallExpr>(expr)){
		// Do we need to consider param transfer in CallExpr?
		// currently probably not.
		if( DEBUG ){
			llvm::outs()<<"Unhandled CallExpr Node Type: "<<std::string(expr->getStmtClassName())<<"\n";
			callexpr->dumpColor();
			llvm::outs()<<"\n\n\n";
		}
	}
	else{
		if( DEBUG){
			llvm::outs()<<"Unhandled Expr Node Type: "<<std::string(expr->getStmtClassName())<<"\n";
			expr->dumpColor();
			llvm::outs()<<"\n\n\n";
		}
	}
	
	return false;
}




void LogStatementASTVisitor::tempWriteToFile(std::vector<std::string> str_txt_vec, int flag){
	std::ofstream out("/home/work_shop/log-compare.txt", std::ios::app);
	for(unsigned k=0; k<str_txt_vec.size(); k++)
		out<<str_txt_vec[k]<<" ";
	out<<"\n\n";
	if( flag == 1)
		out<<"\n\n****************************************************\n\n\n";
	out.close();

	return;
}