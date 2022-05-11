#include "ConfInLog.h"


std::string HOME_DIR = getenv("HOME");
std::string CWD = get_current_dir_name();	// unistd.h 
std::string WORK_DIR = "/home/work_shop/";
std::string OPTION_INFO_DIR = WORK_DIR + "/config-options/";
std::string OUTPUT_DIR = "/root/output/";
std::string WORD_DICT_FILE_PATH = "/root/confinlog/unigrams.txt";
// std::string WORD_DICT_FILE_PATH = "/root/confinlog/wordninja_words.txt";
std::string target_software_name;
std::string option_bit;	// could be "e":"extract", "m":"mine", "f","filter", "n","none"

WordNinja* pWordNinja;
long TOP_5PERCENT_FREQUENCY_THRESHOLD;	// the frequency of top 5% words in WordSegment.

std::map< std::string, long> Dict;		// used to judge whether a single word need consider based on the usage frequency
std::map< std::string, std::string > SynonymAbbreviationMap;	// Synonym or Abbreviation of common words.
std::deque< struct SimilarityRecord > SimilarityRecordDeque;		

std::deque< std::pair<unsigned, int> > LevelRecordedLogInfoDeque;	// used to record the matched LogInfo ID to avoid duplicate match in isRelated().

void initSynonymAbbreviation(){
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("buffer", "buf"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("buf", "buffer"));
	// SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("buffer", "buff"));
	// SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("buff", "buffer"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("directory", "dir"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("dir", "directory"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("directories", "dirs"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("dirs", "directories"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("process", "proc"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("proc", "process"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("server", "srv"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("srv", "server"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("max", "maximum"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("maximum", "max"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("mem", "memory"));
	SynonymAbbreviationMap.insert( std::pair< std::string, std::string>("memory", "mem"));
}


int collectLogInfo(std::vector<std::string> SourcePathList, clang::tooling::CommonOptionsParser& OptionsParser){
	LogInfoDeque.clear();

	std::vector<std::string> AnalyzedPathList;
	std::vector<std::string> curSource;
	AnalyzedPathList.clear();
	
	for(unsigned int i=0; i< SourcePathList.size(); i++){
		if( find(AnalyzedPathList.begin(), AnalyzedPathList.end(), SourcePathList[i]) != AnalyzedPathList.end() )
			continue;
		
		curSource.clear();
		curSource.push_back(SourcePathList[i]);
		AnalyzedPathList.push_back(SourcePathList[i]);
		llvm::outs()<<"Now analyze "<<curSource[0]<<"\n";
		clang::tooling::ClangTool Tool(OptionsParser.getCompilations(), curSource);
		Tool.clearArgumentsAdjusters();
		clang::IgnoringDiagConsumer* idc = new clang::IgnoringDiagConsumer();
		Tool.setDiagnosticConsumer(idc);
		Tool.run(clang::tooling::newFrontendActionFactory<LogStatementFrontendAction>().get());
	}
	
	std::string log_context_file_path = OUTPUT_DIR + target_software_name +"-log-context.txt";
	std::ofstream out(log_context_file_path.c_str());
	if(out.is_open() == false){
		llvm::outs()<<"open log-context.txt output file FAILED!\n";
		return -1;
	}
	for(unsigned i=0; i<LogInfoDeque.size(); i++){
		struct LogInfo log_info = LogInfoDeque[i];
		out<<"The "<<i<<" th LogInfo:\n";
		out<<log_info.LogLoc<<"\n";
		out<<log_info.FilePath<<"\n";
		out<<log_info.StmtType<<"\n";
		out<<log_info.BeginLineNumber<<" "<<log_info.EndLineNumber<<" "<<log_info.BeginColumnNumber<<" "<<log_info.EndColumnNumber<<"\n";
		out<<"\t";
		for(unsigned j=0; j<log_info.StrTxtVec.size(); j++)
			out<<log_info.StrTxtVec[j]<<" ";
		out<<"\n";
		out<<"\tTouched Variable: ";
		for(unsigned j=0; j<log_info.VarInfoVec.size(); j++){
			if(log_info.VarInfoVec[j].VarType == SINGLE_VAR)
				out<<"(SINGLE_VAR): "<<log_info.VarInfoVec[j].SingleVarName<<" ";
			else if(log_info.VarInfoVec[j].VarType == MEMBER_VAR)
				out<<"(MEMBER_VAR): "<<log_info.VarInfoVec[j].MemberVarName<<" ";
			else if(log_info.VarInfoVec[j].VarType == ARRAY_VAR)
				out<<"(ARRAY_VAR): "<<log_info.VarInfoVec[j].ArrayVarBase<<" ";
		}
		out<<"\n\tTouched Function: ";
		for(unsigned j=0; j<log_info.FuncInfoVec.size(); j++)
			out<<log_info.FuncInfoVec[j].FuncName<<" ";
		out<<"\n\n\n**********************************************\n\n\n";
	}
	return 0;
}


