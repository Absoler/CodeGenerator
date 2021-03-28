#include "DataStructureDefine.h"
#include "generateHead.h"

vector<set<string>> RegDescriptor(REGISTER_NUM, set<string>());	//寄存器描述符：记录某寄存器中存放有哪些变量（它们均相等）
//地址描述符在符号表里有了
vector<int> RNextUse(REGISTER_NUM, -1);

vector<Assembly> assemblyCode;
SymbolTable* table;
map<string, int> ASMLabelMap;

bool optimization = false;

typedef set<string> VarSet;
VarSet makeSet(string var) {
	VarSet tmp; tmp.insert(var);
	return tmp;
}



string RegName(int reg) {
	if (reg >= TEMP_BASE && reg < 26) {
		return "$t" + to_string(reg - 16);
	}
	else if (reg >= VAR_BASE) {
		return "$" + to_string(reg);
	}
	else if (reg >= PARAM_BASE) {
		return "$" + to_string(reg);
	}
	else if (reg >= RETURN_BASE) {
		return "$v" + to_string(reg - 2);
	}
}

int getEmptyReg() {
	auto availReg = find(RegDescriptor.begin(), RegDescriptor.end(), set<string>());
	if (availReg != RegDescriptor.end()) {
		return availReg - RegDescriptor.begin();
	}
	else return -1;
}

string getOffset(string place) {
	int offset = 0, neg = 0, i;
	if (place[0] == '[') {
		neg = (place[3] == '-' ? -1 : 1);
		for (i = 4; i < place.length() - 1; i++) {
			offset = offset * 10 + place[i] - '0';
		}
	}
	else {
		neg = (place[0] == '-' ? -1 : 1);
		for (i = 1; i < place.length(); i++) {
			offset = offset * 10 + place[i] - '0';
		}
	}
	offset = neg * offset;
	return to_string(offset);
}

void Save(int reg, string place) {
	//reg必须是绝对编号
	//把reg寄存器里的数存入地址为place的变量内存中
	string addr;
	if (place[0] == '[' || place=="#return") {
		addr = getOffset(place) + "($fp)";
	}
	else if (isalpha(place[0])){
		addr = to_string(globalTable->at(place).offset) + "($zero)";
	}
	assemblyCode.push_back(Assembly("sw", addr, RegName(reg), ""));	//生成sw指令存数
}
void Load(int reg, string place) {
	//把地址place的变量存入reg寄存器中
	//此时该变量不在寄存器中
	string addr;
	if (place[0] == '[' || place == "#return") {
		addr = getOffset(place) + "($fp)";
	}
	else if (isalpha(place[0])) {
		//全局变量
		addr = to_string(globalTable->at(place).offset) + "($zero)";
	}
	assemblyCode.push_back(Assembly("lw", RegName(reg), addr, ""));
}

int storeAndGetReg() {
	//换回寄存器选择算法：
	//首先选出需要生成保存指令条数最少的一个集合
	//然后在其中取距离最近的（非-1）
	int choose = -1, minAmount = 100, minDistance=assemblyCode.size();
	set<int> Alternative;
	for (int i = 0; i < REGISTER_NUM; i++) {
		//优先选择已经在内存中有备份的变量
		//注意！所有变量都分配了地址，但不一定及时更新内容
		//算一下每个寄存器有多少个变量的inM=false，即如果征用该寄存器，需要生成的保存指令条数
		int amount = 0;
		for (string var : RegDescriptor[i]) {
			if (currentTable->at(var).inM == false) {
				amount++;
			}
		}
		if (amount < minAmount) {
			Alternative = set<int>();
			Alternative.insert(i);
		}
		else if (amount == minAmount) {
			Alternative.insert(i);
		}
	}
	//然后在备选集中选距离最近的
	choose = *Alternative.begin();
	for (int i : Alternative) {
		if (RNextUse[i] != -1 && RNextUse[i] < minDistance) {
			minDistance = RNextUse[i];
			choose = i;
		}
	}
	for (string var : RegDescriptor[choose]) {
		if (currentTable->at(var).inM == false) {
			//这里只允许了int
			Save(choose + VAR_BASE, var);
		}
		currentTable->at(var).inR = -1;
		currentTable->at(var).inM = true;
	}
	return choose;
}

