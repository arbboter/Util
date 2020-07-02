// PDbf.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "PDbf.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

void Read(CPDbf& oDbf, map<string, string>& mapRow, const string& strName)
{
    string strValue;
    if (oDbf.ReadString(strName, strValue))
    {
        std::cout << "读取字段失败:" << strName << std::endl;
    }
    mapRow.insert(pair<string, string>(strName, CIDbf::Trim(strValue)));
}
void TestRead()
{
    CPDbf oDbf;

    char szFile[64] = "TEST.DBF";
    if (oDbf.Open(szFile, true))
    {
        cout << "打开文件失败:" << szFile << std::endl;
        return;
    }
    // 启用内存高速模式
    // if (oDbf.EnableMemoryMode())
    // {
    //     cout << "启用高性能内存模式失败" << std::endl;
    // }

    // 读取文件
    size_t nRecNum = oDbf.GetRecNum();
    cout << "文件记录行数:" << nRecNum << std::endl;
    // 读取记录行
    size_t nRead = 16;
    string strValue;
    for (size_t i = 0; i < nRecNum; i += nRead)
    {
        // 计算剩余行
        nRead = std::min(nRead, nRecNum - i);
        // 读取数据到缓存
        if (oDbf.Read(i, nRead))
        {
            cout << "读取记录失败，起始记录号:" << i << std::endl;
            continue;
        }
        // 显示数据
        map<string, string> mapRow;
        cout << setw(16) << "NAME" << setw(32) << "ADDR" << std::endl;
        for (size_t j = 0; j < nRead; j++)
        {
            if (oDbf.ReadGo(j))
            {
                cout << "读取缓存记录行失败:" << j << std::endl;
                continue;
            }
            // 读取字段
            Read(oDbf, mapRow, "NAME");
            Read(oDbf, mapRow, "ADDR");

            cout << setw(16) << mapRow["NAME"] << setw(32) << mapRow["ADDR"] << std::endl;
        }
    }
}
int main()
{
    // 测试字符串函数
    vector<string> vecStr = { "", " ", " 1", " 1 ", "1 ", " 123 ", "456" };

    CIDbf::Rtrim(" ");
    for (auto& m : vecStr)
    {
        cout << "[" << m << "]" << "Ltrim" << "[" << CIDbf::Ltrim(m) << "]" << endl;
    }
    for (auto& m : vecStr)
    {
        cout << "[" << m << "]" << "Rtrim" << "[" << CIDbf::Rtrim(m) << "]" << endl;
    }
    for (auto& m : vecStr)
    {
        cout << "[" << m << "]" << "Trim" << "[" << CIDbf::Trim(m) << "]" << endl;
    }
    TestRead();
    return 0;
}
