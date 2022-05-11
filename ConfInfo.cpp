#include "ConfInfo.h"


std::deque<struct ConfInfo> ConfInfoDeque;

std::vector<std::string> AlreadyVisitedConfStrVec;




void ConfInfoASTConsumer::HandleTranslationUnit(clang::ASTContext& astContext){
	// llvm::outs()<<"Now analyze "<<InFile.str()<<"\n";
    Visitor.TraverseDecl(astContext.getTranslationUnitDecl());
}


bool ConfInfoASTVisitor::VisitStringLiteral(clang::StringLiteral* strliteral){
	if( ! strliteral)
		return true;

	// judge whether it is already visited based on SourceLocation.
	std::string str_loc_txt = strliteral->getBeginLoc().printToString(CI->getSourceManager());
	std::vector<std::string>::iterator ite = std::find(AlreadyVisitedConfStrVec.begin(), AlreadyVisitedConfStrVec.end(), str_loc_txt);
	if( ite != AlreadyVisitedConfStrVec.end())
		return true;
	AlreadyVisitedConfStrVec.push_back(str_loc_txt);
	
	int conf_index = isConfig(strliteral); 
	if( conf_index < 0 )
		return true;

	if (DEBUG )
		llvm::outs()<<str_loc_txt<<"\n";
	// strliteral->dumpColor();
	
	clang::Stmt* ancestor = NULL;
	ancestor = this->getParent(strliteral);
	if( ! ancestor){
		if( DEBUG )
			llvm::outs()<<"NO ancestor found\n";
		return true;
	}
	if( DEBUG )
	{
		llvm::outs()<<"Current ancestor is: "<<std::string(ancestor->getStmtClassName())<<"\n";
		ancestor->dumpColor();
	}
	
	int cur_index = -1;
	if( ! this->getChildIndex(ancestor, strliteral, cur_index)){
		if( DEBUG )
			llvm::outs()<<"getChildIndex() in ConfInfo.cpp return false\n";
		return true;
	}
	
	if( DEBUG )
		llvm::outs()<<"cur_index = "<<cur_index<<"\n";
	
	std::vector<struct RelatedItem> RelatedItemVec;
	RelatedItemVec.clear();
	this->travelRelatedItem(ancestor, cur_index, RelatedItemVec);
	
	
	clang::Stmt* s_ancestor = this->getParent(ancestor);
	clang::Stmt* ss_ancestor = this->getParent(s_ancestor);
	if(llvm::isa<clang::InitListExpr>(ancestor)){
		if( s_ancestor && llvm::isa<clang::InitListExpr>(s_ancestor) && 
			ss_ancestor && llvm::isa<clang::InitListExpr>(ss_ancestor) ){	// situation in postgresql
				
			if( this->getChildIndex(s_ancestor, ancestor, cur_index)){
				this->travelRelatedItem(s_ancestor, cur_index, RelatedItemVec);
			}
		}
	}
	
	// ConfInfoDeque[conf_index].RelatedItemVec.assign(RelatedItemVec.begin(), RelatedItemVec.end());
	this->addToRelatedItemVec(conf_index, RelatedItemVec);
	if( DEBUG )
		llvm::outs()<<ConfInfoDeque[conf_index].ConfigName<<" "<<conf_index<<" "<<"RelatedItemVec size: "<<ConfInfoDeque[conf_index].RelatedItemVec.size()<<"\n";
	
	// TODO How about other type of Config Name usage, e.g. BinaryOperator? Should we consider these?
	
	return true;
}


int ConfInfoASTVisitor::isConfig(clang::StringLiteral* strliteral){
	if( ! strliteral)
		return -1;
	
	// std::string str_txt = strliteral->getString().str();
	std::string str_txt = strliteral->getBytes().str();
	for(int i=0; i<(int)ConfInfoDeque.size(); i++){
		struct ConfInfo conf_info = ConfInfoDeque[i];
		if( this->toLower(conf_info.ConfigName) == this->toLower(str_txt) ){
			return i;
		}
	}
	return -1;
}


void ConfInfoASTVisitor::addToRelatedItemVec(int conf_index, std::vector<struct RelatedItem> RelatedItemVec){
	for(auto ite=RelatedItemVec.begin(); ite!=RelatedItemVec.end(); ite++){
		bool flag_recorded = false;
		for(auto ITE=ConfInfoDeque[conf_index].RelatedItemVec.begin(); ITE!=ConfInfoDeque[conf_index].RelatedItemVec.end(); ITE++){
			// llvm::outs()<<ite->ItemType<<" "<<ite->ItemName<<" "<<ITE->ItemType<<" "<<ITE->ItemName<<"\n"; 
			if( ite->ItemType == ITE->ItemType && ite->ItemName == ITE->ItemName){
				flag_recorded = true;
			}
		}
		if( flag_recorded == false){
			ConfInfoDeque[conf_index].RelatedItemVec.push_back((*ite));
		}
	}

	// ConfInfoDeque[conf_index].RelatedItemVec.insert(ConfInfoDeque[conf_index].RelatedItemVec.end(), RelatedItemVec.begin(), RelatedItemVec.end());
	return;
}


