#include "iostream"
#include "string"
#include <string.h>
#include "vector"
#include "sstream"
#pragma pack(1) // 按1字节对齐
using namespace std;

typedef unsigned char u8; //1字节
typedef unsigned short u16;	//2字节
typedef unsigned int u32; //4字节

extern "C" {
    void asm_print(const char *, const int);
}

void print_asm(const char * str){
    asm_print(str, strlen(str));
}

int BytsPerSec; //每扇区字节数 512
int SecPerClus; //每簇扇区数 1
int RsvdSecCnt; //Root记录占用多少扇区1
int NumFATs; //共有多少FAT表2
int RootENtCnt; //根目录文件数最大值224
int FATSz; //每FAT扇区数9

int fat1Base; //FAT1区域起始字节数512
int rootDirBase; //根目录区起始字节数9728
int dataBase; //数据区起始字节数16896

class ListNode{
public:
    string name;
    string path;
    vector<ListNode*> children;
    ListNode *father = nullptr;
    u32 size; // 文件大小
    int  subFileNum = 0;
    int  subDirNum = 0;
    bool isFile = false;
    char *content = new char[10000];
   ListNode(){};

   ListNode(string name, string path){
        this->name = name;
        this->path = path;
   };

   ListNode(string name, string path, u32 size, bool isFile){
        this->name = name;
        this->path = path;
        this->size = size;
        this->isFile = isFile;
   }

   void addChild(ListNode *child, bool isFile){
        this->children.push_back(child);
        if(isFile){
            this->subFileNum+=1;
        }else{
            this->subDirNum+=1;
        }
   };

};

class BPB{ //引导扇区中的BPB数据结构，共25字节
    u16 BPB_BytsPerSec; //每扇区字节数
    u8 BPB_SecPerClus;  //每簇扇区数
    u16 BPB_RsvdSecCnt; //Boot记录占用的扇区数
    u8 BPB_NumFATs; //共有多少FAT表
    u16 BPB_RootEntCnt; //根目录文件数最大值
    u16 BPB_TotSec16; //扇区总数
    u8 BPB_Media; //介质描述符
    u16 BPB_FATSz16; //每FAT扇区数
    u16 BPB_SecPerTrk; //每磁道扇区数
    u16 BPB_NumHeads; //磁头数
    u32 BPB_HiddSec; //隐藏扇区数
    u32 BPB_TotSec32; //如果BPB_TotSec16是0，由这个值记录扇区数
public:
    BPB(){};
    void load(FILE *fatFile){
        fseek(fatFile, 11, SEEK_SET); //将指针移到文件开头，偏移量为11
        fread(this, 1, 25, fatFile);
        BytsPerSec = this->BPB_BytsPerSec;
        SecPerClus = this->BPB_SecPerClus;
        RsvdSecCnt = this->BPB_RsvdSecCnt;
        NumFATs = this->BPB_NumFATs;
        RootENtCnt = this->BPB_RootEntCnt;
        FATSz = this->BPB_FATSz16;
        fat1Base = RsvdSecCnt * BytsPerSec;
        rootDirBase = (RsvdSecCnt + NumFATs * FATSz) * BytsPerSec;
        int rootDirSectors = (RootENtCnt * 32 + BytsPerSec -1) / BytsPerSec;
        dataBase = (RsvdSecCnt + NumFATs  * FATSz + rootDirSectors) * BytsPerSec;
    };
};

// 根目录条目
class RootDirEntry{
public:
    u8 DIR_NAME[11];  //文件名
    u8 DIR_Attr; //文件属性
    u8 reserve[10]; //保留位
    u16 DIR_WrtTime; //最后一次写入时间
    u16 DIR_WrtDate; //最后一次写入日期
    u16 DIR_FstClus; //此条目对应的开始簇号
    u32 DIR_FileSize; //文件大小

    RootDirEntry(){};

    bool isNameEmpty(){
        if (this->DIR_NAME[0] == '\0'){
            return true;
        }else{
            return false;
        }
    }

    bool isFile(){ //0x20表示文件
        if(this->DIR_Attr == 0x20){
            return true;
        }else{
            return false;
        }
    }

    bool isDir(){ //0x10表示文件夹
        if(this->DIR_Attr == 0x10){
            return true;
        }else{
            return false;
        }
    }

