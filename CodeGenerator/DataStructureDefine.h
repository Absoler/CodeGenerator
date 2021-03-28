#pragma once

#include<iostream>
#include <string>
#include <sstream>  
#include <map>
#include <vector>
#include <set>
#include<fstream>
#include<list>
#include<deque>
#include<iomanip>
#include<stack>
#include <stdlib.h>
#include <iterator>
#include<algorithm>

#define SHIFT 1
#define REDUCTION 2
#define GOTO_STATE 3
#define ERROR 4
#define ACCEPT 5

//局部变量使用的寄存器$8-$23
#define REGISTER_NUM 16	
//各组寄存器的开始编号
#define RETURN_BASE 2
#define PARAM_BASE 4
#define VAR_BASE 8
#define TEMP_BASE 24	
using namespace std;



struct TableItem {
	//从当前状态发出的下一个动作
	TableItem() : action(ERROR), index(0) {};
	TableItem(unsigned int action, unsigned int index) : action(action), index(index) {};
	unsigned int action;
	unsigned int index;
};

struct StackItem {
	//分析栈的单元
	StackItem(unsigned int state, std::string symbol) : state(state), symbol(symbol) {};
	StackItem(unsigned int state, std::string symbol, std::map<std::string, std::string> map)
		: state(state), symbol(symbol), _map(map) {};
	unsigned int state;	//描述本条目的状态
	std::string symbol;	
	std::map<std::string, std::string> _map;	//当前符号的属性表
};

struct Quadruple {
	int type;	//表示四元式的种类，即op的对应序号
	string op, arg1, arg2, des;	//四元式的内容
	string label;	//如果该四元式有标签的话就填标签，否则为空，注意这里并不是跳转时的目的标签

	bool desType = true, arg1Type = true, arg2Type = true;	// true表示该值是一个变量，否则是常数（位于place中）
	//记录块内从后向前分析时的变量活跃情况和下次使用位置
	bool desLive, arg1Live, arg2Live;	
	int desNext, arg1Next, arg2Next;		
	
	Quadruple(const string& _op, const string& _arg1, const string& _arg2, const string& _des)
		:op(_op), arg1(_arg1), arg2(_arg2), des(_des) {
		if (isdigit(arg1[0]) || arg1[0] == '-') {
			arg1Type = false;
		}
		if (isdigit(arg2[0]) || arg2[0] == '-') {
			arg2Type = false;
		}
		if (op == "ADD") {
			type = 1;
		}
		else if (op == "SUB") {
			type = 2;
		}
		else if (op == "MUL") {
			type = 3;
		}
		else if (op == "DIV") {
			type = 4;
		}
		else if (op == "MOD") {
			type = 5;
		}
		else if (op == "param") {
			type = 6;
		}
		else if (op == "call") {
			type = 7;
		}
		else if (op == "return") {
			type = 8;
		}
		else if (op == "=") {
			type = 9;	
		}
		else if (op == "j") {
			type = 10;
		}
		else if (op == "j<") {
			type = 11;
		}
		else if (op == "j<=") {
			type = 12;
		}
		else if (op == "j>") {
			type = 13;
		}
		else if (op == "j>=") {
			type = 14;
		}
		else if (op == "j==") {
			type = 15;
		}
		else if (op == "j!=") {
			type = 16;
		}
		else if (op == "AND") {
			type = 17;
		}
		else if (op == "OR") {
			type = 18;
		}
		else if (op == "XOR") {
			type = 19;
		}
		else if (op == "NOT") {
			type = 20;
		}
		else if (op == "NEG") {
			type = 21;
		}
		else if (op == "ACC") {
			type = 22;
		}
		else if (op == "SLL") {
			type = 23;
		}
		else if (op == "SRL") {
			type = 24;
		}
		else if (op == "[]=") {
			//a[b]=c
			type = 25;
		}
		else if (op == "=[]") {
			//a=b[c]
			type = 26;
		}
		else if (op == "$=") {
			type = 27;
		}
		else if (op == "=$") {
			type = 28;
		}
	}
};