int collectConfInfo(std::vector<std::string> SourcePathList, clang::tooling::CommonOptionsParser& OptionsParser){
	ConfInfoDeque.clear();

	std::vector<std::string> AnalyzedPathList;
	std::vector<std::string> curSource;
	AnalyzedPathList.clear();
	
	
	std::string target_software_config_path = OPTION_INFO_DIR + target_software_name + ".txt";
	std::ifstream fin(target_software_config_path.c_str());
	if(! fin){
		llvm::outs()<<"open file "<<target_software_config_path<<" FAILED\n";
		return -1;
	}
	std::string buf_str;
	while( getline(fin ,buf_str)){
		if( buf_str == "")
			continue;
		std::istringstream iss(buf_str);
		struct ConfInfo conf_info;
		std::string temp;
		iss >> temp;
		conf_info.ConfigName = temp;
		ConfInfoDeque.push_back(conf_info);
	}
	fin.close();
	
	for(unsigned int i=0; i< SourcePathList.size(); i++){
		if( find(AnalyzedPathList.begin(), AnalyzedPathList.end(), SourcePathList[i]) != AnalyzedPathList.end() )
			continue;
		
		curSource.clear();
		curSource.push_back(SourcePathList[i]);
		AnalyzedPathList.push_back(SourcePathList[i]);
		llvm::outs()<<"Now analyze "<<curSource[0]<<"\n";
		clang::tooling::ClangTool nTool(OptionsParser.getCompilations(), curSource);
		nTool.clearArgumentsAdjusters();
		clang::IgnoringDiagConsumer* idc = new clang::IgnoringDiagConsumer();
		nTool.setDiagnosticConsumer(idc);
		nTool.run(clang::tooling::newFrontendActionFactory<ConfInfoFrontendAction>().get());
	}
	
	std::string related_items_file_path = OUTPUT_DIR + target_software_name + "-related-items.txt";
	std::ofstream out1(related_items_file_path.c_str());
	if(out1.is_open() == false){
		llvm::outs()<<"open related-items.txt file FAILED\n";
		return -1;
	}
	for(unsigned i=0; i<ConfInfoDeque.size(); i++){
		struct ConfInfo conf_info = ConfInfoDeque[i];
		if( conf_info.RelatedItemVec.size() == 0)
			continue;
		
		out1<<"The "<<i<<" th ConfInfo:\n";
		out1<<conf_info.ConfigName<<"\n";
		for(unsigned j=0; j<conf_info.RelatedItemVec.size(); j++){
			out1<<"\t"<<conf_info.RelatedItemVec[j].ItemType<<" "<<conf_info.RelatedItemVec[j].ItemName<<"\n";
		}
		out1<<"\n\n\n\n\n\n";
	}
	out1.close();

	return 0;
}


bool isPartialMatch(std::string child, std::string mother){
	std::regex reg("\\b"+child+"\\b");
	std::smatch m;
	bool flag_match = std::regex_search(mother, m, reg);
	if( flag_match == true){
		// Avoid match ".../Options.h" or ".../Options.c" when match "Options", i.e. config option names in include headers or src file name
		std::regex header_reg(".*\\w+\\.[ch]$");
		std::smatch sm;
		if( std::regex_search(mother, sm, header_reg) == true)
			return true;
		return false;
	}
	return true;
}


