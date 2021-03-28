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

//�ֲ�����ʹ�õļĴ���$8-$23
#define REGISTER_NUM 16	
//����Ĵ����Ŀ�ʼ���
#define RETURN_BASE 2
#define PARAM_BASE 4
#define VAR_BASE 8
#define TEMP_BASE 24	
using namespace std;



struct TableItem {
	//�ӵ�ǰ״̬��������һ������
	TableItem() : action(ERROR), index(0) {};
	TableItem(unsigned int action, unsigned int index) : action(action), index(index) {};
	unsigned int action;
	unsigned int index;
};

struct StackItem {
	//����ջ�ĵ�Ԫ
	StackItem(unsigned int state, std::string symbol) : state(state), symbol(symbol) {};
	StackItem(unsigned int state, std::string symbol, std::map<std::string, std::string> map)
		: state(state), symbol(symbol), _map(map) {};
	unsigned int state;	//��������Ŀ��״̬
	std::string symbol;	
	std::map<std::string, std::string> _map;	//��ǰ���ŵ����Ա�
};

struct Quadruple {
	int type;	//��ʾ��Ԫʽ�����࣬��op�Ķ�Ӧ���
	string op, arg1, arg2, des;	//��Ԫʽ������
	string label;	//�������Ԫʽ�б�ǩ�Ļ������ǩ������Ϊ�գ�ע�����ﲢ������תʱ��Ŀ�ı�ǩ

	bool desType = true, arg1Type = true, arg2Type = true;	// true��ʾ��ֵ��һ�������������ǳ�����λ��place�У�
	//��¼���ڴӺ���ǰ����ʱ�ı�����Ծ������´�ʹ��λ��
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
	std::string funName;	//��ǰ���ű�����������
	unsigned int returnSize = 0;	//����ֵ�ֽ���
	unsigned int variableSize = 0;	//��̬�ֲ��������ֽ���
	unsigned int paramSize = 0;	//�������ֽ���
	int beginIndex = -1;	//�������忪ʼ�ͽ�������Ԫʽ���
	int endIndex = -1;
	struct varState {
		//����״̬��CԴ�����ж����ÿ��������ӵ��һ��
		std::string name;		//������
		bool live;	//��ǰ��Ծ
		int nextUse;	//�´�ʹ�õ����
		bool inM;	//��ǰ�������ڴ���
		int inR;	//��ǰ�����ڼĴ����еı�ţ�-1��ʾ���ڼĴ�����
		bool isArray;

		unsigned int offset;	//��ʾ�ñ�������ڻ�ַBP��ƫ����
		string place;		//���������ı�����ַ

		unsigned int space;		//��ʾ�ñ�����ռ�ֽ���
		string type;		//���������ı�������

		varState(const std::string& name, const std::string& type, unsigned int offset, unsigned int space)
			: name(name), type(type), live(false), nextUse(-1),
			inM(false), inR(-1), offset(offset), space(space)
		{};
		
	};
	std::vector<varState> vars;
	std::set<int> leaders;	//�ú������л�����ĵ�һ����Ԫʽ���

	SymbolTable(const string& name) {
		funName = name;
	}

	//�򱾷��ű������һ���������������������ĵ�ַ
	string enterVar(const string& name, const string&type, unsigned int space, bool inStack = true) {
		for (const auto& iterator : vars) {
			if (iterator.name == name) {
				return iterator.place;
			}
		}
		
		if (funName == "global") {
			//ȫ�ֱ����Ŷ�������ַ��������
			varState state(name, type, variableSize, space);
			state.place = name;
			variableSize += space;
			vars.push_back(state);
			return state.place;
		}
		else {
			variableSize += space;	//�ֲ�����+1
			varState state(name, type, variableSize, space);
			if (inStack) {
				state.place = "[BP-" + to_string(variableSize) + "]";
			}
			else {
				//���糣����ȫ�ֱ������������ֱַ�Ӿ������֣�����������ֵ��
				state.place = name;
			}
			vars.push_back(state);
			return state.place;
		}
	}

	//�������Ĳ�������һ�㺯���������ڷ��ű�����ڶ���Ϣ���Ĵ���+bp+ip��֮��
	string enterParam(const string& name, const string&type, unsigned int space) {
		for (const auto& iterator : vars) {
			if (iterator.name == name) {
				return iterator.place;
			}
		}
		varState state(name, type, paramSize, space);
		//��ʱBP�Ѿ�ָ��ոմ�������һ���Ĵ��������Բ���������Ҫ����8��ͨ��+BP+IP
		state.place = "[BP+" + to_string(REGISTER_NUM * 2 + 4*2 + paramSize) + "]";
		state.inM = true;		//�������������ڴ���
		vars.push_back(state);
		paramSize += space;
		return state.place;
	}

	//���ݵ�ַ������������������
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
	//�ô���Դ�ļ���λ��
	unsigned int _line;
	unsigned int _offset;
};

struct Assembly {
	string op, r1, r2, rd;
	string label = "";	//������ǩ
	//rd��ʾ�������մ�����ݵĵ�ַ��swָ�����ʱҪ��һ��
	//����I��ָ��������η���r2��
	Assembly(string _op,string _rd,string _r1,string _r2){
		op = _op;
		r1 = _r1;
		r2 = _r2;
		rd = _rd;
	}
};

struct BasicBlock {
	int begin, end; //�������鿪ʼ�ͽ�������Ԫʽ���
	vector<int> predecessors, successors; //����������ȵ��ͺ��
	set<string> inLiveVar, outLiveVar; //�ڱ����������/���ڴ���Ծ�ı���
	BasicBlock(int ind = -1) {
		begin = ind;
	}
};