int allocateReg(string place, bool load = true) {
	//返回place所在寄存器(var_base开始的相对编号，
	//如果本不在寄存器中，则可选择是否先分配寄存器再把place加载进去
	int reg;
	bool global = false;
	if (currentTable->find(place)) {
		reg = currentTable->at(place).inR;
	}
	else {
		global = true;
		reg = globalTable->at(place).inR;
	}
	//先查表找到了该变量的inR
	if (reg < 0) {
		reg = getEmptyReg();
		if (reg < 0) {
			reg = storeAndGetReg();
		}
		if (load) {
			Load(reg + VAR_BASE, place);
		}
		RegDescriptor[reg]=makeSet(place);	//因为在reg寄存器分配给了place，所以修改寄存器描述符
		//更改变量地址描述符
		if (global) {
			globalTable->at(place).inR = reg;
		}
		else {
			currentTable->at(place).inR = reg;
		}
		
	}
	return reg;
}

void mov_NumToReg(string num, string reg) {
	assemblyCode.push_back(Assembly("addi", reg, "$zero", num));
}
void mov_RegToReg(string src, string des) {
	assemblyCode.push_back(Assembly("addi", des, src, "0"));
}

void generateCal3_asm(Quadruple& code) {
	//四元式type 1-5 17-19 23-24 三地址运算
	//只有add、or、xor支持立即数寻址，其他计算如果有立即数则先转移到$t8$t9再用
	//翻译成包括R型指令中大部分计算指令和I型的少部分
	//乘法、除法、取余要从LO中取出数据
	//首先需要把它们都分配寄存器

	int regArg1, regArg2;
	bool isNum1 = false, isNum2 = false;
	//des必须是变量不能是常数
	int regDes = allocateReg(code.des, false) + VAR_BASE;
	
	//共3种变量place，栈内局部变量，全局变量，常数
	if (code.arg1Type == false) {
		//如果这个数是常数
		mov_NumToReg(code.arg1, "$t8");
		regArg1 = 0 + TEMP_BASE;
		isNum1 = true;
	}
	else {
		//如果是全局变量或局部变量
		regArg1 = allocateReg(code.arg1) + VAR_BASE;
	}
	if (code.op != "SLL" || code.op != "SRL") {
		//左移右移参数二必须是常数
		if (code.arg2Type == false) {
			//如果这个数是常数
			mov_NumToReg(code.arg2, "$t9");
			regArg2 = 1 + TEMP_BASE;
			isNum2 = true;
		}
		else {
			//如果是全局变量或局部变量
			regArg2 = allocateReg(code.arg2) + VAR_BASE;
		}
	}
	//添加计算语句
	//对于 * / % 特殊处理
	if (code.type == 3 || code.type == 4) {
		//乘除
		assemblyCode.push_back(Assembly(code.op, "", RegName(regArg1), RegName(regArg2)));
		assemblyCode.push_back(Assembly("mflo", RegName(regDes), "", ""));
	}
	else if (code.type == 5) {
		//取余
		assemblyCode.push_back(Assembly("div", "", RegName(regArg1), RegName(regArg2)));
		assemblyCode.push_back(Assembly("mfhi", RegName(regDes), "", ""));
	}
	else if (code.type == 23 || code.type == 24) {
		//SLL SRL
		assemblyCode.push_back(Assembly(code.op, RegName(regDes), RegName(regArg1), code.arg2));
	}
	else {
		assemblyCode.push_back(Assembly(code.op, RegName(regDes), RegName(regArg1), RegName(regArg2)));	//执行完后如果用了t8t9，则它们已经作废
	}
	currentTable->at(code.des).inM = -1;	//des获得新值，内存中副本过时
	RNextUse[regDes-VAR_BASE] = code.desNext;

	if (!isNum1) RNextUse[regArg1-VAR_BASE] = code.arg1Next;
	if (!isNum2) RNextUse[regArg2-VAR_BASE] = code.arg2Next;
}

