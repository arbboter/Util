#ifndef __P_DBF_H__
#define __P_DBF_H__
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <assert.h>

// DBF接口
class CIDbf
{
public:
    // 错误码
    enum ERetCode
    {
        DBF_SUCC,           // 成功
        DBF_ERROR,          // 错误
        DBF_FILE_ERROR,     // 文件错误
        DBF_PARA_ERROR,     // 参数错误
        DBF_CACHE_ERROR,    // 缓存错误
    };
    // 判断文件是否打开
    virtual bool IsOpen() = NULL;

    // 打开文件
    virtual int Open(const std::string& strFile, bool bReadOnly = false) = NULL;

    // 关闭DBF文件
    virtual void Close() = NULL;

    // 设置内存模式(文件全部加载到内存)
    virtual int EnableMemoryMode(size_t nAppendRow = 0) = NULL;

    // 文件记录行
    virtual size_t GetRecNum() = NULL;

    // 读取文件记录
    // 读取记录行到缓存
    virtual int Read(int nRecNo, int nRecNum) = NULL;

    // 设置读指针, 从0开始, 缓存记录行行数
    virtual int ReadGo(int nRec) = NULL;

    // 读取字段
    virtual int ReadString(const std::string& strName, std::string& strValue) = NULL;
    virtual int ReadDouble(const std::string& strName, double& fValue) = NULL;
    virtual int ReadInt(const std::string& strName, int& nValue) = NULL;
    virtual int ReadLong(const std::string& strName, int& nValue) = NULL;

    // 字符串函数
    static std::string Ltrim(const std::string& s)
    {
        size_t nPos = 0;
        for (nPos=0; nPos<s.size(); nPos++)
        {
            if (!isspace(s[nPos]))
            {
                break;
            }
        }
        return s.substr(nPos);
    }
    static std::string Rtrim(const std::string& s)
    {
        size_t nPos = 0;
        for (nPos = s.size(); nPos > 0; nPos--)
        {
            if (!isspace(s[nPos-1]))
            {
                break;
            }
        }
        return s.substr(0, nPos);
    }
    static std::string Trim(const std::string& s)
    {
        return Ltrim(Rtrim(s));
    }

public:
    virtual ~CIDbf(){}

};

class CPDbf : public CIDbf
{
public:
    // 构造函数
    CPDbf()
    {
        // 文件句柄
        m_pFile = NULL;
        // 备注信息长度
        m_nRemarkLen = 0;
        // 当前行数
        m_nCurRec = 0;
        // 文件缓存
        m_pFileBuf = NULL;
        // 文件行缓存
        m_pWriteBuf = NULL;
        m_pReadBuf = NULL;
        // 当前行
        m_pCurRow = NULL;
        m_bReadOnly = true;
    }
    ~CPDbf()
    {
        Close();
    }

    // 判断文件是否打开
    inline bool IsOpen() { return m_pFile != NULL; }

    // 打开文件
    int Open(const std::string& strFile, bool bReadOnly = false)
    {
        // 检查是否已打开
        if (IsOpen())
        {
            return DBF_SUCC;
        }
        m_bReadOnly = bReadOnly;
        m_pFile = fopen(strFile.c_str(), bReadOnly ? "rb" : "rb+");
        if (m_pFile == NULL)
        {
            return DBF_FILE_ERROR;
        }
        // 读取文件头信息
        if (ReadHeader())
        {
            return DBF_FILE_ERROR;
        }
        // 读取字段信息
        if (ReadField())
        {
            return DBF_FILE_ERROR;
        }

        // 变量初始化
        m_nCurRec = 0;
        return DBF_SUCC;
    }

    // 关闭DBF文件
    void Close()
    {
        // 内存清理
        delete m_pWriteBuf;
        m_pWriteBuf = NULL;

        delete m_pReadBuf;
        m_pReadBuf = NULL;

        delete[] m_pFileBuf;
        m_pFileBuf = NULL;

        delete[] m_pCurRow;
        m_pCurRow = NULL;

        // 关闭文件句柄
        if(m_pFile)
        {
            fclose(m_pFile);
            m_pFile = NULL;
        }
    }

