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

//转为c++代码
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
vector<string> Operator(const string& str)
{
    vector<char> chs = { '+','-','*','/' };
    vector<string> strs = split_two(str, chs);
    return strs;
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
        else if (str == "int" || str == "long" || str == "double" || str == "float")
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

            if (!found)
            {
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
                else
                {
                    cout << "[Error] Undefined variable: " << varName << endl;
                }
            }

        }

        //...

    }

    system("pause");

    return 0;
}