    void getRealFileName(char *realName){
        int index = 0;
        for (int i=0; i<8; i++){
            if(this->DIR_NAME[i] == ' '){
                break;
            }
            realName[index] = this->DIR_NAME[i];
            index++;
        }
        realName[index] = '.';
        index++;
        for(int i=8 ;i<11; i++){
            realName[index] = this->DIR_NAME[i];
            index++;
        }
        realName[index] = '\0';
    }

    void getRealDirName(char* realName){
        int index = 0;
        for(int i=0; i<11; i++){
            if(this->DIR_NAME[i] == ' '){
                break;
            }else{
                realName[index] = this->DIR_NAME[i];
                index++;
            }
        }
        realName[index] = '\0';
    }

    void load(FILE *fatFile, ListNode *root);

};

int getNextClusNum(FILE *fatFile, int currentClusNum){
    int begin = RsvdSecCnt * BytsPerSec + currentClusNum * 3/2;
    bool flag = (currentClusNum%2 == 0);
    u16 fatEntry;
    u16 *ptr = &fatEntry;
    fseek(fatFile, begin, SEEK_SET);
    fread(ptr, 1, 2, fatFile);
    if(flag){
        fatEntry = fatEntry << 4; //这里移位是为什么？
    }
    return fatEntry >> 4 ;
}

void getContent(ListNode *node, FILE *fatFile, int startClusNum){
    char *head = node->content;
    int start = 0;
    while(true){
        int beginByte = dataBase + (startClusNum - 2) * BytsPerSec * SecPerClus;
        int size = BytsPerSec * SecPerClus;
        char *temp = new char[size];
        char *s = temp;
        fseek(fatFile, beginByte , SEEK_SET);
        fread(s, 1, size, fatFile);
        for(int i=0; i<size ; i++){
            head[start++] = s[i];
        }
        delete[] temp; //不然会导致内存占用过大
        startClusNum = getNextClusNum(fatFile, startClusNum); 
        if(startClusNum == 0xFF7){
            print_asm("Bad cluster!\n");
            break;
        }
        if(startClusNum >= 0xFF8){
            break;
        }   
    }
}

void readChildren(FILE *fatFile, ListNode *root, int startClusNum){
    while(true){
        int beginByte = dataBase + (startClusNum - 2) * BytsPerSec * SecPerClus;
        int begin = 0;
        int max = SecPerClus * BytsPerSec;
        while(begin<max){
            RootDirEntry *rootDirEntry = new RootDirEntry();
            fseek(fatFile, beginByte + begin, SEEK_SET);
            fread(rootDirEntry, 1, 32, fatFile);
            char *realName = new char[12];
            if(rootDirEntry->isFile()){
                string path = root->path + root->name;
                rootDirEntry->getRealFileName(realName);
                ListNode *node = new ListNode(realName, path, rootDirEntry->DIR_FileSize, true);
                root->addChild(node, true);
                node->father = root;
                if(rootDirEntry->DIR_FileSize!=0){
                    getContent(node, fatFile, rootDirEntry->DIR_FstClus);
                }
            }
            if(rootDirEntry->isDir()){
                string path = root->path + root->name + "/";
                rootDirEntry->getRealDirName(realName);
                ListNode *node = new ListNode(realName, path, rootDirEntry->DIR_FileSize, false);
                root->addChild(node, false);
                node->father = root;
                if(node->name != "." and node->name != ".."){
                    readChildren(fatFile, node, rootDirEntry->DIR_FstClus);
                }
            }
            begin += 32;
        }
        startClusNum = getNextClusNum(fatFile, startClusNum); 
        if(startClusNum == 0xFF7){
            print_asm("Bad cluster!\n");
            break;
        }
        if(startClusNum >= 0xFF8){
            break;
        }
    }
}

void RootDirEntry::load(FILE *fatFile, ListNode *root){
    int base = rootDirBase;
    for(int i = 0; i <= RootENtCnt; i++){
        fseek(fatFile, base, SEEK_SET);
        fread(this, 1, 32, fatFile);
        base += 32;
        char *realName = new char[12];
        if(this->isFile()){
            this->getRealFileName(realName);
            ListNode *node = new ListNode(realName, root->path, this->DIR_FileSize, true);
            root->addChild(node, true);
            node->father = root;
            if(this->DIR_FileSize!=0){
                getContent(node, fatFile, this->DIR_FstClus);
            }
        }
        if(this->isDir()){
            this->getRealDirName(realName);
            ListNode *node = new ListNode(realName, root->path, this->DIR_FileSize, false);
            root->addChild(node, false);
            node->father = root;
            readChildren(fatFile, node, this->DIR_FstClus);
        }
    }
}

