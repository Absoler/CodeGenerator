#pragma once
#include "DataStructureDefine.h"

//语法分析的结果：语法表和归约时调用的语义动作
void initTable(map<unsigned int, map<string, TableItem> >& _parseTable);
string getProduction(unsigned int index);
pair<unsigned int, string> performAction(unsigned int index, map<string, string>& reduceHead);

//语义动作工具函数
string enter(string name, string type, unsigned int offset, bool inStack = true);
void createSymbolTable(string name, unsigned int returnSize);
void returnToGlobalTable();
void addFunLabel(int index, string label);
void addToSymbolTable(string itemlist);
void setOutLiveVar(string place);
void error();
string makeParam(string name, string type, unsigned int space);
string newtemp(string var, bool force = false);
string addNum(string str);
string lookupPlace(string name);
string Array(int len, string type);
string gen(int a);
string calArrayAddr(string base, string offset);
void addLeader(int leader);
void emit(string op, string arg1, string arg2, string des);
string makelist(int i);
string merge(const string& p1, const string& p2);
void backpatch(string p, string label);
void outputMiddleCode(string filename);

//生成中间代码总代码
void errorInReduce(list<Token>::iterator token);
bool translateToMiddleCode(list<Token> tokenList, string path);

//汇编生成器工具函数

//汇编生成器总代码
void translateToASM(string path);


//全局变量
extern list<Token> tokenList;

extern deque<StackItem> Stack;
extern stack<string> paramStack;
extern unsigned int offset;

extern vector<SymbolTable*> symbolTables;
extern SymbolTable *globalTable, *currentTable;

extern vector<Quadruple> middleCode;
extern int nextInstr;

extern map<string, int> labelMap;

extern vector<Assembly> assemblyCode;