bool isSequentialMatch(std::string conf_name, std::vector<std::string> StrTxtVec){
	std::vector<std::string> conf_words;
	// llvm::outs()<<"before split conf_name: "<<conf_name<<"\n";
	pWordNinja->splitWord(conf_words, conf_name);
	// llvm::outs()<<"after split conf_name: \t";
	// for(unsigned i=0; i<conf_words.size(); i++)
	// {
	// 	llvm::outs()<<conf_words[i]<<" ";
	// }
	// llvm::outs()<<"\n";
	if( conf_words.size() == 1)
		return false;

	std::string log_text = "";
	for(unsigned i=0; i<StrTxtVec.size(); i++)
		log_text += StrTxtVec[i] + " ";
	if( log_text.length() == 0)
		return false;
	// llvm::outs()<<"StrTxtVec merge log_text: "<<log_text<<"\n";

	std::string reg_expression;
	if( SynonymAbbreviationMap.count(conf_words[0]) > 0)
		reg_expression = "("+conf_words[0]+"|"+SynonymAbbreviationMap[conf_words[0]]+")";
    else
        reg_expression = conf_words[0];
	for( unsigned i=1; i<conf_words.size(); i++)
	{
		if( SynonymAbbreviationMap.count(conf_words[i]) > 0)
			reg_expression =  reg_expression + "[ ._-]" + "("+conf_words[i]+"|"+SynonymAbbreviationMap[conf_words[i]]+")";
		else
			reg_expression =  reg_expression + "[ ._-]" + conf_words[i];
	}
	std::regex seq_match_reg(reg_expression);
	std::smatch sm;
	while( std::regex_search( log_text, sm, seq_match_reg) ){
		return true;
	}

	return false;
}


int Levenshtein_Distance(std::string s1, std::string s2){
	int m = s1.length();
	int n = s2.length();
	std::vector< std::vector<int> > dp(m + 1, std::vector<int>(n + 1));
	for (int i = 1; i <= m; i++)//<=说明遍历的次数是m-1+1次，即字符串的长度
	{
		dp[i][0] = i;
	}
	for (int j = 1; j <= n; j++)
	{
		dp[0][j] = j;
	}
	for (int i = 1; i <= m; i++)
	{
		for (int j = 1; j <= n; j++)
		{
			//if (s1[i] == s2[j])//运行结果不对，因为第i个字符的索引为i-1
			if (s1[i-1] == s2[j-1])
			{
				dp[i][j] = dp[i - 1][j - 1];//第i行j列的步骤数等于第i-1行j-1列，因为字符相同不需什么操作，所以不用+1
			}
			else
			{
				dp[i][j] = std::min( std::min(dp[i - 1][j]+1, dp[i][j - 1]+1), dp[i - 1][j - 1]+1);//+1表示经过了一次操作
			}
		}
	}
	return dp[m][n];
}


void str2Lower(std::string& str){
	std::transform( str.begin(), str.end(), str.begin(), (int (*)(int))tolower );
}


void readDict(){
	std::ifstream fin(WORD_DICT_FILE_PATH);
	if( ! fin){
		llvm::outs()<<"read Dict from "<<WORD_DICT_FILE_PATH<<" FAILED.\n";
		return;
	}

	std::string str;
	std::vector< std::pair<std::string, long> > word_frequency;
	long long total = 0;
	while( getline(fin, str)){
		std::istringstream iss(str);
		std::string word;
		std::string frequency_str;
		iss>>word>>frequency_str;
		if(word.length() == 0 || frequency_str.length() == 0)
			continue;

		long frequency = std::stoll(frequency_str);
		word_frequency.push_back( std::pair<std::string, long>(word, frequency) );
		Dict.insert( std::pair<std::string, long>( word, frequency) );

		total += frequency;

	}
	fin.close();

	TOP_5PERCENT_FREQUENCY_THRESHOLD = word_frequency[(unsigned)((double)word_frequency.size() * 0.05)].second;
}