    // 设置内存模式(文件全部加载到内存)
    int EnableMemoryMode(size_t nAppendRow = 0)
    {
        int nRet = DBF_SUCC;
        if (!IsOpen())
        {
            return DBF_FILE_ERROR;
        }
        // 删除旧数据
        delete m_pFileBuf;
        // 原始文件大小
        size_t nSize = m_oHeader.nHeaderLen + m_oHeader.nRecNum*m_oHeader.nRecLen + 1;
        // 预新增数据大小
        nSize += nAppendRow * m_oHeader.nRecLen;
        m_pFileBuf = new char[nSize];
        // 读取文件数据到内容
        fseek(m_pFile, 0, SEEK_SET);
        size_t nRead = fread(m_pFileBuf, 1, nSize, m_pFile);
        if (nRead != nSize)
        {
            return DBF_FILE_ERROR;
        }
        return nRet;
    }

    // 文件记录行
    inline size_t GetRecNum() { return m_oHeader.nRecNum; }

    // 读取记录行到缓存
    int Read(int nRecNo, int nRecNum)
    {
        // 检查跳转记录行
        if (!IsValidRecNo(nRecNo + nRecNum) || Go(nRecNo))
        {
            return DBF_PARA_ERROR;
        }
        size_t nSize = nRecNum * m_oHeader.nRecLen;
        // 如果当前读缓存不够，销毁读缓存
        if (m_pReadBuf)
        {
            if (nSize > m_pReadBuf->BufSize())
            {
                delete m_pReadBuf;
                m_pReadBuf = NULL;
            }
        }
        // 分配读内存
        if (!m_pReadBuf)
        {
            m_pReadBuf = new CRecordBuf(nRecNum, m_oHeader.nRecLen);
        }
        size_t nRead = _read(m_pReadBuf->Data(), 1, nSize);
        if (nRead != nSize)
        {
            return DBF_ERROR;
        }
        m_pReadBuf->RecNum() = nRecNum;
        return DBF_SUCC;
    }

    // 设置读指针, 从0开始, 缓存记录行行数
    int ReadGo(int nRec)
    {
        if (!m_pReadBuf)
        {
            return DBF_PARA_ERROR;
        }
        if (!m_pReadBuf->Go(nRec))
        {
            return DBF_PARA_ERROR;
        }
        return DBF_SUCC;
    }

    // 读取字段
    int ReadString(const std::string& strName, std::string& strValue)
    {
        return ReadField(strName, strValue);
    }
    int ReadDouble(const std::string& strName, double& fValue)
    {
        std::string strValue;
        int nRet = ReadField(strName, strValue);
        if (nRet)
        {
            return nRet;
        }
        fValue = atof(strValue.c_str());
        return DBF_SUCC;
    }
    int ReadInt(const std::string& strName, int& nValue)
    {
        std::string strValue;
        int nRet = ReadField(strName, strValue);
        if (nRet)
        {
            return nRet;
        }
        nValue = atoi(strValue.c_str());
        return DBF_SUCC;
    }
    int ReadLong(const std::string& strName, int& nValue)
    {
        std::string strValue;
        int nRet = ReadField(strName, strValue);
        if (nRet)
        {
            return nRet;
        }
        nValue = atol(strValue.c_str());
        return DBF_SUCC;
    }

protected:
    // 跳转到指定行，从0开始
    int Go(int nRec)
    {
        int nRet = DBF_SUCC;
        if (!IsOpen())
        {
            return DBF_FILE_ERROR;
        }
        // 校验行是否正确
        if (!IsValidRecNo(nRec))
        {
            return DBF_PARA_ERROR;
        }
        // 设置记录行
        m_nCurRec = nRec;
        return nRet;
    }

