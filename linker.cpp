#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <string.h>
#include<iomanip>
#include <math.h> 
using namespace std;

static fstream inFile;
// fstream outFile;
static char delimit[]=" \t\n";

static int END = -1;
static string EMPTY = "";
static string sERROR = "";
static int iERROR = -1;
static string SUCCESS = "success";
static int MAX_SYM = 16;
static int MAX_DEF_USE = 16;
static int MAX_INSTR = 512;


static int moduleOffset;
static int linenum;
static int lineoffset;
static string currentLine;

class Symbol{
    private:
        string name;
        int value;
        int errorNum;
        int modNum;
        int numDef;

    public:
        Symbol(string n, int val, int mod){
            name = n;
            value = val + moduleOffset;
            errorNum = -1;
            modNum = mod;
            numDef = 1;
        }

        string getName(){
            return name;
        }

        int getValue(){
            return value;
        }
        void setValue(int v){
            value = v;
        }

        int getErrorNum(){
            return errorNum;
        }
        void setErrorNum(int num){
            errorNum = num;
        }

        int getModNum(){
            return modNum;
        }

        bool ifUsed(){
            if(numDef>0){
                return false;
            }
            return true;
        }
        void setUsed(){
            numDef--;
        }
        void setDef(){
            numDef++;
        }
};

static vector<Symbol> symbolTable;

void parseError(int errcode) {
    string errstr[] = {
        "NUM_EXPECTED", // Number expect, anything >= 2^30 is not a number either
        "SYM_EXPECTED", // Symbol Expected
        "ADDR_EXPECTED", // Addressing Expected which is A/E/I/R
        "SYM_TOO_LONG", // Symbol Name is too long
        "TOO_MANY_DEF_IN_MODULE", // > 16
        "TOO_MANY_USE_IN_MODULE", // > 16
        "TOO_MANY_INSTR", // total num_instr exceeds memory size (512)
    };
    // string msg = "Parse Error line "+ to_string(linenum) + " offset "+ to_string(lineoffset) +": "+ errstr[errcode];
    cout << "Parse Error line " << to_string(linenum) << " offset " << to_string(lineoffset+1) << ": "<< errstr[errcode]<< endl;
    
    // printf("Parse Error line %d offset %d: %s\n", linenum, lineoffset, errstr[errcode]);
    // return msg;
}

void errorMsg(int ruleNum, string symName="") {
    map<int, int> ruleNum2Index;
    ruleNum2Index[2] = 0;
    ruleNum2Index[3] = 1;
    ruleNum2Index[6] = 2;
    ruleNum2Index[8] = 3;
    ruleNum2Index[9] = 4;
    ruleNum2Index[10] = 5;
    ruleNum2Index[11] = 6;

    string errstr[] = {
        "This variable is multiple times defined; first value used", // rule 2
        symName + " is not defined; zero used", // rule 3
        "External address exceeds length of uselist; treated as immediate", // rule 6
        "Absolute address exceeds machine size; zero used", // rule 8
        "Relative address exceeds module size; zero used", // rule 9
        "Illegal immediate value; treated as 9999", // rule 10
        "Illegal opcode; treated as 9999", // rule 11
    };
    // string msg = "Error: " + errstr[ruleNum2Index[ruleNum]];
    cout << "Error: " << errstr[ruleNum2Index[ruleNum]] << endl;

    // printf("Error: %s", errstr[ruleNum2Index[ruleNum]]);

    // return msg;
}

void warningMsg(int ruleNum, int moduleNum, string symName, int size=0, int max=0) {

    string msg;
    switch (ruleNum)
    {
    case 4:
        cout << "Warning: Module "<< to_string(moduleNum) <<": "<< symName << " was defined but never used" << endl;
        break;
    case 5:
        cout << "Warning: Module " << to_string(moduleNum) << ": " << symName << " too big " << to_string(size) <<" (max="<< to_string(max) <<") assume zero relative" << endl;
        break;
    case 7:
        cout << "Warning: Module " << to_string(moduleNum) << ": " << symName << " appeared in the uselist but was not actually used" << endl;
        break;
    default:
        // printf("ERROR Warning number, please check.");
        cout << "ERROR Warning number, please check." << endl;
        break;
    }   
}