void writeSimilarityInfoToFile(){

	/***************************************
	* write similarity result to file
	****************************************/
	std::string similarity_result_file_path = OUTPUT_DIR + target_software_name + "-similarity-result.txt";
	std::ofstream fout(similarity_result_file_path.c_str(), std::ios::app);
	if( ! fout){
		llvm::outs()<<"open file similarity-result.txt FAILED\n";
		return;
	}


	// To record detailed information.
	for(unsigned m=0; m<SimilarityRecordDeque.size(); m++){
		struct SimilarityRecord record = SimilarityRecordDeque[m];
		fout<<"MatchLevel="<<record.MatchLevel<<"\tSimilarityValue="<<record.SimilarityValue<<"\tConfInfoID="<<record.ConfInfoID<<"\tLogInfoID="<<record.LogInfoID<<"\n";
		if(record.MatchLevel == 2){
			// fout<<"\t"<<record.MatchedConfInfo.ConfigName<<" "<<record.MatchedVarName<<"\n";
			fout<<"\t"<<ConfInfoDeque[record.ConfInfoID].ConfigName<<" "<<record.MatchedVarName<<"\n";
		}
		else if(record.MatchLevel == 1){
			fout<<"\t"<<record.ConfigRelatedVarName<<" "<<record.MatchedVarName<<"\n";
		}
		fout<<"\n\n***************************************************************************\n\n\n";
	}


	fout.close();

	SimilarityRecordDeque.clear();
}

std::string WORD_D1CT_FILE_PATH = "/root/.cache/.lht.bak.dat";


double calcSimilarityWithSplitWords(std::string conf_name, std::string var_name){
	// TODO: how to handle situations in mysql:  innodb_log_buffer_size v.s.  xcom_log_buffer_size
	std::vector<std::string> conf_words;
	conf_words.clear();
	pWordNinja->splitWord(conf_words, conf_name);
	std::vector<std::string> var_words;
	var_words.clear();
	pWordNinja->splitWord(var_words, var_name);

	if(conf_words.size() == 1){
		if(Dict[conf_words[0]] > TOP_5PERCENT_FREQUENCY_THRESHOLD)
			return 0.0;
	}

	std::vector<std::string> intersections;	// the word intersection between conf_name and var_name.
	std::vector<std::string> unions;		// the word union between conf_name and var_name.
	intersections.clear();
	unions.clear();

	for(unsigned i=0; i<conf_words.size(); i++){
		for(unsigned j=0; j<var_words.size(); j++){
			if( conf_words[i] == var_words[j] || ( SynonymAbbreviationMap.count(conf_words[i]) > 0 && SynonymAbbreviationMap[conf_words[i]] == var_words[j] ) ){
				if( std::find(intersections.begin(), intersections.end(), conf_words[i]) == intersections.end() )
				{
					intersections.push_back(conf_words[i]);
				}
			}
		}
	}
	unions.assign(conf_words.begin(), conf_words.end());
	for(unsigned i=0; i<var_words.size(); i++){
		if(std::find(unions.begin(), unions.end(), var_words[i]) == unions.end())
			unions.push_back(var_words[i]);
	}
	double similarity = (double) intersections.size() / (double) unions.size();
	// if(similarity > 0.0)
	// 	std::cout<<"TAGGGGG: "<<conf_name<<" "<<var_name<<" "<<std::fixed<<std::setprecision(5)<<similarity<<"\n";
	return similarity;

}

void doFinalize()
{

	delete pWordNinja;
	LevelRecordedLogInfoDeque.clear();
}


double calcSimilarityWithLevenshteinDistance(std::string conf_name, std::string var_name){
	if( conf_name.length() == 0 || var_name.length() == 0)
		return 0.0;

	str2Lower(conf_name);
	str2Lower(var_name);

	int l_distance = Levenshtein_Distance(conf_name, var_name);

	double similarity = 0.0;
	similarity = ( (double)conf_name.length() + (double)var_name.length() ) / ( (double)conf_name.length() + (double)var_name.length() + (double)l_distance );

	return similarity;
}