    // 读取数据原始字段
    int ReadField(const std::string& strName, std::string& strValue)
    {
        int nRet = DBF_SUCC;
        if (!m_pReadBuf || m_pReadBuf->IsEmpty())
        {
            return DBF_ERROR;
        }
        // 获取字段位置
        size_t nField = FindField(strName);
        return ReadField(nField, strValue);
    }
    int ReadField(size_t nField, std::string& strValue)
    {
        int nRet = DBF_SUCC;
        // 检查字段位置
        if (nField >= m_vecField.size())
        {
            return DBF_PARA_ERROR;
        }
        if (!m_pReadBuf || m_pReadBuf->IsEmpty())
        {
            return DBF_ERROR;
        }
        // 获取字段信息
        char* pField = m_pReadBuf->GetCurRow() + m_vecField[nField].nPosition;
        strValue = std::string(pField, m_vecField[nField].cLength);
        return nRet;
    }

    // 记录行缓存
    class CRecordBuf
    {
    public:
        CRecordBuf(size_t nRecCapacity, size_t nRecLen)
        {
            m_nRecNum = 0;
            m_nRecCapacity = nRecCapacity;
            m_nRecLen = nRecLen;
            size_t nSize = m_nRecCapacity * m_nRecLen;
            m_pBuf = new char[nSize];
            memset(m_pBuf, 0, nSize);
            m_nCurRec = 0;
        }
        ~CRecordBuf()
        {
            delete[] m_pBuf;
            m_pBuf = NULL;
        }

        // 返回记录数
        inline size_t Size()
        {
            return m_nRecNum;
        }

        // 追加记录行
        inline bool Push(char* pRec, int nNum)
        {
            if (nNum + m_nRecNum > m_nRecCapacity)
            {
                return false;
            }
            memcpy(m_pBuf, pRec, nNum * m_nRecLen);
            return true;
        }
        
        // 获取记录行
        char* At(size_t nIdx)
        {
            if (nIdx >= m_nRecNum)
            {
                return NULL;
            }
            return &m_pBuf[nIdx * m_nRecLen];
        }

        // 修改对象大小
        void Resize(size_t nNum)
        {
            // 减小
            if (nNum < m_nRecCapacity)
            {
                m_nRecCapacity = nNum;
            }
            if (nNum < m_nRecNum)
            {
                m_nRecNum = nNum;
            }
            // 扩大
            if (nNum > m_nRecCapacity)
            {
                char* pBuf = new char[nNum * m_nRecLen];
                memset(pBuf, 0, nNum * m_nRecLen);
                memcpy(pBuf, m_pBuf, m_nRecCapacity*m_nRecLen);
                m_nRecCapacity = nNum;
                // 内存处理
                delete m_pBuf;
                m_pBuf = pBuf;
            }
        }

        // 是否为空
        bool IsEmpty()
        {
            return m_nRecNum == 0;
        }
        // 返回缓存大小
        size_t BufSize()
        {
            return m_nRecCapacity * m_nRecLen;
        }
        // 设置当前行，从0开始
        inline bool Go(size_t nRec)
        {
            if (nRec >= m_nRecNum)
            {
                return false;
            }
            m_nCurRec = nRec;
            return true;
        }
        // 获取当前行数据
        char* GetCurRow()
        {
            return m_pBuf + m_nCurRec * m_nRecLen;
        }

        // 返回数据
        inline char* Data() { return m_pBuf; }
        inline size_t& RecNum() { return m_nRecNum; }
        inline size_t  RecLen() { return m_nRecLen; }
private:
        // 当前记录数
        size_t m_nRecNum;
        // 每条记录长度
        size_t m_nRecLen;
        // 可容纳记录数
        size_t m_nRecCapacity;
        // 数据
        char* m_pBuf;
        // 当前数据行
        size_t m_nCurRec;
    };
    enum EFV
    {
        FV_NS = 0x00,       // 不确定的文件类型
        FV_FB2 = 0x02,      // FoxBASE
        FV_FB3 = 0x03,      // FoxBASE + / Dbase III plus, no memo
        FV_VFP = 0x30,      // Visual FoxPro
        FV_VFPAI = 0x31,    // Visual FoxPro, autoincrement enabled
        FV_D4TABLE = 0x43,  // dBASE IV SQL table files, no memo
        FV_D4FILE = 0x63,   // dBASE IV SQL system files, no memo
        FV_MD3P = 0x83,     // FoxBASE + / dBASE III PLUS, with memo
        FV_MD4 = 0x8B,      // dBASE IV with memo
        FV_MD4TABLE = 0xCB, // dBASE IV SQL table files, with memo
        FV_MFP2 = 0xF5,     // FoxPro 2.x(or earlier) with memo
        FV_FP = 0xFB,       // FoxBASE
    };
    // DBF头(32字节)
    struct TDbfHeader 
    {
        char            cVer;           // 版本标志
        unsigned char   cYy;            // 最后更新年
        unsigned char   cMm;            // 最后更新月
        unsigned char   cDd;            // 最后更日
        unsigned int    nRecNum;        // 文件包含的总记录数
        unsigned short  nHeaderLen;     // 文件头长度
        unsigned short  nRecLen;        // 记录长度
        char            szReserved[20]; // 保留