void offsetUpdate(char * token){
    if (lineoffset == 0){
        lineoffset = currentLine.find(token);
        return;
    } 

    string substring = currentLine.substr(lineoffset);

    size_t min = substring.length();
    for (char c: delimit){
        size_t index = substring.find(c);
        if (index != string::npos && index < min) {
            min = index;   
        }
    }
    string newSubstring = substring.substr(min);
    lineoffset += newSubstring.find(token)+min;
}

string getToken(){
    char * token = strtok(NULL, delimit);
    while(token == nullptr){
        string lastLine = currentLine;
        if (getline(inFile, currentLine)){
            linenum +=1;
            lineoffset = 0;
            char *line = new char[currentLine.length()+1];
            strcpy(line, currentLine.c_str()); 
            token = strtok(line, delimit);
        }else{
            // EOF
            lineoffset = lastLine.length();
            return EMPTY;
        }
    }
    offsetUpdate(token);
    return string(token);
}

int readInt(bool moduleStart=false){
    string token = getToken();
    if (token == EMPTY ){
        if(!moduleStart){
            // syntax: "NUM_EXPECTED"
            // outFile << parseError(0) << "\n";
            parseError(0);
            return iERROR;
        }else{
            // EOF
            return END;
        }  
    }

    for (char c : token){
        if(!isdigit(c)){
            //  syntax: "NUM_EXPECTED"
            // outFile << parseError(0) << "\n";
            parseError(0);
            return iERROR;
        }
    }

    if (stoi(token)>= pow(2,30)){
        //  syntax: "NUM_EXPECTED"
        parseError(0);
        return iERROR;
    }
    return stoi(token);
}

string readSymbol(){
    string token = getToken();
    if (token == EMPTY){
        // syntax: "SYM_EXPECTED"
        // outFile << parseError(1) << "\n";
        parseError(1);
        return sERROR;
    }
    int firstAl = -1;
    int index = 0;
    for (char c : token){
        if(firstAl==-1 && isalpha(c)){
            firstAl = index;
        }
        if(!isalnum(c)){
            // syntax: "SYM_EXPECTED"
            // outFile << parseError(1) << "\n";
            parseError(1);
            return sERROR;
        }
        index++;
    }

    if(firstAl != 0){
       // syntax: "SYM_EXPECTED"
        parseError(1);
        return sERROR; 
    }

    if (token.length() > MAX_SYM){
        // syntax: "SYM_TOO_LONG"
        // outFile << parseError(3) << "\n";
        parseError(3);
        return sERROR;
    }

    return token;
}

string readIAER(){
    string token = getToken();
    if (token == EMPTY){
        // syntax: "ADDR_EXPECTED"
        // outFile << parseError(2) << "\n";
        parseError(2);
        return sERROR;
    }
    char c = token[0];
    if(c != 'I' && c != 'A' && c != 'E' && c != 'R'){
        // syntax: "ADDR_EXPECTED"
        // outFile << parseError(2) << "\n";
        parseError(2);
        return sERROR;
    }
    return token;
}

void updateSymbol(string name, int value, int modNum){
    for(Symbol &sym: symbolTable){
        if (sym.getName() == name){
            // rule 2 symbol defined multiple times
            // Symbol curSym(name,sym.getValue(),modNum);
            // curSym.setErrorNum(2);
            // symbolTable.push_back(curSym);
            sym.setDef();
            sym.setErrorNum(2);
            return;
        }
    }
    symbolTable.push_back(Symbol(name,value,modNum));
}

void checkModule(int modulenum, int size, int modOffset, vector<string> tempSym){
    for(string name: tempSym){
        // rule 5
        for(Symbol &sym: symbolTable){
            if(sym.getName() == name && sym.getValue()-modOffset > size){
                warningMsg(5, modulenum, sym.getName(), sym.getValue()-modOffset, size);
                sym.setValue(modOffset);
            }
        }    
    }
}

