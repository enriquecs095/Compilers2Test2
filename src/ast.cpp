#include "ast.h"
#include <iostream>
#include <sstream>
#include <set>
#include "asm.h"

const char *floatTemps[] = {"$f0",
                            "$f1",
                            "$f2",
                            "$f3",
                            "$f4",
                            "$f5",
                            "$f6",
                            "$f7",
                            "$f8",
                            "$f9",
                            "$f10",
                            "$f11",
                            "$f12",
                            "$f13",
                            "$f14",
                            "$f15",
                            "$f16",
                            "$f17",
                            "$f18",
                            "$f19",
                            "$f20",
                            "$f21",
                            "$f22",
                            "$f23",
                            "$f24",
                            "$f25",
                            "$f26",
                            "$f27",
                            "$f28",
                            "$f29",
                            "$f30",
                            "$f31"};

#define FLOAT_TEMP_COUNT 32
set<string> intTempMap;
set<string> floatTempMap;
int labelCounter = 0;
extern Asm assemblyFile;

map<string, int> codeGenerationVars;

int globalStackPointer = 0;

void releaseFloatTemp(string temp)
{
    floatTempMap.erase(temp);
}

void releaseRegister(string temp)
{
    releaseFloatTemp(temp);
}

string getNewLabel(string prefix)
{
    stringstream ss;
    ss << prefix << labelCounter;
    labelCounter++;
    return ss.str();
}

string saveState()
{
    stringstream ss;
    ss << "sw $ra, " << globalStackPointer << "($sp)" << endl;
    globalStackPointer += 4;
    return ss.str();
}

string retrieveState(string state)
{

    std::string::size_type n = 0;
    string s = "sw";
    while ((n = state.find(s, n)) != std::string::npos)
    {
        state.replace(n, s.size(), "lw");
        n += 2;
        globalStackPointer -= 4;
    }
    return state;
}

string getFloatTemp()
{

    for (int i = 0; i < FLOAT_TEMP_COUNT; i++)
    {
        if (floatTempMap.find(floatTemps[i]) == floatTempMap.end())
        {
            floatTempMap.insert(floatTemps[i]);
            return string(floatTemps[i]);
        }
    }
    cout << "No more float registers!" << endl;
    return "";
}

string Arithmetic(Code &leftCode, Code &rightCode, Code &code, char op)
{
    stringstream ss;
    code.place = getFloatTemp();
    switch (op)
    {
    case '-':
        ss << "sub.s " << code.place << ", " << leftCode.place << ", " << rightCode.place;
        break;
    case '/':
        ss << "div.s " << code.place << ", " << leftCode.place << ", " << rightCode.place;
        break;
    default:
        break;
    }
    return ss.str();
}

void FloatExpr::genCode(Code &code)
{
    string floatTemp = getFloatTemp();
    code.place = floatTemp;
    stringstream ss;
    ss << "li.s " << floatTemp << ", " << this->number << endl;
    code.code = ss.str();
}

void SubExpr::genCode(Code &code)
{
    Code leftCode, rightCode;
    stringstream ss;
    this->expr1->genCode(leftCode);
    this->expr2->genCode(rightCode);
    releaseRegister(leftCode.place);
    releaseRegister(rightCode.place);
    ss << leftCode.code << endl
       << rightCode.code << endl
       << Arithmetic(leftCode, rightCode, code, '-') << endl;
    code.code = ss.str();
}

void DivExpr::genCode(Code &code)
{
    Code leftCode, rightCode;
    stringstream ss;
    this->expr1->genCode(leftCode);
    this->expr2->genCode(rightCode);
    releaseRegister(leftCode.place);
    releaseRegister(rightCode.place);
    ss << leftCode.code << endl
       << rightCode.code << endl
       << Arithmetic(leftCode, rightCode, code, '/') << endl;
    code.code = ss.str();
}

void IdExpr::genCode(Code &code)
{
    stringstream ss;
    string floatTemp = getFloatTemp();
    code.place = floatTemp;
    ss << "l.s " << floatTemp << ", " << codeGenerationVars[this->id] << "($sp)" << endl;
    code.code = ss.str();
}

string ExprStatement::genCode()
{
    Code exprCode;
    this->expr->genCode(exprCode);
    releaseFloatTemp(exprCode.place);
    return exprCode.code;
}

string IfStatement::genCode()
{
    string endIfLabel = getNewLabel("endif");
    Code exprCode;
    this->conditionalExpr->genCode(exprCode);
    stringstream code;
    code << exprCode.code << endl;
    code << "bc1f " << endIfLabel << endl;
    list<Statement *>::iterator itd = this->trueStatement.begin();
    while (itd != this->trueStatement.end())
    {
        Statement *stmt = *itd;
        if (stmt != NULL)
        {
            code << stmt->genCode() << endl;
        }
        itd++;
    }
    list<Statement *>::iterator iterator = this->falseStatement.begin();
    while (iterator != this->falseStatement.end())
    {
        Statement *stmt = *iterator;
        if (stmt != NULL)
        {
            code << stmt->genCode() << endl;
        }
        iterator++;
    }
    code << endIfLabel << " :" << endl;
    releaseFloatTemp(exprCode.place);
    return code.str();
}