void generateCal2_ASM(Quadruple& code) {
	//21 des=NEG arg1
	//20 des=NOT arg1 
	//26 des=arg1[arg2]
	//28 des=$arg1
	int regArg1;
	bool isNum = false;
	int regDes = allocateReg(code.des, false) + VAR_BASE;
	if (code.type == 21 || code.type == 20 || code.type == 28) {
		if (code.arg1Type == false) {
			//如果这个数是常数
			mov_NumToReg(code.arg1, "$t8");
			regArg1 = 0 + TEMP_BASE;
			isNum = true;
		}
		else {
			//如果是全局变量或局部变量
			regArg1 = allocateReg(code.arg1) + VAR_BASE;
		}
		//添加计算语句
		if (code.type == 20) {
			//des=NOT arg1
			//等价于 des = arg1 xor -1
			assemblyCode.push_back(Assembly("xori", RegName(regDes), RegName(regArg1), "-1"));
		}
		else if (code.type == 21) {
			//des=NEG arg1
			//等价于 des=$zero-arg1 
			assemblyCode.push_back(Assembly("sub", RegName(regDes), "$zero", RegName(regArg1)));
		}
		else if (code.type == 28) {
			//des=$arg1 
			//arg1取低16位
			//assemblyCode.push_back(Assembly("andi", RegName(regArg1), RegName(regArg1), "0xFFFF"));
			assemblyCode.push_back(Assembly("lw", RegName(regDes), "0(" + RegName(regArg1) + ')', ""));
		}
		else {
			//des=NEG a 
			assemblyCode.push_back(Assembly(code.op, RegName(regDes), RegName(regArg1), ""));
		}
		currentTable->at(code.des).inM = -1;	//des获得新值，内存中副本过时
		RNextUse[regDes-VAR_BASE] = code.desNext;
		if (!isNum)	RNextUse[regArg1-VAR_BASE] = code.arg1Next;
	}
	else if (code.type == 26) {
		//des = arg1[arg2]

	}
}

void generateCal1_ASM(Quadruple& code) {
	//22 ACC ++des
	int regDes = allocateReg(code.des, false) + VAR_BASE;
	assemblyCode.push_back(Assembly("addi", RegName(regDes), RegName(regDes), "1"));
	RNextUse[regDes-VAR_BASE] = code.desNext;
}
void generateParam_ASM(Quadruple& code) {
	//param指令，将对应参数压栈即可
	int regArg;
	if (code.arg1Type) {
		regArg = allocateReg(code.arg1) + VAR_BASE;
	}
	else {
		mov_NumToReg(code.arg1, "$t8");
		regArg = 0 + TEMP_BASE;
	}
	assemblyCode.push_back(Assembly("push", "", RegName(regArg), ""));
}
void generateCall_ASM(Quadruple& code) {
	//call命令执行的动作就是保存IP+4并跳转
	//BP和其他寄存器等到函数体内再保存
	assemblyCode.push_back(Assembly("jal", code.arg2, "", ""));
}