string pass1(){
    moduleOffset = 0;
    int moduleNum = 1;
    int numIst = 0;
    while (true) {
        // read deflist
        vector<string> tempSym;
        int defcount = readInt(true);
        if (defcount == END)
        { 
            // end of file
            break;
        } 

        if (defcount == iERROR)
        { 
            // syntax: "NUM_EXPECTED"
            return sERROR;
        } 

        if (defcount > MAX_DEF_USE){
            // syntax: "TOO_MANY_DEF_IN_MODULE"
            // outFile << parseError(4) << "\n";
            parseError(4);
            return sERROR;
        }

        for (int i=0;i<defcount;i++) {
            string sym = readSymbol();
            if (sym == sERROR){
                // syntax: "SYM_EXPECTED"
                return sERROR;
            }

            int val = readInt();
            if (val == iERROR){
                // syntax: "NUM_EXPECTED"
                return sERROR;
            }
            tempSym.push_back(sym);
            updateSymbol(sym, val, moduleNum);
        }

        // read uselist
        
        int usecount = readInt(); 
        if (usecount == iERROR)
        { 
            // syntax: "NUM_EXPECTED"
            return sERROR;
        }
        
        if (usecount > MAX_DEF_USE){
            // syntax: "TOO_MANY_USE_IN_MODULE"
            // outFile << parseError(5) << "\n";
            parseError(5);
            return sERROR;           
        }

        for (int i=0;i<usecount;i++) {
            string sym = readSymbol(); // change in pass2 
            if (sym == sERROR){
                // syntax: "SYM_EXPECTED"
                return sERROR;
            }
        }

        // read insturtion 
        int instcount = readInt();
        if (instcount == iERROR)
        { 
            // syntax: "NUM_EXPECTED"
            return sERROR;
        }
        numIst += instcount;
        if (numIst > MAX_INSTR){
            // syntax: "TOO_MANY_INSTR"
            // outFile << parseError(6) << "\n";
            parseError(6);
            return sERROR;           
        }

        for (int i=0;i<instcount;i++) { 
            string addressmode = readIAER();
            if(addressmode == sERROR){
                // syntax: "ADDR_EXPECTED"
                return sERROR;
            }
            int operand = readInt(); // change in pass2
            if (operand == iERROR)
            { 
                // syntax: "NUM_EXPECTED"
                return sERROR;
            }
        }

        // Module check 
        checkModule(moduleNum, instcount-1, moduleOffset, tempSym);

        moduleNum +=1;
        moduleOffset += instcount;
    }
    return SUCCESS;
}

void printSymTable(){
    printf("Symbol Table\n");
    for (Symbol sym: symbolTable){
        // outFile << sym.name << "=" << to_string(sym.value);
        cout << sym.getName() << "=" << to_string(sym.getValue());
        if (sym.getErrorNum() != -1){
            // printf("%s=%d",sym.name.c_str(), sym.value);
            cout << " ";
            errorMsg(2);
        }else{
            // printf("%s=%d\n",sym.name.c_str(), sym.value);
            cout << endl;
        }
    }
}

// string getOutFilePath(string inFilePath){
//     string inFileName = inFilePath.substr(inFilePath.find_last_of("/") + 1);
//     string outFileName = "out" + inFileName.substr(inFileName.find_last_of("-"));
    
//     string directory;
//     const size_t lastSlashIdx = inFilePath.rfind('/');
//     if (string::npos != lastSlashIdx)
//     {
//         directory = inFilePath.substr(0, lastSlashIdx);
//     }

//     return directory + '/' + outFileName;
// }

void printMemoryMap(int index, int opcode, int map, string symName=""){
    cout << setw(3) << setfill('0') << index << ": " << opcode << setw(3) << setfill('0') << map;
    if (symName == ""){
        return;
    }

    for(Symbol &sym: symbolTable){
        if(sym.getName() == symName){
            sym.setUsed();
            break;
        }
    }
}

