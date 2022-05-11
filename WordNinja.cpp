#include "WordNinja.h"

using namespace std;


WordNinja::WordNinja():m_wordMaxLen(0){}
WordNinja::~WordNinja(){}

inline bool WordNinja::isDigit(const std::string& str)
{
	std::stringstream s;
	s << str;
	double d = 0;
	char c;
	return (s >> d) ? !(s >> c): false;
}

int WordNinja::init(const std::string dictPath){
	m_dict.clear();
	//reading dict file
	ifstream infile(dictPath);
	if(!infile.is_open()){
		cout<<"dict file not exist"<<endl;
		return -1;
	}
	string line;
	// string word;
	vector<string> words;
	words.reserve(20000);
	while (!infile.eof()){
		// getline(infile,word);
        // if(!word.size()) continue;
        // words.push_back(word);
		getline(infile, line);
		if(!line.size())
			continue;
		istringstream iss(line);
		std::string word;
		std::string frequency_str;
		iss>>word>>frequency_str;
		if( ! word.size())
			continue;
		words.push_back(word);
	}
	int dictSize = words.size();
	// cout<<"dictsize:"<<dictSize<<endl;
	for(unsigned i=0; i<words.size(); i++)
	{
		m_dict[words[i]] = log((i + 1) * log(dictSize));
		m_wordMaxLen = std::max(m_wordMaxLen,(int)words[i].length());
	}
	return 0;
}


int WordNinja::split(const std::string inputText, std::vector<std::string>& result){
	result.clear();
	if(inputText.empty())
		return -1;
	string lowerStr = "";
	for(unsigned i=0; i<inputText.size(); i++){
		if((inputText[i]>='a' && inputText[i]<='z') || (inputText[i]>='A' && inputText[i]<='Z') || (inputText[i]>='0' && inputText[i]<='9'))
			lowerStr += inputText[i];
	}
	std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), (int (*)(int))tolower);
	
	const int len = lowerStr.size();

	vector<std::pair<float, int>> cost;
	cost.reserve(len + 1);
	cost.push_back(make_pair(0, -1));
	string subStr = "";
	float curCost = 0.0;
	for(int i = 1; i < len + 1; i++){
		float minCost = cost[i - 1].first + 9e9;
		int minCostIndex = i - 1;

		for(int j = i - m_wordMaxLen > 0 ? i - m_wordMaxLen : 0; j < i; j++)
		{
			subStr = lowerStr.substr(j, i - j);
			if(m_dict.find(subStr) == m_dict.end())continue;

			curCost = cost[j].first + m_dict.at(subStr);
			if(minCost > curCost)
			{
				minCost = curCost;
				minCostIndex = j;
			}
		}
		cost.push_back(std::make_pair(minCost, minCostIndex));
	}

	int n = len;
	int preIndex;
	while (n > 0)
	{
		preIndex = cost[n].second;
		string insertStr = inputText.substr(preIndex, n - preIndex);
		if(!result.empty() && isDigit(insertStr + result[0]))
		{
			result[0] = insertStr + result[0];
		}
		else
		{
			result.insert(result.begin(), insertStr);
		}
		n = preIndex;
	}

	return 0;
}

void WordNinja::str2Lower(std::string& str){
	std::transform( str.begin(), str.end(), str.begin(), (int (*)(int))tolower );
}

void WordNinja::splitWord(std::vector<std::string>& Splits, std::string Word){
	char* buf = const_cast<char*>(Word.c_str());
	unsigned len = strlen(buf);
//  printf("<$ %s $>\n\n", buf);
	unsigned phead = 0, ptail = 0;
	bool flag_upper_case_mode = false;
	std::vector<std::string> tmpSplits;
	while(1){
//      printf("<$ %c $> %d %d %d\n", buf[ptail], phead, ptail, flag_upper_case_mode);
		if( ptail == len){
			std::string substr = Word.substr(phead, ptail-phead);
			str2Lower(substr);
			tmpSplits.push_back(substr);
			break;
		}
		else if(buf[ptail] == '-' || buf[ptail] == '.' || buf[ptail] == '_' || buf[ptail] == '\n' || buf[ptail] == '\0'|| buf[ptail] == '<' || buf[ptail] == '>'){
			std::string substr = Word.substr(phead, ptail-phead);
			str2Lower(substr);
			tmpSplits.push_back(substr);
			phead = ++ptail;
		}
		else if( isupper(buf[ptail]) != 0 && phead == ptail){
			ptail++;
			flag_upper_case_mode = true;
		}
		else if( isupper(buf[ptail]) != 0 && phead != ptail && flag_upper_case_mode == true){
			if( ptail+1 < len && isupper(buf[ptail+1]) == 0){
				if( ptail - phead == 1){
					ptail++;
				}
				else{
					flag_upper_case_mode = false;
					std::string substr = Word.substr(phead, ptail-phead);
					str2Lower(substr);
					tmpSplits.push_back(substr);
					phead = ptail;
				}
			}
			else if(ptail+1 < len && isupper(buf[ptail+1]) != 0){
				ptail++;
			}
			else if( ptail +1 == len){
				std::string substr = Word.substr(phead, ptail-phead);
				str2Lower(substr);
				tmpSplits.push_back(substr);
				break;
			}
			else{
				ptail++;
			}
		}
		else if( isupper(buf[ptail]) != 0 && flag_upper_case_mode == false && phead != ptail ){
			std::string substr = Word.substr(phead, ptail-phead);
			str2Lower(substr);
			tmpSplits.push_back(substr);
			phead = ptail;
		}
		else{
			ptail++;
			flag_upper_case_mode = false;
		}
	}

	for(unsigned i=0; i<tmpSplits.size(); i++){
		std::string tmp = tmpSplits[i];
		std::vector<std::string> result;
		this->split(tmp, result);
		for(unsigned j=0; j<result.size();j++){
			Splits.push_back(result[j]);
		}
	}
}