void generateReturn_ASM(Quadruple& code) {
	//这里其实只有返回值为int的函数
	//返回值直接存在v0中，然后在赋值语句中进行特判，如果右值是#return则从v0中取值
	if (currentTable->returnSize > 0) {
		//分类赋值
		if (code.arg1Type) {
			int regReturn = allocateReg(code.arg1) + VAR_BASE;
			mov_RegToReg(RegName(regReturn), "$v0");
		}
		else {
			mov_NumToReg(code.arg1, "$v0");
		}
	}
	if (currentTable->funName != "main") {
		mov_RegToReg("$fp", "$sp");
		for (int i = REGISTER_NUM - 1; i >= 0; i--) {
			assemblyCode.push_back(Assembly("pop", RegName(i + VAR_BASE), "", ""));
		}
		assemblyCode.push_back(Assembly("pop", "$fp", "", ""));
		assemblyCode.push_back(Assembly("pop", "$ra", "", ""));
		assemblyCode.push_back(Assembly("jr", "$ra", "", ""));
	}
	//清除寄存器描述符中该函数的局部变量
	/*for (int i = 0; i < REGISTER_NUM; i++) {
		for (string var : RegDescriptor[i]) {
			if (currentTable->find(var) || var == "") {
				RegDescriptor[i].erase(var);
			}
		}
	}*/
}
void func_protect() {
	//在函数代码执行开始前保存$ra、BP和变量寄存器
	assemblyCode.push_back(Assembly("push", "$ra", "", ""));
	assemblyCode.push_back(Assembly("push", "$fp", "", ""));
	for (int i = 0; i < REGISTER_NUM; i++) {
		assemblyCode.push_back(Assembly("push", RegName(i + VAR_BASE), "", ""));
	}
}
void func_prework(Quadruple& code) {
	//构建本函数的活动记录，调整BP和SP的值
	assemblyCode.push_back(Assembly("addi", "$fp", "$sp", "0"));
	for (auto& table : symbolTables) {
		if (code.label == table->funName) {
			assemblyCode.push_back(Assembly("addi", "$sp", '-' + to_string(table->variableSize), "0"));
			break;
		}
	}
}
void generateAssign_ASM(Quadruple& code) {
	//9 des=arg1, arg1==#return时特殊处理
	//25 arg1[arg2]=des
	//27 $des=arg1
	if (code.type == 9) {
		//des必须是变量，且赋值后在寄存器中
		//arg1如果是常数则numToReg，如果是变量则RegToReg
		
		if (code.arg1 == "#return") {
			int regDes = allocateReg(code.des, false) + VAR_BASE;
			mov_RegToReg("$v0", RegName(regDes));
		}
		else if (code.arg1Type) {
			//如果arg是普通变量，则可以找到regArg后直接修改寄存器描述符和地址描述符，让des指向它
			int regArg1 = allocateReg(code.arg1);	//这里是相对编号
			table = (currentTable->find(code.des) ? currentTable : globalTable);
			table->at(code.des).inM = false;
			table->at(code.des).inR = regArg1;
			RegDescriptor[regArg1].insert(code.des);

			RNextUse[regArg1] = code.arg1Next;//??
		}
		else {
			int regDes = allocateReg(code.des, false) + VAR_BASE;
			mov_NumToReg(code.arg1, RegName(regDes));
		}
	}
	else if (code.type == 27) {
		//$des=arg1 两个值都可以既是常数又是变量
		int regDes, regArg;
		if (code.arg1Type == false) {
			mov_NumToReg(code.arg1, "$t8");
			regArg = 0 + TEMP_BASE;
		}
		else {
			regArg = allocateReg(code.arg1) + VAR_BASE;
		}
		if (code.desType == false) {
			mov_NumToReg(code.des, "$t9");
			regDes = 1 + TEMP_BASE;
		}
		else {
			regDes = allocateReg(code.des) + VAR_BASE;
		}
		//地址只取低16位
		//assemblyCode.push_back(Assembly("andi", RegName(regDes), RegName(regDes), "0xFFFF"));
		assemblyCode.push_back(Assembly("sw", "0(" + RegName(regDes) + ')', RegName(regArg), ""));
	}
}
void generateJump_ASM(Quadruple& code) {
	//10 j
	//11-12 j< j<= 13-14 j> j>= 
	//15 j!= 16 j==
	//条件跳转实现方法：条件取反跳转到下下一句，然后下一句用j跳转到目标标签
	if (code.type == 10) {
		assemblyCode.push_back(Assembly("j", string("label") + code.des.substr(6, code.des.length() - 6), "", ""));
		return;
	}
	int regArg1, regArg2;
	if (code.arg1Type == false) {
		mov_NumToReg(code.arg1, "$t8");
		regArg1 = 0 + TEMP_BASE;
	}
	else {
		regArg1 = allocateReg(code.arg1) + VAR_BASE;
	}
	if (code.arg2Type == false) {
		mov_NumToReg(code.arg2, "$t9");
		regArg2 = 1 + TEMP_BASE;
	}
	else {
		regArg2 = allocateReg(code.arg2) + VAR_BASE;
	}
	string ins;
	if (code.type == 11) ins = "jge";
	else if (code.type == 12) ins = "jg";
	else if (code.type == 13) ins = "jle";
	else if (code.type == 14) ins = "jl";
	else if (code.type == 15) ins = "beq";
	else if (code.type == 16) ins = "bne";
	assemblyCode.push_back(Assembly(ins, to_string(4), RegName(regArg1), RegName(regArg2)));
	assemblyCode.push_back(Assembly("j", string("label") + code.des.substr(6, code.des.length() - 6), "", ""));
}

void generateASM(Quadruple& code) {
	if (!code.label.empty()) {
		if (code.label == "main") {
			mov_NumToReg(to_string(32767), "$fp");
			mov_RegToReg("$fp", "$sp");

		}
		else if (code.label.substr(0, 6) != "LABEL_") {
			func_protect();
			func_prework(code);
		}
	}
	int t = code.type;
	if ((t >= 1 && t <= 5)\
		|| (t >= 17 && t <= 19)\
		|| (t >= 23 && t <= 24)) {
		generateCal3_asm(code);
	}
	else if (t == 20 || t == 21 || t == 26 || t == 28) {
		generateCal2_ASM(code);
	}
	else if (t == 21) {
		generateCal1_ASM(code);
	}
	else if (t == 6) {
		generateParam_ASM(code);
	}
	else if (t == 7) {
		generateCall_ASM(code);
	}
	else if (t == 8) {
		generateReturn_ASM(code);
	}
	else if (t >= 10 && t <= 16) {
		generateJump_ASM(code);
	}
	else if (t == 9 || t == 25 || t == 27) {
		generateAssign_ASM(code);
	}
}
void analysisVarState(int beginInd, int endInd, VarSet& outLiveVar, VarSet& inLiveVar);
void outputASM(string filename);