struct SymbolTable {
	std::string funName;	//当前符号表所属函数名
	unsigned int returnSize = 0;	//返回值字节数
	unsigned int variableSize = 0;	//动态局部变量的字节数
	unsigned int paramSize = 0;	//参数总字节数
	int beginIndex = -1;	//本函数体开始和结束的四元式序号
	int endIndex = -1;
	struct varState {
		//变量状态，C源代码中定义的每个变量都拥有一个
		std::string name;		//变量名
		bool live;	//当前活跃
		int nextUse;	//下次使用的序号
		bool inM;	//当前变量在内存中
		int inR;	//当前变量在寄存器中的标号，-1表示不在寄存器中
		bool isArray;

		unsigned int offset;	//表示该变量相对于基址BP的偏移量
		string place;		//语言描述的变量地址

		unsigned int space;		//表示该变量所占字节数
		string type;		//语言描述的变量类型

		varState(const std::string& name, const std::string& type, unsigned int offset, unsigned int space)
			: name(name), type(type), live(false), nextUse(-1),
			inM(false), inR(-1), offset(offset), space(space)
		{};
		
	};
	std::vector<varState> vars;
	std::set<int> leaders;	//该函数所有基本块的第一句四元式编号

	SymbolTable(const string& name) {
		funName = name;
	}

	//向本符号表中添加一个变量，返回文字描述的地址
	string enterVar(const string& name, const string&type, unsigned int space, bool inStack = true) {
		for (const auto& iterator : vars) {
			if (iterator.name == name) {
				return iterator.place;
			}
		}
		
		if (funName == "global") {
			//全局变量放堆区，地址向上增加
			varState state(name, type, variableSize, space);
			state.place = name;
			variableSize += space;
			vars.push_back(state);
			return state.place;
		}
		else {
			variableSize += space;	//局部变量+1
			varState state(name, type, variableSize, space);
			if (inStack) {
				state.place = "[BP-" + to_string(variableSize) + "]";
			}
			else {
				//例如常数、全局变量等情况，地址直接就是名字（常数还等于值）
				state.place = name;
			}
			vars.push_back(state);
			return state.place;
		}
	}

	//本函数的参数是上一层函数传来，在符号表保存的众多信息（寄存器+bp+ip）之上
	string enterParam(const string& name, const string&type, unsigned int space) {
		for (const auto& iterator : vars) {
			if (iterator.name == name) {
				return iterator.place;
			}
		}
		varState state(name, type, paramSize, space);
		//此时BP已经指向刚刚储存的最后一个寄存器，所以参数访问需要跳过8个通用+BP+IP
		state.place = "[BP+" + to_string(REGISTER_NUM * 2 + 4*2 + paramSize) + "]";
		state.inM = true;		//参数都放在了内存里
		vars.push_back(state);
		paramSize += space;
		return state.place;
	}

	//根据地址的文字描述索引变量
	varState& at(const string& place) {
		for (auto& var : vars) {
			if (var.place == place) {
				return var;
			}
		}
		return *vars.begin();
	}
	bool find(string place) {
		for (auto& var : vars) {
			if (var.place == place) {
				return true;
			}
		}
		return false;
	}
};

struct Token {
	Token(std::string word,
		std::string attr,
		unsigned int line,
		unsigned int offset)
		: _lexecal(word),
		_attribute(attr),
		_line(line),
		_offset(offset) {}

	// data member
	std::string _lexecal;
	std::string _attribute;
	//该词在源文件的位置
	unsigned int _line;
	unsigned int _offset;
};

struct Assembly {
	string op, r1, r2, rd;
	string label = "";	//所属标签
	//rd表示运算最终存放数据的地址，sw指令输出时要反一下
	//对于I型指令，将常数段放在r2中
	Assembly(string _op,string _rd,string _r1,string _r2){
		op = _op;
		r1 = _r1;
		r2 = _r2;
		rd = _rd;
	}
};

struct BasicBlock {
	int begin, end; //本基本块开始和结束的四元式编号
	vector<int> predecessors, successors; //本基本块的先导和后继
	set<string> inLiveVar, outLiveVar; //在本基本块入口/出口处活跃的变量
	BasicBlock(int ind = -1) {
		begin = ind;
	}
};
