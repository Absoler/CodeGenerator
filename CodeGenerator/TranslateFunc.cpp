#pragma once

#include "DataStructureDefine.h"
#include "generateHead.h"


string enter(string name, string type, unsigned int space, bool inStack) {
	if (name == "#return") {
		//��������ֵ
		return currentTable->enterVar(name, type, space, false);
	}
	else if (currentTable->funName == "global") {
		//����Ǽ�ȫ�ֱ������ز���ջ��
		return globalTable->enterVar(name, type, space, false);
	}
	else {
		return currentTable->enterVar(name, type, space, inStack);
	}
}

void createSymbolTable(string name, unsigned int returnSize) {
	//������������ʱ���ã�Ϊ�����ɷ��ű�
	if (globalTable->endIndex < 0) {
		//������ϣ���ȶ���ȫ�ֱ����ٶ����������
		globalTable->endIndex = nextInstr;
	}
	currentTable = new SymbolTable(name);
	currentTable->returnSize = returnSize;
	currentTable->beginIndex = nextInstr;
}

void returnToGlobalTable() {
	//������һ������֮��
	currentTable->endIndex = nextInstr;
	symbolTables.push_back(currentTable);
	currentTable = globalTable;
}

void addFunLabel(int index, string label) {
	//�����index�Ǻ�����ĵ�һ���м���룬Ҫ��������Ϻ������ı�ǩ
	labelMap.insert(make_pair(label, index));
}

void addToSymbolTable(string itemlist) {
	//��������������б�name,type,offset/...������������뵱ǰ���ű�
	//��һ�����ڷ����ʱ��Ͱѱ�����ַ��ã���ʱ�ٶ�BP�ѱ���ֵΪ�µĻ��¼��ʼ��
	string sep = ",/";
	string::size_type posL = itemlist.find_first_not_of(sep), posR = 0, end=string::npos;
	string name, type, space;
	while (posL != end) {
		posR = itemlist.find_first_of(sep, posL);
		if (posR != end) {
			name = itemlist.substr(posL, posR - posL);
			posL = posR + 1;
		}
		posR = itemlist.find_first_of(sep, posL);
		if (posR != end) {
			type = itemlist.substr(posL, posR - posL);
			posL = posR + 1;
		}
		posR = itemlist.find_first_of(sep, posL);
		if (posR != end) {
			space = itemlist.substr(posL, posR - posL);
			posL = posR + 1;
		}
		currentTable->enterParam(name, type, atoi(space.c_str()));	//offset???
		posL = itemlist.find_first_not_of(sep, posL);
	}
}

void setOutLiveVar(string place) {
	currentTable->at(place).live = true;
}

void error() {
	//cerr << info << endl;
	exit(1);
}

//�Ѳ�������״�����ʽ
string makeParam(string name, string type, unsigned int space) {
	return name + "," + type + "," + to_string(space) + "/";
}

static int t_count = 0;
//����tϵ����ʱ����
string newtemp(string var, bool force) {
	//Ҫ��ñ�����var����һ��
	//���var�ǳ���ʱ����ͨ��forceָ�����봴��һ��space=4��int�������ǳ���
	string name = "t" + to_string(t_count++);
	bool global = !currentTable->find(var);
	if (global) {
		auto& state = globalTable->at(var);
		int space = (force ? max(4, (int)state.space) : state.space);
		return enter(name, state.type, space);
	}
	auto& state = currentTable->at(var);
	int space = (force ? max(4, (int)state.space) : state.space);
	return enter(name, state.type, space);
}

string addNum(string str) {
	//num���ͱ������־���ֵ
	enter(str, "num", 0, false);
	return str;
}

string lookupPlace(string name) {
	if (name == "#return") {
		return name;
	}
	//����ֲ�������ȫ�ֱ����������Ծֲ�����Ϊ׼
	for (auto& iterator : currentTable->vars) {
		if (iterator.name == name) {
			return iterator.place;
		}
	}
	for (auto& iterator : globalTable->vars) {
		if (iterator.name == name) {
			return iterator.place;
		}
	}
	cerr << name << " is not in symbol table" << endl;
	return name;
}


//������������;���Array(len,type)
string Array(int len, string type) {
	return "Array(" + to_string(len) + "," + type + ")";
}

string gen(int a) {
	return to_string(a);
}
string calArrayAddr(string base, string offset) {
	//Ҫ��[BP-/+num]��offsetת����[BP+num+offset]����ʽ
	int num = 0, neg, i;
	neg = (base[3] == '-' ? -1 : 1);
	for (i = 4; i < base.length() - 1; i++) {
		num = num * 10 + base[i] - '0';
	}
	num = neg * neg + atoi(offset.c_str());
	return "[BP" + (num < 0 ? '-' : '+') + to_string(num) + ']';
}
void addLeader(int leader) {
	currentTable->leaders.insert(leader);
}

void emit(string op, string arg1, string arg2, string des) {
	middleCode.push_back(Quadruple(op, arg1, arg2, des));
	++nextInstr;
	if (middleCode.back().type >= 10 && middleCode.back().type <= 16) {
		addLeader(nextInstr);
	}
}

//����Ҫ����ͬһ��ָ�����Ԫʽ���
string makelist(int i) {
	return to_string(i) + "/";
}
string merge(const string& p1, const string& p2) {
	return p1 + p2;
}
//����p��list�������е�ָ����תĿ�ĵؽ��л������label
void backpatch(string p, string label) {
	if (p.empty()) {
		return;
	}
	int labelIndex = atoi(label.substr(6).c_str());	//label_ǰ�����ַ���������
	//��ΪҪ��ת����һ�䣬������һ���Ȼ��һ��������Ŀ�ʼ
	addLeader(labelIndex);
	
	string sep = "/";
	string::size_type posL = p.find_first_not_of(sep), posR = 0, end = string::npos;
	string index;

	while (posL != end) {
		posR = p.find_first_of(sep, posL);
		if (posR != end) {
			index = p.substr(posL, posR - posL);
			posL = posR + 1;
		}
		middleCode.at(atoi(index.c_str())).des = label;
		posL = p.find_first_not_of(sep, posL);
	}
	labelMap.insert(make_pair(label, labelIndex));
}

//�������
void outputMiddleCode(string filename) {
	ofstream out(filename);
	out << "index " << setw(13) << "label" << \
		setw(10) << "op" << setw(10) << "arg1" << setw(10) << "arg2" << setw(10) << "des" << endl;
	for (int i = 0; i < middleCode.size(); i++) {
		auto& code = middleCode.at(i);
		out << setw(4) << i << ") ";
		out << setw(13) << code.label + ": ";
		out << setw(10) << code.op;
		out << setw(10) << code.arg1;
		out << setw(10) << code.arg2;
		out << setw(10) << code.des << endl;
	}
	out.close();
}