void translateToASM(string path) {
	for (auto& table : symbolTables) {
		currentTable = table;
		//对每个函数分别分别分析，符号表中记录了该函数的四元式开始位置

		//记录一下本函数的开始位置，方便插标签
		ASMLabelMap.insert(make_pair(currentTable->funName, assemblyCode.size()));

		map<int, BasicBlock> flowGraph;
		//接下来为本函数基本块构建流图
		deque<int> leaders(currentTable->leaders.begin(), currentTable->leaders.end());//leaders是本函数中各个基本块的开始指令序号
		leaders.push_back(currentTable->endIndex);	//为了获取最后一个块的末尾指令序号，注意这里可能会有两个空的，当源代码以ifwhile块结尾时
		for (auto& leader : leaders) {
			//初始化基本块变量
			flowGraph.insert(make_pair(leader, BasicBlock(leader)));
		}
		//接下来我们要构建本函数中基本块的流图
		int beginIndex = leaders.front();
		auto cur = leaders.begin(); ++cur;
		int endIndex = beginIndex;
		while (cur != leaders.end()) {
			endIndex = *cur;

			auto& block = flowGraph.at(beginIndex);
			block.end = endIndex;	//这里end就是当前块最后一句的下一句
			auto& code = middleCode.at(endIndex - 1);	//当前块最后一条四元式

			//根据最后一条四元式的情况，判断后继会有哪些块
			if (code.type == 10) {
				//无条件跳转则只会把跳到的块作为后继
				string label = code.des;
				int code_ind = labelMap[label];
				flowGraph[code_ind].predecessors.push_back(beginIndex);
				block.successors.push_back(code_ind);	//block是引用，直接插入节约内存
			}
			else if (code.type >= 11 && code.type <= 16) {
				//如果是条件跳转，则它的后继既有跳转到的基本块，也有顺序的下一块
				string label = code.des;
				int code_ind = labelMap[label];
				flowGraph[code_ind].predecessors.push_back(beginIndex);
				block.successors.push_back(code_ind);	//block是引用，直接插入节约内存

				flowGraph[endIndex].predecessors.push_back(beginIndex);
				block.successors.push_back(endIndex);
			}
			else {
				//如果是普通指令，则只会把顺序的下一块作为后继
				flowGraph[endIndex].predecessors.push_back(beginIndex);
				block.successors.push_back(endIndex);
			}
			beginIndex = endIndex;
			cur++;
		}

		//活跃变量分析算法 龙书9.14
bool change = true;
while (change) {
	change = false;
	for (auto i = leaders.rbegin() + 1; i != leaders.rend(); i++) {	//+1为了跳过上面附加的空块
		auto& block = flowGraph[*i];
		VarSet inLiveVar;
		//第1步
		analysisVarState(block.begin, block.end, block.outLiveVar, inLiveVar);
		//第2步
		for (auto& predecessor : block.predecessors) {
			auto& Oldout = flowGraph[predecessor].outLiveVar;
			VarSet NewOut;
			set_union(inLiveVar.begin(), inLiveVar.end(), Oldout.begin(), Oldout.end(), inserter(NewOut, NewOut.begin()));
			if (NewOut.size() > flowGraph[predecessor].outLiveVar.size()) {
				flowGraph[predecessor].outLiveVar = NewOut;
				change = true;
			}
			//flowGraph[predecessor].outLiveVar = NewOut;
		}
	}
}

		for (auto i = leaders.begin(); i != leaders.end() - 1; ++i) {

			auto& block = flowGraph[*i];
			analysisVarState(block.begin, block.end, block.outLiveVar, block.inLiveVar);
			
			
			if (block.begin >= middleCode.size()) {
				break;	
				//这里是防止如果函数体最后有一个if或while块，由于回填会多出一个基本块，导致middleCode的vector访问越界的问题
			}
			string label;
			//移植四元式中的label到下一条汇编
			/*if (!label.empty() && label != "main") {
				ASMLabelMap.insert(make_pair(label, assemblyCode.size()));
			}*/
			//生成汇编
			for (int i = block.begin; i < block.end; ++i) {
				label = middleCode.at(i).label;
				if (!label.empty() && label != currentTable->funName) {
					ASMLabelMap.insert(make_pair(string("label") + label.substr(6, label.length() - 6), assemblyCode.size()));
				}
				generateASM(middleCode.at(i));
			}

			// save live variables
			for (int reg = 0; reg < REGISTER_NUM; ++reg) {
				for (auto& var : RegDescriptor[reg]) {
					if (var.empty()) continue;
					table = (currentTable->find(var) ? currentTable : globalTable);
					if (!table->find(var)) continue;
					if (block.outLiveVar.find(var) != block.outLiveVar.end()
						&& table->at(var).inM == false) {
						Save(reg + VAR_BASE, var);
						table->at(var).inM = true;
					}
					table->at(var).inR = -1;
				}
			}
		}
	}
	for (const auto& map : ASMLabelMap) {
		assemblyCode.at(map.second).label = map.first;
	}
	outputASM(path + "ASMcode.txt");
}