void MethodInvocationExpr::genCode(Code &code)
{ /////falta

    list<Expr *>::iterator it = this->expressions.begin();
    list<Code> codes;
    stringstream ss;
    Code argCode;
    while (it != this->expressions.end())
    {
        (*it)->genCode(argCode);
        ss << argCode.code << endl;
        codes.push_back(argCode);
        it++;
    }
    int i = 0;
    list<Code>::iterator placesIt = codes.begin();
    while (placesIt != codes.end())
    {
        releaseRegister((*placesIt).place);
        ss << "mfc1 $a" << i << ", " << (*placesIt).place << endl;
        i++;
        placesIt++;
    }
    ss << "jal " << this->id << endl;
    string reg;
    reg = getFloatTemp();
    ss << "mfc1 $v0, " << reg << endl;
    code.code = ss.str();
    code.place = reg;
}

string AssignationStatement::genCode()
{

    Code rightSideCode;
    Code exprCode;
    stringstream ss;
    this->value->genCode(rightSideCode);
    ss << rightSideCode.code << endl;
    string name = this->id;
    list<Expr *>::iterator it = this->expressions.begin();
    while (it != this->expressions.end())
    {
        (*it)->genCode(exprCode);
        ss << exprCode.code << endl;
        releaseFloatTemp(exprCode.place);
        it++;
    }
    if (name != "")
    {
        codeGenerationVars[name]=globalStackPointer;
        ss << "s.s " << rightSideCode.place << ", " << codeGenerationVars[name] << "($sp)" << endl;
        globalStackPointer+=4;
        releaseRegister(rightSideCode.place);
      
    }
    releaseFloatTemp(rightSideCode.place);
    return ss.str();
}

void GteExpr::genCode(Code &code)
{

    Code leftSideCode;
    Code rightSideCode;
    stringstream ss;
    this->expr1->genCode(leftSideCode);
    this->expr2->genCode(rightSideCode);
    ss << leftSideCode.code << endl
       << rightSideCode.code << endl;
    releaseFloatTemp(leftSideCode.place);
    releaseFloatTemp(rightSideCode.place);
    ss << "c.le.s " << rightSideCode.place << ", " << leftSideCode.place << endl;
    code.code = ss.str();
}

void LteExpr::genCode(Code &code)
{

    Code leftSideCode;
    Code rightSideCode;
    stringstream ss;
    this->expr1->genCode(leftSideCode);
    this->expr2->genCode(rightSideCode);
    ss << leftSideCode.code << endl
       << rightSideCode.code << endl;
    releaseFloatTemp(leftSideCode.place);
    releaseFloatTemp(rightSideCode.place);
    ss << "c.lt.s " << rightSideCode.place << ", " << leftSideCode.place << endl;
    code.code = ss.str();
}

void EqExpr::genCode(Code &code)
{

    Code leftSideCode;
    Code rightSideCode;
    stringstream ss;
    this->expr1->genCode(leftSideCode);
    this->expr2->genCode(rightSideCode);
    ss << leftSideCode.code << endl
       << rightSideCode.code << endl;
    releaseFloatTemp(leftSideCode.place);
    releaseFloatTemp(rightSideCode.place);
    ss << "c.eq.s " << rightSideCode.place << ", " << leftSideCode.place << endl;
    code.code = ss.str();
}

void ReadFloatExpr::genCode(Code &code)
{
    // this->genCode(code);
}

string PrintStatement::genCode()
{
  Code exprCode;
    list<Expr *>::iterator it = this->expressions.begin();
    stringstream ss;
    stringstream asciiLabel;
    string label = getNewLabel("string");
    asciiLabel << label <<": .asciiz" << this->id << ""<<endl;
    assemblyFile.data += asciiLabel.str();
    while (it != this->expressions.end())
    {   
        (*it)->genCode(exprCode);
        ss << exprCode.code << endl;
        releaseFloatTemp(exprCode.place);
        it++;
    }
    ss << "la $a0, " << label << endl;
    ss << "li $v0, 4" << endl;
    ss << "syscall" << endl;
    return ss.str();
}



string ReturnStatement::genCode()
{
    Code exprCode;
    this->expr->genCode(exprCode);
    releaseRegister(exprCode.place);
    stringstream ss;
    ss << exprCode.code << endl
       << "move $v0, " << exprCode.place << endl;
    return ss.str();
}

string MethodDefinitionStatement::genCode()
{
    if (this->stmts.size() == 0)
        return "";
    int stackPointer = 4;
    globalStackPointer = 0;
    stringstream code;
    code << this->id << ": " << endl;
    string state = saveState();
    code << state << endl;
    if (this->params.size() > 0)
    {
        list<string>::iterator it = this->params.begin();
        for (int i = 0; i < this->params.size(); i++)
        {
            code << "sw $a" << i << ", " << stackPointer << "($sp)" << endl;
            codeGenerationVars[(*it)] = stackPointer;
            stackPointer += 4;
            globalStackPointer += 4;
            it++;
        }
    }
    list<Statement *>::iterator stmt = this->stmts.begin();
    while (stmt != stmts.end())
    {
        code << (*stmt)->genCode() << endl;
        stmt++;
    }
    stringstream sp;
    int currentStackPointer = globalStackPointer;
    sp << endl
       << "addiu $sp, $sp, -" << currentStackPointer << endl;
    code << retrieveState(state);
    code << "addiu $sp, $sp, " << currentStackPointer << endl;
    code << "jr $ra" << endl;
    floatTempMap.clear();
    string result = code.str();
    result.insert(id.size() + 2, sp.str());
    return result;
}