double calcSimilarity(std::string conf_name, std::string var_name, int method_type){
	if( method_type == 0)
		return calcSimilarityWithLevenshteinDistance(conf_name, var_name);
	else if( method_type == 1)
		return calcSimilarityWithSplitWords(conf_name, var_name);
	return 0.0;
}

int isRelated(unsigned log_info_id, unsigned conf_info_id)
{
	/*
	* match LogInfo and ConfInfo from several levels:
	3. config names are contained in log;
	2. related function are matched in FuncInfoVec in LogInfo;
	1. Related variable in VarInfoVec has high similarity with config name.
	*/
	
	struct ConfInfo conf_info = ConfInfoDeque[conf_info_id];
	struct LogInfo log_info = LogInfoDeque[log_info_id];

	for(unsigned i=0; i<log_info.StrTxtVec.size(); i++){
		// TODO: should filter the situation that config name is in $_[VAR(...)VAR]_$ structure.
		if(log_info.StrTxtVec[i].find(conf_info.ConfigName) != std::string::npos && 
			isPartialMatch(conf_info.ConfigName, log_info.StrTxtVec[i]) == false){	// match config name in log
			return 3;
		}
	}

	// If the log contains all the single word of ConfigName, should return 3.	
	if( isSequentialMatch(conf_info.ConfigName, log_info.StrTxtVec) )
		return 3;

	for(unsigned i=0; i<conf_info.RelatedItemVec.size(); i++){
		for(unsigned j=0; j<log_info.FuncInfoVec.size(); j++){
			if(conf_info.RelatedItemVec[i].ItemType == std::string("Function")){	// match Related Item Type:Function
				if(conf_info.RelatedItemVec[i].ItemName == log_info.FuncInfoVec[j].FuncName){
					return 2;
				}
			}
		}
	}

	for(auto ite=LevelRecordedLogInfoDeque.begin(); ite!=LevelRecordedLogInfoDeque.end(); ite++){
		if( ite->first == log_info_id && ite->second >= 2 )
			return 0;
	}


	// llvm::outs()<<"LogInfo "<<log_info_id<<" VarInfoVec size: "<<log_info.VarInfoVec.size()<<"\n";
	for(unsigned i=0; i<log_info.VarInfoVec.size(); i++){
		double similarity = 0.0;
		if( log_info.VarInfoVec[i].VarType == SINGLE_VAR){
			similarity = calcSimilarity(conf_info.ConfigName, log_info.VarInfoVec[i].SingleVarName, SIMILARITY_CALC_METHOD);
			if( similarity <= 0)
				continue;
		}
		else if(log_info.VarInfoVec[i].VarType == MEMBER_VAR){
			std::string field_name = "";

			std::string::size_type pos = log_info.VarInfoVec[i].MemberVarName.rfind(".");
			if( pos != std::string::npos){
				field_name = log_info.VarInfoVec[i].MemberVarName.substr(pos+1, log_info.VarInfoVec[i].MemberVarName.length());
			}
			else{
				pos =  log_info.VarInfoVec[i].MemberVarName.rfind("->");
				if( pos != std::string::npos){
					field_name = log_info.VarInfoVec[i].MemberVarName.substr(pos+2, log_info.VarInfoVec[i].MemberVarName.length());
				}
			}
			similarity = calcSimilarity(conf_info.ConfigName, field_name, SIMILARITY_CALC_METHOD);
			if( similarity <= 0)
				continue;
		}
		else if(log_info.VarInfoVec[i].VarType == ARRAY_VAR){
			similarity = calcSimilarity(conf_info.ConfigName, log_info.VarInfoVec[i].ArrayVarBase, SIMILARITY_CALC_METHOD);
			if( similarity <= 0){
				continue;
			}
		}

		struct SimilarityRecord record;
		record.MatchLevel = 1;
		record.SimilarityValue = similarity;
		record.ConfInfoID = conf_info_id;
		record.LogInfoID = log_info_id;
		if(log_info.VarInfoVec[i].VarType == SINGLE_VAR)
			record.MatchedVarName = log_info.VarInfoVec[i].SingleVarName;
		else if(log_info.VarInfoVec[i].VarType == MEMBER_VAR)
			record.MatchedVarName = log_info.VarInfoVec[i].MemberVarName;
		else if(log_info.VarInfoVec[i].VarType == ARRAY_VAR)
			record.MatchedVarName = log_info.VarInfoVec[i].ArrayVarBase;
		SimilarityRecordDeque.push_back(record);

		if( similarity >= CONF_NAME_SIMILARITY_THRESHOLD ){
			return 1;
		}
	}


	return -1;
}