void LSWithoutParameter(ListNode *root){
    // cout<<root->path<<root->name;
    print_asm(root->path.c_str());
    print_asm(root->name.c_str());
    if(root->name != ""){
        print_asm("/");
    }
    print_asm(":\n");
    for(int i=0; i<root->children.size(); i++){
        if(root->children.at(i)->isFile){
            // cout<<root->children.at(i)->name<<"  ";
            print_asm(root->children.at(i)->name.c_str());
            print_asm(" ");
        }else{
            // cout << "\033[31m" << root->children.at(i)->name << "\033[0m"<<"  ";
            print_asm("\033[31m");
            print_asm(root->children.at(i)->name.c_str());
            print_asm("\033[0m");
            print_asm(" ");
        }
    }
    print_asm("\n");
    for(int i=0; i<root->children.size(); i++){
        if(root->children.at(i)->name != "." and root->children.at(i)->name != ".."){
            if(!root->children.at(i)->isFile){
                LSWithoutParameter(root->children.at(i));
            }
        }
    }
}

void LSWithParameter(ListNode *root){
    // cout<<root->path<<root->name;
    print_asm(root->path.c_str());
    print_asm(root->name.c_str());
    if(root->name != ""){ //不是根目录
        // cout<<"/"<<" "<<root->subDirNum-2<<" "<<root->subFileNum; //不是根目录的话要减去.和..的个数
        print_asm("/ ");
        print_asm((to_string(root->subDirNum-2).c_str()));
        print_asm(" ");
        print_asm(to_string(root->subFileNum).c_str());
    }else{
        // cout<<" "<<root->subDirNum<<" "<<root->subFileNum;
        print_asm(" ");
        print_asm(to_string(root->subDirNum).c_str());
        print_asm(" ");
        print_asm(to_string(root->subFileNum).c_str());
    }
    print_asm(":\n");
    for(int i=0; i<root->children.size(); i++){
        if(root->children.at(i)->isFile){
            // cout<<root->children.at(i)->name<<"  "<<root->children.at(i)->size<<endl;
            print_asm((root->children.at(i)->name).c_str());
            print_asm(" ");
            print_asm((to_string(root->children.at(i)->size)).c_str());
            print_asm("\n");
        }else{
            // cout << "\033[31m" << root->children.at(i)->name << "\033[0m"<<"  ";
            print_asm("\033[31m");
            print_asm(root->children.at(i)->name.c_str());
            print_asm("\033[0m ");
            print_asm(" ");
            if(root->children.at(i)->name!="." and root->children.at(i)->name!=".."){
                // cout<<root->children.at(i)->subDirNum-2<<" "<<root->children.at(i)->subFileNum<<endl;
                print_asm(to_string(root->children.at(i)->subDirNum-2).c_str());
                print_asm(" ");
                print_asm(to_string(root->children.at(i)->subFileNum).c_str());
                print_asm("\n");
            }else{
                print_asm("\n");
            }
        }
    }
    print_asm("\n");
    for(int i=0; i<root->children.size(); i++){
        if(root->children.at(i)->name != "." and root->children.at(i)->name != ".."){
            if(!root->children.at(i)->isFile){
                LSWithParameter(root->children.at(i));
            }
        }
    }
}

ListNode* findNodeByPath(ListNode *root, string dirPath){
    if(dirPath == "/"){
        return root;
    }
    int length = dirPath.size();
    if(dirPath.at(length-1) == '/'){
        dirPath = dirPath.substr(0, length-1);
    }
    vector<int> index;
    for(int i=0; i<dirPath.length(); i++){
        if(dirPath.at(i) == '/'){
            index.push_back(i);
        }
    }
    ListNode *temp = root;
    for(int i=0; i<index.size(); i++){
        int begin = index.at(i);
        int end;
        if( i == index.size()-1 ){
            end = dirPath.length();
        }else{
            end = index.at(i+1);
        }
        string subString = dirPath.substr(begin+1, end-begin-1);
        if(subString == "."){
            if(i == index.size() - 1) return temp;
            continue;
        }else if(subString == ".."){
            if(temp->father != nullptr){ //根目录的父目录还是自己
                temp = temp->father;
            }
            if(i == index.size() - 1) return temp;
        }
        for(int  j = 0; j<temp->children.size(); j++){
            if(temp->children.at(j)->name == subString){
                temp = temp->children.at(j);
                if( i == index.size() - 1) return temp;
                break;
            }
        }
    }
    return nullptr;
}

