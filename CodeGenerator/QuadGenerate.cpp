#include "DataStructureDefine.h"
#include "generateHead.h"
using namespace std;

deque<StackItem> Stack;
map<unsigned int, std::map<std::string, TableItem> > _parseTable;
list<Token> tokenList;

//中间代码生成阶段分析结果，四元式集合+各符号表
vector<SymbolTable*> symbolTables;
SymbolTable *globalTable, *currentTable;
vector<Quadruple> middleCode;
stack<string> paramStack;
int nextInstr = 0;

//label对行号的映射
map<string, int> labelMap;

void errorInReduce(list<Token>::iterator token) {
	cout << "error at line " << token->_line << ",offset " << token->_offset << endl;

}
bool translateToMiddleCode(list<Token> tokenList, string path) {
	//验证归约序列
	ofstream sequence(path + "reduce_sequence.txt");
	initTable(_parseTable);	//初始化语法分析表
	globalTable = new SymbolTable("global");
	globalTable->beginIndex = nextInstr;
	currentTable = globalTable;

	tokenList.push_back(Token("$", "$", 0, 0));
	Stack.push_back(StackItem(0, "$"));
	auto token = tokenList.begin(), end = tokenList.end();
	TableItem tableitem;
	//reduceHead存放产生式左部的各个属性
	map<string, TableItem> curTable;
	//curTable是状态机中当前状态的转换表

	while (token != end) {	
		
		curTable = _parseTable.find(Stack.back().state)->second;
		//根据当前token从转换表中找操作，是移入下一个状态还是归约
		map<string,TableItem>::iterator it = curTable.find(token->_attribute);
		if (it != curTable.end()) {
			tableitem = it->second;
		}
		else { 
			return false; 
		}
		if (tableitem.action == SHIFT) {
			map<string, string> map;
			map["lexeme"] = token->_lexecal;
			//把转移到的下一个状态存入分析栈。
			Stack.push_back(StackItem(tableitem.index, token->_attribute, map));
			token++;
		}
		else if (tableitem.action == REDUCTION) {	
			//执行规约时语义动作，返回产生式右边符号数量和左边非终结符名字
			map<string, string> reduceHead;
			pair<unsigned int, string> item = performAction(tableitem.index, reduceHead);
			for (int i = 0; i < item.first; i++) {
				Stack.pop_back();	//弹出规约掉的符号
			}
			//栈弹出后，找到之前所在的状态
			const auto& lastTable = _parseTable.find(Stack.back().state)->second;
			//然后从这个状态，向着刚归约出符号的那条边跳转
			auto curItem = lastTable.find(item.second);
			if (curItem == lastTable.end()) {
				errorInReduce(token);
			}
			unsigned int curState = curItem->second.index;
			Stack.push_back(StackItem(curState, item.second, reduceHead));
			sequence << getProduction(tableitem.index) << endl;
		}
		else if (tableitem.action == ACCEPT) {
			sequence << getProduction(tableitem.index) << endl;
			break;
		}
		else if (tableitem.action == ERROR) {
			errorInReduce(token);
			return false;
		}
		else{
			errorInReduce(token);
			return false;
		}
	}
	//while (!Stack.empty()) {
	//	pair<unsigned int, string> item = performAction(tableitem._index, reduceHead);
	//	for (int i = 0; i < item.first; i++) {
	//		Stack.pop_back();	//弹出规约掉的符号
	//	}
	//	//这里后面可以看看能不能改成tableitem
	//	const auto& tableitem2 = _parseTable.find(Stack.back()._state)->second;
	//	auto found = tableitem2.find(item.second);
	//	if (found == tableitem2.end()) {
	//		errorInReduce(token);
	//	}
	//	//这里如果出错则继续归约？
	//	unsigned int next_state = found->second._index;
	//	Stack.push_back(StackItem(next_state, item.second, reduceHead));
	//	sequence << getProduction(tableitem._index) << endl;
	//}
	sequence.close();

	//symbolTables.push_back(globalTable);

	for (const auto& map : labelMap) {
		if (map.second == 0) {
			for (auto& code : middleCode) {
				if (code.des == "LABEL_0") {
					code.des = currentTable->funName;
				}
			}
		}
		middleCode.at(map.second).label = map.first;

	}
	outputMiddleCode(path+"MiddleCode.txt");

	return true;
}

