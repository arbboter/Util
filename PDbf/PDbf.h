#ifndef __P_DBF_H__
#define __P_DBF_H__
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <assert.h>

// DBF�ӿ�
class CIDbf
{
public:
    // ������
    enum ERetCode
    {
        DBF_SUCC,           // �ɹ�
        DBF_ERROR,          // ����
        DBF_FILE_ERROR,     // �ļ�����
        DBF_PARA_ERROR,     // ��������
        DBF_CACHE_ERROR,    // �������
    };
    // �ж��ļ��Ƿ��
    virtual bool IsOpen() = NULL;

    // ���ļ�
    virtual int Open(const std::string& strFile, bool bReadOnly = false) = NULL;

    // �ر�DBF�ļ�
    virtual void Close() = NULL;

    // �����ڴ�ģʽ(�ļ�ȫ�����ص��ڴ�)
    virtual int EnableMemoryMode(size_t nAppendRow = 0) = NULL;

    // �ļ���¼��
    virtual size_t GetRecNum() = NULL;

    // ��ȡ�ļ���¼
    // ��ȡ��¼�е�����
    virtual int Read(int nRecNo, int nRecNum) = NULL;

    // ���ö�ָ��, ��0��ʼ, �����¼������
    virtual int ReadGo(int nRec) = NULL;

    // ��ȡ�ֶ�
    virtual int ReadString(const std::string& strName, std::string& strValue) = NULL;
    virtual int ReadDouble(const std::string& strName, double& fValue) = NULL;
    virtual int ReadInt(const std::string& strName, int& nValue) = NULL;
    virtual int ReadLong(const std::string& strName, int& nValue) = NULL;

    // �ַ�������
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
    // ���캯��
    CPDbf()
    {
        // �ļ����
        m_pFile = NULL;
        // ��ע��Ϣ����
        m_nRemarkLen = 0;
        // ��ǰ����
        m_nCurRec = 0;
        // �ļ�����
        m_pFileBuf = NULL;
        // �ļ��л���
        m_pWriteBuf = NULL;
        m_pReadBuf = NULL;
        // ��ǰ��
        m_pCurRow = NULL;
        m_bReadOnly = true;
    }
    ~CPDbf()
    {
        Close();
    }

    // �ж��ļ��Ƿ��
    inline bool IsOpen() { return m_pFile != NULL; }

    // ���ļ�
    int Open(const std::string& strFile, bool bReadOnly = false)
    {
        // ����Ƿ��Ѵ�
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
        // ��ȡ�ļ�ͷ��Ϣ
        if (ReadHeader())
        {
            return DBF_FILE_ERROR;
        }
        // ��ȡ�ֶ���Ϣ
        if (ReadField())
        {
            return DBF_FILE_ERROR;
        }

        // ������ʼ��
        m_nCurRec = 0;
        return DBF_SUCC;
    }

    // �ر�DBF�ļ�
    void Close()
    {
        // �ڴ�����
        delete m_pWriteBuf;
        m_pWriteBuf = NULL;

        delete m_pReadBuf;
        m_pReadBuf = NULL;

        delete[] m_pFileBuf;
        m_pFileBuf = NULL;

        delete[] m_pCurRow;
        m_pCurRow = NULL;

        // �ر��ļ����
        if(m_pFile)
        {
            fclose(m_pFile);
            m_pFile = NULL;
        }
    }

    // �����ڴ�ģʽ(�ļ�ȫ�����ص��ڴ�)
    int EnableMemoryMode(size_t nAppendRow = 0)
    {
        int nRet = DBF_SUCC;
        if (!IsOpen())
        {
            return DBF_FILE_ERROR;
        }
        // ɾ��������
        delete m_pFileBuf;
        // ԭʼ�ļ���С
        size_t nSize = m_oHeader.nHeaderLen + m_oHeader.nRecNum*m_oHeader.nRecLen + 1;
        // Ԥ�������ݴ�С
        nSize += nAppendRow * m_oHeader.nRecLen;
        m_pFileBuf = new char[nSize];
        // ��ȡ�ļ����ݵ�����
        fseek(m_pFile, 0, SEEK_SET);
        size_t nRead = fread(m_pFileBuf, 1, nSize, m_pFile);
        if (nRead != nSize)
        {
            return DBF_FILE_ERROR;
        }
        return nRet;
    }

    // �ļ���¼��
    inline size_t GetRecNum() { return m_oHeader.nRecNum; }