void outputASM(string filename) {
	ofstream out(filename);
	out << ".data" << endl;
	if (globalTable->variableSize > 0) {
		//全局变量区
		for (auto& state : globalTable->vars) {
			out << ".word " << state.name<<" 0";
			if (state.isArray) {
				for (int i = 1; i < state.space / 4; i++) {
					out << " 0";
				}
			}
			out << endl;
		}
	}
	out << ".text" << endl;
	for (const auto& code : assemblyCode) {
		if (code.label.empty()) {
			out << setw(10) << ' ';
		}
		else {
			out << setw(10) << code.label + ": ";
		}
		out << setw(6) << code.op;	//输出汇编指令符
		if (code.op == "sw") {
			out << setw(10) << code.r1+',' << setw(10) << code.rd;
		}
		else if (code.op == "jg" || code.op == "jge" || \
			code.op == "jl" || code.op == "jle" || \
			code.op == "beq" || code.op == "bne") {
			//六个条件跳转的offset在后面
			out << setw(10) << code.r1 + ',' << setw(10) << code.r2 + ',' << setw(10) << code.rd;
		}
		else {
			out << setw(10) << code.rd;
			if (!code.r1.empty()) out << setw(10) << ','+code.r1;
			if (!code.r2.empty()) out << setw(10) << ','+code.r2;
		}
		out << endl;
	}
	out.close();
}
void analysisVarState(int beginInd, int endInd, VarSet& outLiveVar, VarSet& inLiveVar) {
	//inLiveVar和outLiveVar分别指在本基本块中入口处活跃的变量集和在出口处活跃的变量集
	//本函数旨在算出入口处活跃变量集
	for (auto& var : outLiveVar) {
		//对于这些在出口处活跃的变量，算最后一行前先在表中将其标记为活跃
		currentTable->at(var).live = true;
		currentTable->at(var).nextUse = endInd;
	}

	//从最后一条指令向前计算，赋值会杀死活跃
	for (int i = endInd - 1; i >= beginInd; i--) {
		auto& code = middleCode.at(i);
		if (code.type == 7) {
			continue;	//call
		}
		bool desExist = !(code.des.empty() || code.op[0] == 'j');
		bool arg1Exist = !code.arg1.empty();
		bool arg2Exist = !code.arg2.empty();

		if (desExist) {
			//定值杀死了des变量
			code.desLive = currentTable->at(code.des).live;
			code.desNext = currentTable->at(code.des).nextUse;
			currentTable->at(code.des).live = false;
			currentTable->at(code.des).nextUse = -1;
		}
		if (code.arg1 == "#return") {
			continue;
		}
		if (arg1Exist) {
			//引用了arg1
			code.arg1Live = currentTable->at(code.arg1).live;
			code.arg1Next = currentTable->at(code.arg1).nextUse;
			currentTable->at(code.arg1).live = true;
			currentTable->at(code.arg1).nextUse = i;
		}
		if (arg2Exist) {
			//引用了arg2
			code.arg2Live = currentTable->at(code.arg2).live;
			code.arg2Next = currentTable->at(code.arg2).nextUse;
			currentTable->at(code.arg1).live = true;
			currentTable->at(code.arg1).nextUse = i;
		}

	}
	for (auto& var : currentTable->vars) {
		if (var.live == true && var.space > 0) {
			//入口处活跃，即use集
			inLiveVar.insert(var.place);
		}
		var.live = false;
		var.nextUse = -1;
	}
}