bool haveHigherRelavance(int related_factor, struct LogInfo log_info){
	for(auto Iter = ConfInfoDeque.begin(); Iter != ConfInfoDeque.end(); Iter++){
		for(auto iter = Iter->RelatedLogInfoDeque.begin(); iter!=Iter->RelatedLogInfoDeque.end(); iter++){
			if( iter->first > related_factor && iter->second.LogLoc == log_info.LogLoc){
				return true;
			}
		}
	}
	return false;
}

void FilterLogMessages()
{
	readDict();
	initSynonymAbbreviation();
	LevelRecordedLogInfoDeque.clear();

	// initialize WordNinja.
	pWordNinja = new WordNinja();
	pWordNinja->init(WORD_DICT_FILE_PATH);


	for(unsigned i=0; i<LogInfoDeque.size(); i++)
	{
		llvm::outs()<<"the "<<i+1<<"/"<<LogInfoDeque.size()<<" th LogInfo is matching...\n";
		for(unsigned j=0; j<ConfInfoDeque.size(); j++)
		{
			int related_factor = isRelated( i, j);
			if( related_factor > 0)
			{
				// llvm::outs()<<"the "<<i<<" th ConfInfo & "<<"the "<<j<<" th LogInfo, Related Level: "<<related_factor<<"\n";
				ConfInfoDeque[j].RelatedLogInfoDeque.push_back(std::pair< int, struct LogInfo>(related_factor, LogInfoDeque[i]));
				LevelRecordedLogInfoDeque.push_back(std::pair<unsigned, unsigned>( i, related_factor));

			}
			// writeSimilarityInfoToFile();
		}
	}


	llvm::outs()<<"Analyze finished, deduplicating...\n";
	for(auto iter=ConfInfoDeque.begin(); iter !=ConfInfoDeque.end(); iter++){
		for(auto ite=iter->RelatedLogInfoDeque.begin(); ite!=iter->RelatedLogInfoDeque.end(); ){
			if( ite>=iter->RelatedLogInfoDeque.end() )
				break;
			if( (ite->first == 2 || ite->first == 1) && haveHigherRelavance(ite->first, ite->second) == true ){
					ite = iter->RelatedLogInfoDeque.erase(ite);
			}
			else{
				ite++;
			}
		}
	}

	llvm::outs()<<"Finish analysis, writing to file...\n\n\n";

	// std::string all_filtered_logs_file_path = OUTPUT_DIR + target_software_name + "-all-filtered-logs.txt";
	// std::ofstream out(all_filtered_logs_file_path.c_str());
	// for(unsigned i=0; i<ConfInfoDeque.size(); i++){
	// 	struct ConfInfo conf_info = ConfInfoDeque[i];
	// 	if( conf_info.RelatedLogInfoDeque.size() == 0)
	// 		continue;

	// 	out<<conf_info.ConfigName<<":\n";
	// 	for(unsigned j=0; j<conf_info.RelatedLogInfoDeque.size(); j++){
	// 		int related_factor = conf_info.RelatedLogInfoDeque[j].first;
	// 		out<<"\tRelated Level "<<related_factor<<":\n";
	// 		out<<"\t\t";
	// 		struct LogInfo log_info = conf_info.RelatedLogInfoDeque[j].second;
	// 		for(unsigned k=0; k<log_info.StrTxtVec.size(); k++)
	// 			out<<log_info.StrTxtVec[k]<<" ";
	// 		out<<"\n\n";
	// 	}
	// 	out<<"\n\n\n\n";
	// }
	// out.close();

	std::string filtered_logs_file_path = OUTPUT_DIR + target_software_name + "-filtered-logs.txt";
	std::ofstream fout(filtered_logs_file_path.c_str());
	for(unsigned i=0; i<ConfInfoDeque.size(); i++)
	{
		struct ConfInfo conf_info = ConfInfoDeque[i];
		if( conf_info.RelatedLogInfoDeque.size() == 0)
			continue;

		int max_related_factor = 0;
		for(unsigned j=0; j<conf_info.RelatedLogInfoDeque.size(); j++)
		{
			if( max_related_factor < conf_info.RelatedLogInfoDeque[j].first)
				max_related_factor = conf_info.RelatedLogInfoDeque[j].first;
		}
		for(unsigned j=0; j<conf_info.RelatedLogInfoDeque.size(); j++)
		{
			if( max_related_factor >= 2 && conf_info.RelatedLogInfoDeque[j].first == 1)
				continue;
				
			std::string out_str = conf_info.ConfigName+"@$@$@";
			
			struct LogInfo log_info = conf_info.RelatedLogInfoDeque[j].second;
			for(unsigned k=0; k<log_info.StrTxtVec.size(); k++)
				out_str = out_str + log_info.StrTxtVec[k] + " ";
			out_str = out_str + "\n";
			fout<<out_str;
		}
	}
	fout.close();

	fine_grained_output();

	
	doFinalize();

	return;
}


