#include<TCPKernel.h>
#include "packdef.h"
#include<stdio.h>

using namespace std;


////注册
//#define  DEF_PACK_REGISTER_RQ    (DEF_PACK_BASE + 0)
//#define  DEF_PACK_REGISTER_RS    (DEF_PACK_BASE + 1)
////登录
//#define  DEF_PACK_LOGIN_RQ    (DEF_PACK_BASE + 2)
//#define  DEF_PACK_LOGIN_RS    (DEF_PACK_BASE + 3)

static const ProtocolMap m_ProtocolMapEntries[] =
{
    {DEF_PACK_REGISTER_RQ , &TcpKernel::RegisterRq},
    {DEF_PACK_LOGIN_RQ , &TcpKernel::LoginRq},
    {DEF_PACK_UPDATEPICTURE_RQ,&TcpKernel::UpdatePictureRq},
    {0,0}
};
#define RootPath   "/home/colin/Video/"

int TcpKernel::Open()
{
    m_sql = new CMysql;
    m_tcp = new TcpNet(this);
    m_tcp->SetpThis(m_tcp);
    pthread_mutex_init(&m_tcp->alock,NULL);
    pthread_mutex_init(&m_tcp->rlock,NULL);
    if(  !m_sql->ConnectMysql("localhost","root","123123","Myqq")  )
    {
        printf("Conncet Mysql Failed...\n");
        return FALSE;
    }
    else
    {
        printf("MySql Connect Success...\n");
    }
    if( !m_tcp->InitNetWork()  )
    {
        printf("InitNetWork Failed...\n");
        return FALSE;
    }
    else
    {
        printf("Init Net Success...\n");
    }

    return TRUE;
}

void TcpKernel::Close()
{
    m_sql->DisConnect();
    m_tcp->UnInitNetWork();
}


void TcpKernel::DealData(int clientfd,char *szbuf,int nlen)
{
    PackType *pType = (PackType*)szbuf;
    int i = 0;
    while(1)
    {
        if(*pType == m_ProtocolMapEntries[i].m_type)
        {
            auto fun= m_ProtocolMapEntries[i].m_pfun;
            (this->*fun)(clientfd,szbuf,nlen);
        }
        else if(m_ProtocolMapEntries[i].m_type == 0 &&
                m_ProtocolMapEntries[i].m_pfun == 0)
            return;
        ++i;
    }
    return;
}


//注册
void TcpKernel::RegisterRq(int clientfd,char* szbuf,int nlen)
{
    printf("clientfd:%d RegisterRq\n", clientfd);

    STRU_REGISTER_RQ * rq = (STRU_REGISTER_RQ *)szbuf;
    STRU_REGISTER_RS rs;

    m_tcp->SendData( clientfd , (char*)&rs , sizeof(rs) );
}
//登录
void TcpKernel::LoginRq(int clientfd ,char* szbuf,int nlen)
{
    printf("clientfd:%d LoginRq\n", clientfd);

    STRU_LOGIN_RQ * rq = (STRU_LOGIN_RQ *)szbuf;
    STRU_LOGIN_RS rs;

    m_tcp->SendData( clientfd , (char*)&rs , sizeof(rs) );
}

