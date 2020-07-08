# DBF库(header-only)
支持DBF格式文件批量读写的C++库，使用原始的STL编写，使用C++标准语法

# 特性
1.支持普通文件模式和内存模式，使用内存模式时所有操作均在内存完成，提升文件读写效率
2.支持数据的批量读写操作

# 示例代码
1.批量读：
```cpp
// 读取数据到缓存,nRead为预读取行数
if (oDbf.Read(i, nRead))
{
    cout << "读取记录失败，起始记录号:" << i << std::endl;
    return;
}
// 显示数据
string strValue;
for (size_t j = 0; j < nRead; j++)
{
    // 跳转到预读取行
    if (oDbf.ReadGo(j))
    {
        cout << "读取缓存记录行失败:" << j << std::endl;
        continue;
    }
    
    // 读取字段
    oDbf.ReadString("NAME", strValue);
    oDbf.ReadString("ADDR", strValue);
}
```

2.批量写
```cpp
// 打开文件
CPDbf oDbf1;
if (oDbf1.Open(strDst, false))
{
    printf("打开DBF文件失败\n");
    return;
}

// 清空DBF文件
if (oDbf1.Zap())
{
    printf("清空DBF文件失败\n");
    return;
}


// 打开数据源
CPDbf oDbf2;
if (oDbf2.Open(strSrc, true))
{
    printf("打开DBF文件失败\n");
    return;
}

// 读取拷贝数据
size_t nFieldNum = oDbf2.GetFieldNum();
size_t nRecNum = oDbf2.GetRecNum();
size_t nReadNum = 100;
string strField;
size_t nSucc = 0;
for (size_t i = 0; i < nRecNum; i += nReadNum)
{
    nReadNum = min(nReadNum, nRecNum - i);
    if (oDbf2.Read(i, nReadNum))
    {
        printf("读取记录失败:%d,%d", i, nReadNum);
        continue;
    }
    if (oDbf1.PrepareAppend(nReadNum))
    {
        printf("预追加数据失败:%d", nReadNum);
        continue;
    }

    // 字段拷贝
    for (size_t j = 0; j < nReadNum; j++)
    {
        if (oDbf2.ReadGo(j))
        {
            printf("设置读缓存行指针失败:%d\n", j);
            continue;
        }
        if (oDbf1.WriteGo(j))
        {
            printf("设置写缓存行指针失败:%d\n", j);
            continue;
        }
        for (size_t k = 0; k < nFieldNum; k++)
        {
            if (oDbf2.ReadString(k, strField))
            {
                printf("读字段失败\n");
                continue;
            }
            if (oDbf1.WriteString(k, strField))
            {
                printf("写字段失败\n");
                continue;
            }
        }
    }

    // 写缓存提交
    if (oDbf1.WriteCommit())
    {
        printf("写缓存提交失败\n");
    }
    else
    {
        printf("已提交缓存数:%d\n", nSucc += nReadNum);
    }
}
// 文件提交
if (oDbf1.FileCommit())
{
    printf("文件提交失败");
}
```
