#include "DataStructureDefine.h"
#include "generateHead.h"

vector<set<string>> RegDescriptor(REGISTER_NUM, set<string>());	//�Ĵ�������������¼ĳ�Ĵ����д������Щ���������Ǿ���ȣ�
//��ַ�������ڷ��ű�������
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
	//reg�����Ǿ��Ա��
	//��reg�Ĵ�������������ַΪplace�ı����ڴ���
	string addr;
	if (place[0] == '[' || place=="#return") {
		addr = getOffset(place) + "($fp)";
	}
	else if (isalpha(place[0])){
		addr = to_string(globalTable->at(place).offset) + "($zero)";
	}
	assemblyCode.push_back(Assembly("sw", addr, RegName(reg), ""));	//����swָ�����
}
void Load(int reg, string place) {
	//�ѵ�ַplace�ı�������reg�Ĵ�����
	//��ʱ�ñ������ڼĴ�����
	string addr;
	if (place[0] == '[' || place == "#return") {
		addr = getOffset(place) + "($fp)";
	}
	else if (isalpha(place[0])) {
		//ȫ�ֱ���
		addr = to_string(globalTable->at(place).offset) + "($zero)";
	}
	assemblyCode.push_back(Assembly("lw", RegName(reg), addr, ""));
}

int storeAndGetReg() {
	//���ؼĴ���ѡ���㷨��
	//����ѡ����Ҫ���ɱ���ָ���������ٵ�һ������
	//Ȼ��������ȡ��������ģ���-1��
	int choose = -1, minAmount = 100, minDistance=assemblyCode.size();
	set<int> Alternative;
	for (int i = 0; i < REGISTER_NUM; i++) {
		//����ѡ���Ѿ����ڴ����б��ݵı���
		//ע�⣡���б����������˵�ַ������һ����ʱ��������
		//��һ��ÿ���Ĵ����ж��ٸ�������inM=false����������øüĴ�������Ҫ���ɵı���ָ������
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
	//Ȼ���ڱ�ѡ����ѡ���������
	choose = *Alternative.begin();
	for (int i : Alternative) {
		if (RNextUse[i] != -1 && RNextUse[i] < minDistance) {
			minDistance = RNextUse[i];
			choose = i;
		}
	}
	for (string var : RegDescriptor[choose]) {
		if (currentTable->at(var).inM == false) {
			//����ֻ������int
			Save(choose + VAR_BASE, var);
		}
		currentTable->at(var).inR = -1;
		currentTable->at(var).inM = true;
	}
	return choose;
}

int allocateReg(string place, bool load = true) {
	//����place���ڼĴ���(var_base��ʼ����Ա�ţ�
	//��������ڼĴ����У����ѡ���Ƿ��ȷ���Ĵ����ٰ�place���ؽ�ȥ
	int reg;
	bool global = false;
	if (currentTable->find(place)) {
		reg = currentTable->at(place).inR;
	}
	else {
		global = true;
		reg = globalTable->at(place).inR;
	}
	//�Ȳ���ҵ��˸ñ�����inR
	if (reg < 0) {
		reg = getEmptyReg();
		if (reg < 0) {
			reg = storeAndGetReg();
		}
		if (load) {
			Load(reg + VAR_BASE, place);
		}
		RegDescriptor[reg]=makeSet(place);	//��Ϊ��reg�Ĵ����������place�������޸ļĴ���������
		//���ı�����ַ������
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
	//��Ԫʽtype 1-5 17-19 23-24 ����ַ����
	//ֻ��add��or��xor֧��������Ѱַ�������������������������ת�Ƶ�$t8$t9����
	//����ɰ���R��ָ���д󲿷ּ���ָ���I�͵��ٲ���
	//�˷���������ȡ��Ҫ��LO��ȡ������
	//������Ҫ�����Ƕ�����Ĵ���

	int regArg1, regArg2;
	bool isNum1 = false, isNum2 = false;
	//des�����Ǳ��������ǳ���
	int regDes = allocateReg(code.des, false) + VAR_BASE;
	
	//��3�ֱ���place��ջ�ھֲ�������ȫ�ֱ���������
	if (code.arg1Type == false) {
		//���������ǳ���
		mov_NumToReg(code.arg1, "$t8");
		regArg1 = 0 + TEMP_BASE;
		isNum1 = true;
	}
	else {
		//�����ȫ�ֱ�����ֲ�����
		regArg1 = allocateReg(code.arg1) + VAR_BASE;
	}
	if (code.op != "SLL" || code.op != "SRL") {
		//�������Ʋ����������ǳ���
		if (code.arg2Type == false) {
			//���������ǳ���
			mov_NumToReg(code.arg2, "$t9");
			regArg2 = 1 + TEMP_BASE;
			isNum2 = true;
		}
		else {
			//�����ȫ�ֱ�����ֲ�����
			regArg2 = allocateReg(code.arg2) + VAR_BASE;
		}
	}
	//��Ӽ������
	//���� * / % ���⴦��
	if (code.type == 3 || code.type == 4) {
		//�˳�
		assemblyCode.push_back(Assembly(code.op, "", RegName(regArg1), RegName(regArg2)));
		assemblyCode.push_back(Assembly("mflo", RegName(regDes), "", ""));
	}
	else if (code.type == 5) {
		//ȡ��
		assemblyCode.push_back(Assembly("div", "", RegName(regArg1), RegName(regArg2)));
		assemblyCode.push_back(Assembly("mfhi", RegName(regDes), "", ""));
	}
	else if (code.type == 23 || code.type == 24) {
		//SLL SRL
		assemblyCode.push_back(Assembly(code.op, RegName(regDes), RegName(regArg1), code.arg2));
	}
	else {
		assemblyCode.push_back(Assembly(code.op, RegName(regDes), RegName(regArg1), RegName(regArg2)));	//ִ������������t8t9���������Ѿ�����
	}
	currentTable->at(code.des).inM = -1;	//des�����ֵ���ڴ��и�����ʱ
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
			//���������ǳ���
			mov_NumToReg(code.arg1, "$t8");
			regArg1 = 0 + TEMP_BASE;
			isNum = true;
		}
		else {
			//�����ȫ�ֱ�����ֲ�����
			regArg1 = allocateReg(code.arg1) + VAR_BASE;
		}
		//��Ӽ������
		if (code.type == 20) {
			//des=NOT arg1
			//�ȼ��� des = arg1 xor -1
			assemblyCode.push_back(Assembly("xori", RegName(regDes), RegName(regArg1), "-1"));
		}
		else if (code.type == 21) {
			//des=NEG arg1
			//�ȼ��� des=$zero-arg1 
			assemblyCode.push_back(Assembly("sub", RegName(regDes), "$zero", RegName(regArg1)));
		}
		else if (code.type == 28) {
			//des=$arg1 
			//arg1ȡ��16λ
			//assemblyCode.push_back(Assembly("andi", RegName(regArg1), RegName(regArg1), "0xFFFF"));
			assemblyCode.push_back(Assembly("lw", RegName(regDes), "0(" + RegName(regArg1) + ')', ""));
		}
		else {
			//des=NEG a 
			assemblyCode.push_back(Assembly(code.op, RegName(regDes), RegName(regArg1), ""));
		}
		currentTable->at(code.des).inM = -1;	//des�����ֵ���ڴ��и�����ʱ
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
	//paramָ�����Ӧ����ѹջ����
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
	//call����ִ�еĶ������Ǳ���IP+4����ת
	//BP�������Ĵ����ȵ����������ٱ���
	assemblyCode.push_back(Assembly("jal", code.arg2, "", ""));
}

void generateReturn_ASM(Quadruple& code) {
	//������ʵֻ�з���ֵΪint�ĺ���
	//����ֱֵ�Ӵ���v0�У�Ȼ���ڸ�ֵ����н������У������ֵ��#return���v0��ȡֵ
	if (currentTable->returnSize > 0) {
		//���ำֵ
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
	//����Ĵ����������иú����ľֲ�����
	/*for (int i = 0; i < REGISTER_NUM; i++) {
		for (string var : RegDescriptor[i]) {
			if (currentTable->find(var) || var == "") {
				RegDescriptor[i].erase(var);
			}
		}
	}*/
}
void func_protect() {
	//�ں�������ִ�п�ʼǰ����$ra��BP�ͱ����Ĵ���
	assemblyCode.push_back(Assembly("push", "$ra", "", ""));
	assemblyCode.push_back(Assembly("push", "$fp", "", ""));
	for (int i = 0; i < REGISTER_NUM; i++) {
		assemblyCode.push_back(Assembly("push", RegName(i + VAR_BASE), "", ""));
	}
}
void func_prework(Quadruple& code) {
	//�����������Ļ��¼������BP��SP��ֵ
	assemblyCode.push_back(Assembly("addi", "$fp", "$sp", "0"));
	for (auto& table : symbolTables) {
		if (code.label == table->funName) {
			assemblyCode.push_back(Assembly("addi", "$sp", '-' + to_string(table->variableSize), "0"));
			break;
		}
	}
}
void generateAssign_ASM(Quadruple& code) {
	//9 des=arg1, arg1==#returnʱ���⴦��
	//25 arg1[arg2]=des
	//27 $des=arg1
	if (code.type == 9) {
		//des�����Ǳ������Ҹ�ֵ���ڼĴ�����
		//arg1����ǳ�����numToReg������Ǳ�����RegToReg
		
		if (code.arg1 == "#return") {
			int regDes = allocateReg(code.des, false) + VAR_BASE;
			mov_RegToReg("$v0", RegName(regDes));
		}
		else if (code.arg1Type) {
			//���arg����ͨ������������ҵ�regArg��ֱ���޸ļĴ����������͵�ַ����������desָ����
			int regArg1 = allocateReg(code.arg1);	//��������Ա��
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
		//$des=arg1 ����ֵ�����Լ��ǳ������Ǳ���
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
		//��ַֻȡ��16λ
		//assemblyCode.push_back(Assembly("andi", RegName(regDes), RegName(regDes), "0xFFFF"));
		assemblyCode.push_back(Assembly("sw", "0(" + RegName(regDes) + ')', RegName(regArg), ""));
	}
}
void generateJump_ASM(Quadruple& code) {
	//10 j
	//11-12 j< j<= 13-14 j> j>= 
	//15 j!= 16 j==
	//������תʵ�ַ���������ȡ����ת������һ�䣬Ȼ����һ����j��ת��Ŀ���ǩ
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
		//��ÿ�������ֱ�ֱ���������ű��м�¼�˸ú�������Ԫʽ��ʼλ��

		//��¼һ�±������Ŀ�ʼλ�ã�������ǩ
		ASMLabelMap.insert(make_pair(currentTable->funName, assemblyCode.size()));

		map<int, BasicBlock> flowGraph;
		//������Ϊ�����������鹹����ͼ
		deque<int> leaders(currentTable->leaders.begin(), currentTable->leaders.end());//leaders�Ǳ������и���������Ŀ�ʼָ�����
		leaders.push_back(currentTable->endIndex);	//Ϊ�˻�ȡ���һ�����ĩβָ����ţ�ע��������ܻ��������յģ���Դ������ifwhile���βʱ
		for (auto& leader : leaders) {
			//��ʼ�����������
			flowGraph.insert(make_pair(leader, BasicBlock(leader)));
		}
		//����������Ҫ�����������л��������ͼ
		int beginIndex = leaders.front();
		auto cur = leaders.begin(); ++cur;
		int endIndex = beginIndex;
		while (cur != leaders.end()) {
			endIndex = *cur;

			auto& block = flowGraph.at(beginIndex);
			block.end = endIndex;	//����end���ǵ�ǰ�����һ�����һ��
			auto& code = middleCode.at(endIndex - 1);	//��ǰ�����һ����Ԫʽ

			//�������һ����Ԫʽ��������жϺ�̻�����Щ��
			if (code.type == 10) {
				//��������ת��ֻ��������Ŀ���Ϊ���
				string label = code.des;
				int code_ind = labelMap[label];
				flowGraph[code_ind].predecessors.push_back(beginIndex);
				block.successors.push_back(code_ind);	//block�����ã�ֱ�Ӳ����Լ�ڴ�
			}
			else if (code.type >= 11 && code.type <= 16) {
				//�����������ת�������ĺ�̼�����ת���Ļ����飬Ҳ��˳�����һ��
				string label = code.des;
				int code_ind = labelMap[label];
				flowGraph[code_ind].predecessors.push_back(beginIndex);
				block.successors.push_back(code_ind);	//block�����ã�ֱ�Ӳ����Լ�ڴ�

				flowGraph[endIndex].predecessors.push_back(beginIndex);
				block.successors.push_back(endIndex);
			}
			else {
				//�������ָͨ���ֻ���˳�����һ����Ϊ���
				flowGraph[endIndex].predecessors.push_back(beginIndex);
				block.successors.push_back(endIndex);
			}
			beginIndex = endIndex;
			cur++;
		}

		//��Ծ���������㷨 ����9.14
bool change = true;
while (change) {
	change = false;
	for (auto i = leaders.rbegin() + 1; i != leaders.rend(); i++) {	//+1Ϊ���������渽�ӵĿտ�
		auto& block = flowGraph[*i];
		VarSet inLiveVar;
		//��1��
		analysisVarState(block.begin, block.end, block.outLiveVar, inLiveVar);
		//��2��
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
				//�����Ƿ�ֹ��������������һ��if��while�飬���ڻ������һ�������飬����middleCode��vector����Խ�������
			}
			string label;
			//��ֲ��Ԫʽ�е�label����һ�����
			/*if (!label.empty() && label != "main") {
				ASMLabelMap.insert(make_pair(label, assemblyCode.size()));
			}*/
			//���ɻ��
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
		//ȫ�ֱ�����
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
		out << setw(6) << code.op;	//������ָ���
		if (code.op == "sw") {
			out << setw(10) << code.r1+',' << setw(10) << code.rd;
		}
		else if (code.op == "jg" || code.op == "jge" || \
			code.op == "jl" || code.op == "jle" || \
			code.op == "beq" || code.op == "bne") {
			//����������ת��offset�ں���
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
	//inLiveVar��outLiveVar�ֱ�ָ�ڱ�����������ڴ���Ծ�ı��������ڳ��ڴ���Ծ�ı�����
	//������ּ�������ڴ���Ծ������
	for (auto& var : outLiveVar) {
		//������Щ�ڳ��ڴ���Ծ�ı����������һ��ǰ���ڱ��н�����Ϊ��Ծ
		currentTable->at(var).live = true;
		currentTable->at(var).nextUse = endInd;
	}

	//�����һ��ָ����ǰ���㣬��ֵ��ɱ����Ծ
	for (int i = endInd - 1; i >= beginInd; i--) {
		auto& code = middleCode.at(i);
		if (code.type == 7) {
			continue;	//call
		}
		bool desExist = !(code.des.empty() || code.op[0] == 'j');
		bool arg1Exist = !code.arg1.empty();
		bool arg2Exist = !code.arg2.empty();

		if (desExist) {
			//��ֵɱ����des����
			code.desLive = currentTable->at(code.des).live;
			code.desNext = currentTable->at(code.des).nextUse;
			currentTable->at(code.des).live = false;
			currentTable->at(code.des).nextUse = -1;
		}
		if (code.arg1 == "#return") {
			continue;
		}
		if (arg1Exist) {
			//������arg1
			code.arg1Live = currentTable->at(code.arg1).live;
			code.arg1Next = currentTable->at(code.arg1).nextUse;
			currentTable->at(code.arg1).live = true;
			currentTable->at(code.arg1).nextUse = i;
		}
		if (arg2Exist) {
			//������arg2
			code.arg2Live = currentTable->at(code.arg2).live;
			code.arg2Next = currentTable->at(code.arg2).nextUse;
			currentTable->at(code.arg1).live = true;
			currentTable->at(code.arg1).nextUse = i;
		}

	}
	for (auto& var : currentTable->vars) {
		if (var.live == true && var.space > 0) {
			//��ڴ���Ծ����use��
			inLiveVar.insert(var.place);
		}
		var.live = false;
		var.nextUse = -1;
	}
}