// 接受到客戶端的圖片更新請求
void TcpKernel::UpdatePictureRq(int clientfd ,char* szbuf,int nlen)
{
    printf("clientfd:%d UpdatePictureRq\n", clientfd);

    list<FileInfo*> lstFile;
    GetDownloadList(lstFile);
    // 若列表的大小不爲0
    while(lstFile.size() != 0)
    {
        FileInfo* info = lstFile.front();
        lstFile.pop_front();
        // 首先發送文件頭
        STRU_UPDATEPICTURE_RS rs;
        rs.m_nFileID = info->m_nFileID;
        rs.m_nVideoID = info->m_nVideoID;
        rs.m_nType = DEF_PACK_UPDATEPICTURE_RS;
        rs.m_nFileSize = info->m_nFilesize;
        strcpy(rs.m_szFileName,info->m_szFileName);
        strcpy(rs.m_szRtmp,info->m_szRtmp);
        this->m_tcp->SendData(clientfd,(char*)&rs,sizeof(rs));

        // 打開文件指針
        info->m_pFile = fopen(info->m_szFilePath,"r");
        cout << info->m_szFilePath << endl;
        if(info->m_pFile)
        {
            while(1)
            {
                STRU_DOWNLOAD_FILEBLOCK_RQ FileBlockRq;
                FileBlockRq.m_nType = DEF_PACK_DOWNLOADFILEBLOCK_RQ;
                int64_t nRealReadNum = fread(FileBlockRq.m_szFileContent,1,MAX_CONTENT_LEN,info->m_pFile);

                FileBlockRq.m_nBlockLen = nRealReadNum;
                FileBlockRq.m_nFileID = info->m_nFileID;
                FileBlockRq.m_nUserID = info->m_nUserID;
                int nSizeofq = sizeof(FileBlockRq);
                this->m_tcp->SendData(clientfd,(char*)&FileBlockRq,nSizeofq);
                cout << "sendFile " << FileBlockRq.m_nBlockLen <<  endl;
                info->m_nPos += nRealReadNum;
                // 若當前文件發送完畢，則關閉pFile,並退出
                if(info->m_nPos == info->m_nFilesize)
                {
                    fclose(info->m_pFile);
                    free(info);
                    break;
                }
            }
        }
    }

}

// 獲取下載列表
void TcpKernel::GetDownloadList(list<FileInfo*>& lstFileinfo)
{
    FileInfo* pInfo1 = new FileInfo;
    pInfo1->m_nPos = 0;
    pInfo1->m_nFileID = 1;
    pInfo1->m_nVideoID = 1;
    strcpy(pInfo1->m_szFileName,"1.jpg");
    strcpy(pInfo1->m_szFilePath,"/home/liu/video/flv/1.jpg");
    strcpy(pInfo1->m_szRtmp,"rtmp://192.168.153.129/vod/1.mkv");

    FILE* pFile = fopen(pInfo1->m_szFilePath,"r");
    fseek(pFile,0,SEEK_END);
    pInfo1->m_nFilesize = ftell(pFile);
    fseek(pFile,0,SEEK_SET);
    fclose(pFile);

    pInfo1->m_pFile = 0;
    lstFileinfo.push_back(pInfo1);


    FileInfo* pInfo2 = new FileInfo;
    pInfo2->m_nPos = 0;
    pInfo2->m_nFileID = 2;
    pInfo2->m_nVideoID = 2;
    strcpy(pInfo2->m_szFileName,"2.jpg");
    strcpy(pInfo2->m_szFilePath,"/home/liu/video/flv/2.jpg");
    strcpy(pInfo2->m_szRtmp,"rtmp://192.168.153.129/vod/2.mp4");

    FILE* pFile2 = fopen(pInfo2->m_szFilePath,"r");
    fseek(pFile2,0,SEEK_END);
    pInfo2->m_nFilesize = ftell(pFile2);
    fseek(pFile2,0,SEEK_SET);
    fclose(pFile2);

    pInfo2->m_pFile = 0;
    lstFileinfo.push_back(pInfo2);

    FileInfo* pInfo3 = new FileInfo;
    pInfo3->m_nPos = 0;
    pInfo3->m_nFileID = 3;
    pInfo3->m_nVideoID = 3;
    strcpy(pInfo3->m_szFileName,"3.jpg");
    strcpy(pInfo3->m_szFilePath,"/home/liu/video/flv/3.jpg");
    strcpy(pInfo3->m_szRtmp,"rtmp://192.168.153.129/vod/3.flv");

    FILE* pFile3 = fopen(pInfo3->m_szFilePath,"r");
    fseek(pFile3,0,SEEK_END);
    pInfo3->m_nFilesize = ftell(pFile3);
    fseek(pFile3,0,SEEK_SET);
    fclose(pFile3);

    pInfo3->m_pFile = 0;
    lstFileinfo.push_back(pInfo3);

}

