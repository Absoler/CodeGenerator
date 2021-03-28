#include "DataStructureDefine.h"
#include "generateHead.h"
using namespace std;

deque<StackItem> Stack;
map<unsigned int, std::map<std::string, TableItem> > _parseTable;
list<Token> tokenList;

//�м�������ɽ׶η����������Ԫʽ����+�����ű�
vector<SymbolTable*> symbolTables;
SymbolTable *globalTable, *currentTable;
vector<Quadruple> middleCode;
stack<string> paramStack;
int nextInstr = 0;

//label���кŵ�ӳ��
map<string, int> labelMap;

void errorInReduce(list<Token>::iterator token) {
	cout << "error at line " << token->_line << ",offset " << token->_offset << endl;

}
bool translateToMiddleCode(list<Token> tokenList, string path) {
	//��֤��Լ����
	ofstream sequence(path + "reduce_sequence.txt");
	initTable(_parseTable);	//��ʼ���﷨������
	globalTable = new SymbolTable("global");
	globalTable->beginIndex = nextInstr;
	currentTable = globalTable;

	tokenList.push_back(Token("$", "$", 0, 0));
	Stack.push_back(StackItem(0, "$"));
	auto token = tokenList.begin(), end = tokenList.end();
	TableItem tableitem;
	//reduceHead��Ų���ʽ�󲿵ĸ�������
	map<string, TableItem> curTable;
	//curTable��״̬���е�ǰ״̬��ת����

	while (token != end) {	
		
		curTable = _parseTable.find(Stack.back().state)->second;
		//���ݵ�ǰtoken��ת�������Ҳ�������������һ��״̬���ǹ�Լ
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
			//��ת�Ƶ�����һ��״̬�������ջ��
			Stack.push_back(StackItem(tableitem.index, token->_attribute, map));
			token++;
		}
		else if (tableitem.action == REDUCTION) {	
			//ִ�й�Լʱ���嶯�������ز���ʽ�ұ߷�����������߷��ս������
			map<string, string> reduceHead;
			pair<unsigned int, string> item = performAction(tableitem.index, reduceHead);
			for (int i = 0; i < item.first; i++) {
				Stack.pop_back();	//������Լ���ķ���
			}
			//ջ�������ҵ�֮ǰ���ڵ�״̬
			const auto& lastTable = _parseTable.find(Stack.back().state)->second;
			//Ȼ������״̬�����Ÿչ�Լ�����ŵ���������ת
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
	//		Stack.pop_back();	//������Լ���ķ���
	//	}
	//	//���������Կ����ܲ��ܸĳ�tableitem
	//	const auto& tableitem2 = _parseTable.find(Stack.back()._state)->second;
	//	auto found = tableitem2.find(item.second);
	//	if (found == tableitem2.end()) {
	//		errorInReduce(token);
	//	}
	//	//������������������Լ��
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