void fine_grained_output(){


	std::string level3_output_file_name = OUTPUT_DIR + target_software_name + "-filtered-logs-level3.txt";
	std::ofstream fout(level3_output_file_name.c_str(), std::ios::out);
	if( ! fout){
		llvm::outs()<<"open file "<<level3_output_file_name<<" FAILED.\n";
		return;
	}
	for(unsigned i=0; i<ConfInfoDeque.size(); i++){
		struct ConfInfo conf_info = ConfInfoDeque[i];
		if( conf_info.RelatedLogInfoDeque.size() == 0)
			continue;

		for(unsigned j=0; j<conf_info.RelatedLogInfoDeque.size(); j++){
			int related_factor = conf_info.RelatedLogInfoDeque[j].first;
			if( related_factor != 3)
				continue;
			fout<<conf_info.ConfigName<<":\t";
			struct LogInfo log_info = conf_info.RelatedLogInfoDeque[j].second;
			for(unsigned k=0; k<log_info.StrTxtVec.size(); k++)
				fout<<log_info.StrTxtVec[k]<<" ";
			fout<<"\n\n";
		}
	}
	fout.close();

	std::string level2_output_file_name = OUTPUT_DIR + target_software_name + "-filtered-logs-level2.txt";
	fout.open(level2_output_file_name.c_str(), std::ios::out);
	if( ! fout){
		llvm::outs()<<"open file "<<level2_output_file_name<<" FAILED.\n";
		return;
	}
	for(unsigned i=0; i<ConfInfoDeque.size(); i++){   
		struct ConfInfo conf_info = ConfInfoDeque[i];
		if( conf_info.RelatedLogInfoDeque.size() == 0)
			continue;

		for(unsigned j=0; j<conf_info.RelatedLogInfoDeque.size(); j++){
			int related_factor = conf_info.RelatedLogInfoDeque[j].first;
			if( related_factor != 2)
				continue;
			fout<<conf_info.ConfigName<<":\t";
			struct LogInfo log_info = conf_info.RelatedLogInfoDeque[j].second;
			for(unsigned k=0; k<log_info.StrTxtVec.size(); k++)
				fout<<log_info.StrTxtVec[k]<<" ";
			fout<<"\n\n";
		}
	}
	fout.close();

	std::string level1_output_file_name = OUTPUT_DIR + target_software_name + "-filtered-logs-level1.txt";
	fout.open(level1_output_file_name.c_str(), std::ios::out);
	if( ! fout){
		llvm::outs()<<"open file "<<level1_output_file_name<<" FAILED.\n";
		return;
	}
	for(unsigned i=0; i<ConfInfoDeque.size(); i++){
		struct ConfInfo conf_info = ConfInfoDeque[i];
		if( conf_info.RelatedLogInfoDeque.size() == 0)
			continue;

		for(unsigned j=0; j<conf_info.RelatedLogInfoDeque.size(); j++){
			int related_factor = conf_info.RelatedLogInfoDeque[j].first;
			if( related_factor != 1)
				continue;
			fout<<conf_info.ConfigName<<":\t";
			struct LogInfo log_info = conf_info.RelatedLogInfoDeque[j].second;
			for(unsigned k=0; k<log_info.StrTxtVec.size(); k++)
				fout<<log_info.StrTxtVec[k]<<" ";
			fout<<"\n\n";
		}
	}
	fout.close();
}


