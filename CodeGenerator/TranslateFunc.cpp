#pragma once

#include "DataStructureDefine.h"
#include "generateHead.h"


string enter(string name, string type, unsigned int space, bool inStack) {
	if (name == "#return") {
		//函数返回值
		return currentTable->enterVar(name, type, space, false);
	}
	else if (currentTable->funName == "global") {
		//如果是加全局变量，必不在栈中
		return globalTable->enterVar(name, type, space, false);
	}
	else {
		return currentTable->enterVar(name, type, space, inStack);
	}
}

void createSymbolTable(string name, unsigned int returnSize) {
	//遇到函数定义时调用，为它生成符号表
	if (globalTable->endIndex < 0) {
		//这里是希望先定义全局变量再定义各个函数
		globalTable->endIndex = nextInstr;
	}
	currentTable = new SymbolTable(name);
	currentTable->returnSize = returnSize;
	currentTable->beginIndex = nextInstr;
}

void returnToGlobalTable() {
	//翻译完一个函数之后
	currentTable->endIndex = nextInstr;
	symbolTables.push_back(currentTable);
	currentTable = globalTable;
}

void addFunLabel(int index, string label) {
	//这里的index是函数体的第一句中间代码，要在这里打上函数名的标签
	labelMap.insert(make_pair(label, index));
}

void addToSymbolTable(string itemlist) {
	//解析变量或变量列表（name,type,offset/...）并将其添加入当前符号表
	//这一步是在翻译的时候就把变量地址填好，此时假定BP已被赋值为新的活动记录起始处
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

//把参数变成易传递形式
string makeParam(string name, string type, unsigned int space) {
	return name + "," + type + "," + to_string(space) + "/";
}

static int t_count = 0;
//创建t系列临时变量
string newtemp(string var, bool force) {
	//要求该变量和var保持一致
	//如果var是常数时，可通过force指定必须创建一个space=4的int变量而非常数
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
	//num类型变量名字就是值
	enter(str, "num", 0, false);
	return str;
}

string lookupPlace(string name) {
	if (name == "#return") {
		return name;
	}
	//如果局部变量和全局变量重名，以局部变量为准
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


//数组变量的类型就是Array(len,type)
string Array(int len, string type) {
	return "Array(" + to_string(len) + "," + type + ")";
}

string gen(int a) {
	return to_string(a);
}
string calArrayAddr(string base, string offset) {
	//要把[BP-/+num]和offset转换成[BP+num+offset]的形式
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

//存有要跳到同一条指令处的四元式编号
string makelist(int i) {
	return to_string(i) + "/";
}
string merge(const string& p1, const string& p2) {
	return p1 + p2;
}
//解析p的list，对其中的指令跳转目的地进行回填，填入label
void backpatch(string p, string label) {
	if (p.empty()) {
		return;
	}
	int labelIndex = atoi(label.substr(6).c_str());	//label_前六个字符后是数字
	//因为要跳转到这一句，所以这一句必然是一个基本块的开始
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

//输出函数
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