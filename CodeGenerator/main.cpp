#include "DataStructureDefine.h"
#include "generateHead.h"
using namespace std;

void read_token(string path);


int main() {
	string path = "../../测试/";
	read_token(path);
	bool translateSuccess = translateToMiddleCode(tokenList, path);
	if (!translateSuccess) {
		cout << "中间代码生成出错" << endl;
		return 0;
	}
	translateToASM(path);
}

string hexToInt(string hex) {
	int tmp = 0;
	for (int i = 2; i < hex.length(); i++) {
		int cur = isdigit(hex[i]) ? hex[i] - '0' : hex[i] - 'A' + 10;
		tmp = (16 * tmp + cur);
	}
	return to_string(tmp);
}
void read_token(string path) {
	ifstream in;
	in.open(path + "tokens.txt");
	string curLine, sep = "#", lexcal, attr, line, offset;
	size_t posL, posR;
	while (!in.eof()) {
		getline(in, curLine);
		if (curLine.empty()) {
			break;
		}
		posL = 0;
		posR = curLine.find_first_of(sep, posL);
		lexcal = curLine.substr(posL, posR - posL);
		posL = posR + 1;

		posR = curLine.find_first_of(sep, posL);
		attr = curLine.substr(posL, posR - posL);
		if (attr == "hex") {
			//读取token的时候把16进制数改int
			lexcal = hexToInt(lexcal);
			attr = "num";
		}
		posL = posR + 1;

		posR = curLine.find_first_of(sep, posL);
		line = curLine.substr(posL, posR - posL);
		posL = posR + 1;

		size_t n = curLine.length();
		offset = curLine.substr(posL, n - posL + 1);
		tokenList.push_back(Token(lexcal, attr, atoi(line.c_str()), atoi(offset.c_str())));
	}
	in.close();
}