        TDbfHeader()
        {
            assert(sizeof(*this) == 32);
            cVer=FV_NS;
            cYy=0;
            cMm=0;
            cDd=0;
            nRecNum=0;
            nHeaderLen=0;
            nRecLen=0;
            memset(szReserved, 0, sizeof(szReserved));
        }
    };
    // 字段
    struct TDbfField
    {
        // 字段名
        char szName[11];
        // 字段类型，是ASCII码值
        // B-二进制 C-字符型 D-日期类型YYYYMMDD G-各种字符 N-数值型 L-逻辑性 M-各种字符
        unsigned char cType;
        // 保留字节，用于以后添加新的说明性信息时使用，这里用0来填写
        unsigned int nReserved1;
        // 记录项长度，二进制型
        unsigned char cLength;
        // 记录项的精度，二进制型
        unsigned char cPrecisionLength;
        // 保留字节，用于以后添加新的说明性信息时使用，这里用0来填写
        // 本程序在打开文件时，记录对应字段的其实偏移值
        unsigned short nPosition;
        // 工作区ID
        unsigned char cWorkId;
        // 保留字节，用于以后添加新的说明性信息时使用，这里用0来填写
        char szReserved[10];
        // MDX标识。如果存在一个MDX 格式的索引文件，那么这个记录项为真，否则为空
        char cMdxFlag;

        TDbfField()
        {
            assert(sizeof(*this) == 32);
            memset(szName, 0, sizeof(szName));
            cType = 0;
            nReserved1 = 0;
            cLength = 0;
            cPrecisionLength = 0;
            nPosition = 0;
            cWorkId = 0;
            memset(szReserved, 0, sizeof(szReserved));
            cMdxFlag = 0;
        }
    };

    // 判断行号是否合法，包括已写入的文件行数+缓存行数
    bool IsValidRecNo(unsigned int nNum)
    {
        size_t nRec = nNum;
        size_t nWrite = m_pWriteBuf ? m_pWriteBuf->RecNum() : 0;
        if (nRec > m_oHeader.nRecNum + nWrite)
        {
            return false;
        }
        return true;
    }

    // 数据记录起始偏移值
    size_t RecordOffset()
    {
        assert(IsOpen());
        return m_oHeader.nHeaderLen + m_nRemarkLen;
    }

private:
    // 读取文件头信息
    int ReadHeader()
    {
        assert(IsOpen());
        // 读取头信息
        char szHeader[32] = { 0 };
        fseek(m_pFile, SEEK_SET, 0);
        size_t nRead = fread(szHeader, 1, sizeof(m_oHeader), m_pFile);
        if (nRead != sizeof(m_oHeader))
        {
            return DBF_FILE_ERROR;
        }
        memcpy(&m_oHeader, szHeader, sizeof(szHeader));

        // 更新备注信息长度
        switch (m_oHeader.cVer)
        {
        case FV_MD3P:
        case FV_MD4:
        case FV_MD4TABLE:
        case FV_MFP2:
            m_nRemarkLen = 263;
        default:
            break;
        }
        return DBF_SUCC;
    }

