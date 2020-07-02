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