    // ��ȡ��¼�е�����
    int Read(int nRecNo, int nRecNum)
    {
        // �����ת��¼��
        if (!IsValidRecNo(nRecNo + nRecNum) || Go(nRecNo))
        {
            return DBF_PARA_ERROR;
        }
        size_t nSize = nRecNum * m_oHeader.nRecLen;
        // �����ǰ�����治�������ٶ�����
        if (m_pReadBuf)
        {
            if (nSize > m_pReadBuf->BufSize())
            {
                delete m_pReadBuf;
                m_pReadBuf = NULL;
            }
        }
        // ������ڴ�
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

    // ���ö�ָ��, ��0��ʼ, �����¼������
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

    // ��ȡ�ֶ�
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
    // ��ת��ָ���У���0��ʼ
    int Go(int nRec)
    {
        int nRet = DBF_SUCC;
        if (!IsOpen())
        {
            return DBF_FILE_ERROR;
        }
        // У�����Ƿ���ȷ
        if (!IsValidRecNo(nRec))
        {
            return DBF_PARA_ERROR;
        }
        // ���ü�¼��
        m_nCurRec = nRec;
        return nRet;
    }

    // ��ȡ����ԭʼ�ֶ�
    int ReadField(const std::string& strName, std::string& strValue)
    {
        int nRet = DBF_SUCC;
        if (!m_pReadBuf || m_pReadBuf->IsEmpty())
        {
            return DBF_ERROR;
        }
        // ��ȡ�ֶ�λ��
        size_t nField = FindField(strName);
        return ReadField(nField, strValue);
    }
    int ReadField(size_t nField, std::string& strValue)
    {
        int nRet = DBF_SUCC;
        // ����ֶ�λ��
        if (nField >= m_vecField.size())
        {
            return DBF_PARA_ERROR;
        }
        if (!m_pReadBuf || m_pReadBuf->IsEmpty())
        {
            return DBF_ERROR;
        }
        // ��ȡ�ֶ���Ϣ
        char* pField = m_pReadBuf->GetCurRow() + m_vecField[nField].nPosition;
        strValue = std::string(pField, m_vecField[nField].cLength);
        return nRet;
    }

    // ��¼�л���
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

        // ���ؼ�¼��
        inline size_t Size()
        {
            return m_nRecNum;
        }

        // ׷�Ӽ�¼��
        inline bool Push(char* pRec, int nNum)
        {
            if (nNum + m_nRecNum > m_nRecCapacity)
            {
                return false;
            }
            memcpy(m_pBuf, pRec, nNum * m_nRecLen);
            return true;
        }
        
        // ��ȡ��¼��
        char* At(size_t nIdx)
        {
            if (nIdx >= m_nRecNum)
            {
                return NULL;
            }
            return &m_pBuf[nIdx * m_nRecLen];
        }

        // �޸Ķ����С
        void Resize(size_t nNum)
        {
            // ��С
            if (nNum < m_nRecCapacity)
            {
                m_nRecCapacity = nNum;
            }
            if (nNum < m_nRecNum)
            {
                m_nRecNum = nNum;
            }
            // ����
            if (nNum > m_nRecCapacity)
            {
                char* pBuf = new char[nNum * m_nRecLen];
                memset(pBuf, 0, nNum * m_nRecLen);
                memcpy(pBuf, m_pBuf, m_nRecCapacity*m_nRecLen);
                m_nRecCapacity = nNum;
                // �ڴ洦��
                delete m_pBuf;
                m_pBuf = pBuf;
            }
        }

        // �Ƿ�Ϊ��
        bool IsEmpty()
        {
            return m_nRecNum == 0;
        }
        // ���ػ����С
        size_t BufSize()
        {
            return m_nRecCapacity * m_nRecLen;
        }
        // ���õ�ǰ�У���0��ʼ
        inline bool Go(size_t nRec)
        {
            if (nRec >= m_nRecNum)
            {
                return false;
            }
            m_nCurRec = nRec;
            return true;
        }
        // ��ȡ��ǰ������
        char* GetCurRow()
        {
            return m_pBuf + m_nCurRec * m_nRecLen;
        }

        // ��������
        inline char* Data() { return m_pBuf; }
        inline size_t& RecNum() { return m_nRecNum; }
        inline size_t  RecLen() { return m_nRecLen; }
private:
        // ��ǰ��¼��
        size_t m_nRecNum;
        // ÿ����¼����
        size_t m_nRecLen;
        // �����ɼ�¼��
        size_t m_nRecCapacity;
        // ����
        char* m_pBuf;
        // ��ǰ������
        size_t m_nCurRec;
    };
    enum EFV
    {
        FV_NS = 0x00,       // ��ȷ�����ļ�����
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
    // DBFͷ(32�ֽ�)
    struct TDbfHeader 
    {
        char            cVer;           // �汾��־
        unsigned char   cYy;            // ��������
        unsigned char   cMm;            // ��������
        unsigned char   cDd;            // ������
        unsigned int    nRecNum;        // �ļ��������ܼ�¼��
        unsigned short  nHeaderLen;     // �ļ�ͷ����
        unsigned short  nRecLen;        // ��¼����
        char            szReserved[20]; // ����

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
    // �ֶ�
    struct TDbfField
    {
        // �ֶ���
        char szName[11];
        // �ֶ����ͣ���ASCII��ֵ
        // B-������ C-�ַ��� D-��������YYYYMMDD G-�����ַ� N-��ֵ�� L-�߼��� M-�����ַ�
        unsigned char cType;
        // �����ֽڣ������Ժ�����µ�˵������Ϣʱʹ�ã�������0����д
        unsigned int nReserved1;
        // ��¼��ȣ���������
        unsigned char cLength;
        // ��¼��ľ��ȣ���������
        unsigned char cPrecisionLength;
        // �����ֽڣ������Ժ�����µ�˵������Ϣʱʹ�ã�������0����д
        // �������ڴ��ļ�ʱ����¼��Ӧ�ֶε���ʵƫ��ֵ
        unsigned short nPosition;
        // ������ID
        unsigned char cWorkId;
        // �����ֽڣ������Ժ�����µ�˵������Ϣʱʹ�ã�������0����д
        char szReserved[10];
        // MDX��ʶ���������һ��MDX ��ʽ�������ļ�����ô�����¼��Ϊ�棬����Ϊ��
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