clang::Stmt* ConfInfoASTVisitor::getParent(clang::Stmt* stmt){
	if( ! stmt)
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


bool ConfInfoASTVisitor::getChildIndex(clang::Stmt* parent, clang::Stmt* cur_child, int& cur_index){
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


int ConfInfoASTVisitor::_getChildIndex(clang::Stmt* parent, clang::Stmt* cur_child, int& total_children){
	total_children = 0;
	int cur_index = -1;
	
	if(! parent || ! cur_child ){
		return false;
	}
	
	for(clang::Stmt::child_iterator ite=parent->child_begin(), e=parent->child_end(); ite!=e; ite++){
		clang::Stmt* child = *ite;
		
		if( ! child)
			continue;
		
		int temp_total_child = 0;
		if( child == cur_child || this->_getChildIndex(child, cur_child, temp_total_child) >= 0){
			cur_index = total_children;
		}
		total_children++;
	}
	return cur_index;
}


void ConfInfoASTVisitor::travelRelatedItem(clang::Stmt* parent, int index, std::vector<struct RelatedItem>& RelatedItemVec){
	if(! parent)
		return;
	
	int cur_index = 0;
	for(auto ite=parent->child_begin(), e=parent->child_end(); ite!=e; ite++){
		if(cur_index == index){
			cur_index++;
			continue;
		}
		
		clang::Stmt* stmt = *ite;
		handleStmt(stmt, RelatedItemVec);
		
		cur_index++;
	}
}


void ConfInfoASTVisitor::handleStmt(clang::Stmt* stmt, std::vector<struct RelatedItem>& RelatedItemVec){
	if( ! stmt)
		return;
	
	if(clang::UnaryOperator* unaop = llvm::dyn_cast<clang::UnaryOperator>(stmt)){
		clang::Expr* operand = unaop->getSubExpr();
		return this->handleStmt(operand, RelatedItemVec);
	}
	else if(clang::BinaryOperator* binop = llvm::dyn_cast<clang::BinaryOperator>(stmt)){
		clang::Expr* lhs = binop->getLHS();
		clang::Expr* rhs = binop->getRHS();
		this->handleStmt(lhs, RelatedItemVec);
		this->handleStmt(rhs, RelatedItemVec);
	}
	else if(clang::CastExpr* castexpr = llvm::dyn_cast<clang::CastExpr>(stmt)){
		clang::Expr* subexpr = castexpr->getSubExpr();
		this->handleStmt(subexpr, RelatedItemVec);
	}
	else if(clang::ParenExpr* paren = llvm::dyn_cast<clang::ParenExpr>(stmt)){
		clang::Expr* subexpr = paren->getSubExpr();
		this->handleStmt(subexpr, RelatedItemVec);
	}
	else if(clang::InitListExpr* initexpr = llvm::dyn_cast<clang::InitListExpr>(stmt)){
		clang::Expr* subexpr = initexpr->getInit(0);
		this->handleStmt(subexpr, RelatedItemVec);
	}
	else if(clang::DesignatedInitExpr* dinitexpr = llvm::dyn_cast<clang::DesignatedInitExpr>(stmt)){
		clang::Expr* subexpr = dinitexpr->getSubExpr(0);
		this->handleStmt(subexpr, RelatedItemVec);
	}
	else if(llvm::isa<clang::UnaryExprOrTypeTraitExpr>(stmt)){
		// should do nothing ?
	}
	else if(clang::MemberExpr* memberexpr = llvm::dyn_cast<clang::MemberExpr>(stmt)){
		// TODO special MemberExpr in Httpd
		struct RelatedItem item;
		item.ItemName = this->member2str(memberexpr);
		item.ItemType = std::string("MEMBER_VAR");
		RelatedItemVec.push_back(item);
	}
	else if(clang::DeclRefExpr* dre = llvm::dyn_cast<clang::DeclRefExpr>(stmt)){
// 		llvm::outs()<<"It is a DeclRefExpr:\n";
// 		dre->dumpColor();
		struct RelatedItem item;
		item.ItemName = dre->getNameInfo().getAsString();
		item.ItemType = std::string(dre->getDecl()->getDeclKindName());
		if( item.ItemType == "Function" && ! isCommonAPI(item.ItemName) ){
			RelatedItemVec.push_back(item);
		}
		if( item.ItemType != "Function" && item.ItemType != "EnumConstant" ){
			RelatedItemVec.push_back(item);
		}
		
	}
	else if(clang::StringLiteral* strliteral = llvm::dyn_cast<clang::StringLiteral>(stmt)){
// 		llvm::outs()<<"It is a StringLiteral:\n";
// 		strliteral->dumpColor();
		struct RelatedItem item;
		// item.ItemName = strliteral->getString().str();
		item.ItemName = strliteral->getBytes().str();
		item.ItemType = std::string("STR_CONSTANT");
		RelatedItemVec.push_back(item);
	}
	else{
		if( DEBUG )
		{
			llvm::outs()<<"Unhandled stmt in handleStmt() in collecting ConfInfo : "<<std::string(stmt->getStmtClassName())<<"\n";
		}
	}
	return;
}


bool ConfInfoASTVisitor::isCommonAPI(std::string func_name){
	for(auto ite=common_func_names.begin(); ite!=common_func_names.end(); ite++){
		if( (*ite) == func_name)
			return true;
	}
	return false;
}


std::string ConfInfoASTVisitor::member2str(clang::MemberExpr* expr){
	if( ! expr)
		return "";
	
	clang::FullSourceLoc begin_loc = CI->getASTContext().getFullLoc(expr->getBeginLoc());
	clang::FullSourceLoc end_loc = CI->getASTContext().getFullLoc(expr->getEndLoc());
	clang::SourceRange sr(begin_loc.getExpansionLoc(), end_loc.getExpansionLoc());
	std::string str_txt = clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(sr), CI->getSourceManager(), clang::LangOptions(), 0);
	
	if(str_txt.length() == 0){
		clang::SourceRange sr2(begin_loc.getSpellingLoc(), end_loc.getSpellingLoc());
		str_txt = clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(sr2), CI->getSourceManager(), clang::LangOptions(), 0);
	}
	
	return str_txt;
}


std::string ConfInfoASTVisitor::toLower(std::string str){
	std::transform( str.begin(), str.end(), str.begin(), (int (*)(int))tolower );
	return str;
}