    // 读取字段信息
    int ReadField()
    {
        assert(IsOpen());
        // 字段总长度 = 头(32) + n*字段(32) + 1(0x1D)
        int nFieldLen = m_oHeader.nHeaderLen - sizeof(m_oHeader);
        if (nFieldLen % 32 != 1)
        {
            return DBF_FILE_ERROR;
        }

        // 读取字段信息
        int nRet = DBF_SUCC;
        char* pField = new char[nFieldLen];
        fseek(m_pFile, sizeof(m_oHeader), SEEK_SET);
        size_t nRead = fread(pField, 1, nFieldLen, m_pFile);
        if (nRead != nFieldLen)
        {
            delete[] pField;
            pField = NULL;
            return DBF_FILE_ERROR;
        }

        // 解析字段
        TDbfField oField;
        char* pCur = pField;
        char* pEnd = pField + nFieldLen - sizeof(m_oHeader);
        // 字段偏移值从1开始，首位为标志位
        int nOffset = 1;
        m_vecField.clear();
        m_mapField.clear();
        while (pCur < pEnd)
        {
            // 拷贝字段信息
            memcpy(&oField, pCur, sizeof(oField));
            pCur += sizeof(oField);
            
            // 计算字段偏移值
            oField.nPosition = nOffset;
            nOffset += oField.cLength;

            // 保存字段
            m_vecField.push_back(oField);
            m_mapField.insert(std::pair<std::string, size_t>(oField.szName, m_vecField.size() - 1));
        }
        // 校验最后一位是否为0x0D
        if (*pCur != 0x0D)
        {
            nRet = DBF_FILE_ERROR;
        }
        // 内存清理
        delete[] pField;
        pField = NULL;
        return nRet;
    }

protected:
    // 读取记录行函数，不允许跨文件数据与缓存数据读取，且读取数据时必须按行读取
    virtual size_t _read(void* ptr, size_t size, size_t nmemb)
    {
        int nRet = DBF_SUCC;
        assert(IsOpen());
        // 判断行数
        if (m_nCurRec < m_oHeader.nRecNum)
        {
            // 计算目标记录行的位置
            size_t nCurOffset = m_nCurRec * m_oHeader.nRecLen + RecordOffset();
            if (m_pFileBuf)
            {
                char* pData = m_pFileBuf + nCurOffset;
                if (!memcpy(ptr, pData, size * nmemb))
                {
                    nRet = DBF_ERROR;
                }
                else
                {
                    nRet = nmemb;
                }
            }
            else
            {
                // 切换到对应记录行
                fseek(m_pFile, nCurOffset, SEEK_SET);
                return fread(ptr, size, nmemb, m_pFile);
            }
        }
        // 从缓存行读取数据
        else if(m_pWriteBuf)
        {
            char* pData = m_pWriteBuf->Data() + (m_nCurRec - m_oHeader.nRecNum)*m_oHeader.nRecLen;
            if (!memcpy(ptr, pData, size * nmemb))
            {
                nRet = DBF_ERROR;
            }
            else
            {
                nRet = nmemb;
            }
        }
        else
        {
            nRet = DBF_PARA_ERROR;
        }
        return nRet;
    }
    virtual size_t _write()
    {
        int nRet = DBF_SUCC;
        assert(IsOpen());

        return nRet;
    }

    // 查找字段位置
    virtual size_t FindField(const std::string& strField)
    {
        std::map<std::string, size_t>::iterator e = m_mapField.find(strField);
        if (e != m_mapField.end())
        {
            return e->second;
        }
        return -1;
    }

private:
    // 文件对象
    FILE* m_pFile;
    // 是否只读
    bool m_bReadOnly;
    // 文件头信息
    TDbfHeader m_oHeader;
    // 文件字段信息
    std::vector<TDbfField> m_vecField;
    // 字段名字
    std::map<std::string, size_t> m_mapField;
    // 备注信息长度
    size_t m_nRemarkLen;
    // 当前行数
    size_t m_nCurRec;

    // 文件缓存
    char* m_pFileBuf;

    // 文件写记录行缓存
    CRecordBuf* m_pWriteBuf;
    // 文件读记录行缓存
    CRecordBuf* m_pReadBuf;

    // 当前记录行
    char* m_pCurRow;
};

#endif