void getMemoryMap(int index, int moduleOffset, int modSize, string mode, int op, vector<string> symbols, vector<int> &used){
    int map = 0;
    int opcode = op/1000;
    int oprand = op%1000;
    
    switch (mode[0])
    {
    case 'R':
    {
        if(opcode>= 10){
            // rule 11
            printMemoryMap(index, 9, 999);
            cout << " ";
            errorMsg(11);
            return;
        }

        if(oprand > modSize){
            // rule 9
            printMemoryMap(index, opcode, moduleOffset);
            cout<< " ";
            errorMsg(9);
            return;
        }
        printMemoryMap(index, opcode, moduleOffset+oprand);
        cout << endl;
        break;
    }
    case 'E':
    {
        if(opcode>= 10){
            // rule 11
            printMemoryMap(index, 9, 999);
            cout << " ";
            errorMsg(11);
            return;
        }

        // rule 6
        if (oprand >= symbols.size()){
            printMemoryMap(index, opcode, oprand);
            cout << " ";
            errorMsg(6);
            return;
        }
        string symName = symbols[oprand];
        for(Symbol sym: symbolTable){
            if (sym.getName() == symName){
                map = sym.getValue();
                printMemoryMap(index, opcode, map, symName);
                used[oprand] = 1;
                cout << endl;
                return;
            }
        }
        // rule 3
        printMemoryMap(index, opcode, 0, symName);
        used[oprand] = 1;
        cout << " ";
        errorMsg(3, symName);
        break;
    }
    case 'I':
        if (op >= 10000){
            // rule 10
            printMemoryMap(index, 9, 999);
            cout << " ";
            errorMsg(10);
            return;
        }

        printMemoryMap(index, opcode, oprand);
        cout << endl;
        break;

    case 'A':
        if(opcode>= 10){
            // rule 11
            printMemoryMap(index, 9, 999);
            cout << " ";
            errorMsg(11);
            return;
        }

        if(oprand > MAX_INSTR){
            // rule 8
            printMemoryMap(index, opcode, 0);
            cout << " ";
            errorMsg(8);
            return; 
        }
        printMemoryMap(index, opcode, oprand);
        cout << endl;
        break;

    default:
        break;
    }
}

void pass2ModuleCheck(vector<string> syms, vector<int> &used, int modNum){
    int index = 0;
    for(int &i : used){
        if(i == 0){
            // rule 7
            i = 1;
            warningMsg(7, modNum, syms[index]);
        }
        index++;
    }
    
}

string pass2(){
    moduleOffset = 0;
    int moduleNum = 1;
    int numIst = 0;

    int index = 0;

    while (true) {
        // read deflist
        int defcount = readInt(true);
        if (defcount == END)
        { 
            // end of file
            break;
        } 

        for (int i=0;i<defcount;i++) {
            string sym = readSymbol();
            int val = readInt();
        }

        // read uselist
        int usecount = readInt(); 

        vector<string> symbols;
        vector<int> used;
        for (int i=0;i<usecount;i++) {
            symbols.push_back(readSymbol()); // change in pass2 
            used.push_back(0);
        }

        // read insturtion 
        int instcount = readInt();
        numIst += instcount;

        for (int i=0;i<instcount;i++) { 
            string addressmode = readIAER();
            int operand = readInt(); // change in pass2
            getMemoryMap(index, moduleOffset, instcount, addressmode, operand, symbols, used);
            index++;
        }

        // Module check 
        pass2ModuleCheck(symbols,used, moduleNum);

        moduleNum +=1;
        moduleOffset += instcount;
     
    }
    return SUCCESS;
}

void finalCheck(){
    cout<<endl;
    for(Symbol sym: symbolTable){
        if (!sym.ifUsed()){
            // rule 4 
            warningMsg(4,sym.getModNum(), sym.getName());
        }
    }
}

int main(int argc, char **argv){
    string inFilePath = argv[1];
    inFile.open(inFilePath);
    // string outFilePath = getOutFilePath(inFilePath);
    // outFile.open(outFilePath);

    linenum = 0;
    lineoffset = 0;
    if(pass1() != SUCCESS){
        return 0;
    }

    printSymTable();

    printf("\nMemory Map\n");
    inFile.close();
    inFile.open(inFilePath);
    pass2();
    finalCheck();
    cout<<endl;
}