int main(){
    FILE *fatFile;
    fatFile = fopen("/home/huyuling/桌面/vscode projects/lab2/a.img", "rb");
    BPB *bpb = new BPB();
    bpb->load(fatFile);
    ListNode *root = new ListNode("", "/");
    RootDirEntry *rootDirEntry = new RootDirEntry();
    rootDirEntry->load(fatFile, root);
   
    string commend;
    while(true){
        print_asm(">");
        getline(cin, commend);
        
        // parse commend
        istringstream record(commend);
        vector<string> str;
        string word;
        bool is_l = false;
        while (record >> word){
            str.push_back(word); //将commend按空格分开
        }
        bool isValid = true;
        int sum = 0;
        string dirPath = "";
        // 是否需要-ls；是否有路径参数
        for(int i=0; i<str.size(); i++){
            if(str.at(i).at(0) == '-'){
                string s = str.at(i);
                if((s.at(1) == 'l' and s.length() == 2) or (s.substr(1,2) == "ll" and s.length() == 3) or (s.substr(1,3) == "lll" and s.length() == 4) or (s.substr(1,4) == "llll" and s.length() == 5)){
                    is_l = true;
                    isValid = true;
                }else isValid = false;
            }
            // if(str.at(i).at(0) == '/' or str.at(i).find(".")!=string::npos){
            //     sum += 1;
            //     dirPath = str.at(i);
            // }
            if(str.at(i) != "ls" and str.at(i) !="cat" and str.at(i)!="-l" and str.at(i)!="-ll" and str.at(i)!="-lll" and str.at(i)!="-llll" and str.at(i).find("-") == string::npos){
                sum += 1;
                dirPath = str.at(i);
            }
        }
        if(commend == "exit"){
            break;
        }else if(sum > 1 or !isValid){
            print_asm("Invalid command!\n");
        }else if(commend == "ls"){
            LSWithoutParameter(root); // ls的情况
            // cout<<endl;
            print_asm("\n");
        }else if(str.at(0) == "ls" and !is_l and dirPath!=""){ //ls 路径名的情况
            if(dirPath.at(0) != '/') dirPath = "/" + dirPath;
            ListNode *thisNode = findNodeByPath(root, dirPath);
            if(thisNode == nullptr){
                print_asm("This path does not exist!\n");
            }else if(thisNode->isFile){
                print_asm("This is not a directory path!\n");
            }else{
                LSWithoutParameter(thisNode);
                print_asm("\n");
            }
        }else if(str.at(0) == "ls" and is_l and dirPath == ""){// ls -l的情况
            LSWithParameter(root);
        }else if(str.at(0) == "ls" and is_l and dirPath!=""){// ls -l 路径名
            if(dirPath.at(0) != '/') dirPath = "/" + dirPath;
            ListNode *thisNode = findNodeByPath(root, dirPath);
            if(thisNode == nullptr){
                print_asm("This path does not exist!\n");
            }else if(thisNode->isFile){
                print_asm("This is not a directory path!\n");
            }else{
                LSWithParameter(thisNode);
            }
        }else if(str.at(0) == "cat"){ // cat 路径名
            // ListNode *thisNode = nullptr;
            string filePath = str.at(1);
            // if(filePath.find("/") == string::npos){
            //     for(int k = 0; k<root->children.size(); k++){
            //         if(filePath == root->children.at(k)->name){
            //             if(!root->children.at(k)->isFile){
            //                 print_asm("This is not a file path!\n");
            //             }else{
            //                 thisNode = root->children.at(k);
            //                 break;
            //             }
            //         }
            //     }
            // }else{
            //     thisNode = findNodeByPath(root, dirPath);
            // }
            if(filePath.at(0) != '/') filePath = "/" + filePath;
            ListNode *thisNode = findNodeByPath(root, filePath);
            if(thisNode == nullptr){
                print_asm("This path does not exist!\n");
            }else if(!thisNode->isFile){
                print_asm("This is not a file path!\n");
            }else{
                print_asm(thisNode->content);
                print_asm("\n");
            }
        }else{
            print_asm("Invalid command!\n");
        }
    }
    fclose(fatFile);
}