    // �ж��к��Ƿ�Ϸ���������д����ļ�����+��������
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

    // ���ݼ�¼��ʼƫ��ֵ
    size_t RecordOffset()
    {
        assert(IsOpen());
        return m_oHeader.nHeaderLen + m_nRemarkLen;
    }

private:
    // ��ȡ�ļ�ͷ��Ϣ
    int ReadHeader()
    {
        assert(IsOpen());
        // ��ȡͷ��Ϣ
        char szHeader[32] = { 0 };
        fseek(m_pFile, SEEK_SET, 0);
        size_t nRead = fread(szHeader, 1, sizeof(m_oHeader), m_pFile);
        if (nRead != sizeof(m_oHeader))
        {
            return DBF_FILE_ERROR;
        }
        memcpy(&m_oHeader, szHeader, sizeof(szHeader));

        // ���±�ע��Ϣ����
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

    // ��ȡ�ֶ���Ϣ
    int ReadField()
    {
        assert(IsOpen());
        // �ֶ��ܳ��� = ͷ(32) + n*�ֶ�(32) + 1(0x1D)
        int nFieldLen = m_oHeader.nHeaderLen - sizeof(m_oHeader);
        if (nFieldLen % 32 != 1)
        {
            return DBF_FILE_ERROR;
        }

        // ��ȡ�ֶ���Ϣ
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

        // �����ֶ�
        TDbfField oField;
        char* pCur = pField;
        char* pEnd = pField + nFieldLen - sizeof(m_oHeader);
        // �ֶ�ƫ��ֵ��1��ʼ����λΪ��־λ
        int nOffset = 1;
        m_vecField.clear();
        m_mapField.clear();
        while (pCur < pEnd)
        {
            // �����ֶ���Ϣ
            memcpy(&oField, pCur, sizeof(oField));
            pCur += sizeof(oField);
            
            // �����ֶ�ƫ��ֵ
            oField.nPosition = nOffset;
            nOffset += oField.cLength;

            // �����ֶ�
            m_vecField.push_back(oField);
            m_mapField.insert(std::pair<std::string, size_t>(oField.szName, m_vecField.size() - 1));
        }
        // У�����һλ�Ƿ�Ϊ0x0D
        if (*pCur != 0x0D)
        {
            nRet = DBF_FILE_ERROR;
        }
        // �ڴ�����
        delete[] pField;
        pField = NULL;
        return nRet;
    }

protected:
    // ��ȡ��¼�к�������������ļ������뻺�����ݶ�ȡ���Ҷ�ȡ����ʱ���밴�ж�ȡ
    virtual size_t _read(void* ptr, size_t size, size_t nmemb)
    {
        int nRet = DBF_SUCC;
        assert(IsOpen());
        // �ж�����
        if (m_nCurRec < m_oHeader.nRecNum)
        {
            // ����Ŀ���¼�е�λ��
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
                // �л�����Ӧ��¼��
                fseek(m_pFile, nCurOffset, SEEK_SET);
                return fread(ptr, size, nmemb, m_pFile);
            }
        }
        // �ӻ����ж�ȡ����
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

    // �����ֶ�λ��
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
    // �ļ�����
    FILE* m_pFile;
    // �Ƿ�ֻ��
    bool m_bReadOnly;
    // �ļ�ͷ��Ϣ
    TDbfHeader m_oHeader;
    // �ļ��ֶ���Ϣ
    std::vector<TDbfField> m_vecField;
    // �ֶ�����
    std::map<std::string, size_t> m_mapField;
    // ��ע��Ϣ����
    size_t m_nRemarkLen;
    // ��ǰ����
    size_t m_nCurRec;

    // �ļ�����
    char* m_pFileBuf;

    // �ļ�д��¼�л���
    CRecordBuf* m_pWriteBuf;
    // �ļ�����¼�л���
    CRecordBuf* m_pReadBuf;

    // ��ǰ��¼��
    char* m_pCurRow;
};

#endif
