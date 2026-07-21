// Lizard.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
// 这是一个解释器代码
// 此代码使用GPLv3

#define NOMINMAX
#include <unordered_set>
#include <algorithm>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <limits> 

using namespace std;

struct Operator_uct
{
    string num;
    char ch;
};

struct two_variables_int
{
    string name;
    int value;
};

struct two_variables_long
{
    string name;
    long long value;
};

struct two_variables_double
{
    string name;
    double value;
};

struct two_variables_float
{
    string name;
    float value;
};

struct two_variables_bool
{
    string name;
    bool value;
};

struct two_variables_string
{
    string name;
    string value;
};

// 读取文件内容
vector<string> ReadFileFromPath(const string& Path)
{
    vector<string> fileContent;
    ifstream file(Path);  // 打开文件
    if (!file.is_open())
    {
        cerr << "Can't open the file: " << Path << endl;
        return {};  // 返回空 vector
    }

    string line;
    while (getline(file, line))
    {
        fileContent.push_back(line);
    }

    file.close();
    return fileContent;
}

// 按单个分隔符拆分字符串
vector<string> split(const string& str, char delimiter)
{
    vector<string> result;
    string token;
    stringstream ss(str);

    while (getline(ss, token, delimiter))
    {
        result.push_back(token);
    }

    return result;
}

// 按多个分隔符拆分字符串
vector<string> split_two(const string& str, const vector<char>& delimiters)
{
    unordered_set<char> delimSet(delimiters.begin(), delimiters.end());
    vector<string> result;
    string token;

    for (char c : str)
    {
        if (delimSet.count(c))
        {
            if (!token.empty())
            {
                result.push_back(token);
                token.clear();
            }
        }
        else
        {
            token.push_back(c);
        }
    }
    if (!token.empty())
    {
        result.push_back(token);
    }
    return result;
}

// 转为c++代码
vector<string> transform_c(const vector<vector<string>>& files_split)
{
    vector<string> c;
    c.push_back("#include <bits/stdc++.h>");
    c.push_back("using namespace std;");
    c.push_back("int main(){");

    for (const auto& line : files_split)
    {
        string combined;
        for (const auto& token : line)
        {
            combined += token + " ";
        }
        c.push_back(combined);
    }

    c.push_back("return 0;");
    c.push_back("}");
    return c;
}


// 操作符分割
vector<Operator_uct> Operator(const string& str)
{
    vector<Operator_uct> result;
    string num;

    for (char c : str)
    {
        if (isdigit(c)) {
            num.push_back(c);  // 累积数字
        }
        else if (c == '+' || c == '-' || c == '*' || c == '/') {
            if (!num.empty()) {
                result.push_back({ num, c });
                num.clear();
            }
        }
    }

    // 最后一个数字（可能没有跟运算符）
    if (!num.empty()) {
        result.push_back({ num, '\0' }); // '\0' 表示没有运算符
    }

    return result;
}

// 处理加减乘除（运算符）
int evalExpression(const string& expr, int& pos) 
{
    vector<int> values;
    vector<char> ops;

    auto applyOp = [&](char op) 
        {
        int b = values.back(); values.pop_back();
        int a = values.back(); values.pop_back();
        switch (op) {
        case '+': values.push_back(a + b); break;
        case '-': values.push_back(a - b); break;
        case '*': values.push_back(a * b); break;
        case '/':
            if (b == 0) 
            {
                cout << "[Error] Division by zero" << endl;
                values.push_back(0);
            }
            else 
            {
                values.push_back(a / b);
            }
            break;
        }
        };

    auto precedence = [&](char op) 
        {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/') return 2;
        return 0;
        };

    while (pos < expr.size()) {
        char c = expr[pos];

        if (isdigit(c)) 
        {
            int val = 0;
            while (pos < expr.size() && isdigit(expr[pos])) 
            {
                val = val * 10 + (expr[pos] - '0');
                pos++;
            }
            values.push_back(val);
            continue;
        }
        else if (c == '(') 
        {
            pos++;
            int val = evalExpression(expr, pos);
            values.push_back(val);
        }
        else if (c == ')') 
        {
            pos++;
            break;
        }
        else if (c == '+' || c == '-' || c == '*' || c == '/') 
        {
            while (!ops.empty() && precedence(ops.back()) >= precedence(c)) 
            {
                applyOp(ops.back());
                ops.pop_back();
            }
            ops.push_back(c);
            pos++;
        }
        else 
        {
            pos++;
        }
    }

    while (!ops.empty()) 
    {
        applyOp(ops.back());
        ops.pop_back();
    }

    return values.back();
}