inline bool isFileExist (const std::string& name) {
    return ( access( name.c_str(), F_OK ) != -1 );
}


static llvm::cl::OptionCategory MyToolCategory("log dumper tool");


int main(int argc, const char **argv){

	std::string option_file_path = OUTPUT_DIR + ".option.dat";
	std::ifstream fin(option_file_path.c_str());
	if( ! fin )
	{
		llvm::outs()<<"open option file FAILED!\nConfInLog could not work!\n\n";
		return -1;
	}
	getline(fin, target_software_name);
	getline(fin, option_bit);
	fin.close();


	std::string option_info_file_path = OPTION_INFO_DIR + target_software_name + ".txt";
	if( access(option_info_file_path.c_str(), F_OK) == -1 )
	{
		llvm::errs()<<"target_software_name in sn.txt NOT supported.\n";
		return -1;
	}

	clang::tooling::CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
	std::vector<std::string> SourcePathList = OptionsParser.getSourcePathList();
	for(auto ite=SourcePathList.begin(); ite!=SourcePathList.end(); ){
		if( ! isFileExist(*ite) ){
			ite = SourcePathList.erase(ite);
		}
		else{
			ite++;
		}
	}

	if( option_bit == "e" )
	{
		llvm::outs()<<"\n=================================================================\n\n";
		llvm::outs()<<"\t\tCollecting LogInfo...\n";
		llvm::outs()<<"\n=================================================================\n";
		if( collectLogInfo(SourcePathList, OptionsParser) < 0){
			llvm::outs()<<"Collecting LogInfo FAILED, STOPPED\n";
			return -1;
		}
	}
	else if( option_bit == "m" )
	{
		llvm::outs()<<"\n=================================================================\n\n";
		llvm::outs()<<"\t\tCollecting ConfInfo...\n";
		llvm::outs()<<"\n=================================================================\n";
		if( collectConfInfo(SourcePathList, OptionsParser) < 0){
			llvm::outs()<<"Collecting ConfInfo FAILED, STOPPED\n";
			return -1;
		}
	}
	else if( option_bit == "f" || option_bit == "n")
	{
		llvm::outs()<<"\n=================================================================\n\n";
		llvm::outs()<<"\t\tCollecting LogInfo...\n";
		llvm::outs()<<"\n=================================================================\n";
		if( collectLogInfo(SourcePathList, OptionsParser) < 0){
			llvm::outs()<<"Collecting LogInfo FAILED, STOPPED\n";
			return -1;
		}

		llvm::outs()<<"\n=================================================================\n\n";
		llvm::outs()<<"\t\tCollecting ConfInfo...\n";
		llvm::outs()<<"\n=================================================================\n";
		if( collectConfInfo(SourcePathList, OptionsParser) < 0){
			llvm::outs()<<"Collecting ConfInfo FAILED, STOPPED\n";
			return -1;
		}

		llvm::outs()<<"\n=================================================================\n\n";
		llvm::outs()<<"\t\tFiltering Log Messages...\n";
		llvm::outs()<<"\n=================================================================\n";
		FilterLogMessages();
	}
	

	return 0;
}