// 判断是不是表达式
bool judgment_Operator(string varName) 
{
    bool found = true;
    if (!(varName[0] >= '0' && varName[0] <= '9'))
    {
        found = false;
    }
    for (int j = 0;j < varName.size();j++)
    {
        if (!found) break;
        char ch = varName[j];
        if (ch != '*' && ch != '//' && ch != '+' && ch != '-' && !(ch >= '0' && ch <= '9'))
        {
            found = false;
            break;
        }
    }
    return found;
}

// 将string转为bool
bool stob(string str) 
{
    if (str == "true") return true;
    else return false;
}

// 清空输入缓冲区
void cleanbuffer()
{
    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int main(int argc, char* argv[])
{
    char exeFullPath[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exeFullPath, MAX_PATH);

    string exepath(exeFullPath);

    system("assoc .cl=LizardFile");

    string shell = "ftype LizardFile=\"" + exepath + "\" \"%1\"";
    system(shell.c_str());

    //if (argv[0] != "Lizard" || argv[0] != "Lizard.exe") return 0;

    string path(argv[1]);
    //cout << "请输入文件路径: ";
    //cin >> path;
    //cleanbuffer();

    bool error = 0;
    bool warning = 0;
    int error_num = 0;
    int warning_num = 0;

    // 读取文件
    vector<string> files = ReadFileFromPath(path);

    // 导入的头文件
    vector<string> import;

    //要执行的操作
    vector<string> operate;

    //变量
    vector<two_variables_int> variable_int;
    vector<two_variables_long> variable_long;
    vector<two_variables_double> variable_double;
    vector<two_variables_float> variable_float;
    vector<two_variables_bool> variable_bool;
    vector<two_variables_string> variable_string;

    // 按空格拆分每一行
    vector<vector<string>> files_split(files.size());

    for (int i = 0; i < files.size(); i++)
    {
        files_split[i] = split(files[i], ' ');
    }

    if (argv[2] == "--C++")
    {
        vector<string> code = transform_c(files_split);
        ofstream file("path");
        for (const auto& line : code) {
            file << line << endl;
        }
        file.close();
        return 0;
    }

    cout << "files_split: No mistake" << endl;
    cout << "Explaining right now" << endl;

    for (int i = 0; i < files.size(); i++)
    {

        vector<string> Analysis = files_split[i];

        string str = Analysis[0];

        if (str == "#import") {
            char buffers[MAX_PATH];
            string str2 = Analysis[1];
            DWORD warehouse = GetModuleFileNameA(NULL, buffers, MAX_PATH);
            string warehouse2(buffers);
            string upPath = warehouse2;

            for (int i = 0; i < 3; i++)
            {
                size_t p = upPath.find_last_of("\\/");
                if (p == string::npos) break;
                upPath = upPath.substr(0, p);
            }

            upPath += "\\";
            upPath += str2;
            DWORD attr = GetFileAttributesA(upPath.c_str());

            // 检查是否已经导入过
            auto it = find(import.begin(), import.end(), str2);

            if (it != import.end()) {
                cout << "line:" << i << " [Warning] Repeated import.(" << str2 << ")" << endl;
                warning = 1;
                warning_num++;
            }

            if (attr == INVALID_FILE_ATTRIBUTES)
            {
                cout << "line:" << i << "[Error] No function.(" << str2 << ")" << endl;
                error = 1;
                error_num++;
            }
            else
            {
import.push_back(str2);
            }

        }
        else if (str == "print")
        {
            string str2 = Analysis[1];

            operate.push_back("print " + str2);

            //...
            //还有很多
        }
        else if (str == "int" || str == "long" || str == "double" || str == "float" || str == "bool" || str == "string")
        {
            string str2 = Analysis[1];
            string str3 = "0";

            if (Analysis.size() >= 2) str3 = Analysis[2];

            if (str == "int")
            {
                variable_int.push_back({ str2, atoi(str3.c_str()) });
            }
            else if (str == "long")
            {
                variable_long.push_back({ str2, atoll(str3.c_str()) });
            }
            else if (str == "double")
            {
                variable_double.push_back({ str2, stod(str3) });
            }
            else if (str == "float")
            {
                variable_float.push_back({ str2, stof(str3) });
            }
            else if (str == "bool")
            {
                variable_bool.push_back({ str2, stob(str3) });
            }
            else if (str == "string")
            {
                string str4 = str3.substr(1, str3.size() - 2);
                variable_string.push_back({ str2, str4 });
                variable_string.push_back({ str2, str4 });
            }
        }
        else if (str == "if") {
            string condition = Analysis[1];
            int pos = 0;
            bool condValue = evalExpression(condition, pos) != 0; // 非零为真

            if (condValue) {
                // 执行 if 后面的语句
                string action = Analysis[2]; // 比如 print
                string arg = Analysis[3];
                operate.push_back(action + " " + arg);
            }
            else {
                // 查找 else
                if (Analysis.size() > 4 && Analysis[4] == "else") {
                    string action = Analysis[5];
                    string arg = Analysis[6];
                    operate.push_back(action + " " + arg);
                }
            }
        }
        else {
            cout << "line:" << i << "[Error] No function.(" << str << ")" << endl;
        }
    }

    cout << "Error:" << error_num << "    " << "Warning:" << warning_num << endl;

    if (error) return 0;

    vector<vector<string>> operate_split(operate.size());

    for (int i = 0; i < operate.size(); i++)
    {
        operate_split[i] = split(operate[i], ' ');
    }

    for (int i = 0; i < operate.size(); i++) {
        vector<string> values = split(operate[i], ' ');

        if (values[0] == "print") {
            string varName = values[1];
            bool found = false;

            for (auto& v : variable_int)
            {
                if (v.name == varName)
                {
                    cout << v.value << endl;
                    found = true;
                    break;
                }
            }

            for (auto& v : variable_long)
            {
                if (v.name == varName)
                {
                    cout << v.value << endl;
                    found = true;
                    break;
                }
            }

            for (auto& v : variable_double)
            {
                if (v.name == varName)
                {
                    cout << v.value << endl;
                    found = true;
                    break;
                }
            }

            for (auto& v : variable_float)
            {
                if (v.name == varName)
                {
                    cout << v.value << endl;
                    found = true;
                    break;
                }
            }

            for (auto& v : variable_string)
            {
                if (v.name == varName)
                {
                    cout << v.value << endl;
                    found = true;
                    break;
                }
            }

            for (auto& v : variable_bool)
            {
                if (v.name == varName)
                {
                    cout << v.value << endl;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                bool found1 = true;
                if (varName[0] == '"' || varName == "endl")
                {
                    if (varName == "endl")
                    {
                        cout << endl;
                        cleanbuffer();
                    }
                    else
                    {
                        cout << varName.substr(1, varName.size() - 2) << endl;
                    }
                }
                else if (found1)
                {
                    int pos = 0;
                    int num = evalExpression(varName, pos);
                    cout << num;
                }
                else
                {
                    cout << "[Error] Undefined variable: " << varName << endl;
                }
            }

        }
        else if (values[0] == "if") 
        {
            //...
        }

        //...
        //这是解读，不要二次报错/警告
    }

    system("pause");

    return 0;
}