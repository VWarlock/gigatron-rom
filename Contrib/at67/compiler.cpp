#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <cmath>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "memory.h"
#include "cpu.h"
#include "assembler.h"
#include "compiler.h"
#include "keywords.h"
#include "operators.h"
#include "optimiser.h"
#include "validater.h"
#include "linker.h"


namespace Compiler
{
    enum VarResult {VarError=-1, VarNotFound, VarCreated, VarUpdated, VarExists};
    enum FloatSize {Float16=2, Float32=4};
    enum LabelResult {LabelError=-1, LabelNotFound, LabelFound};


    const std::vector<std::string> _sysInitNames = {"InitEqOp", "InitNeOp", "InitLeOp", "InitGeOp", "InitLtOp", "InitGtOp", "InitArray2d", "InitArray3d"};


    uint16_t _vasmPC                 = USER_CODE_START;
    uint16_t _tempVarStart           = TEMP_VAR_START;
    uint16_t _userVarStart           = USER_VAR_START;
    uint16_t _userVarsAddr           = _userVarStart;
    uint16_t _runtimeEnd             = 0x7FFF;
    uint16_t _runtimeStart           = 0x7FFF;
    uint16_t _strWorkArea            = 0x0000;
    uint16_t _spriteStripeChunks     = 15;
    uint16_t _spriteStripeMinAddress = USER_CODE_START;
    
    Memory::FitType _spriteStripeFitType = Memory::FitDescending;
    CodeOptimiseType _codeOptimiseType = CodeSpeed;
    Cpu::RomType _codeRomType = Cpu::ROMv3;

    bool _codeIsAsm = false;
    bool _compilingError = false;
    bool _arrayIndiciesOne = false;
    bool _createNumericLabelLut = false;
    bool _createTimeData = false;

    int _currentLabelIndex = -1;
    int _currentCodeLineIndex = 0;
    int _jumpFalseUniqueId = 0;

    std::string _runtimePath = ".";
    std::string _tempVarStartStr;
    std::string _nextInternalLabel;

    std::vector<std::string> _input;
    std::vector<std::string> _output;
    std::vector<std::string> _runtime;

    std::vector<Label>         _labels;
    std::vector<std::string>   _gosubLabels;
    std::vector<std::string>   _equateLabels;
    std::vector<InternalLabel> _internalLabels;
    std::vector<InternalLabel> _discardedLabels;

    std::vector<CodeLine>   _codeLines;
    std::vector<Constant>   _constants;
    std::vector<IntegerVar> _integerVars;
    std::vector<StringVar>  _stringVars;

    std::stack<ForNextData>     _forNextDataStack;
    std::stack<ElseIfData>      _elseIfDataStack;
    std::stack<EndIfData>       _endIfDataStack;
    std::stack<WhileWendData>   _whileWendDataStack;
    std::stack<RepeatUntilData> _repeatUntilDataStack;

    std::vector<std::string> _macroLines;
    std::map<int, MacroNameEntry> _macroNameEntries;
    std::map<std::string, MacroIndexEntry> _macroIndexEntries;

    std::vector<DefDataByte>   _defDataBytes;
    std::vector<DefDataWord>   _defDataWords;
    std::vector<DefDataImage>  _defDataImages;
    
    std::map<int, DefDataSprite> _defDataSprites;
    SpritesAddrLut _spritesAddrLut;

    std::map<int, DefDataFont> _defDataFonts;
    FontsAddrLut _fontsAddrLut;

    std::map<std::string, DefFunction> _defFunctions;

    std::vector<std::unique_ptr<DataObject>> _dataObjects;


    uint16_t getVasmPC(void) {return _vasmPC;}
    uint16_t getRuntimeEnd(void) {return _runtimeEnd;}
    uint16_t getRuntimeStart(void) {return _runtimeStart;}
    uint16_t getTempVarStart(void) {return _tempVarStart;}
    uint16_t getStrWorkArea(void) {return _strWorkArea;}
    uint16_t getSpriteStripeChunks(void) {return _spriteStripeChunks;}
    uint16_t getSpriteStripeMinAddress(void) {return _spriteStripeMinAddress;}
    Memory::FitType getSpriteStripeFitType(void) {return _spriteStripeFitType;}
    CodeOptimiseType getCodeOptimiseType(void) {return _codeOptimiseType;}
    Cpu::RomType getCodeRomType(void) {return _codeRomType;}
    bool getCompilingError(void) {return _compilingError;}
    bool getArrayIndiciesOne(void) {return _arrayIndiciesOne;}
    int getCurrentLabelIndex(void) {return _currentLabelIndex;}
    const std::string& getRuntimePath(void) {return _runtimePath;}
    const std::string& getTempVarStartStr(void) {return _tempVarStartStr;}
    const std::string& getNextInternalLabel(void) {return _nextInternalLabel;}

    void setCodeIsAsm(bool codeIsAsm) {_codeIsAsm = codeIsAsm;}
    void setRuntimeEnd(uint16_t runtimeEnd) {_runtimeEnd = runtimeEnd;}
    void setRuntimePath(const std::string& runtimePath) {_runtimePath = runtimePath;}
    void setRuntimeStart(uint16_t runtimeStart) {_runtimeStart = runtimeStart;}
    void setTempVarStart(uint16_t tempVarStart) {_tempVarStart = tempVarStart;}
    void setStrWorkArea(uint16_t strWorkArea) {_strWorkArea = strWorkArea;}
    void setSpriteStripeChunks(uint16_t spriteStripeChunks) {_spriteStripeChunks = spriteStripeChunks;}
    void setSpriteStripeMinAddress(uint16_t spriteStripeMinAddress) {_spriteStripeMinAddress = spriteStripeMinAddress;}
    void setSpriteStripeFitType(Memory::FitType spriteStripeFitType) {_spriteStripeFitType = spriteStripeFitType;}
    void setCodeOptimiseType(CodeOptimiseType codeOptimiseType) {_codeOptimiseType = codeOptimiseType;}
    void setCodeRomType(Cpu::RomType codeRomType) {_codeRomType = codeRomType;}
    void setCreateNumericLabelLut(bool createNumericLabelLut) {_createNumericLabelLut = createNumericLabelLut;}
    void setCreateTimeData(bool createTimeData) {_createTimeData = createTimeData;}
    void setCompilingError(bool compilingError) {_compilingError = compilingError;}
    void setArrayIndiciesOne(bool arrayIndiciesOne) {_arrayIndiciesOne = arrayIndiciesOne;}

    int getNextJumpFalseUniqueId(void) {return _jumpFalseUniqueId++;}

    std::vector<Label>& getLabels(void) {return _labels;}
    std::vector<Constant>& getConstants(void) {return _constants;}
    std::vector<CodeLine>& getCodeLines(void) {return _codeLines;}
    std::vector<IntegerVar>& getIntegerVars(void) {return _integerVars;}
    std::vector<StringVar>& getStringVars(void) {return _stringVars;}
    std::vector<InternalLabel>& getInternalLabels(void) {return _internalLabels;}
    std::vector<InternalLabel>& getDiscardedLabels(void) {return _discardedLabels;}
    std::vector<std::string>& getOutput(void) {return _output;}
    std::vector<std::string>& getRuntime(void) {return _runtime;}
    std::vector<DefDataByte>& getDefDataBytes(void) {return _defDataBytes;}
    std::vector<DefDataWord>& getDefDataWords(void) {return _defDataWords;}
    std::vector<DefDataImage>& getDefDataImages(void) {return _defDataImages;}

    std::map<int, DefDataSprite>& getDefDataSprites(void) {return _defDataSprites;}
    SpritesAddrLut& getSpritesAddrLut(void) {return _spritesAddrLut;}

    std::map<int, DefDataFont>& getDefDataFonts(void) {return _defDataFonts;}
    FontsAddrLut& getFontsAddrLut(void) {return _fontsAddrLut;}

    std::map<std::string, DefFunction>& getDefFunctions(void) {return _defFunctions;}
    std::vector<std::unique_ptr<DataObject>>& getDataObjects(void) {return _dataObjects;}
    std::map<std::string, MacroIndexEntry>& getMacroIndexEntries(void) {return _macroIndexEntries;}

    std::stack<ForNextData>& getForNextDataStack(void) {return _forNextDataStack;}
    std::stack<ElseIfData>& getElseIfDataStack(void) {return _elseIfDataStack;}
    std::stack<EndIfData>& getEndIfDataStack(void) {return _endIfDataStack;}
    std::stack<WhileWendData>& getWhileWendDataStack(void) {return _whileWendDataStack;}
    std::stack<RepeatUntilData>& getRepeatUntilDataStack(void) {return _repeatUntilDataStack;}


    // Vertical blank interrupt uses 0x30-0x33 for context save/restore, (vPC and vAC)
    bool moveVblankVars(void)
    {
        if(_codeRomType < Cpu::ROMv5a) return false;

        if(_userVarsAddr < USER_VAR_START + Int16*2)
        {
            _userVarStart = USER_VAR_START + Int16*2;
            _userVarsAddr = _userVarStart;
        }

        for(int i=0; i<int(_integerVars.size()); i++)
        {
            if(_integerVars[i]._address == USER_VAR_START  ||  _integerVars[i]._address == USER_VAR_START + Int16)
            {
                emitVcpuAsm("LDW", Expression::wordToHexString(_integerVars[i]._address), false);
                emitVcpuAsm("STW", Expression::wordToHexString(_userVarsAddr), false);
                _integerVars[i]._address = _userVarsAddr;
                _userVarsAddr += Int16;
            }
        }

        return true;
    }

    // If _nextInternalLabel is already queued, add it to discarded labels so that it is fixed later in outputLabels
    void setNextInternalLabel(const std::string& label)
    {
        if(_nextInternalLabel.size()) _discardedLabels.push_back({_vasmPC, _nextInternalLabel});

        _nextInternalLabel = label;
    }

    void adjustDiscardedLabels(const std::string name, uint16_t address)
    {
        uint16_t match;
        Expression::stringToU16(name.substr(name.size() - 6, 6), match);
        for(int i=0; i<int(_discardedLabels.size()); i++)
        {
            if(_discardedLabels[i]._address == match) _discardedLabels[i]._address = address;
        }
    }

    bool setBuildPath(const std::string& buildpath, const std::string& filepath)
    {
        if(buildpath.size() == 0) return false;

        // Prepend current file path to relative paths
        if(buildpath.find(":") == std::string::npos  &&  buildpath[0] != '/')
        {
            std::string path = filepath;
            size_t slash = path.find_last_of("\\/");
            path = (slash != std::string::npos) ? path.substr(0, slash) : ".";
            std::string includePath = path + "/" + buildpath;
            Assembler::setIncludePath(includePath);
        }
        else
        {
            Assembler::setIncludePath(buildpath);
        }

        Compiler::setRuntimePath(buildpath);

        return true;
    }

    bool initialise(void)
    {
        return true;
    }


    bool readInputFile(std::ifstream& infile, const std::string& filename, int& numLines)
    {
        std::string lineToken;

        if(!infile.is_open())
        {
            fprintf(stderr, "Compiler::readInputFile() : Failed to open file : '%s'\n", filename.c_str());
            return false;
        }

        // Read input .gbas file
        while(!infile.eof())
        {
            std::getline(infile, lineToken);
            _input.push_back(lineToken);

            if(!infile.good() && !infile.eof())
            {
                fprintf(stderr, "Compiler::readInputFile() : Bad line : '%s' : in '%s' : on line %d\n", lineToken.c_str(), filename.c_str(), numLines+1);
                return false;
            }

            numLines++;
        }

        return true;
    }

    bool writeOutputFile(std::ofstream& outfile, const std::string& filename)
    {
        if(!outfile.is_open())
        {
            fprintf(stderr, "Compiler::writeOutputFile() : failed to open '%s'\n", filename.c_str());
            return false;
        }

        // Write output .vasm file
        for(int i=0; i<int(_output.size()); i++)
        {
            outfile.write((char *)_output[i].c_str(), _output[i].size());
            if(outfile.bad() || outfile.fail())
            {
                fprintf(stderr, "Compiler::writeOutputFile() : write error in '%s'\n", filename.c_str());
                return false;
            }
        }

        return true;
    }


    int findLabel(const std::string& labelName)
    {
        for(int i=0; i<int(_labels.size()); i++)
        {
            if(_labels[i]._name == labelName) return i;
        }

        return -1;
    }
    
    int findLabel(uint16_t address)
    {
        for(int i=0; i<int(_labels.size()); i++)
        {
            if(_labels[i]._address == address) return i;
        }

        return -1;
    }

    int findInternalLabel(const std::string& labelName)
    {
        for(int i=0; i<int(_internalLabels.size()); i++)
        {
            if(_internalLabels[i]._name == labelName) return i;
        }

        return -1;
    }
    
    int findInternalLabel(uint16_t address)
    {
        for(int i=0; i<int(_internalLabels.size()); i++)
        {
            if(_internalLabels[i]._address == address) return i;
        }

        return -1;
    }

    int findConst(std::string& constName)
    {
        // Valid chars are alpha and 'address of'
        constName = Expression::getSubAlpha(constName);
        for(int i=0; i<int(_constants.size()); i++)
        {
            if(_constants[i]._name == constName) return i;
        }

        return -1;
    }

    int findVar(std::string& varName, bool subAlpha)
    {
        // Valid chars are alpha and 'address of'
        if(subAlpha) varName = Expression::getSubAlpha(varName);
        for(int i=0; i<int(_integerVars.size()); i++)
        {
            if(_integerVars[i]._name == varName) return i;
        }

        return -1;
    }

    int findStr(std::string& strName)
    {
        // Valid chars are alpha and 'address of'
        strName = Expression::getSubAlpha(strName);
        for(int i=0; i<int(_stringVars.size()); i++)
        {
            if(_stringVars[i]._name == strName) return i;
        }

        return -1;
    }


    void createLabel(uint16_t address, const std::string& name, int codeLineIndex, Label& label, bool numeric, bool addUnderscore, bool pageJump, bool gosub)
    {
        std::string n = name;
        Expression::stripWhitespace(n);
        std::string o = (addUnderscore) ? "_" + n : n;
        Expression::addString(o, LABEL_TRUNC_SIZE - int(o.size()));
        size_t space = o.find_first_of(" ");
        if(space == std::string::npos  ||  space >= LABEL_TRUNC_SIZE - 1)
        {
            o = o.substr(0, LABEL_TRUNC_SIZE);
            o[LABEL_TRUNC_SIZE - 1] = ' ';
        }

        label = {address, n, o, codeLineIndex, numeric, pageJump, gosub};
        Expression::stripWhitespace(label._name);
        _labels.push_back(label);
        _currentLabelIndex = int(_labels.size() - 1);
    }

    void createIntVar(const std::string& varName, int16_t data, int16_t init, CodeLine& codeLine, int codeLineIndex, bool containsVars, int& varIndex)
    {
        // Create var
        varIndex = int(_integerVars.size());
        codeLine._containsVars = containsVars;
        codeLine._varIndex = varIndex;
        codeLine._varType = VarInt16;

        std::vector<uint16_t> arrSizes;
        std::vector<int16_t> arrInits;
        std::vector<std::vector<uint16_t>> arrAddrs;
        std::vector<uint16_t> arrLut;
        IntegerVar integerVar = {data, init, _userVarsAddr, varName, varName, codeLineIndex, VarInt16, Int16, arrSizes, arrInits, arrAddrs, arrLut};
        _integerVars.push_back(integerVar);

        // Create var output
        std::string line = "_" + _integerVars[varIndex]._name;
        Expression::addString(line, LABEL_TRUNC_SIZE - int(line.size()));
        size_t space = line.find_first_of(" ");
        if(space == std::string::npos  ||  space >= LABEL_TRUNC_SIZE - 1)
        {
            line = line.substr(0, LABEL_TRUNC_SIZE);
            line[LABEL_TRUNC_SIZE - 1] = ' ';
        }
        _integerVars[varIndex]._output = line;
        _userVarsAddr += Int16;
        if(_userVarsAddr >= USER_VAR_END)
        {
            _userVarsAddr = _userVarStart;
            fprintf(stderr, "Warning: you have exceeded the maximum number of page zero global variables : on line %d\n", codeLineIndex); 
        }
    }

    void createIntVar(const std::string& varName, int16_t data, int16_t init, CodeLine& codeLine, int codeLineIndex, bool containsVars, int& varIndex, VarType varType, int intSize,
                      uint16_t address, std::vector<uint16_t>& arrSizes, const std::vector<int16_t>& arrInits, std::vector<std::vector<uint16_t>>& arrAddrs, std::vector<uint16_t>& arrLut)
    {
        // Create var
        varIndex = int(_integerVars.size());
        codeLine._containsVars = containsVars;
        codeLine._varIndex = varIndex;
        codeLine._varType = VarInt16;

        uint16_t varAddr = (varType == VarInt16) ? _userVarsAddr : address;
        IntegerVar integerVar = {data, init, varAddr, varName, varName, codeLineIndex, varType, intSize, arrSizes, arrInits, arrAddrs, arrLut};
        _integerVars.push_back(integerVar);

        // Create var output
        std::string line = "_" + _integerVars[varIndex]._name;
        Expression::addString(line, LABEL_TRUNC_SIZE - int(line.size()));
        size_t space = line.find_first_of(" ");
        if(space == std::string::npos  ||  space >= LABEL_TRUNC_SIZE - 1)
        {
            line = line.substr(0, LABEL_TRUNC_SIZE);
            line[LABEL_TRUNC_SIZE - 1] = ' ';
        }
        _integerVars[varIndex]._output = line;

        if(varType == VarInt16)
        {
            _userVarsAddr += Int16;
            if(_userVarsAddr >= USER_VAR_END)
            {
                _userVarsAddr = _userVarStart;
                fprintf(stderr, "Warning: you have exceeded the maximum number of page zero global variables : on line %d\n", codeLineIndex); 
            }
        }
    }

    VarResult createCodeVar(CodeLine& codeLine, int codeLineIndex, int& varIndex)
    {
        size_t equals = Expression::findNonStringEquals(codeLine._code) - codeLine._code.begin();
        if(codeLine._code.size() < 2  ||  equals >= codeLine._code.size()) return VarNotFound;

        // Check all tokens individually, don't just do a find as a var may exist with a reserved keyword embedded within it
        for(int i=0; i<int(codeLine._tokens.size()); i++)
        {
            std::string token = codeLine._tokens[i];
            Expression::stripWhitespace(token);
            Expression::strToUpper(token);

            // No need to check after the equals
            if(token == "=") break;

            // Check tokens that are reserved keywords using equals
            if(Keywords::getEqualsKeywords().find(token) != Keywords::getEqualsKeywords().end()) return VarNotFound;
        }

        // Name
        std::string varName = codeLine._code.substr(0, equals);
        Expression::stripWhitespace(varName);

        // Byte write configuration
        size_t dot = varName.find('.');
        if(dot != std::string::npos)
        {
            std::string dotName = varName.substr(dot);
            varName = varName.substr(0, dot);
            Expression::strToUpper(dotName);
            if(dotName == ".LO") codeLine._int16Byte = Expression::Int16Low;
            if(dotName == ".HI") codeLine._int16Byte = Expression::Int16High;
        }

        // String variables are not handled here
        if(varName.back() == '$') return VarError;

        // Var already exists?
        varIndex = findVar(varName);
        if(varIndex != -1)
        {
            codeLine._containsVars = false;
            codeLine._varIndex = varIndex;
            codeLine._varType = VarInt16;

            return VarExists;
        }

        createIntVar(varName, 0, 0, codeLine, codeLineIndex, false, varIndex);

        return VarCreated;
    }

    VarResult createCodeStr(CodeLine& codeLine, int codeLineIndex, int& strIndex)
    {
        // Code and string have minimum size requirements
        size_t equals = Expression::findNonStringEquals(codeLine._code) - codeLine._code.begin();
        if(codeLine._code.size() < 2  ||  equals >= codeLine._code.size() - 2  ||  codeLine._tokens.size() < 2) return VarError;

        // Check all tokens individually, don't just do a find as a str name may exist with a reserved keyword embedded within it
        for(int i=0; i<int(codeLine._tokens.size()); i++)
        {
            std::string token = codeLine._tokens[i];
            Expression::stripWhitespace(token);
            Expression::strToUpper(token);

            // Check tokens that are reserved keywords using equals
            if(Keywords::getEqualsKeywords().find(token) != Keywords::getEqualsKeywords().end()) return VarError;
        }

        // String name validation
        std::string strName = codeLine._code.substr(0, equals);
        Expression::stripWhitespace(strName);
        if(strName.back() != '$') return VarError;
        if(!Expression::isVarNameValid(strName)) return VarError;
        strIndex = findStr(strName);
        if(strIndex != -1)
        {
            codeLine._varIndex = strIndex;
            codeLine._varType = VarStr;

            return VarExists;
        }

        // String data validation
        std::string strData = codeLine._tokens[1];
        Expression::stripNonStringWhitespace(strData);
        std::string strKeyword = strData;

        // Check for string keywords
        if(Keywords::getStringKeywords().find(Expression::strToUpper(strKeyword)) != Keywords::getStringKeywords().end())
        {
            strData = "";
        }
        // Check for constant string
        else
        {
            if(Expression::isStringValid(strData))
            {
                // Strip quotes
                strData.erase(0, 1);
                strData.erase(strData.size()-1, 1);
            }
            // Check for assignment of another variable
            else
            {
                if(strData.back() != '$') return VarError;
                strData = "";
            }
        }

        // Create string
        uint16_t address;
        if(getOrCreateString(codeLine, codeLineIndex, strData, strName, address, USER_STR_SIZE, false) == -1) return VarError;
        strIndex = int(_stringVars.size()) - 1;
        codeLine._varIndex = strIndex;
        codeLine._varType = VarStr;

        return VarCreated;
    }

    uint32_t isExpression(std::string& input, int& varIndex, int& constIndex, int& strIndex)
    {
        uint32_t expressionType = Expression::HasNumbers;

        if(input.find_first_of("[]") != std::string::npos) return Expression::IsInvalid;
        if(input.find("++") != std::string::npos) return Expression::IsInvalid;
        if(input.find("--") != std::string::npos) return Expression::IsInvalid;

        // Check for strings
        if(input.find("\"") != std::string::npos) expressionType |= Expression::HasStrings;

        std::string stripped = Expression::stripStrings(input);
        std::vector<std::string> tokens = Expression::tokenise(stripped, "-+/*%&<>=();, ", false);

        // Check for pragmas
        for(int i=0; i<int(tokens.size()); i++)
        {
            std::string token = tokens[i];
            Expression::strToUpper(token);
            if(Keywords::getPragmas().find(token) != Keywords::getPragmas().end())
            {
                expressionType |= Expression::HasPragmas;
                break;
            }
        }

        // Check for keywords
        for(int i=0; i<int(tokens.size()); i++)
        {
            std::string token = tokens[i];
            Expression::strToUpper(token);
            if(Keywords::getKeywords().find(token) != Keywords::getKeywords().end())
            {
                expressionType |= Expression::HasKeywords;
                break;
            }
        }

        // Check for inbuilt functions and user functions
        for(int i=0; i<int(tokens.size()); i++)
        {
            std::string token = tokens[i];
            Expression::strToUpper(token);
            if((Keywords::getFunctions().find(token) != Keywords::getFunctions().end())  ||  (Compiler::getDefFunctions().find(token) != Compiler::getDefFunctions().end()))
            {
                expressionType |= Expression::HasFunctions;
                break;
            }
        }

        // Check for string keywords
        for(int i=0; i<int(tokens.size()); i++)
        {
            std::string token = tokens[i];
            Expression::strToUpper(token);
            if(Keywords::getStringKeywords().find(token) != Keywords::getStringKeywords().end())
            {
                // If first token is a string keyword then don't create strings
                if(i == 0) expressionType |= Expression::HasOptimisedPrint;

                expressionType |= Expression::HasStringKeywords;
                break;
            }
        }

        // Check for consts
        for(int i=0; i<int(tokens.size()); i++)
        {
            constIndex = findConst(tokens[i]);
            if(constIndex != -1  &&  tokens[i][0] != '@') // 'address of' operator returns numbers
            {
                if(_constants[constIndex]._constType == ConstInt16)
                {
                    expressionType |= Expression::HasIntConsts;
                    break;
                }
                if(_constants[constIndex]._constType == ConstStr)
                {
                    expressionType |= Expression::HasStrConsts;
                    break;
                }
            }
        }

        // Check for vars
        for(int i=0; i<int(tokens.size()); i++)
        {
            varIndex = findVar(tokens[i]);
            if(varIndex != -1  &&  tokens[i][0] != '@') // 'address of' operator returns numbers
            {
                // Array variables are treated as a function call
                if(_integerVars[varIndex]._varType == VarArray1  ||  _integerVars[varIndex]._varType == VarArray2  ||  _integerVars[varIndex]._varType == VarArray3)
                {
                    expressionType |= Expression::HasFunctions;
                    break;
                }

                expressionType |= Expression::HasIntVars;
                break;
            }
        }

        // Check for string vars
        for(int i=0; i<int(tokens.size()); i++)
        {
            strIndex = findStr(tokens[i]);
            if(strIndex != -1  &&  _stringVars[strIndex]._constant == false  &&  tokens[i][0] != '@') // 'address of' operator returns numbers)
            {
                expressionType |= Expression::HasStrVars;
                break;
            }
        }

        // Check for operators for non string expressions
        bool isStrExpression = ((expressionType >= Expression::HasStrings)  &&  (expressionType <= Expression::IsStringExpression));
        if(!isStrExpression)
        {
            if(stripped.find_first_of("-+/*%&<>=") != std::string::npos) expressionType |= Expression::HasOperators;
            std::string mod = stripped;
            Expression::strToUpper(mod);
            for(int i=0; i<int(Keywords::getOperators().size()); i++)
            {
                if(mod.find(Keywords::getOperators()[i]) != std::string::npos) expressionType |= Expression::HasOperators;
            }
        }

        return expressionType;
    }

    void updateVar(int16_t data, CodeLine& codeLine, int varIndex, bool containsVars)
    {
        codeLine._containsVars = containsVars;
        codeLine._varIndex = varIndex;
        codeLine._varType = VarInt16;
        _integerVars[varIndex]._data = data;
    }

    bool createAsmLine(const std::string& code)
    {
        std::string line, vasm = code;
        Expression::trimWhitespace(vasm);
        std::vector<std::string> tokens = Expression::tokenise(vasm, ' ', false);
        if(tokens.size() < 1)
        {
            fprintf(stderr, "Compiler::createAsmLine() : vASM syntax error, missing opcode on line %d\n", _currentCodeLineIndex);
            return false;
        }

        std::string opcodeStr = tokens[0];
        std::string operandStr = "";
        for(int i=1; i<int(tokens.size()); i++) operandStr += tokens[i];

        Expression::stripWhitespace(opcodeStr);
        Expression::strToUpper(opcodeStr);
        Expression::stripWhitespace(operandStr);

        int vasmSize = createVcpuAsm(opcodeStr, operandStr, int(_codeLines.size()), line);
        if(vasmSize == 0)
        {
            // This is a bit of a hack, but unfortunately needed as createAsmLine is called before the keyword ENDASM can be processed
            if(opcodeStr == "ENDASM") return true;

            fprintf(stderr, "Compiler::createAsmLine() : vASM syntax error, undefined opcode on line %d\n", _currentCodeLineIndex);
            return false;
        }

        _codeLines[_currentCodeLineIndex]._vasm.push_back({uint16_t(_vasmPC - vasmSize), opcodeStr, operandStr, line, "", false, vasmSize});
        _codeLines[_currentCodeLineIndex]._vasmSize += vasmSize;
        _codeLines[_currentCodeLineIndex]._dontParse = true;

        return true;
    }

    bool createCodeLine(const std::string& code, int codeLineOffset, int labelIndex, int varIndex, Expression::Int16Byte int16Byte, bool vars, CodeLine& codeLine)
    {
        // Handle variables
        size_t equal = code.find_first_of("=");
        std::string expression = (equal != std::string::npos) ? code.substr(equal + 1) : code;
        Expression::trimWhitespace(expression);

        std::vector<int> onLut;
        OnGotoGosubLut onGotoGosubLut = {0x0000, "", onLut};

        std::vector<uint16_t> concatStrs;
        StrConcatLut strConcatLut = {0x0000, concatStrs};

        std::vector<uint16_t> inputVars;
        std::vector<uint16_t> inputStrs;
        std::vector<uint16_t> inputTypes;
        InputLut inputLut = {0x0000, 0x0000, 0x0000, 0x0000, inputVars, inputStrs, inputTypes};

        std::vector<VasmLine> vasm;
        std::string text = code.substr(codeLineOffset, code.size() - (codeLineOffset));
        Expression::trimWhitespace(text);
        std::string codeText = Expression::collapseWhitespaceNotStrings(text);
        codeText = Expression::removeCommentsNotInStrings(codeText);
        std::vector<size_t> offsets;
        std::vector<std::string> tokens = Expression::tokeniseLine(codeText, " (),=", offsets);
        codeLine = {text, codeText, tokens, offsets, vasm, expression, onGotoGosubLut, strConcatLut, inputLut, 0, labelIndex, varIndex, VarInt16, int16Byte, vars, false, false};
        Expression::operatorReduction(codeLine._expression);

        // ASM keyword
        if(_codeIsAsm) return createAsmLine(code);

        return true;
    }


    void getNextTempVar(void)
    {
        static int prevCodeLineIndex = -1;
        if(_currentCodeLineIndex != prevCodeLineIndex)
        {
            prevCodeLineIndex = _currentCodeLineIndex;
            _tempVarStart = TEMP_VAR_START;
        }
        else
        {
            _tempVarStart += 2;
            if(_tempVarStart >= TEMP_VAR_START + 0x10) _tempVarStart = TEMP_VAR_START; // 16 bytes of temporary expression variables allows for 8 expression depths
        }

        _tempVarStartStr = Expression::wordToHexString(_tempVarStart);
    }

    // Find text in a macro
    bool findMacroText(const std::string& macroName, const std::string& text)
    {
        if(_macroIndexEntries.find(macroName) == _macroIndexEntries.end()) return false;

        std::string textU = text;
        Expression::strToUpper(textU);

        int indexStart = _macroIndexEntries[macroName]._indexStart;
        int indexEnd = _macroIndexEntries[macroName]._indexEnd;
        for(int i=indexStart+1; i<indexEnd; i++)
        {
            size_t commentStart = _macroLines[i].find_first_of(";#");
            std::string macroLine = (commentStart != std::string::npos) ? _macroLines[i].substr(0, commentStart) : _macroLines[i];
            std::vector<std::string> tokens = Expression::tokeniseLine(macroLine);

            for(int j=0; j<int(tokens.size()); j++)
            {
                Expression::stripWhitespace(tokens[j]);
                Expression::strToUpper(tokens[j]);
                if(tokens[j] == textU) return true;
            }

            // Check for nested macros
            for(int j=0; j<int(tokens.size()); j++)
            {
                if(findMacroText(tokens[j], text)) return true;
            }
        }

        return false;
    }

    // Find macro and work out it's vASM byte size
    int getMacroSize(const std::string& macroName)
    {
        if(_macroIndexEntries.find(macroName) == _macroIndexEntries.end()) return 0;

        int opcodesSize = 0;
        int indexStart = _macroIndexEntries[macroName]._indexStart;
        int indexEnd = _macroIndexEntries[macroName]._indexEnd;
        for(int i=indexStart+1; i<indexEnd; i++)
        {
            size_t commentStart = _macroLines[i].find_first_of(";#");
            std::string macroLine = (commentStart != std::string::npos) ? _macroLines[i].substr(0, commentStart) : _macroLines[i];
            std::vector<std::string> tokens = Expression::tokeniseLine(macroLine);

            int opcodeSize = 0;
            for(int j=0; j<int(tokens.size()); j++)
            {
                opcodeSize = Assembler::getAsmOpcodeSize(tokens[j]);
                if(opcodeSize)
                {
                    opcodesSize += opcodeSize;
                    break;
                }
            }

            // Check for nested macros
            if(opcodeSize == 0)
            {
                for(int j=0; j<int(tokens.size()); j++)
                {
                    opcodeSize = getMacroSize(tokens[j]);
                    if(opcodeSize)
                    {
                        opcodesSize += opcodeSize;
                        break;
                    }
                }
            }            
        }

        return opcodesSize;
    }

    bool initialiseMacros(void)
    {
        std::string filename = (_codeRomType < Cpu::ROMv5a) ? "/macros.i" : "/macros_ROMv5a.i";
        filename = Assembler::getIncludePath() + filename;
        std::ifstream infile(filename);

        if(!infile.is_open())
        {
            fprintf(stderr, "Compiler::initialiseMacros() : Failed to open file : '%s'\n", filename.c_str());
            return false;
        }

        std::string lineToken;
        while(!infile.eof())
        {
            std::getline(infile, lineToken);
            _macroLines.push_back(lineToken);
        }

        // Macro names
        int macroIndex = 0;
        std::string macroName;
        bool foundMacro = false;
        for(int i=0; i<int(_macroLines.size()); i++)
        {
            std::vector<std::string> tokens = Expression::tokeniseLine(_macroLines[i]);
            if(!foundMacro  &&  tokens.size() >= 2  &&  tokens[0] == "%MACRO")
            {
                macroIndex = i;
                macroName = tokens[1];

                MacroNameEntry macroNameEntry = {macroName, 0, 0};
                _macroNameEntries[macroIndex] = macroNameEntry;

                MacroIndexEntry macroIndexEntry = {macroIndex, 0, 0};
                _macroIndexEntries[macroName] = macroIndexEntry;

                foundMacro = true;
            }
            else if(foundMacro  &&  tokens.size() >= 1  &&  tokens[0] == "%ENDM")
            {
                _macroNameEntries[macroIndex]._indexEnd = i;
                _macroIndexEntries[macroName]._indexEnd = i;
                foundMacro = false;
            }
        }

        // %MACRO is missing a %ENDM
        if(foundMacro)
        {
            //_macroNameEntries.erase(macroIndex);
            //_macroIndexEntries.erase(macroName);

            fprintf(stderr, "Compiler::initialiseMacros() : %%MACRO %s on line %d: is missing a %%ENDM\n", macroName.c_str(), macroIndex);
            return false;
        }

        // Calculate macros vASM byte sizes
        for(auto it=_macroNameEntries.begin(); it!=_macroNameEntries.end(); ++it)
        {
            it->second._byteSize = getMacroSize(it->second._name);
            _macroIndexEntries[it->second._name]._byteSize = it->second._byteSize;
            //macroIndex = _macroIndexEntries[it->second._name]._indexStart;
            //fprintf(stderr, "%s  %d %d  %d %d bytes\n", it->second._name.c_str(), macroIndex, it->second._indexEnd, it->second._byteSize, _macroIndexEntries[it->second._name]._byteSize);
        }

        return true;
    }


    int createVcpuAsm(const std::string& opcodeStr, const std::string& operandStr, int codeLineIdx, std::string& line)
    {
        UNREFERENCED_PARAM(codeLineIdx);

        int vasmSize = 0;
        std::string opcode = std::string(opcodeStr);

        // Get macro size
        if(opcode.size()  &&  opcode[0] == '%')
        {
            opcode.erase(0, 1);
            if(_macroIndexEntries.find(opcode) != _macroIndexEntries.end())
            {
                vasmSize = _macroIndexEntries[opcode]._byteSize;
            }
        }
        // Get opcode size
        else
        {
            vasmSize = Assembler::getAsmOpcodeSize(opcode);
        }

        _vasmPC += uint16_t(vasmSize);

        //fprintf(stderr, "%s  %d %04x\n", opcode.c_str(), vasmSize, _vasmPC);

        std::string operand = std::string(operandStr);
        line = opcode + std::string(OPCODE_TRUNC_SIZE - opcode.size(), ' ') + operand;

        return vasmSize;
    }

    std::pair<int, int> emitVcpuAsm(const std::string& opcodeStr, const std::string& operandStr, bool nextTempVar, int codeLineIdx, const std::string& internalLabel, bool pageJump)
    {
        if(codeLineIdx == -1) codeLineIdx = _currentCodeLineIndex;

        std::string line;
        int vasmSize = createVcpuAsm(opcodeStr, operandStr, codeLineIdx, line);

        // NEXT and THEN don't know where the next vasm instruction is, so they use _nextInternalLabel, (which has priority over internalLabel)
        std::string intLabel = (_nextInternalLabel.size()) ? _nextInternalLabel : internalLabel;

        // Discarded labels are replaced correctly later in outputLabels()
        if(_nextInternalLabel.size()  &&  internalLabel.size()) _discardedLabels.push_back({_vasmPC, internalLabel});

        _codeLines[codeLineIdx]._vasm.push_back({uint16_t(_vasmPC - vasmSize), opcodeStr, operandStr, line, intLabel, pageJump, vasmSize});
        _codeLines[codeLineIdx]._vasmSize += vasmSize;

        if(nextTempVar) getNextTempVar();
        _nextInternalLabel = "";

        // Return current vasm instruction index
        return std::make_pair(codeLineIdx, int(_codeLines[codeLineIdx]._vasm.size()) - 1);
    }

    void createVcpuAsmLabel(int codeLineIdxBra, int vcpuAsmBra, int codeLineIdxDst, int vcpuAsmDst, const std::string& label)
    {
        std::string opcode = _codeLines[codeLineIdxBra]._vasm[vcpuAsmBra]._opcode;
        _codeLines[codeLineIdxBra]._vasm[vcpuAsmBra]._code = opcode + std::string(OPCODE_TRUNC_SIZE - opcode.size(), ' ') + label;
        _codeLines[codeLineIdxBra]._vasm[vcpuAsmBra]._operand = label;
        _codeLines[codeLineIdxDst]._vasm[vcpuAsmDst]._internalLabel = label;
    }

    // Array1d LDW expression parser
    uint32_t parseArray1dVarExpression(int codeLineIndex, std::string& expression, Expression::Numeric& numeric)
    {
        int varIndex, constIndex, strIndex;
        Expression::parse(expression, codeLineIndex, numeric);
        uint32_t expressionType = isExpression(expression, varIndex, constIndex, strIndex);
        if(((expressionType & Expression::HasIntVars)  &&  (expressionType & Expression::HasOperators))  ||  (expressionType & Expression::HasKeywords)  ||  (expressionType & Expression::HasStringKeywords))
        {
            emitVcpuAsm("LDW", Expression::byteToHexString(uint8_t(_tempVarStart)), false, codeLineIndex);
        }
        else if(expressionType & Expression::HasIntVars)
        {
            switch(numeric._int16Byte)
            {
                case Expression::Int16Low:  emitVcpuAsm("LD",  "_" + _integerVars[varIndex]._name,          false, codeLineIndex); break;
                case Expression::Int16High: emitVcpuAsm("LD",  "_" + _integerVars[varIndex]._name + " + 1", false, codeLineIndex); break;
                case Expression::Int16Both: emitVcpuAsm("LDW", "_" + _integerVars[varIndex]._name,          false, codeLineIndex); break;

                default: break;
            }
        }

        return expressionType;
    }
    bool writeArray1d(CodeLine& codeLine, int codeLineIndex, size_t lbra, size_t rbra, int intSize, uint16_t arrayPtr)
    {
        // Array index from expression
        Expression::Numeric arrIndex;
        std::string arrText = codeLine._code.substr(lbra + 1, rbra - (lbra + 1));
        uint32_t expressionType = parseArray1dVarExpression(codeLineIndex, arrText, arrIndex);

        // Constant index
        if(!(expressionType & Expression::HasIntVars))
        {
            emitVcpuAsm("LDWI", Expression::wordToHexString(arrayPtr + int16_t(std::lround(arrIndex._value))*uint16_t(intSize)), false, codeLineIndex);
            emitVcpuAsm("STW",  "memAddr", false, codeLineIndex);
            emitVcpuAsm("LDW",  "memValue", false, codeLineIndex);
            switch(codeLine._int16Byte)
            {
                case Expression::Int16Low:  emitVcpuAsm("POKE", "memAddr", false, codeLineIndex);                                                       break;
                case Expression::Int16High: emitVcpuAsm("INC",  "memAddr", false, codeLineIndex); emitVcpuAsm("POKE", "memAddr", false, codeLineIndex); break;
                case Expression::Int16Both: emitVcpuAsm("DOKE", "memAddr", false, codeLineIndex);                                                       break;

                default: break;
            }
        }
        else
        {
            emitVcpuAsm("STW",  "memIndex0", false, codeLineIndex);
            emitVcpuAsm("LDWI", Expression::wordToHexString(arrayPtr), false, codeLineIndex);
            emitVcpuAsm("ADDW", "memIndex0", false, codeLineIndex);
            emitVcpuAsm("ADDW", "memIndex0", false, codeLineIndex);
            emitVcpuAsm("STW",  "memAddr", false, codeLineIndex);
            emitVcpuAsm("LDW",  "memValue", false, codeLineIndex);
            switch(codeLine._int16Byte)
            {
                case Expression::Int16Low:  emitVcpuAsm("POKE", "memAddr", false, codeLineIndex);                                                       break;
                case Expression::Int16High: emitVcpuAsm("INC",  "memAddr", false, codeLineIndex); emitVcpuAsm("POKE", "memAddr", false, codeLineIndex); break;
                case Expression::Int16Both: emitVcpuAsm("DOKE", "memAddr", false, codeLineIndex);                                                       break;

                default: break;
            }
        }

        return true;
    }

    // ArrayXd LDW expression parser
    uint32_t parseArrayXdVarExpression(int codeLineIndex, std::string& expression, Expression::Numeric& numeric)
    {
        int varIndex, constIndex, strIndex;
        Expression::parse(expression, codeLineIndex, numeric);
        uint32_t expressionType = isExpression(expression, varIndex, constIndex, strIndex);
        if(((expressionType & Expression::HasIntVars)  &&  (expressionType & Expression::HasOperators))  ||  (expressionType & Expression::HasKeywords)  ||  (expressionType & Expression::HasStringKeywords))
        {
            emitVcpuAsm("LDW", Expression::byteToHexString(uint8_t(_tempVarStart)), false, codeLineIndex);
        }
        else if(expressionType & Expression::HasIntVars)
        {
            switch(numeric._int16Byte)
            {
                case Expression::Int16Low:  emitVcpuAsm("LD",  "_" + _integerVars[varIndex]._name,          false, codeLineIndex); break;
                case Expression::Int16High: emitVcpuAsm("LD",  "_" + _integerVars[varIndex]._name + " + 1", false, codeLineIndex); break;
                case Expression::Int16Both: emitVcpuAsm("LDW", "_" + _integerVars[varIndex]._name,          false, codeLineIndex); break;

                default: break;
            }
        }
        else
        {
            emitVcpuAsm("LDI", std::to_string(uint8_t(std::lround(numeric._value))), false, codeLineIndex);
        }

        return expressionType;
    }
    bool writeArray2d(CodeLine& codeLine, int codeLineIndex, size_t lbra, size_t rbra, int intSize, uint16_t arrayPtr)
    {
        UNREFERENCED_PARAM(intSize);

        // Array index from expression
        std::string arrText = codeLine._code.substr(lbra + 1, rbra - (lbra + 1));
        std::vector<std::string> indexTokens = Expression::tokenise(arrText, ',', true);
        if(indexTokens.size() != 2)
        {
            fprintf(stderr, "Compiler::writeArray2d() : Number of dimensions must be equal to 2, found %d in '%s' on line %d\n", int(indexTokens.size()), codeLine._code.c_str(), codeLineIndex);
            return false;
        }

        for(int i=0; i<int(indexTokens.size()); i++)
        {
            Expression::Numeric arrIndex;
            std::string indexToken = indexTokens[i];
            Expression::stripWhitespace(indexToken);
            parseArrayXdVarExpression(codeLineIndex, indexToken, arrIndex);
            emitVcpuAsm("STW", "memIndex" + std::to_string(i), false, codeLineIndex);
        }

        emitVcpuAsm("LDWI", Expression::wordToHexString(arrayPtr), false, codeLineIndex);
        (getCodeRomType() >= Cpu::ROMv5a) ? emitVcpuAsm("CALLI", "convertArr2d", false, codeLineIndex) : emitVcpuAsm("CALL", "convertArr2dAddr", false, codeLineIndex);
        emitVcpuAsm("LDW",  "memValue", false, codeLineIndex);
        switch(codeLine._int16Byte)
        {
            case Expression::Int16Low:  emitVcpuAsm("POKE", "memAddr", false, codeLineIndex);                                                       break;
            case Expression::Int16High: emitVcpuAsm("INC",  "memAddr", false, codeLineIndex); emitVcpuAsm("POKE", "memAddr", false, codeLineIndex); break;
            case Expression::Int16Both: emitVcpuAsm("DOKE", "memAddr", false, codeLineIndex);                                                       break;

            default: break;
        }

        return true;
    }

    bool writeArray3d(CodeLine& codeLine, int codeLineIndex, size_t lbra, size_t rbra, int intSize, uint16_t arrayPtr)
    {
        UNREFERENCED_PARAM(intSize);

        // Array index from expression
        std::string arrText = codeLine._code.substr(lbra + 1, rbra - (lbra + 1));
        std::vector<std::string> indexTokens = Expression::tokenise(arrText, ',', true);
        if(indexTokens.size() != 3)
        {
            fprintf(stderr, "Compiler::writeArray3d() : Number of dimensions must be equal to 3, found %d in '%s' on line %d\n", int(indexTokens.size()), codeLine._code.c_str(), codeLineIndex);
            return false;
        }

        for(int i=0; i<int(indexTokens.size()); i++)
        {
            Expression::Numeric arrIndex;
            std::string indexToken = indexTokens[i];
            Expression::stripWhitespace(indexToken);
            parseArrayXdVarExpression(codeLineIndex, indexToken, arrIndex);
            emitVcpuAsm("STW",  "memIndex" + std::to_string(i), false, codeLineIndex);
        }

        emitVcpuAsm("LDWI", Expression::wordToHexString(arrayPtr), false, codeLineIndex);
        (getCodeRomType() >= Cpu::ROMv5a) ? emitVcpuAsm("CALLI", "convertArr3d", false, codeLineIndex) : emitVcpuAsm("CALL", "convertArr3dAddr", false, codeLineIndex);
        emitVcpuAsm("LDW",  "memValue", false, codeLineIndex);
        switch(codeLine._int16Byte)
        {
            case Expression::Int16Low:  emitVcpuAsm("POKE", "memAddr", false, codeLineIndex);                                                       break;
            case Expression::Int16High: emitVcpuAsm("INC",  "memAddr", false, codeLineIndex); emitVcpuAsm("POKE", "memAddr", false, codeLineIndex); break;
            case Expression::Int16Both: emitVcpuAsm("DOKE", "memAddr", false, codeLineIndex);                                                       break;

            default: break;
        }

        return true;
    }

    bool writeArrayVar(CodeLine& codeLine, int codeLineIndex, int varIndex)
    {
        // Array var?
        size_t lbra, rbra;
        if(!Expression::findMatchingBrackets(codeLine._code, 0, lbra, rbra)) return false;
        size_t equals = codeLine._code.find("=");
        if(equals == std::string::npos  ||  equals < rbra) return false;

        // Previous expression result
        emitVcpuAsm("STW", "memValue", false, codeLineIndex);

        int intSize = _integerVars[varIndex]._intSize;
        uint16_t arrayPtr = _integerVars[varIndex]._address;
        Compiler::VarType varType = _integerVars[varIndex]._varType;

        switch(varType)
        {
            case VarType::VarArray1: writeArray1d(codeLine, codeLineIndex, lbra, rbra, intSize, arrayPtr); break;
            case VarType::VarArray2: writeArray2d(codeLine, codeLineIndex, lbra, rbra, intSize, arrayPtr); break;
            case VarType::VarArray3: writeArray3d(codeLine, codeLineIndex, lbra, rbra, intSize, arrayPtr); break;

            default: break;
        }

        return true;
    }

    void handleInt16Byte(const std::string& opcode, const std::string& operand, Expression::Numeric& numeric, bool nextTempVar)
    {
        switch(numeric._int16Byte)
        {
            case Expression::Int16Both: 
            {
                emitVcpuAsm(opcode, operand, nextTempVar);
                return;
            }
            break;

            case Expression::Int16Low:
            {
                getNextTempVar();
                if(opcode != "SUBW")
                {
                    emitVcpuAsm("STW", Expression::byteToHexString(uint8_t(getTempVarStart())), false);
                    emitVcpuAsm("LD", operand, false);
                    emitVcpuAsm(opcode, Expression::byteToHexString(uint8_t(getTempVarStart())), nextTempVar);
                    return;
                }
                else
                {
                    std::string temp0 = Expression::byteToHexString(uint8_t(getTempVarStart()));
                    emitVcpuAsm("STW", temp0, false);
                    emitVcpuAsm("LD", operand, false);
                    getNextTempVar();
                    std::string temp1 = Expression::byteToHexString(uint8_t(getTempVarStart()));
                    emitVcpuAsm("STW", temp1, false);
                    emitVcpuAsm("LDW", temp0, false);
                    emitVcpuAsm("SUBW", temp1, nextTempVar);
                    return;
                }
            }
            break;

            case Expression::Int16High:
            {
                getNextTempVar();
                if(opcode != "SUBW")
                {
                    emitVcpuAsm("STW", Expression::byteToHexString(uint8_t(getTempVarStart())), false);
                    emitVcpuAsm("LD", operand + " + 1", false);
                    emitVcpuAsm(opcode, Expression::byteToHexString(uint8_t(getTempVarStart())), nextTempVar);
                    return;
                }
                else
                {
                    std::string temp0 = Expression::byteToHexString(uint8_t(getTempVarStart()));
                    emitVcpuAsm("STW", temp0, false);
                    emitVcpuAsm("LD", operand + " + 1", false);
                    getNextTempVar();
                    std::string temp1 = Expression::byteToHexString(uint8_t(getTempVarStart()));
                    emitVcpuAsm("STW", temp1, false);
                    emitVcpuAsm("LDW", temp0, false);
                    emitVcpuAsm("SUBW", temp1, nextTempVar);
                    return;
                }
            }
            break;

            default: break;
        }
    }

    bool emitVcpuAsmUserVar(const std::string& opcodeStr, Expression::Numeric& numeric, bool nextTempVar)
    {
        std::string opcode = std::string(opcodeStr);
        std::string varName = std::string(numeric._name);
        int varIndex = findVar(varName);
        if(varIndex == -1)
        {
            fprintf(stderr, "Compiler::emitVcpuAsmUserVar() : couldn't find variable name '%s'\n", varName.c_str());
            return false;
        }

        std::string operand = "_" + _integerVars[varIndex]._name;
        if(opcode == "LDW")
        {
            switch(numeric._int16Byte)
            {
                case Expression::Int16Both: opcode = "LDW";                    break;
                case Expression::Int16Low:  opcode = "LD";                     break;
                case Expression::Int16High: opcode = "LD";  operand += " + 1"; break;

                default: break;
            }

            emitVcpuAsm(opcode, operand, nextTempVar);
            return true;
        }

        // ADDW, SUBW, ANDW, ORW, XORW
        handleInt16Byte(opcode, operand, numeric, nextTempVar);
        return true;
    }

    // Generic expression parser
    OperandType parseExpression(int codeLineIndex, std::string& expression, std::string& operand, Expression::Numeric& numeric)
    {
        int varIndex, constIndex, strIndex;
        Expression::parse(expression, codeLineIndex, numeric);
        uint32_t expressionType = isExpression(expression, varIndex, constIndex, strIndex);

        if(((expressionType & Expression::HasIntVars)  &&  (expressionType & Expression::HasOperators))  ||  (expressionType & Expression::HasKeywords)  ||  
            (expressionType & Expression::HasStringKeywords)  ||  (expressionType & Expression::HasFunctions))
        {
            operand = Expression::byteToHexString(uint8_t(_tempVarStart));
            return OperandTemp;
        }
        else if(expressionType & Expression::HasIntVars)
        {
            operand = _integerVars[varIndex]._name;
            return OperandVar;
        }

        operand = std::to_string(int16_t(std::lround(numeric._value)));
        return OperandConst;
    }

    // LDW expression parser
    uint32_t parseExpression(int codeLineIndex, std::string& expression, Expression::Numeric& numeric)
    {
        int varIndex, constIndex, strIndex;
        Expression::parse(expression, codeLineIndex, numeric);
        uint32_t expressionType = isExpression(expression, varIndex, constIndex, strIndex);

        if(((expressionType & Expression::HasIntVars)  &&  (expressionType & Expression::HasOperators))  ||  (expressionType & Expression::HasKeywords)  ||
            (expressionType & Expression::HasStringKeywords)  ||  (expressionType & Expression::HasFunctions))
        {
            emitVcpuAsm("LDW", Expression::byteToHexString(uint8_t(_tempVarStart)), false, codeLineIndex);
        }
        else if(expressionType & Expression::HasIntVars)
        {
            switch(numeric._int16Byte)
            {
                case Expression::Int16Low:  emitVcpuAsm("LD",  "_" + _integerVars[varIndex]._name,          false, codeLineIndex); break;
                case Expression::Int16High: emitVcpuAsm("LD",  "_" + _integerVars[varIndex]._name + " + 1", false, codeLineIndex); break;
                case Expression::Int16Both: emitVcpuAsm("LDW", "_" + _integerVars[varIndex]._name,          false, codeLineIndex); break;

                default: break;
            }
        }
        else
        {
            int16_t value = int16_t(std::lround(numeric._value));
            (value >= 0  &&  value <= 255) ? emitVcpuAsm("LDI", std::to_string(value), false, codeLineIndex) : emitVcpuAsm("LDWI", std::to_string(value), false, codeLineIndex);
        }

        return expressionType;
    }

    // Handle expression, (use this when expression has already been parsed)
    uint32_t handleExpression(int codeLineIndex, std::string& expression, Expression::Numeric numeric)
    {
        int varIndex, constIndex, strIndex;
        uint32_t expressionType = isExpression(expression, varIndex, constIndex, strIndex);

        if(((expressionType & Expression::HasIntVars)  &&  (expressionType & Expression::HasOperators))  ||  (expressionType & Expression::HasKeywords)  ||
            (expressionType & Expression::HasStringKeywords)  ||  (expressionType & Expression::HasFunctions))
        {
            emitVcpuAsm("LDW", Expression::byteToHexString(uint8_t(_tempVarStart)), false, codeLineIndex);
        }
        else if(expressionType & Expression::HasIntVars)
        {
            switch(numeric._int16Byte)
            {
                case Expression::Int16Low:  emitVcpuAsm("LD",  "_" + _integerVars[varIndex]._name,          false, codeLineIndex); break;
                case Expression::Int16High: emitVcpuAsm("LD",  "_" + _integerVars[varIndex]._name + " + 1", false, codeLineIndex); break;
                case Expression::Int16Both: emitVcpuAsm("LDW", "_" + _integerVars[varIndex]._name,          false, codeLineIndex); break;

                default: break;
            }
        }
        else
        {
            int16_t value = int16_t(std::lround(numeric._value));
            (value >= 0  &&  value <= 255) ? emitVcpuAsm("LDI", std::to_string(value), false, codeLineIndex) : emitVcpuAsm("LDWI", std::to_string(value), false, codeLineIndex);
        }

        return expressionType;
    }

    bool isGosubLabel(const std::string& label)
    {
        for(int i=0; i<int(_gosubLabels.size()); i++)
        {
            if(_gosubLabels[i] == label) return true;
        }

        return false;
    }

    bool checkForGosubLabel(const std::string& code, int lineNumber)
    {
        std::vector<std::string> tokens = Expression::tokeniseLine(code, " :=");
        for(int i=0; i<int(tokens.size()); i++)
        {
            if(Expression::strToUpper(tokens[i]) == "GOSUB")
            {
                if(i+1 >= int(tokens.size()))
                {
                    fprintf(stderr, "Compiler::checkForGosubLabel() : missing label after GOSUB in '%s' on line %d\n", code.c_str(), lineNumber + 1);
                    return false;
                }
                _gosubLabels.push_back(tokens[i+1]);
            }
        }

        return true;
    }

    LabelResult checkForLabel(std::string& code, int lineNumber)
    {
        Label label;
        CodeLine codeLine;
        std::string gosubOpcode = "";

        // Numeric labels
        if(code.size() > 1  &&  isdigit((unsigned char)code[0]))
        {
            size_t space = code.find_first_of(" \n\r\f\t\v,");
            if(space == std::string::npos) space = code.size() - 1;

            // Force space between line numbers and line
            for(size_t i=1; i<space; i++)
            {
                if(!isdigit((unsigned char)code[i])  &&  code[i] != ':'  &&  code[i] != '!')
                {
                    space = i;
                    code.insert(i, " ");
                    break;
                }
            }

            if(code.size() - (space + 1) <= 2)
            {
                fprintf(stderr, "Compiler::checkForLabel() : line number cannot exist on its own : '%s' : on line %d\n", code.c_str(), lineNumber + 1);
                return LabelError;
            }

            if(code[0] == '0')
            {
                fprintf(stderr, "Compiler::checkForLabel() : line number cannot be zero or start with zero : '%s' : on line %d\n", code.c_str(), lineNumber + 1);
                return LabelError;
            }

            // Create label
            bool numeric = false;
            bool foundGosub = false;
            std::string labelName = code.substr(0, space);
            size_t colon = labelName.find(':');
            size_t exclamation = labelName.find('!');

            if(colon != std::string::npos)
            {
                numeric = true;
                foundGosub = true;
                labelName.erase(colon, 1);
            }
            else if(exclamation != std::string::npos)
            {
                numeric = true;
                foundGosub = false;
                labelName.erase(exclamation, 1);
            }
            else
            {
                foundGosub = isGosubLabel(labelName);
            }
            createLabel(_vasmPC, labelName, int(_codeLines.size()), label, numeric, true, false, foundGosub);
            if(createCodeLine(code, int(space + 1), _currentLabelIndex, -1, Expression::Int16Both, false, codeLine)) _codeLines.push_back(codeLine);

            return LabelFound;
        }

        // Text label
        static int nonNumericLabelIndex = -1;
        size_t colon = code.find_first_of(":");
        if(colon != std::string::npos)
        {
            std::string labelName = code.substr(0, colon);
            if(Expression::isLabNameValid(labelName))
            {
                bool validCode = false;

                // Create label
                bool foundGosub = isGosubLabel(labelName);
                createLabel(_vasmPC, labelName, int(_codeLines.size()), label, false, true, false, foundGosub);

                // Check for label with code
                if(code.size() > colon + 1)
                {
                    std::string labelCode = code.substr(colon  + 1);
                    Expression::stripWhitespace(labelCode);
                    if(labelCode.size() > 2  &&  createCodeLine(code, int(colon  + 1), _currentLabelIndex, -1, Expression::Int16Both, false, codeLine))
                    {
                        validCode = true;
                        _codeLines.push_back(codeLine);
                    }
                }

                // Check for label without code
                if(!validCode)
                {
                    nonNumericLabelIndex = _currentLabelIndex;
                }

                return LabelFound;
            }
        }

        // Non label code, (except if previous line had a non numeric label without code)
        if(createCodeLine(code, 0, nonNumericLabelIndex, -1, Expression::Int16Both, false, codeLine))
        {
            // Don't reset nonNumericLabelIndex until it has been assigned to a valid line
            if(codeLine._code.size() >= 2) nonNumericLabelIndex = -1;
            _codeLines.push_back(codeLine);
        }

        return LabelNotFound;
    }

    bool parsePragmas(int numLines)
    {
        // Parse each line of input for pragmas, (pragmas are case sensitive)
        for(int j=0; j<numLines; j++)
        {
            std::vector<size_t> offsets;
            std::string input = _input[j];
            input = Expression::removeCommentsNotInStrings(input);
            Keywords::KeywordResult keywordResult = Keywords::handlePragmas(input, j);
            switch(keywordResult)
            {
                case Keywords::KeywordFound:
                {
                    _input[j].erase(0);
                }
                break;

                case Keywords::KeywordError:
                {
                    fprintf(stderr, "Compiler::parsePragmas() : Syntax error in Pragma, in %s on line %d\n", _input[j].c_str(), j + 1);
                    return false;
                }
                break;

                default: break;
            }
        }

        return true;
    }

    // Uncomments sys init funcs as they are used, (effectively inserting them into the code)
    // vASM and label addresses need to be fixed, this is done at the end of parseCode()
    void enableSysInitFunc(const std::string& sysInitFunc)
    {
        for(int i=0; i<int(_codeLines[0]._vasm.size()); i++)
        {
            if(_codeLines[0]._vasm[i]._opcode == std::string(";%" + sysInitFunc))
            {
                Expression::replaceText(_codeLines[0]._vasm[i]._opcode, ";%", "");
                Expression::replaceText(_codeLines[0]._vasm[i]._code, ";%", "");
                break;
            }
        }
    }


    bool initialiseCode(void)
    {
        // Relies on _codeRomType_, so make sure it is already initialised
        initialiseMacros();

        // Entry point initialisation
        Label label;
        createLabel(_vasmPC, "_entryPoint_", 0, label, false, false, false, false);

        // BASIC INIT
        CodeLine codeLine;
        if(createCodeLine("INIT", 0, 0, -1, Expression::Int16Both, false, codeLine)) _codeLines.push_back(codeLine);

        // Rom check, (always used for versions greater than ROMv1)
        if(_codeRomType > Cpu::ROMv1)
        {
            emitVcpuAsm("LDI", Expression::byteToHexString(uint8_t(_codeRomType)), false, 0);
            emitVcpuAsm("STW", "romType", false, 0);
            emitVcpuAsm("%RomCheck", "", false, 0);
        }

        // Initialise
        emitVcpuAsm("%Initialise", "", false, 0);

        // Realtime proc and relational operators are CALLS from zero page for efficiency
        if(_codeRomType < Cpu::ROMv5a)
        {
            // Initialise relational operators, (only when actually used, the semi-colon can be removed later by enableSysInitFunc())
            emitVcpuAsm(";%InitEqOp", "", false, 0);
            emitVcpuAsm(";%InitNeOp", "", false, 0);
            emitVcpuAsm(";%InitLeOp", "", false, 0);
            emitVcpuAsm(";%InitGeOp", "", false, 0);
            emitVcpuAsm(";%InitLtOp", "", false, 0);
            emitVcpuAsm(";%InitGtOp", "", false, 0);

            // Initialise array converters, (only when actually used, the semi-colon can be removed later by enableSysInitFunc())
            emitVcpuAsm(";%InitArray2d", "", false, 0);
            emitVcpuAsm(";%InitArray3d", "", false, 0);

            // Initialise real time stub, (always used for versions lower than ROMv5a)
            emitVcpuAsm("%InitRealTimeStub", "", false, 0);
        }

        return true;
    }

    bool parseLabels(int numLines)
    {
        // GOSUB labels
        for(int i=0; i<numLines; i++)
        {
            if(!checkForGosubLabel(_input[i], i)) return false;
        }

        // All labels
        for(int i=0; i<numLines; i++)
        {
            switch(checkForLabel(_input[i], i))
            {
                case LabelFound:    break;
                case LabelNotFound: break;
                case LabelError:    return false;

                default: break;
            }
        }

        return true;
    }

    // Get or create string
    int getOrCreateString(CodeLine& codeLine, int codeLineIndex, const std::string& str, std::string& name, uint16_t& address, uint8_t maxSize, bool constString, VarType varType)
    {
        int index = -1, strLength = int(str.size());

        // Don't count escape char '\'
        int escCount = 0;
        for(int i=0; i<strLength; i++)
        {
            if(str[i] == '\\') escCount++;
        }
        strLength -= escCount;

        // Reuse const string if possible
        if(constString)
        {
            for(int j=0; j<int(_stringVars.size()); j++)
            {
                if(_stringVars[j]._text == str) 
                {
                    index = j;
                    break;
                }
            }
            if(index != -1)
            {
                name  = _stringVars[index]._name;
                address = _stringVars[index]._address;
            }
            else
            {
                // Allocate RAM for string + length and delimiter bytes
                if(!Memory::getFreeRAM(Memory::FitDescending, strLength + 2, USER_CODE_START, _runtimeStart, address))
                {
                    fprintf(stderr, "Compiler::getOrCreateString() : Not enough RAM for string %s='%s' of size %d, in '%s' on line %d\n", name.c_str(), str.c_str(), strLength + 2, 
                                                                                                                                          codeLine._code.c_str(), codeLineIndex);
                    return -1;
                }

                std::vector<uint16_t> arrAddrs;
                std::vector<std::string> arrInits;
                name = "str_" + Expression::wordToHexString(address);
                StringVar stringVar = {uint8_t(strLength), uint8_t(strLength), address, str, name, "_" + name + std::string(LABEL_TRUNC_SIZE - name.size() - 1, ' '), varType, -1, true, arrInits, arrAddrs};
                _stringVars.push_back(stringVar);
                index = int(_stringVars.size()) - 1;
            }
        }
        // Variable strings
        else
        {
            // Allocate RAM for string + length and delimiter bytes
            if(!Memory::getFreeRAM(Memory::FitDescending, maxSize + 2, USER_CODE_START, _runtimeStart, address))
            {
                fprintf(stderr, "Compiler::getOrCreateString() : Not enough RAM for string %s='%s' of size %d, in '%s' on line %d\n", name.c_str(), str.c_str(), maxSize + 2,
                                                                                                                                      codeLine._code.c_str(), codeLineIndex);
                return -1;
            }

            std::vector<uint16_t> arrAddrs;
            std::vector<std::string> arrInits;
            StringVar stringVar = {uint8_t(strLength), maxSize, address, str, name, "_" + name + std::string(LABEL_TRUNC_SIZE - name.size() - 1, ' '), varType, -1, false, arrInits, arrAddrs};
            _stringVars.push_back(stringVar);
            index = int(_stringVars.size()) - 1;
        }

        return index;
    }

    // Get or create constant string
    uint16_t getOrCreateConstString(const std::string& input, int& index)
    {
        std::string output = input;

        std::string name;
        uint16_t address;
        index = getOrCreateString(_codeLines[_currentCodeLineIndex], _currentCodeLineIndex, output, name, address);
        return address;
    }

    // Get or create constant string from int
    uint16_t getOrCreateConstString(ConstStrType constStrType, int16_t input, int& index)
    {
        char output[16] = "";
        switch(constStrType)
        {
            case StrChar:  sprintf(output, "%c",   uint8_t(input) & 0x7F); break;
            case StrHex:   sprintf(output, "%02X", uint8_t(input));        break;
            case StrHexw:  sprintf(output, "%04X", uint16_t(input));       break;

            default: break;
        }

        std::string name;
        uint16_t address;
        index = getOrCreateString(_codeLines[_currentCodeLineIndex], _currentCodeLineIndex, std::string(output), name, address);
        return address;
    }

    // Get or create constant sub-string
    uint16_t getOrCreateConstString(ConstStrType constStrType, const std::string& input, int8_t length, uint8_t offset, int& index)
    {
        std::string output;
        switch(constStrType)
        {
            case StrLeft:  output = input.substr(0, length);             break;
            case StrRight: output = input.substr(input.size() - length); break;
            case StrMid:   output = input.substr(offset, length);        break;

            default: break;
        }

        std::string name;
        uint16_t address;
        index = getOrCreateString(_codeLines[_currentCodeLineIndex], _currentCodeLineIndex, output, name, address);
        return address;
    }

    // Create an array of strings
    int createStringArray(CodeLine& codeLine, int codeLineIndex, std::string& name, uint8_t size, std::vector<std::string>& arrInits, std::vector<uint16_t>& arrAddrs)
    {
        int index = -1;

        if(size > USER_STR_SIZE)
        {
            fprintf(stderr, "Compiler::getOrCreateString() : Length %d of string is larger than maximum of %d, in '%s' on line %d\n", size, USER_STR_SIZE,
                                                                                                                                      codeLine._code.c_str(), codeLineIndex);
            return -1;
        }

        // Allocate RAM for array of strings
        for(int i=0; i<int(arrAddrs.size()); i++)
        {
            if(!Memory::getFreeRAM(Memory::FitDescending, size + 2, USER_CODE_START, _runtimeStart, arrAddrs[i]))
            {
                fprintf(stderr, "Compiler::getOrCreateString() : Not enough RAM for string %s of size %d, in '%s' on line %d\n", name.c_str(), size + 2,
                                                                                                                                 codeLine._code.c_str(), codeLineIndex);
                return -1;
            }
        }

        // Array of pointers to strings
        uint16_t address = 0x0000;
        int arraySize = int(arrAddrs.size())*2;
        if(!Memory::getFreeRAM(Memory::FitDescending, arraySize, USER_CODE_START, _runtimeStart, address, false)) // arrays do not need to be contained within pages
        {
            fprintf(stderr, "Keywords::getOrCreateString() : Not enough RAM for int array of size %d in '%s' on line %d\n", arraySize, codeLine._code.c_str(), codeLineIndex);
            return -1;
        }

        const std::string text(94, ' ');
        StringVar stringVar = {size, USER_STR_SIZE, address, text, name, "_" + name + std::string(LABEL_TRUNC_SIZE - name.size() - 1, ' '), VarStr2, -1, false, arrInits, arrAddrs};
        _stringVars.push_back(stringVar);
        index = int(_stringVars.size()) - 1;

        return index;
    }


    // ********************************************************************************************
    // Recursive Descent Parser helpers
    // ********************************************************************************************
    char peek(bool skipSpaces)
    {
        // Skipping spaces can attach hex numbers to variables, keywords, etc
        while(skipSpaces  &&  Expression::peek() == ' ')
        {
            if(!Expression::advance(1)) return 0;
        }

        return Expression::peek();
    }

    char get(bool skipSpaces)
    {
        // Skipping spaces can attach hex numbers to variables, keywords, etc
        while(skipSpaces  &&  Expression::peek() == ' ')
        {
            if(!Expression::advance(1)) return 0;
        }

        return Expression::get();
    }

    bool find(char chr)
    {
        if(peek(true) == chr)
        {
            get(true);
            return true;
        }

        return false;
    }

    bool number(double& value)
    {
        char uchr;

        bool isDouble = false;
        std::string valueStr;
        uchr = char(toupper((unsigned char)peek(true)));
        valueStr.push_back(uchr); get(true);
        uchr = char(toupper((unsigned char)peek(true)));

        if((uchr >= '0'  &&  uchr <= '9')  ||  uchr == 'X'  ||  uchr == 'H'  ||  uchr == 'B'  ||  uchr == 'O'  ||  uchr == 'Q'  ||  uchr == '.')
        {
            if(uchr == '.') isDouble = true;

            valueStr.push_back(uchr); get(true);
            uchr = char(toupper((unsigned char)peek(true)));
            while(uchr  &&  ((uchr >= '0'  &&  uchr <= '9')  ||  (uchr >= 'A'  &&  uchr <= 'F')  ||  uchr == '.'))
            {
                if(uchr == '.')
                {
                    // Check for multiple periods
                    if(isDouble) return false;
                    isDouble = true;
                }

                // Don't skip spaces here, as hex numbers can become attached to variables, keywords, etc
                valueStr.push_back(get(false));
                uchr = char(toupper((unsigned char)peek(false)));
            }
        }

        if(!isDouble)
        {
            int16_t ivalue;
            bool success = Expression::stringToI16(valueStr, ivalue);
            value = double(ivalue);
            return success;
        }

        Expression::stringToDouble(valueStr, value);
        return true;
    }

    Expression::Numeric getString()
    {
        // First quote
        get(true);

        // Don't skip spaces within string
        std::string text;
        while(peek(false)  &&  peek(false) != '"')
        {
            text += get(false);
        }

        if(!peek(true))
        {
            fprintf(stderr, "Compiler::getString() : Syntax error in string '%s' in '%s' on line %d\n", text.c_str(), _codeLines[_currentCodeLineIndex]._code.c_str(), Expression::getLineNumber());
            //_PAUSE_;
            _compilingError = true;
            return Expression::Numeric();
        }

        // Last quote
        get(true);

        return Expression::Numeric(0, -1, true, false, false, Expression::String, Expression::BooleanCC, Expression::Int16Both, std::string(""), text);
    }

    Expression::Numeric addressOf(void)
    {
        std::string varName = Expression::getExpression();
        if(varName.back() == ')') varName.erase(varName.size()-1);

        int varIndex   = findVar(varName);
        int strIndex   = findStr(varName);
        int labIndex   = findLabel(varName);
        int constIndex = findConst(varName);

        Expression::advance(varName.size());

        // Int and int arrays
        uint16_t address = 0x0000;
        if(varIndex != -1)
        {
            address = _integerVars[varIndex]._address;
        }
        // String and string arrays
        else if(strIndex != -1)
        {
            address = _stringVars[strIndex]._address;
        }
        // labels
        else if(labIndex != -1)
        {
            address = _labels[labIndex]._address;
        }
        // Constants
        else if(constIndex != -1)
        {
            address = _constants[constIndex]._address;
        }
        else
        {
            fprintf(stderr, "Compiler::addressOf() : Syntax error in '%s' on line %d\n", _codeLines[_currentCodeLineIndex]._code.c_str(), Expression::getLineNumber());
            //_PAUSE_;
            _compilingError = true;
            return Expression::Numeric();
        }

        bool relocatable = (labIndex > 0) ? true : false;
        return Expression::Numeric(address, -1, true, false, relocatable, Expression::Number, Expression::BooleanCC, Expression::Int16Both, std::string(""), std::string(""));
    }

    Expression::Numeric lengthOf(void)
    {
        std::string varName = Expression::getExpression();
        if(varName.back() == ')') varName.erase(varName.size()-1);

        int varIndex = findVar(varName);
        int strIndex = findStr(varName);
        int constIndex = findConst(varName);

        Expression::advance(varName.size());

        // Int and int arrays
        uint16_t length = 0;
        if(varIndex != -1)
        {
            length = uint16_t(_integerVars[varIndex]._intSize);
            switch(_integerVars[varIndex]._varType)
            {
                case VarArray1: length *= _integerVars[varIndex]._arrSizes[2];                                                                             break;
                case VarArray2: length *= _integerVars[varIndex]._arrSizes[1] * _integerVars[varIndex]._arrSizes[2];                                       break;
                case VarArray3: length *= _integerVars[varIndex]._arrSizes[0] * _integerVars[varIndex]._arrSizes[1] * _integerVars[varIndex]._arrSizes[2]; break;

                default: break;
            }
        }
        // String and string arrays
        else if(strIndex != -1)
        {
            switch(_stringVars[strIndex]._varType)
            {
                case VarStr:  length = USER_STR_SIZE;                                                              break;
                case VarStr2: length = uint16_t(_stringVars[strIndex]._arrAddrs.size()) * (USER_STR_SIZE + 2 + 2); break;

                default: break;
            }
        }
        // Constants
        else if(constIndex != -1)
        {
            length = _constants[constIndex]._size;
        }
        else
        {
            fprintf(stderr, "Compiler::lengthOf() : Syntax error in '%s' on line %d\n", _codeLines[_currentCodeLineIndex]._code.c_str(), Expression::getLineNumber());
            //_PAUSE_;
            _compilingError = true;
            return Expression::Numeric();
        }

        return Expression::Numeric(length, -1, true, false, false, Expression::Number, Expression::BooleanCC, Expression::Int16Both, std::string(""), std::string(""));
    }

    bool userFunc(const std::string& name)
    {
        size_t lbra, rbra;
        std::string funcText = Expression::getExpression();
        if(!Expression::findMatchingBrackets(funcText, 0, lbra, rbra))
        {
            fprintf(stderr, "Compiler::userFunc() : Parenthesis error in '%s' on line %d\n", _codeLines[_currentCodeLineIndex]._code.c_str(), Expression::getLineNumber());
            return false;
        }

        funcText = funcText.substr(lbra + 1, rbra - (lbra + 1));
        std::vector<std::string> params = Expression::tokenise(funcText, ',', true);
        if(params.size() == 0)
        {
            fprintf(stderr, "Compiler::userFunc() : Syntax error, need at least one parameter, in '%s' on line %d\n",  _codeLines[_currentCodeLineIndex]._code.c_str(), Expression::getLineNumber());
            return false;
        }
        int paramsSize = int(Compiler::getDefFunctions()[name]._params.size());
        if(paramsSize != int(params.size()))
        {
            fprintf(stderr, "Compiler::userFunc() : Syntax error, wrong number of parameters, expecting %d, in '%s' on line %d\n", paramsSize,  _codeLines[_currentCodeLineIndex]._code.c_str(), Expression::getLineNumber());
            return false;
        }
        std::string func = Compiler::getDefFunctions()[name]._function;
        for(int i=0; i<int(params.size()); i++)
        {
            Expression::stripWhitespace(params[i]);
            Expression::replaceText(func, Compiler::getDefFunctions()[name]._params[i], params[i]);
        }

        // Substitute function and re-create expression, (factor() then parses the new expression string)
        //int nameSize = int(name.size());
        intptr_t offset = Expression::getExpression() - (char *)Expression::getExpressionToParseString().c_str();
        Expression::replaceText(Expression::getExpressionToParseString(), funcText, func, offset);
        //Expression::getExpressionToParseString().erase(offset - nameSize, nameSize);
        //Expression::setExpression(Expression::getExpressionToParseString(), offset - nameSize);
        Expression::setExpression(Expression::getExpressionToParseString(), offset);

        return true;
    }

    Expression::Numeric factor(int16_t defaultValue)
    {
        double value = 0;
        Expression::Numeric numeric;

        if(peek(true) == '(')
        {
            get(true);
            numeric = expression();
            
            // Parameters
            while(peek(true)  &&  peek(true) != ')')
            {
                if(get(true) == ',') numeric._parameters.push_back(expression());
            }

            if(peek(true) != ')')
            {
                fprintf(stderr, "Compiler::factor() : Found '%c' : expecting ')' in '%s' on line %d\n", peek(true), Expression::getExpressionToParse(), Expression::getLineNumber());
                //_PAUSE_;
                _compilingError = true;
                numeric = Expression::Numeric();
            }
            get(true);
        }
        else if((peek(true) >= '0'  &&  peek(true) <= '9')  ||  peek(true) == '&')
        {
            // Number
            if(number(value))
            {
                numeric = Expression::Numeric(value, -1, true, false, false, Expression::Number, Expression::BooleanCC, Expression::Int16Both, std::string(""), std::string(""));
            }
            else
            {
                fprintf(stderr, "Compiler::factor() : Syntax error in number '%s' on line %d\n", _codeLines[_currentCodeLineIndex]._code.c_str(), Expression::getLineNumber());
                //_PAUSE_;
                _compilingError = true;
                numeric = Expression::Numeric();
            }
        }
        // Strings
        else if(peek(true) == '"')
        {
            // Handles quotes internally
            numeric = getString();
        }
        // 'Address of' operator
        else if(peek(true) == '@')
        {
            get(true); numeric = addressOf();
        }
        // Length
        else if(peek(true) == '#')
        {
            get(true); numeric = lengthOf();
        }
        // Unary operators
        else if(peek(true) == '+')
        {
            get(true); numeric = factor(0); numeric = Operators::operatorPOS(numeric);
        }
        else if(peek(true) == '-')
        {
            get(true); numeric = factor(0); numeric = Operators::operatorNEG(numeric);
        }
        else if(Expression::find("NOT "))
        {
            numeric = factor(0); numeric = Operators::operatorNOT(numeric);
        }
        // Functions with no parameters
        else if(Expression::find("TIME$"))
        {
            numeric = Keywords::functionTIME$(numeric, _currentCodeLineIndex);
        }
        // Functions
        else if(Expression::findFunc("PEEK"))
        {
            numeric = factor(0); numeric = Keywords::functionPEEK(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("DEEK"))
        {
            numeric = factor(0); numeric = Keywords::functionDEEK(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("USR"))
        {
            numeric = factor(0); numeric = Keywords::functionUSR(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("RND"))
        {
            numeric = factor(0); numeric = Keywords::functionRND(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("LEN"))
        {
            numeric = factor(0); numeric = Keywords::functionLEN(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("GET"))
        {
            numeric = factor(0); numeric = Keywords::functionGET(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("ABS"))
        {
            numeric = factor(0); numeric = Keywords::functionABS(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("SGN"))
        {
            numeric = factor(0); numeric = Keywords::functionSGN(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("ASC"))
        {
            numeric = factor(0); numeric = Keywords::functionASC(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("CMP"))
        {
            numeric = factor(0); numeric = Keywords::functionCMP(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("VAL"))
        {
            numeric = factor(0); numeric = Keywords::functionVAL(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("LUP"))
        {
            numeric = factor(0); numeric = Keywords::functionLUP(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("ADDR"))
        {
            numeric = factor(0); numeric = Keywords::functionADDR(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("POINT"))
        {
            numeric = factor(0); numeric = Keywords::functionPOINT(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("CHR$"))
        {
            numeric = factor(0); numeric = Keywords::functionCHR$(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("STR$"))
        {
            numeric = factor(0); numeric = Keywords::functionSTR$(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("HEX$"))
        {
            numeric = factor(0); numeric = Keywords::functionHEX$(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("HEXW$"))
        {
            numeric = factor(0); numeric = Keywords::functionHEXW$(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("LEFT$"))
        {
            numeric = factor(0); numeric = Keywords::functionLEFT$(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("RIGHT$"))
        {
            numeric = factor(0); numeric = Keywords::functionRIGHT$(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("MID$"))
        {
            numeric = factor(0); numeric = Keywords::functionMID$(numeric, _currentCodeLineIndex);
        }
        else if(Expression::findFunc("CEIL"))
        {
            numeric = factor(0); numeric = Operators::operatorCEIL(numeric);
        }
        else if(Expression::findFunc("FLOOR"))
        {
            numeric = factor(0); numeric = Operators::operatorFLOOR(numeric);
        }
        else if(Expression::findFunc("POW"))
        {
            numeric = factor(0); numeric = Operators::operatorPOWF(numeric);
        }
        else if(Expression::findFunc("SQRT"))
        {
            numeric = factor(0); numeric = Operators::operatorSQRT(numeric);
        }
        else if(Expression::findFunc("EXP2"))
        {
            numeric = factor(0); numeric = Operators::operatorEXP2(numeric);
        }
        else if(Expression::findFunc("EXP"))
        {
            numeric = factor(0); numeric = Operators::operatorEXP(numeric);
        }
        else if(Expression::findFunc("LOG10"))
        {
            numeric = factor(0); numeric = Operators::operatorLOG10(numeric);
        }
        else if(Expression::findFunc("LOG2"))
        {
            numeric = factor(0); numeric = Operators::operatorLOG2(numeric);
        }
        else if(Expression::findFunc("LOG"))
        {
            numeric = factor(0); numeric = Operators::operatorLOG(numeric);
        }
        else if(Expression::findFunc("SIN"))
        {
            numeric = factor(0); numeric = Operators::operatorSIN(numeric);
        }
        else if(Expression::findFunc("COS"))
        {
            numeric = factor(0); numeric = Operators::operatorCOS(numeric);
        }
        else if(Expression::findFunc("TAN"))
        {
            numeric = factor(0); numeric = Operators::operatorTAN(numeric);
        }
        else if(Expression::findFunc("ASIN"))
        {
            numeric = factor(0); numeric = Operators::operatorASIN(numeric);
        }
        else if(Expression::findFunc("ACOS"))
        {
            numeric = factor(0); numeric = Operators::operatorACOS(numeric);
        }
        else if(Expression::findFunc("ATAN2"))
        {
            numeric = factor(0); numeric = Operators::operatorATAN2(numeric);
        }
        else if(Expression::findFunc("ATAN"))
        {
            numeric = factor(0); numeric = Operators::operatorATAN(numeric);
        }
        else if(Expression::findFunc("RAND"))
        {
            numeric = factor(0); numeric = Operators::operatorRAND(numeric);
        }
        else if(Expression::findFunc("REV16"))
        {
            numeric = factor(0); numeric = Operators::operatorREV16(numeric);
        }
        else if(Expression::findFunc("REV8"))
        {
            numeric = factor(0); numeric = Operators::operatorREV8(numeric);
        }
        else if(Expression::findFunc("REV4"))
        {
            numeric = factor(0); numeric = Operators::operatorREV4(numeric);
        }
        else
        {
            // User functions, (DEF FN), names are NOT case sensitive, (like inbuilt functions)
            for(auto it=Compiler::getDefFunctions().begin(); it!=Compiler::getDefFunctions().end(); ++it)
            {
                std::string name = it->first;
                if(Expression::findFunc(name))
                {
                    if(!userFunc(it->first)) return numeric;
                    return factor(0);
                }
            }

            switch(peek(true))
            {
                // Reached end of expression
                case 0: numeric = Expression::Numeric(defaultValue, -1, false, false, false, Expression::Number, Expression::BooleanCC, Expression::Int16Both, std::string(""), std::string("")); break;

                default:
                {
                    // Variables
                    bool foundParams = false;
                    std::string varName = Expression::getExpression();
                    if(varName.back() == ')') varName.erase(varName.size()-1);
                    if(varName.find('(') != std::string::npos) foundParams = true;

                    int varIndex = findVar(varName);
                    int strIndex = findStr(varName);
                    int constIndex = findConst(varName);
                    if(varIndex != -1)
                    {
                        Expression::Int16Byte int16Byte = Expression::Int16Both;

                        Expression::advance(varName.size());

                        // Arrays
                        if(_integerVars[varIndex]._varType == VarArray1  ||  _integerVars[varIndex]._varType == VarArray2  ||  _integerVars[varIndex]._varType == VarArray3)
                        {
                            Expression::VarType varType = Expression::Arr1Var;
                            switch(_integerVars[varIndex]._varType)
                            {
                                case VarArray1: varType = Expression::Arr1Var; break;
                                case VarArray2: varType = Expression::Arr2Var; break;
                                case VarArray3: varType = Expression::Arr3Var; break;

                                default: break;
                            }

                            // Array numeric
                            numeric = Expression::Numeric(defaultValue, int16_t(varIndex), true, false, false, varType, Expression::BooleanCC, Expression::Int16Both, varName, std::string(""));

                            // Array index parameters, (commands like LEN expect no array indices)
                            if(foundParams)
                            {
                                Expression::Numeric param = factor(0); 
                                numeric._parameters.push_back(param);
                                for(int i=0; i<int(param._parameters.size()); i++)
                                {
                                    numeric._parameters.push_back(param._parameters[i]);
                                }

                                // Read both, low or high bytes, (DEEK <X>, PEEK <X>, PEEK <X+1>)
                                if(Expression::find(".LO")) int16Byte = Expression::Int16Low;
                                if(Expression::find(".HI")) int16Byte = Expression::Int16High;
                                numeric._int16Byte = int16Byte;

                                numeric = Keywords::functionIARR(numeric, _currentCodeLineIndex);
                            }
                        }
                        // Vars
                        else
                        {
                            // Read both, low or high bytes, (LDW <X>, LD <X>, LD <X+1>)
                            if(Expression::find(".LO")) int16Byte = Expression::Int16Low;
                            if(Expression::find(".HI")) int16Byte = Expression::Int16High;

                            // Numeric is now passed back to compiler, (rather than just numeric._value), so make sure all fields are valid
                            numeric = Expression::Numeric(defaultValue, int16_t(varIndex), true, false, false, Expression::IntVar, Expression::BooleanCC, int16Byte, varName, std::string(""));
                        }
                    }
                    // Strings
                    else if(strIndex != -1)
                    {
                        Expression::advance(varName.size());

                        // Arrays
                        if(_stringVars[strIndex]._varType == VarStr2)
                        {
                            // String array numeric
                            numeric = Expression::Numeric(defaultValue, int16_t(strIndex), true, false, false, Expression::Str2Var, Expression::BooleanCC, Expression::Int16Both, varName, _stringVars[strIndex]._text);

                            // Array index parameters, (commands like LEN expect no array indices)
                            if(foundParams)
                            {
                                Expression::Numeric param = factor(0); 
                                numeric._parameters.push_back(param);
                                for(int i=0; i<int(param._parameters.size()); i++)
                                {
                                    numeric._parameters.push_back(param._parameters[i]);
                                }

                                numeric = Keywords::functionSARR(numeric, _currentCodeLineIndex);
                            }
                        }
                        // Vars
                        else
                        {
                            // Numeric is now passed back to compiler, (rather than just numeric._value), so make sure all fields are valid
                            numeric = Expression::Numeric(defaultValue, int16_t(strIndex), true, false, false, Expression::StrVar, Expression::BooleanCC, Expression::Int16Both, varName, _stringVars[strIndex]._text);
                        }
                    }
                    // Constants
                    else if(constIndex != -1)
                    {
                        Expression::advance(varName.size());
                        
                        switch(_constants[constIndex]._constType)
                        {
                            // Numeric is now passed back to compiler, (rather than just numeric._value), so make sure all fields are valid
                            case ConstInt16:
                            {
                                numeric = Expression::Numeric(_constants[constIndex]._data, int16_t(constIndex), true, false, false, Expression::Number, Expression::BooleanCC, Expression::Int16Both, varName, std::string(""));
                            }
                            break;

                            case ConstStr:
                            {
                                numeric = Expression::Numeric(defaultValue, int16_t(constIndex), true, false, false, Expression::Constant, Expression::BooleanCC, Expression::Int16Both, varName, _constants[constIndex]._text);
                            }
                            break;

                            default: break;
                        }
                    }
                    // Unknown symbol
                    else
                    {
                        numeric = Expression::Numeric(defaultValue, -1, false, false, false, Expression::Number, Expression::BooleanCC, Expression::Int16Both, std::string(""), std::string(""));

                        // Pragma parsing happens before any code has been parsed, so _codeLines[] may be empty
                        std::string codeText = (int(_codeLines.size()) > _currentCodeLineIndex) ? _codeLines[_currentCodeLineIndex]._code : "PRAGMA";
                        if(varName.size())
                        {
                            fprintf(stderr, "\nCompiler::factor() : Found an unknown symbol '%s' : in '%s' on line %d\n\n", varName.c_str(), codeText.c_str(), Expression::getLineNumber());
                        }
                        else
                        {
                            fprintf(stderr, "\nCompiler::factor() : Found an unknown symbol in '%s' on line %d\n\n", codeText.c_str(), Expression::getLineNumber());
                        }

                        //_PAUSE_;
                        _compilingError = true;
                    }
                }
                break;
            }
        }

        return numeric;
    }

    Expression::Numeric term(void)
    {
        Expression::Numeric numeric, result = factor(0);

        for(;;)
        {
            if(Expression::find("**"))       {numeric = factor(0); result = Operators::operatorPOW(result, numeric);}
            else if(find('*'))               {numeric = factor(0); result = Operators::operatorMUL(result, numeric);}
            else if(find('/'))               {numeric = factor(0); result = Operators::operatorDIV(result, numeric);}
            else if(find('%'))               {numeric = factor(0); result = Operators::operatorMOD(result, numeric);}
            else if(Expression::find("MOD")) {numeric = factor(0); result = Operators::operatorMOD(result, numeric);}
            else return result;
        }
    }

    Expression::Numeric expr(void)
    {
        Expression::Numeric numeric, result = term();

        for(;;)
        {
            if(find('+'))      {numeric = term(); result = Operators::operatorADD(result, numeric);}
            else if(find('-')) {numeric = term(); result = Operators::operatorSUB(result, numeric);}

            else return result;
        }
    }

    Expression::Numeric logical(void)
    {
        Expression::Numeric numeric, result = expr();

        for(;;)
        {
            if(Expression::find("AND"))      {numeric = expr(); result = Operators::operatorAND(result, numeric);}
            else if(Expression::find("XOR")) {numeric = expr(); result = Operators::operatorXOR(result, numeric);}
            else if(Expression::find("OR"))  {numeric = expr(); result = Operators::operatorOR(result,  numeric);}
            else if(Expression::find("LSL")) {numeric = expr(); result = Operators::operatorLSL(result, numeric);}
            else if(Expression::find("LSR")) {numeric = expr(); result = Operators::operatorLSR(result, numeric);}
            else if(Expression::find("ASR")) {numeric = expr(); result = Operators::operatorASR(result, numeric);}
            else if(Expression::find("<<"))  {numeric = expr(); result = Operators::operatorLSL(result, numeric);}
            else if(Expression::find(">>"))  {numeric = expr(); result = Operators::operatorLSR(result, numeric);}
            else if(Expression::find("&>>")) {numeric = expr(); result = Operators::operatorASR(result, numeric);}

            else return result;
        }
    }

    Expression::Numeric expression(void)
    {
        Expression::Numeric numeric, result = logical();

        for(;;)
        {
            // Boolean conditionals
            if(Expression::find("=="))      {numeric = logical(); result = Operators::operatorEQ(result, numeric, Expression::BooleanCC);}
            else if(find('='))              {numeric = logical(); result = Operators::operatorEQ(result, numeric, Expression::BooleanCC);}
            else if(Expression::find("<>")) {numeric = logical(); result = Operators::operatorNE(result, numeric, Expression::BooleanCC);}
            else if(Expression::find("<=")) {numeric = logical(); result = Operators::operatorLE(result, numeric, Expression::BooleanCC);}
            else if(Expression::find(">=")) {numeric = logical(); result = Operators::operatorGE(result, numeric, Expression::BooleanCC);}
            else if(find('<'))              {numeric = logical(); result = Operators::operatorLT(result, numeric, Expression::BooleanCC);}
            else if(find('>'))              {numeric = logical(); result = Operators::operatorGT(result, numeric, Expression::BooleanCC);}

            // Normal conditionals
            else if(Expression::find("&==")) {numeric = logical(); result = Operators::operatorEQ(result, numeric, Expression::NormalCC);}
            else if(Expression::find("&="))  {numeric = logical(); result = Operators::operatorEQ(result, numeric, Expression::NormalCC);}
            else if(Expression::find("&<>")) {numeric = logical(); result = Operators::operatorNE(result, numeric, Expression::NormalCC);}
            else if(Expression::find("&<=")) {numeric = logical(); result = Operators::operatorLE(result, numeric, Expression::NormalCC);}
            else if(Expression::find("&>=")) {numeric = logical(); result = Operators::operatorGE(result, numeric, Expression::NormalCC);}
            else if(Expression::find("&<"))  {numeric = logical(); result = Operators::operatorLT(result, numeric, Expression::NormalCC);}
            else if(Expression::find("&>"))  {numeric = logical(); result = Operators::operatorGT(result, numeric, Expression::NormalCC);}

            // Fast conditionals
            else if(Expression::find("&&==")) {numeric = logical(); result = Operators::operatorEQ(result, numeric, Expression::FastCC);}
            else if(Expression::find("&&="))  {numeric = logical(); result = Operators::operatorEQ(result, numeric, Expression::FastCC);}
            else if(Expression::find("&&<>")) {numeric = logical(); result = Operators::operatorNE(result, numeric, Expression::FastCC);}
            else if(Expression::find("&&<=")) {numeric = logical(); result = Operators::operatorLE(result, numeric, Expression::FastCC);}
            else if(Expression::find("&&>=")) {numeric = logical(); result = Operators::operatorGE(result, numeric, Expression::FastCC);}
            else if(Expression::find("&&<"))  {numeric = logical(); result = Operators::operatorLT(result, numeric, Expression::FastCC);}
            else if(Expression::find("&&>"))  {numeric = logical(); result = Operators::operatorGT(result, numeric, Expression::FastCC);}

            else return result;
        }
    }

    uint16_t getStringSrcAddr(Expression::Numeric& numeric)
    {
        uint16_t srcAddr = 0x0000;

        // String assignment from literal
        if(numeric._index == -1  &&  numeric._varType == Expression::String)
        {
            int index;
            getOrCreateConstString(numeric._text, index);
            if(index != -1) srcAddr = getStringVars()[index]._address;
        }
        // String assignment from var or const
        else if(numeric._index != -1)
        {
            switch(numeric._varType)
            {
                case Expression::Constant: srcAddr = getConstants()[numeric._index]._address;  break;
                case Expression::StrVar:   srcAddr = getStringVars()[numeric._index]._address; break;

                default: break;
            }
        }

        return srcAddr;
    }

    uint16_t getStringSrcAddr(std::string& strText)
    {
        uint16_t srcAddr = 0x0000;

        if(strText.back() == '$'  &&  Expression::isVarNameValid(strText))
        {
            int index = findStr(strText);
            if(index != -1)
            {
                srcAddr = getStringVars()[index]._address;
            }
            else
            {
                index = findConst(strText);
                if(index != -1)
                {
                    srcAddr = getConstants()[index]._address;
                }
            }
        }
        else if(Expression::isStringValid(strText))
        {
            // Strip quotes
            strText.erase(0, 1);
            strText.erase(strText.size()-1, 1);

            int index;
            getOrCreateConstString(strText, index);
            if(index != -1) srcAddr = getStringVars()[index]._address;
        }

        return srcAddr;
    }

    bool handleStrings(CodeLine& codeLine, int codeLineIndex, Expression::Numeric& numeric, uint32_t expressionType)
    {
        if(codeLine._text.size() < 2) return false;

        int dstIndex = codeLine._varIndex;
        if(dstIndex == -1)
        {
            fprintf(stderr, "Compiler::handleStrings() : Syntax error in '%s' on line %d\n", codeLine._text.c_str(), codeLineIndex);
            //_PAUSE_;
            _compilingError = true;
            return false;
        }

        bool isStrExpression = ((expressionType >= Expression::HasStrings)  &&  (expressionType <= Expression::IsStringExpression));
        if(isStrExpression  ||  numeric._varType == Expression::Str2Var)
        {
            std::vector<std::string> tokens = Expression::tokenise(codeLine._expression, "+", false);
            if(tokens.size() == 1)
            {
                // String assignment, from var or const or literal
                uint16_t srcAddr = getStringSrcAddr(numeric);
                if(srcAddr == 0x0000  &&  numeric._varType != Expression::Str2Var)
                {
                    fprintf(stderr, "Compiler::handleStrings() : Syntax error in '%s' on line %d\n", codeLine._text.c_str(), codeLineIndex);
                    //_PAUSE_;
                    _compilingError = true;
                    return false;
                }

                // No need to copy if src and dst are equal
                uint16_t dstAddr = _stringVars[dstIndex]._address;
                if(srcAddr != dstAddr)
                {
                    if(numeric._varType == Expression::Str2Var)
                    {
                        emitVcpuAsm("LDW", Expression::byteToHexString(uint8_t(Compiler::getTempVarStart())), false, codeLineIndex);
                    }
                    else
                    {
                        emitVcpuAsm("LDWI", Expression::wordToHexString(srcAddr), false, codeLineIndex);
                    }
                    
                    emitVcpuAsm("STW", "strSrcAddr", false, codeLineIndex);
                    emitVcpuAsm("LDWI", Expression::wordToHexString(dstAddr), false, codeLineIndex);
                    emitVcpuAsm("%StringCopy", "", false, codeLineIndex);
                }

                return true;
            }
            // TODO: doesn't work with string arrays
            // String concatenation
            else
            {
                if(tokens.size() < 2)
                {
                    fprintf(stderr, "Compiler::handleStrings() : Syntax error in '%s' on line %d\n", codeLine._text.c_str(), codeLineIndex);
                    //_PAUSE_;
                    _compilingError = true;
                    return false;
                }

                // Source string addresses, (extra 0x0000 delimiter used by VASM runtime)
                std::vector<uint16_t> strAddrs(tokens.size() + 1, 0x0000);
                for(int i=0; i<int(tokens.size()); i++)
                {
                    Expression::stripNonStringWhitespace(tokens[i]);
                    strAddrs[i] = getStringSrcAddr(tokens[i]);
                    if(strAddrs[i] == 0x0000)
                    {
                        fprintf(stderr, "Compiler::handleStrings() : Syntax error in '%s' on line %d\n", codeLine._text.c_str(), codeLineIndex);
                        //_PAUSE_;
                        _compilingError = true;
                        return false;
                    }
                }

                // Source string addresses LUT
                uint16_t lutAddress;
                if(!Memory::getFreeRAM(Memory::FitDescending, int(strAddrs.size()*2), USER_CODE_START, _runtimeStart, lutAddress))
                {
                    fprintf(stderr, "Compiler::handleStrings() : Not enough RAM for string concatenation LUT of size %d\n", int(strAddrs.size()));
                    return false;
                }
                _codeLines[codeLineIndex]._strConcatLut = {lutAddress, strAddrs}; // save LUT in global codeLine not local copy

                // Destination address
                uint16_t dstAddr = _stringVars[dstIndex]._address;

                // TODO: fix this bodge, (this is used by the Optimiser to remove a spurious STW)
                emitVcpuAsm("LDW", Expression::byteToHexString(uint8_t(getTempVarStart())), false, codeLineIndex);

                // Concatenate multiple source strings to string work area
                emitVcpuAsm("LDWI", Expression::wordToHexString(lutAddress), false, codeLineIndex);
                emitVcpuAsm("STW", "strLutAddr", false, codeLineIndex);
                emitVcpuAsm("LDWI", Expression::wordToHexString(getStrWorkArea()), false, codeLineIndex);
                emitVcpuAsm("%StringConcat", "", false, codeLineIndex);

                // Copy work area to dst string
                emitVcpuAsm("LDWI", Expression::wordToHexString(getStrWorkArea()), false, codeLineIndex);
                emitVcpuAsm("STW", "strSrcAddr", false, codeLineIndex);
                emitVcpuAsm("LDWI", Expression::wordToHexString(dstAddr), false, codeLineIndex);
                emitVcpuAsm("%StringCopy", "", false, codeLineIndex);

                return true;
            }
        }

        return false;
    }

    StatementResult createVasmCode(CodeLine& codeLine, int codeLineIndex)
    {
        // Check for subroutine start, make sure PUSH is emitted only once, even for multi-statement lines, (codeLine is a local copy of each statement within a multi-statement codeLine)
        if(!_codeLines[_currentCodeLineIndex]._pushEmitted  &&  codeLine._labelIndex >= 0  &&  _labels[codeLine._labelIndex]._gosub)
        {
            _codeLines[_currentCodeLineIndex]._pushEmitted = true;
            emitVcpuAsm("PUSH", "", false, codeLineIndex);
        }

        // Specific parsing requirements for most keywords, (*NOT* functions), some keywords like IF will also parse multi-statements
        for(int i=0; i<int(codeLine._tokens.size()); i++)
        {
            Keywords::KeywordFuncResult result;
            Keywords::KeywordResult keywordResult = Keywords::handleKeywords(codeLine, codeLine._tokens[i], codeLineIndex, i, result);
            if(keywordResult == Keywords::KeywordError) return StatementError;

            // Search for keyword, if found return it's statement type result
            std::string token = codeLine._tokens[i];
            Expression::strToUpper(token);
            if(Keywords::getKeywords().find(token) != Keywords::getKeywords().end()) return Keywords::getKeywords()[token]._result;
        }

        int varIndexRhs = -1, constIndexRhs = -1, strIndexRhs = -1;
        uint32_t expressionType = isExpression(codeLine._expression, varIndexRhs, constIndexRhs, strIndexRhs);
        if(expressionType == Expression::IsInvalid) return StatementError;

        // Parse expression, handles ints, strings, operators and functions
        bool isStringVar = false;
        Expression::Numeric numeric;
        if(codeLine._varIndex != -1)
        {
            std::string name;
            Expression::VarType varType = Expression::Number;
            switch(codeLine._varType)
            {
                case VarInt16: varType = Expression::IntVar; name = _integerVars[codeLine._varIndex]._name;                     break;
                case VarStr:   varType = Expression::StrVar; name = _stringVars[codeLine._varIndex]._name;  isStringVar = true; break;

                default: break;
            }
            
            // Output variable, (functions can access this variable within parse())
            numeric = Expression::Numeric(0, int16_t(codeLine._varIndex), true, false, false, varType, Expression::BooleanCC, Expression::Int16Both, name, std::string(""));
        }
        Expression::parse(codeLine._expression, codeLineIndex, numeric);

        // String assignments and operators, (exit early for a string result)
        bool stringResult = handleStrings(codeLine, codeLineIndex, numeric, expressionType);
        if(stringResult  ||  isStringVar) return StringStatementParsed;

        // Update result variable
        if(codeLine._varIndex != -1)
        {
            // Expression that contains one or more int vars
            bool intVarAssignment = (expressionType & Expression::HasIntVars);
            updateVar(int16_t(std::lround(numeric._value)), codeLine, codeLine._varIndex, intVarAssignment);
        }

        // TODO: only works with Int16, fix for all var types
        // Variable assignment
        if(codeLine._varIndex != -1)
        {
            // Assignment with a var expression
            if(codeLine._containsVars)
            {
                // Try and optimise LDW away if possible
                if(varIndexRhs >= 0  &&  _integerVars[varIndexRhs]._varType != VarArray1  &&  !(expressionType & Expression::HasOperators)  &&  !(expressionType & Expression::HasFunctions))
                {
                    switch(numeric._int16Byte)
                    {
                        case Expression::Int16Low:  emitVcpuAsm("LD",  "_" + _integerVars[varIndexRhs]._name,          false, codeLineIndex); break;
                        case Expression::Int16High: emitVcpuAsm("LD",  "_" + _integerVars[varIndexRhs]._name + " + 1", false, codeLineIndex); break;
                        case Expression::Int16Both: emitVcpuAsm("LDW", "_" + _integerVars[varIndexRhs]._name,          false, codeLineIndex); break;

                        default: break;
                    }
                }

                if(_integerVars[codeLine._varIndex]._varType == VarArray1  ||  _integerVars[codeLine._varIndex]._varType == VarArray2  ||  _integerVars[codeLine._varIndex]._varType == VarArray3)
                {
                    writeArrayVar(codeLine, codeLineIndex, codeLine._varIndex);
                }
                else
                {
                    switch(codeLine._int16Byte)
                    {
                        case Expression::Int16Low:  emitVcpuAsm("ST",  "_" + _integerVars[codeLine._varIndex]._name,          false, codeLineIndex); break;
                        case Expression::Int16High: emitVcpuAsm("ST",  "_" + _integerVars[codeLine._varIndex]._name + " + 1", false, codeLineIndex); break;
                        case Expression::Int16Both: emitVcpuAsm("STW", "_" + _integerVars[codeLine._varIndex]._name,          false, codeLineIndex); break;

                        default: break;
                    }
                }
            }
            // Standard assignment
            else
            {
                // Skip for functions unless function parameter was a literal
                if(!(expressionType & Expression::HasFunctions)  ||  numeric._varType == Expression::Number)
                {
                    // 8bit constants
                    if(_integerVars[codeLine._varIndex]._data >=0  &&  _integerVars[codeLine._varIndex]._data <= 255)
                    {
                        emitVcpuAsm("LDI", std::to_string(_integerVars[codeLine._varIndex]._data), false, codeLineIndex);
                    }
                    // 16bit constants
                    else
                    {
                        emitVcpuAsm("LDWI", std::to_string(_integerVars[codeLine._varIndex]._data), false, codeLineIndex);
                    }
                }

                if(_integerVars[codeLine._varIndex]._varType == VarArray1  ||  _integerVars[codeLine._varIndex]._varType == VarArray2  ||  _integerVars[codeLine._varIndex]._varType == VarArray3)
                {
                    writeArrayVar(codeLine, codeLineIndex, codeLine._varIndex);
                }
                else
                {
                    switch(codeLine._int16Byte)
                    {
                        case Expression::Int16Low:  emitVcpuAsm("ST",  "_" + _integerVars[codeLine._varIndex]._name,          false, codeLineIndex); break;
                        case Expression::Int16High: emitVcpuAsm("ST",  "_" + _integerVars[codeLine._varIndex]._name + " + 1", false, codeLineIndex); break;
                        case Expression::Int16Both: emitVcpuAsm("STW", "_" + _integerVars[codeLine._varIndex]._name,          false, codeLineIndex); break;

                        default: break;
                    }
                }
            }
        }

        return StatementExpression;
    }

    StatementResult parseMultiStatements(const std::string& code, CodeLine& codeLine, int codeLineIndex, int& varIndex, int& strIndex)
    {
        // Make a local copy, otherwise original tokens are destroyed
        CodeLine codeline = codeLine;

        // Tokenise and parse multi-statement lines
        StatementResult statementResult = StatementSuccess;
        std::vector<std::string> tokens = Expression::tokenise(code, ':', false);
        for(int j=0; j<int(tokens.size()); j++)
        {
REDO:
            createCodeLine(tokens[j], 0, _codeLines[codeLineIndex]._labelIndex, -1, Expression::Int16Both, false, codeline);
            if(_codeLines[codeLineIndex]._dontParse) return StatementSuccess;

            // Create vars and vasm
            createCodeVar(codeline, codeLineIndex, varIndex);
            createCodeStr(codeline, codeLineIndex, strIndex);
            statementResult = createVasmCode(codeline, codeLineIndex);
            if(statementResult == StatementError) break;

            // Some commands, (such as FN), modify the BASIC source and cause a re-evaluation
            if(statementResult == RedoStatementParse)
            {
                goto REDO;
            }

            // Some commands, (such as IF), process multi-statements themselves
            if(statementResult == MultiStatementParsed) break;
        }

        return statementResult;
    }

    void addLabelToJumpCC(std::vector<VasmLine>& vasm, const std::string& label)
    {
        for(int i=0; i<int(vasm.size()); i++)
        {
            if(vasm[i]._code.substr(0, sizeof("Jump")-1) == "Jump"  ||  vasm[i]._code.substr(0, sizeof("B")-1) == "B")
            {
                vasm[i]._code += label;
                return;
            }
        }
    }

    void addLabelToJump(std::vector<VasmLine>& vasm, const std::string& label)
    {
        for(int i=0; i<int(vasm.size()); i++)
        {
            if(_codeRomType >= Cpu::ROMv5a)
            {
                if(vasm[i]._code.find("CALLI_JUMP") != std::string::npos)
                {
                    vasm[i]._opcode = "CALLI";
                    vasm[i]._code = "CALLI" + std::string(OPCODE_TRUNC_SIZE - (sizeof("CALLI")-1), ' ') + label;
                    return;
                }
            }
            else
            {
                if(vasm[i]._code.find("BRA_JUMP") != std::string::npos)
                {
                    vasm[i]._opcode = "BRA";
                    vasm[i]._code = "BRA" + std::string(OPCODE_TRUNC_SIZE - (sizeof("BRA")-1), ' ') + label;
                    return;
                }
                else if(vasm[i]._code.find("LDWI_JUMP") != std::string::npos)
                {
                    vasm[i]._opcode = "LDWI";
                    vasm[i]._code = "LDWI" + std::string(OPCODE_TRUNC_SIZE - (sizeof("LDWI")-1), ' ') + label;
                    return;
                }
            }
        }
    }


    bool parseCode(void)
    {
        CodeLine codeLine;

        // Add END to code
        if(_codeLines.size())
        {
            bool foundEnd = false;
            for(int i=0; i<int(_codeLines[_codeLines.size() - 1]._tokens.size()); i++)
            {
                std::string token = _codeLines[_codeLines.size() - 1]._tokens[i];
                Expression::strToUpper(token);
                if(token =="END")
                {
                    foundEnd = true;
                    break;
                }
            }

            if(!foundEnd)
            {
                if(createCodeLine("END", 0, -1, -1, Expression::Int16Both, false, codeLine)) _codeLines.push_back(codeLine);
            }
        }

        // Parse code creating vars and vasm code, (BASIC code lines were created in ParseLabels())
        int varIndex, strIndex;
        for(int i=0; i<int(_codeLines.size()); i++)
        {
            _currentCodeLineIndex = i;

            // First line of BASIC code is always a dummy INIT line, ignore it
            if(i > 0  &&  _codeLines[i]._code.size() >= 2)
            {
                // Adjust label address
                if(_codeLines[i]._labelIndex >= 0) _labels[_codeLines[i]._labelIndex]._address = _vasmPC;

                // Multi-statements
                StatementResult statementResult = parseMultiStatements(_codeLines[i]._code, codeLine, i, varIndex, strIndex);
                if(statementResult == StatementError) return false;
            }
        }

        // Check for inserted sys init funcs and adjust vASM and label addresses
        for(int i=0; i<int(_codeLines[0]._vasm.size()); i++)
        {
            for(int j=0; j<int(_sysInitNames.size()); j++)
            {
                if(_codeLines[0]._vasm[i]._opcode == _sysInitNames[j])
                {
                    uint16_t address = _codeLines[0]._vasm[i]._address;
                    Validater::adjustLabelAddresses(address, SYS_INIT_FUNC_LEN);
                    Validater::adjustVasmAddresses(0, address, SYS_INIT_FUNC_LEN);
                    _codeLines[0]._vasm[i]._address = address;
                    break;
                }
            }
        }

        return true;
    }


    void outputReservedWords(void)
    {
        std::string line = "_startAddress_ ";
        Expression::addString(line, LABEL_TRUNC_SIZE - int(line.size()));
        line += "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(USER_CODE_START) + "\n";
        _output.push_back(line);
    }

    void outputLabels(void)
    {
        std::string line;

        _output.push_back("; Labels\n");

        // BASIC labels
        for(int i=0; i<int(_labels.size()); i++)
        {
            std::string address = Expression::wordToHexString(_labels[i]._address);
            _output.push_back(_labels[i]._output + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + address + "\n");
            _equateLabels.push_back(_labels[i]._name);
        }

        // Internal labels
        for(int i=0; i<int(_codeLines.size()); i++)
        {
            for(int j=0; j<int(_codeLines[i]._vasm.size()); j++)
            {
                std::string internalLabel = _codeLines[i]._vasm[j]._internalLabel;
                if(internalLabel.size())
                {
                    std::string address = Expression::wordToHexString(_codeLines[i]._vasm[j]._address);
                    _output.push_back(internalLabel + std::string(LABEL_TRUNC_SIZE - internalLabel.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + address + "\n");
                    _internalLabels.push_back({_codeLines[i]._vasm[j]._address, internalLabel});
                    _equateLabels.push_back(internalLabel);
                }
            }
        }

        // Check for label conflicts
        for(int i=0; i<int(_codeLines.size()); i++)
        {
            for(int j=0; j<int(_codeLines[i]._vasm.size()); j++)
            {
                // BASIC label conflict
                for(int k=0; k<int(_internalLabels.size()); k++)
                {
                    int labelIndex = findLabel(_internalLabels[k]._address);
                    if(labelIndex >= 0)
                    {   
                        std::string basicLabel = _labels[labelIndex]._output;
                        Expression::stripWhitespace(basicLabel);
                        Expression::replaceText(_codeLines[i]._vasm[j]._code, _internalLabels[k]._name, basicLabel);

                        // BASIC labels override internal labels
                        if(_codeLines[i]._vasm[j]._address == _labels[labelIndex]._address)
                        {
                            _codeLines[i]._vasm[j]._internalLabel = basicLabel;
                        }
                    }

                    // Discarded internal label
                    for(int l=0; l<int(_discardedLabels.size()); l++)
                    {
                        // Match on unique address embedded within names or the real address
                        std::string internalName = _internalLabels[k]._name.substr(_internalLabels[k]._name.size() - 4, 4);
                        std::string discardedName = _discardedLabels[l]._name.substr(_discardedLabels[l]._name.size() - 4, 4);
                        if(internalName == discardedName  ||  _internalLabels[k]._address == _discardedLabels[l]._address)
                        {
                            Expression::replaceText(_codeLines[i]._vasm[j]._code, _discardedLabels[l]._name, _internalLabels[k]._name);
                        }
                    }
                }
            }
        }

        _output.push_back("\n");
    }

    void outputConsts(void)
    {
        _output.push_back("; Constants\n");

        for(int i=0; i<int(_constants.size()); i++)
        {
            int16_t data = _constants[i]._data;
            std::string name = _constants[i]._name;
            std::string internalName = _constants[i]._internalName;
            ConstType constType = _constants[i]._constType;

            switch(constType)
            {
                case ConstInt16:
                {
                    _output.push_back(internalName + std::string(LABEL_TRUNC_SIZE - internalName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(data) + "\n");
                }
                break;
    
                case ConstStr:
                {
                }
                break;

                default: break;
            }
        }

        _output.push_back("\n");
    }

    void outputVars(void)
    {
        _output.push_back("; Variables\n");

        for(int varIndex=0; varIndex<int(_integerVars.size()); varIndex++)
        {
            switch(_integerVars[varIndex]._varType)
            {
                case VarInt16:
                {
                    std::string address = Expression::wordToHexString(_integerVars[varIndex]._address);
                    _output.push_back(_integerVars[varIndex]._output + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + address + "\n");
                }
                break;

                default: break;
            }
        }

        _output.push_back("\n");
    }

    void outputArrs(void)
    {
        _output.push_back("; Arrays\n");

        for(int varIndex=0; varIndex<int(_integerVars.size()); varIndex++)
        {
            switch(_integerVars[varIndex]._varType)
            {
                case VarArray1:
                {
                    std::string arrName = "_" + _integerVars[varIndex]._name + "_array";
                    _output.push_back(arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_integerVars[varIndex]._arrAddrs[0][0]) + "\n");
                    std::string dwString = arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');

                    // I array values
                    for(int i=0; i<_integerVars[varIndex]._arrSizes[2]; i++)
                    {
                        // Single initialisation value
                        if(_integerVars[varIndex]._arrInits.size() == 0)
                        {
                            dwString += Expression::wordToHexString(_integerVars[varIndex]._init) + " ";
                        }
                        // Multiple initialisation values
                        else
                        {
                            // Number of initialisation values may be smaller than array size
                            if(i < int(_integerVars[varIndex]._arrInits.size()))
                            {
                                dwString += Expression::wordToHexString(_integerVars[varIndex]._arrInits[i]) + " ";
                            }
                            // Use default initialisation value for the rest of the array
                            else
                            {
                                dwString += Expression::wordToHexString(_integerVars[varIndex]._init) + " ";
                            }
                        }
                    }
                    _output.push_back(dwString + "\n");
                }
                break;

                case VarArray2:
                {
                    std::string arrName = "_" + _integerVars[varIndex]._name + "_array";
                    _output.push_back(arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_integerVars[varIndex]._address) + "\n");
                    std::string dwString = arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');

                    // J array pointers
                    for(int j=0; j<_integerVars[varIndex]._arrSizes[1]; j++)
                    {
                        dwString += Expression::wordToHexString(_integerVars[varIndex]._arrAddrs[0][j]) + " ";
                    }
                    _output.push_back(dwString + "\n");

                    // J arrays
                    int initIndex = 0;
                    for(int j=0; j<_integerVars[varIndex]._arrSizes[1]; j++)
                    {
                        arrName = "_" + _integerVars[varIndex]._name + "_" + Expression::wordToHexString(_integerVars[varIndex]._arrAddrs[0][j]);
                        _output.push_back(arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_integerVars[varIndex]._arrAddrs[0][j]) + "\n");
                        dwString = arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');

                        // I array values
                        for(int i=0; i<_integerVars[varIndex]._arrSizes[2]; i++)
                        {
                            // Single initialisation value
                            if(_integerVars[varIndex]._arrInits.size() == 0)
                            {
                                dwString += Expression::wordToHexString(_integerVars[varIndex]._init) + " ";
                            }
                            // Multiple initialisation values
                            else
                            {
                                // Number of initialisation values may be smaller than array size
                                if(initIndex < int(_integerVars[varIndex]._arrInits.size()))
                                {
                                    dwString += Expression::wordToHexString(_integerVars[varIndex]._arrInits[initIndex++]) + " ";
                                }
                                // Use default initialisation value for the rest of the array
                                else
                                {
                                    dwString += Expression::wordToHexString(_integerVars[varIndex]._init) + " ";
                                }
                            }
                        }
                        _output.push_back(dwString + "\n");
                    }
                }
                break;

                case VarArray3:
                {
                    std::string arrName = "_" + _integerVars[varIndex]._name + "_array";
                    _output.push_back(arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_integerVars[varIndex]._address) + "\n");
                    std::string dwString = arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');

                    // K array pointers
                    for(int k=0; k<_integerVars[varIndex]._arrSizes[0]; k++)
                    {
                        dwString += Expression::wordToHexString(_integerVars[varIndex]._arrLut[k]) + " ";
                    }
                    _output.push_back(dwString + "\n");

                    // K * J array pointers
                    for(int k=0; k<_integerVars[varIndex]._arrSizes[0]; k++)
                    {
                        arrName = "_" + _integerVars[varIndex]._name + "_lut_" + std::to_string(k);
                        _output.push_back(arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_integerVars[varIndex]._arrLut[k]) + "\n");
                        dwString = arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');

                        // J array pointers
                        for(int j=0; j<_integerVars[varIndex]._arrSizes[1]; j++)
                        {
                            dwString += Expression::wordToHexString(_integerVars[varIndex]._arrAddrs[k][j]) + " ";
                        }
                        _output.push_back(dwString + "\n");
                    }

                    // K * J arrays
                    int initIndex = 0;
                    for(int k=0; k<_integerVars[varIndex]._arrSizes[0]; k++)
                    {
                        // J arrays
                        for(int j=0; j<_integerVars[varIndex]._arrSizes[1]; j++)
                        {
                            arrName = "_" + _integerVars[varIndex]._name + "_" + Expression::wordToHexString(_integerVars[varIndex]._arrAddrs[k][j]);
                            _output.push_back(arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_integerVars[varIndex]._arrAddrs[k][j]) + "\n");
                            dwString = arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');

                            // I array values
                            for(int i=0; i<_integerVars[varIndex]._arrSizes[2]; i++)
                            {
                                // Single initialisation value
                                if(_integerVars[varIndex]._arrInits.size() == 0)
                                {
                                    dwString += Expression::wordToHexString(_integerVars[varIndex]._init) + " ";
                                }
                                // Multiple initialisation values
                                else
                                {
                                    // Number of initialisation values may be smaller than array size
                                    if(initIndex < int(_integerVars[varIndex]._arrInits.size()))
                                    {
                                        dwString += Expression::wordToHexString(_integerVars[varIndex]._arrInits[initIndex++]) + " ";
                                    }
                                    // Use default initialisation value for the rest of the array
                                    else
                                    {
                                        dwString += Expression::wordToHexString(_integerVars[varIndex]._init) + " ";
                                    }
                                }
                            }
                            _output.push_back(dwString + "\n");
                        }
                    }
                }
                break;

                default: break;
            }
        }

        _output.push_back("\n");
    }

    bool outputStrs(void)
    {
        _output.push_back("; Strings\n");

        // User strings
        for(int i=0; i<int(_stringVars.size()); i++)
        {
            // Normal strings
            if(_stringVars[i]._varType == VarStr  &&  !_stringVars[i]._constant)
            {
                _output.push_back(_stringVars[i]._output + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_stringVars[i]._address) + "\n");
                _output.push_back(_stringVars[i]._output + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ') + std::to_string(_stringVars[i]._size) + " '" + _stringVars[i]._text + "' 0\n");
            }
            // Array of strings
            else if(_stringVars[i]._varType == VarStr2)
            {
                std::string arrName = "_" + _stringVars[i]._name;
                _output.push_back(arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_stringVars[i]._address) + "\n");
                std::string dwString = arrName + std::string(LABEL_TRUNC_SIZE - arrName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');

                // J array pointers
                for(int j=0; j<int(_stringVars[i]._arrAddrs.size()); j++)
                {
                    dwString += Expression::wordToHexString(_stringVars[i]._arrAddrs[j]) + " ";
                }
                _output.push_back(dwString + "\n");

                // J strings
                std::string defaultStr = (_stringVars[i]._arrInits.size()) ? _stringVars[i]._arrInits.back() : _stringVars[i]._text;
                for(int j=0; j<int(_stringVars[i]._arrAddrs.size()); j++)
                {
                    std::string strName = "_" + _stringVars[i]._name + "_" + Expression::wordToHexString(_stringVars[i]._arrAddrs[j]);
                    _output.push_back(strName + std::string(LABEL_TRUNC_SIZE - strName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_stringVars[i]._arrAddrs[j]) + "\n");

                    std::string initStr = (j < int(_stringVars[i]._arrInits.size())) ? _stringVars[i]._arrInits[j] : defaultStr;
                    _output.push_back(strName + std::string(LABEL_TRUNC_SIZE - strName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ') + std::to_string(initStr.size()) + " '" + initStr + "' 0\n");
                }
            }
        }
        _output.push_back("\n");

        _output.push_back("; Constant Strings\n");

        // Constant strings
        for(int i=0; i<int(_stringVars.size()); i++)
        {
            // Normal strings
            if(_stringVars[i]._varType == VarStr  &&  _stringVars[i]._constant)
            {
                _output.push_back(_stringVars[i]._output + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_stringVars[i]._address) + "\n");
                _output.push_back(_stringVars[i]._output + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ') + std::to_string(_stringVars[i]._size) + " '" + _stringVars[i]._text + "' 0\n");
            }
        }
        _output.push_back("\n");

        return true;
    }

    bool outputDATA(void)
    {
        _output.push_back("; Data\n");

        // Output DATA fields
        for(int i=0; i<int(_dataObjects.size()); i++)
        {
            DataObject* pObject = _dataObjects[i].get();
            switch(pObject->_dataType)
            {
                case DataInteger:
                {
                    DataInt* pData = (DataInt*)pObject;
                    int16_t var = pData->_data;
                    int size = 2;
                    if(!Memory::getFreeRAM(Memory::FitDescending, size, USER_CODE_START, _runtimeStart, pData->_address, true))
                    {
                        fprintf(stderr, "Compiler::outputDATA() : Not enough RAM for data of size %d\n", size);
                        return false;
                    }
                    std::string defName = "_data_" + Expression::wordToHexString(pData->_address);
                    _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(pData->_address) + "\n");
                    _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ') + std::to_string(var) + "\n");
                }
                break;

                case DataString:
                {
                    DataStr* pData = (DataStr*)pObject;
                    std::string str = pData->_data;
                    int size = int(str.size()) + 2;
                    if(!Memory::getFreeRAM(Memory::FitDescending, size, USER_CODE_START, _runtimeStart, pData->_address, true))
                    {
                        fprintf(stderr, "Compiler::outputDATA() : Not enough RAM for data of size %d\n", size);
                        return false;
                    }
                    std::string defName = "_data_" + Expression::wordToHexString(pData->_address);
                    _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(pData->_address) + "\n");
                    _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ') + std::to_string(size-2) + " '" + str + "' 0\n");
                }
                break;

                default: break;
            }
        }

        // Output DATA address LUT
        uint16_t address = 0x0000;
        int size = int(_dataObjects.size() + 1)*2; // +1 for the terminating zero
        if(!Memory::getFreeRAM(Memory::FitDescending, size, USER_CODE_START, _runtimeStart, address, false))
        {
            fprintf(stderr, "Compiler::outputDATA() : Not enough RAM for data LUT of size %d\n", size);
            return false;
        }
        std::string defName = "_data_";
        _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(address) + "\n");
        std::string dwString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
        for(int i=0; i<int(_dataObjects.size()); i++)
        {
            DataObject* pObject = _dataObjects[i].get();
            dwString += Expression::wordToHexString(pObject->_address) + " ";
        }
        _output.push_back(dwString + "0\n"); // terminating 0

        // Output DATA index
        address = 0x0000;
        size = 2;
        if(!Memory::getFreeRAM(Memory::FitDescending, size, USER_CODE_START, _runtimeStart, address, false))
        {
            fprintf(stderr, "Compiler::outputDATA() : Not enough RAM for data LUT of size %d\n", size);
            return false;
        }
        defName = "_dataIndex_";
        _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(address) + "\n");
        _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ') + "0\n");
        _output.push_back("\n");

        return true;
    }

    bool outputTIME(void)
    {
        _output.push_back("; Time\n");

        // Time array and string
        if(_createTimeData)
        {
            int timeArraySize = 3;
            uint16_t timeArrayAddress;
            if(!Memory::getFreeRAM(Memory::FitDescending, timeArraySize, USER_CODE_START, _runtimeStart, timeArrayAddress))
            {
                fprintf(stderr, "Compiler::outputTIME() : Not enough RAM for time array of size %d\n", timeArraySize);
                return false;
            }
            std::string defName = "_timeArray_";
            _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(timeArrayAddress) + "\n");
            std::string dbString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            _output.push_back(dbString + "00 00 00\n");

            int timeStringSize = 10;
            uint16_t timeStringAddress;
            if(!Memory::getFreeRAM(Memory::FitDescending, timeStringSize, USER_CODE_START, _runtimeStart, timeStringAddress))
            {
                fprintf(stderr, "Compiler::outputTIME() : Not enough RAM for time string of size %d\n", timeStringSize);
                return false;
            }
            defName = "_timeString_";
            _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(timeStringAddress) + "\n");
            dbString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            _output.push_back(dbString + std::to_string(timeStringSize - 2) + " '00:00:00' 0\n");
        }
        _output.push_back("\n");

        return true;
    }

    bool outputSpriteDef(int spriteId, int numStripeChunks, uint16_t address, int& dataIndex)
    {
        std::string defName = "def_sprites_" + Expression::wordToHexString(address);
        _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(address) + "\n");
        std::string dbString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');

        // Output a stripe of chunks
        for(int i=0; i<numStripeChunks; i++)
        {
            for(int j=0; j<SPRITE_CHUNK_SIZE; j++)
            {
                dbString += std::to_string(_defDataSprites[spriteId]._data[dataIndex++]) + " ";
            }
        }

        // Output stripe delimiter
        dbString += std::to_string(_defDataSprites[spriteId]._data[dataIndex++]) + " ";
        _output.push_back(dbString + "\n");

        return true;
    }
    bool outputDefs(void)
    {
        // Create def byte data
        _output.push_back("; Define Bytes\n");
        for(int i=0; i<int(_defDataBytes.size()); i++)
        {
            std::string defName = "def_bytes_" + Expression::wordToHexString(_defDataBytes[i]._address);
            _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_defDataBytes[i]._address) + "\n");
            
            std::string dbString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            for(int j=0; j<int(_defDataBytes[i]._data.size()); j++)
            {
                dbString += std::to_string(_defDataBytes[i]._data[j]) + " ";
            }
            _output.push_back(dbString + "\n");
        }
        _output.push_back("\n");

        // Create def word data
        _output.push_back("; Define Words\n");
        for(int i=0; i<int(_defDataWords.size()); i++)
        {
            std::string defName = "def_words_" + Expression::wordToHexString(_defDataWords[i]._address);
            _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(_defDataWords[i]._address) + "\n");
            
            std::string dwString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            for(int j=0; j<int(_defDataWords[i]._data.size()); j++)
            {
                dwString += std::to_string(_defDataWords[i]._data[j]) + " ";
            }
            _output.push_back(dwString + "\n");
        }
        _output.push_back("\n");

        // Create def image data
        _output.push_back("; Define Images\n");
        for(int i=0; i<int(_defDataImages.size()); i++)
        {
            uint16_t offset = 0x0000;
            uint16_t address = _defDataImages[i]._address;
            uint16_t width = _defDataImages[i]._width;

            // For each scanline of image data
            for(int j=0; j<_defDataImages[i]._height; j++)
            {
                // TODO: find a way around this monster hack, 'Loader.gcl' is resident at these addresses when loading *.gt1 files, you can overwrite these locations only AFTER the loading process has finished
                if(address + offset < 0x5900  ||  address + offset > 0x5BFF)
                {
                    std::string defName = "def_images_" + Expression::wordToHexString(address + offset);
                    _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(address + offset) + "\n");
            
                    std::string dbString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                    for(int k=0; k<width; k++)
                    {
                        dbString += std::to_string(_defDataImages[i]._data[j * width  +  k]) + " ";
                    }
                    _output.push_back(dbString + "\n");
                }

                offset += _defDataImages[i]._stride;
            }
        }
        _output.push_back("\n");

        // Create def sprite data
        _output.push_back("; Define Sprites\n");
        for(auto it=_defDataSprites.begin(); it!=_defDataSprites.end(); ++it)
        {
            int spriteId = it->first;

            // Skip invalid sprite
            if(it->second._stripeAddrs.size() == 0)
            {
                fprintf(stderr, "Compiler::outputDefs() : Warning, sprite %d is missing stripe addresses\n", spriteId);
                continue;
            }

            int dataIndex = 0;
            uint16_t numColumns = it->second._numColumns;
            uint16_t numStripesPerCol = it->second._numStripesPerCol;
            uint16_t numStripeChunks = it->second._numStripeChunks;
            uint16_t remStripeChunks = it->second._remStripeChunks;
            uint16_t isInstanced = it->second._isInstanced;

            if(!isInstanced)
            {
                // For each column of sprite data
                for(int j=0; j<numColumns; j++)
                {
                    // Multiple stripes per column
                    if(numStripesPerCol > 1)
                    {
                        for(int k=0; k<numStripesPerCol-1; k++)
                        {
                            uint16_t address = it->second._stripeAddrs[j*numStripesPerCol*2 + k*2];
                            outputSpriteDef(spriteId, numStripeChunks, address, dataIndex);
                        }
                        uint16_t address = it->second._stripeAddrs[j*numStripesPerCol*2 + (numStripesPerCol-1)*2];
                        outputSpriteDef(spriteId, remStripeChunks, address, dataIndex);
                    }
                    // Single stripe per column
                    else
                    {
                        uint16_t address = it->second._stripeAddrs[j*2];
                        outputSpriteDef(spriteId, numStripeChunks, address, dataIndex);
                    }
                }
            }
        }
        _output.push_back("\n");

        // Create def font data
        _output.push_back("; Define Fonts\n");
        if(_defDataFonts.size() > 0)
        {
            int fontIdSize = 1;
            uint16_t fontIdAddress;
            if(!Memory::getFreeRAM(Memory::FitDescending, fontIdSize, USER_CODE_START, _runtimeStart, fontIdAddress))
            {
                fprintf(stderr, "Compiler::outputDefs() : Not enough RAM for font id var of size %d\n", fontIdSize);
                return false;
            }
            std::string defName = "_fontId_";
            _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(fontIdAddress) + "\n");
            std::string dbString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            _output.push_back(dbString + "0\n");
        }
        for(auto it=_defDataFonts.begin(); it!=_defDataFonts.end(); ++it)
        {
            int fontId = it->first;

            // Skip invalid font
            if(it->second._mapping.size() == 0)
            {
                fprintf(stderr, "Compiler::outputDefs() : Warning, font %d is missing mapping table\n", fontId);
                continue;
            }

            // Font mapping table, (null means 32 to 127 of the ASCII set is represented, so a mapping table is not required)
            uint16_t address = it->second._mapAddr;
            if(address)
            {
                int mapSize = int(it->second._mapping.size());
                std::string defName = "def_map_" + Expression::wordToHexString(address);
                _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(address) + "\n");
                std::string dbString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                for(int i=0; i<mapSize; i++)
                {
                    uint8_t mapData = it->second._mapping[i];
                    dbString += std::to_string(mapData) + " ";
                }
                _output.push_back(dbString + "\n");
            }

            // For each char of font data
            int numChars = int(it->second._data.size());
            for(int i=0; i<numChars; i++)
            {
                address = it->second._charAddrs[i];

                std::string defName = "def_char_" + Expression::wordToHexString(address);
                _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(address) + "\n");
                std::string dbString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');

                // Output each char
                std::vector<uint8_t>& charData = it->second._data[i];
                for(int j=0; j<int(charData.size()); j++)
                {
                    dbString += std::to_string(charData[j]) + " ";
                }
                _output.push_back(dbString + "\n");
            }

            // Baseline for each char, (shared by all chars in one font)
            address = it->second._baseAddr;
            uint16_t fgbgColour = it->second._fgbgColour;
            std::string defName = "def_baseline_" + Expression::wordToHexString(address);
            _output.push_back(defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(address) + "\n");
            std::string dbString = defName + std::string(LABEL_TRUNC_SIZE - defName.size(), ' ') + "DB" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            for(int i=0; i<6; i++) dbString += Expression::byteToHexString(fgbgColour & 0x00FF) + " ";
            dbString += "255";
            _output.push_back(dbString + "\n");
        }
        _output.push_back("\n");

        return true;
    }

    bool outputLuts(void)
    {
        _output.push_back("; Lookup Tables\n");

        // Numeric labels LUT
        if(_createNumericLabelLut)
        {
            std::vector<uint16_t> numericLabels;
            std::vector<uint16_t> numericAddresses;
            for(int i=0; i<int(_labels.size()); i++)
            {
                if(_labels[i]._numeric)
                {
                    uint16_t numericLabel;
                    if(!Expression::stringToU16(_labels[i]._name, numericLabel))
                    {
                        fprintf(stderr, "Compiler::outputLuts() : bad numeric label %s : %04x\n", _labels[i]._name.c_str(), numericLabel);
                        return false;
                    }

                    numericLabels.push_back(numericLabel);
                    numericAddresses.push_back(_labels[i]._address);
                }
            }

            if(numericLabels.size())
            {
                // Create numeric labels LUT, (delimited by 0)
                int lutSize = int(numericLabels.size())*2;
                uint16_t lutAddress;
                if(!Memory::getFreeRAM(Memory::FitDescending, lutSize + 2, USER_CODE_START, _runtimeStart, lutAddress))
                {
                    fprintf(stderr, "Compiler::outputLuts() : Not enough RAM for numeric labels LUT of size %d\n", lutSize + 2);
                    return false;
                }

                std::string lutName = "_lut_numericLabs";
                _output.push_back(lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(lutAddress) + "\n");
            
                std::string dwString = lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                for(int i=0; i<int(numericLabels.size()); i++)
                {
                    dwString += std::to_string(numericLabels[i]) + " ";
                }
                _output.push_back(dwString + "0x0000\n");

                // Create numeric addresses LUT, (same size as above, but no delimiter)
                if(!Memory::getFreeRAM(Memory::FitDescending, lutSize, USER_CODE_START, _runtimeStart, lutAddress))
                {
                    fprintf(stderr, "Compiler::outputLuts() : Not enough RAM for numeric addresses LUT of size %d\n", lutSize);
                    return false;
                }

                lutName = "_lut_numericAddrs";
                _output.push_back(lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(lutAddress) + "\n");
            
                dwString = lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                for(int i=0; i<int(numericAddresses.size()); i++)
                {
                    dwString += Expression::wordToHexString(numericAddresses[i]) + " ";
                }
                _output.push_back(dwString + "\n");
            }
        }

        // ON GOTO GOSUB label LUTs
        for(int i=0; i<int(_codeLines.size()); i++)
        {
            // Output LUT if it exists
            int lutSize = int(_codeLines[i]._onGotoGosubLut._lut.size());
            uint16_t lutAddress = _codeLines[i]._onGotoGosubLut._address;
            std::string lutName = _codeLines[i]._onGotoGosubLut._name;
            if(lutSize)
            {
                _output.push_back(lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(lutAddress) + "\n");
            
                std::string dwString = lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                for(int j=0; j<lutSize; j++)
                {
                    int index = _codeLines[i]._onGotoGosubLut._lut[j];
                    if(index == -1) fprintf(stderr, "Compiler::outputLuts() : Warning, label index is invalid for LUT entry %d\n", j);

                    uint16_t labelAddress = _labels[index]._address;
                    dwString += Expression::wordToHexString(labelAddress) + " ";
                }
                _output.push_back(dwString + "\n");
            }
        }

        // String concatenation LUTs
        for(int i=0; i<int(_codeLines.size()); i++)
        {
            // Output LUT if it exists
            int lutSize = int(_codeLines[i]._strConcatLut._lut.size());
            uint16_t lutAddress = _codeLines[i]._strConcatLut._address;
            std::string lutName = "_concat_" + Expression::wordToHexString(lutAddress);
            if(lutSize)
            {
                _output.push_back(lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(lutAddress) + "\n");
            
                std::string dwString = lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                for(int j=0; j<lutSize; j++)
                {
                    uint16_t address = _codeLines[i]._strConcatLut._lut[j];
                    dwString += Expression::wordToHexString(address) + " ";
                }
                _output.push_back(dwString + "\n");
            }
        }

        // INPUT LUTs
        for(int i=0; i<int(_codeLines.size()); i++)
        {
            // Output varsLUT if it exists
            int varsSize = int(_codeLines[i]._inputLut._varsLut.size());
            uint16_t varsAddr = _codeLines[i]._inputLut._varsAddr;
            std::string varsName = "_inputVars_" + Expression::wordToHexString(varsAddr);
            if(varsSize)
            {
                _output.push_back(varsName + std::string(LABEL_TRUNC_SIZE - varsName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(varsAddr) + "\n");
            
                std::string dwString = varsName + std::string(LABEL_TRUNC_SIZE - varsName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                for(int j=0; j<varsSize; j++)
                {
                    uint16_t address = _codeLines[i]._inputLut._varsLut[j];
                    dwString += Expression::wordToHexString(address) + " ";
                }
                _output.push_back(dwString + "\n");
            }

            // Output strsLUT if it exists
            int strsSize = int(_codeLines[i]._inputLut._strsLut.size());
            uint16_t strsAddr = _codeLines[i]._inputLut._strsAddr;
            std::string strsName = "_inputStrs_" + Expression::wordToHexString(strsAddr);
            if(strsSize)
            {
                _output.push_back(strsName + std::string(LABEL_TRUNC_SIZE - strsName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(strsAddr) + "\n");
            
                std::string dwString = strsName + std::string(LABEL_TRUNC_SIZE - strsName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                for(int j=0; j<strsSize; j++)
                {
                    uint16_t address = _codeLines[i]._inputLut._strsLut[j];
                    dwString += Expression::wordToHexString(address) + " ";
                }
                _output.push_back(dwString + "\n");
            }

            // Output typesLUT if it exists
            int typesSize = int(_codeLines[i]._inputLut._typesLut.size());
            uint16_t typesAddr = _codeLines[i]._inputLut._typesAddr;
            std::string typesName = "_inputTypes_" + Expression::wordToHexString(typesAddr);
            if(typesSize)
            {
                _output.push_back(typesName + std::string(LABEL_TRUNC_SIZE - typesName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(typesAddr) + "\n");
            
                std::string dwString = typesName + std::string(LABEL_TRUNC_SIZE - typesName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                for(int j=0; j<typesSize; j++)
                {
                    uint16_t type = _codeLines[i]._inputLut._typesLut[j];
                    dwString += Expression::wordToHexString(type) + " ";
                }
                _output.push_back(dwString + "\n");
            }

            // Output INPUT LUT
            if(varsSize  &&  strsSize  &&  typesSize  &&  _codeLines[i]._inputLut._address)
            {
                uint16_t lutAddress = _codeLines[i]._inputLut._address;
                std::string name = "_input_" + Expression::wordToHexString(lutAddress);
                _output.push_back(name + std::string(LABEL_TRUNC_SIZE - name.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(lutAddress) + "\n");
            
                std::string dwString = name + std::string(LABEL_TRUNC_SIZE - name.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
                dwString += Expression::wordToHexString(varsAddr) + " ";
                dwString += Expression::wordToHexString(strsAddr) + " ";
                dwString += Expression::wordToHexString(typesAddr) + " ";
                _output.push_back(dwString + "\n");
            }
        }

        // SPRITE ADDRESS LUTs
        for(auto it=_defDataSprites.begin(); it!=_defDataSprites.end(); ++it)
        {
            int spriteId = it->first;

            // Skip invalid sprite
            if(it->second._stripeAddrs.size() == 0)
            {
                fprintf(stderr, "Compiler::outputLuts() : Warning, sprite %d is missing stripe addresses\n", spriteId);
                continue;
            }

            // Allocate RAM for sprite addresses/offsets and delimiter
            uint16_t lutAddress;
            int lutSize = int(it->second._stripeAddrs.size())*2 + 2;
            if(!Memory::getFreeRAM(Memory::FitDescending, lutSize, USER_CODE_START, _runtimeStart, lutAddress))
            {
                fprintf(stderr, "Compiler::outputLuts() : Not enough RAM for sprite %d address LUT of size %d\n", spriteId, lutSize);
                return false;
            }
            _spritesAddrLut._spriteAddrs.push_back(lutAddress);

            std::string lutName = "_spriteLut_" + Expression::wordToHexString(lutAddress);
            _output.push_back(lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(lutAddress) + "\n");
            
            std::string dwString = lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            for(int j=0; j<int(it->second._stripeAddrs.size()); j++)
            {
                uint16_t address = it->second._stripeAddrs[j];
                dwString += Expression::wordToHexString(address) + " ";
            }
            _output.push_back(dwString + "0x0000\n");
        }

        // SPRITES LUT
        if(_spritesAddrLut._spriteAddrs.size())
        {
            // Allocate RAM for sprites LUT
            uint16_t lutAddress;
            int lutSize = int(_spritesAddrLut._spriteAddrs.size()) * 2;
            if(!Memory::getFreeRAM(Memory::FitDescending, lutSize, USER_CODE_START, _runtimeStart, lutAddress))
            {
                fprintf(stderr, "Compiler::outputLuts() : Not enough RAM for sprites LUT of size %d\n", lutSize);
                return false;
            }
            _spritesAddrLut._address = lutAddress;

            std::string lutName = "_spritesLut_";
            _output.push_back(lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(lutAddress) + "\n");
            
            std::string dwString = lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            for(int i=0; i<int(_spritesAddrLut._spriteAddrs.size()); i++)
            {
                uint16_t address = _spritesAddrLut._spriteAddrs[i];
                dwString += Expression::wordToHexString(address) + " ";
            }
            _output.push_back(dwString + "\n");
        }

        // FONT ADDRESS LUTs
        for(auto it=_defDataFonts.begin(); it!=_defDataFonts.end(); ++it)
        {
            int fontId = it->first;

            // Skip invalid font
            if(it->second._charAddrs.size() == 0)
            {
                fprintf(stderr, "Compiler::outputLuts() : Warning, font %d is missing char addresses\n", fontId);
                continue;
            }

            // Allocate memory for font chars + map address + baseline address
            uint16_t lutAddress;
            int lutSize = int(it->second._charAddrs.size())*2 + 4;
            if(!Memory::getFreeRAM(Memory::FitDescending, lutSize, USER_CODE_START, _runtimeStart, lutAddress))
            {
                fprintf(stderr, "Compiler::outputLuts() : Not enough RAM for font %d address LUT of size %d\n", fontId, lutSize);
                return false;
            }
            _fontsAddrLut._fontAddrs.push_back(lutAddress);

            std::string lutName = "_fontLut_" + Expression::wordToHexString(lutAddress);
            _output.push_back(lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(lutAddress) + "\n");

            // Mapping table, (mapping address is null when full ASCII set, 32 to 127, is represented)
            std::string dwString = lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            uint16_t address = it->second._mapAddr;
            dwString += Expression::wordToHexString(address) + " ";

            // Font baseline address, baseline pixels shared by every char in a font
            address = it->second._baseAddr;
            dwString += Expression::wordToHexString(address) + " ";

            // Characters
            for(int j=0; j<int(it->second._charAddrs.size()); j++)
            {
                address = it->second._charAddrs[j];
                dwString += Expression::wordToHexString(address) + " ";
            }
            _output.push_back(dwString + "\n");
        }

        // FONTS LUT
        if(_fontsAddrLut._fontAddrs.size())
        {
            // Allocate RAM for fonts addresses
            uint16_t lutAddress;
            int lutSize = int(_fontsAddrLut._fontAddrs.size()) * 2;
            if(!Memory::getFreeRAM(Memory::FitDescending, lutSize, USER_CODE_START, _runtimeStart, lutAddress))
            {
                fprintf(stderr, "Compiler::outputLuts() : Not enough RAM for fonts LUT of size %d\n", lutSize);
                return false;
            }
            _fontsAddrLut._address = lutAddress;

            std::string lutName = "_fontsLut_";
            _output.push_back(lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(lutAddress) + "\n");
            
            std::string dwString = lutName + std::string(LABEL_TRUNC_SIZE - lutName.size(), ' ') + "DW" + std::string(OPCODE_TRUNC_SIZE - 2, ' ');
            for(int i=0; i<int(_fontsAddrLut._fontAddrs.size()); i++)
            {
                uint16_t address = _fontsAddrLut._fontAddrs[i];
                dwString += Expression::wordToHexString(address) + " ";
            }
            _output.push_back(dwString + "\n");
        }

        _output.push_back("\n");

        return true;
    }


    void outputInternalEquates(void)
    {
        _output.push_back("\n");
        _output.push_back("; Internal variables\n");
        _output.push_back("serialRawPrev" + std::string(LABEL_TRUNC_SIZE - strlen("serialRawPrev"), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(0x0081) + "\n");
        _output.push_back("register0"     + std::string(LABEL_TRUNC_SIZE - strlen("register0"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(INT_VAR_START) + "\n");
        _output.push_back("register1"     + std::string(LABEL_TRUNC_SIZE - strlen("register1"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x02\n");
        _output.push_back("register2"     + std::string(LABEL_TRUNC_SIZE - strlen("register2"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x04\n");
        _output.push_back("register3"     + std::string(LABEL_TRUNC_SIZE - strlen("register3"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x06\n");
        _output.push_back("register4"     + std::string(LABEL_TRUNC_SIZE - strlen("register4"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x08\n");
        _output.push_back("register5"     + std::string(LABEL_TRUNC_SIZE - strlen("register5"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x0A\n");
        _output.push_back("register6"     + std::string(LABEL_TRUNC_SIZE - strlen("register6"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x0C\n");
        _output.push_back("register7"     + std::string(LABEL_TRUNC_SIZE - strlen("register7"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x0E\n");
        _output.push_back("register8"     + std::string(LABEL_TRUNC_SIZE - strlen("register8"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x10\n");
        _output.push_back("register9"     + std::string(LABEL_TRUNC_SIZE - strlen("register9"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x12\n");
        _output.push_back("register10"    + std::string(LABEL_TRUNC_SIZE - strlen("register10"), ' ')    + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x14\n");
        _output.push_back("register11"    + std::string(LABEL_TRUNC_SIZE - strlen("register11"), ' ')    + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x16\n");
        _output.push_back("register12"    + std::string(LABEL_TRUNC_SIZE - strlen("register12"), ' ')    + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x18\n");
        _output.push_back("register13"    + std::string(LABEL_TRUNC_SIZE - strlen("register13"), ' ')    + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x1A\n");
        _output.push_back("register14"    + std::string(LABEL_TRUNC_SIZE - strlen("register14"), ' ')    + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x1C\n");
        _output.push_back("register15"    + std::string(LABEL_TRUNC_SIZE - strlen("register15"), ' ')    + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x1E\n");
        _output.push_back("fgbgColour"    + std::string(LABEL_TRUNC_SIZE - strlen("fgbgColour"), ' ')    + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x20\n");
        _output.push_back("cursorXY"      + std::string(LABEL_TRUNC_SIZE - strlen("cursorXY"), ' ')      + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x22\n");
        _output.push_back("midiStream"    + std::string(LABEL_TRUNC_SIZE - strlen("midiStream"), ' ')    + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x24\n");
        _output.push_back("midiDelay"     + std::string(LABEL_TRUNC_SIZE - strlen("midiDelay"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x26\n");
        _output.push_back("miscFlags"     + std::string(LABEL_TRUNC_SIZE - strlen("miscFlags"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x28\n");
        _output.push_back("timerTick"     + std::string(LABEL_TRUNC_SIZE - strlen("timerTick"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x2A\n");
        _output.push_back("timerPrev"     + std::string(LABEL_TRUNC_SIZE - strlen("timerPrev"), ' ')     + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + "register0 + 0x2C\n");
        _output.push_back("\n");

        _output.push_back("; Internal Constants\n");
        _output.push_back("ENABLE_SCROLL_BIT" + std::string(LABEL_TRUNC_SIZE - strlen("ENABLE_SCROLL_BIT"), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(ENABLE_SCROLL_BIT) + "\n");
        _output.push_back("ON_BOTTOM_ROW_BIT" + std::string(LABEL_TRUNC_SIZE - strlen("ON_BOTTOM_ROW_BIT"), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(ON_BOTTOM_ROW_BIT) + "\n");
        _output.push_back("ENABLE_SCROLL_MSK" + std::string(LABEL_TRUNC_SIZE - strlen("ENABLE_SCROLL_MSK"), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(ENABLE_SCROLL_MSK) + "\n");
        _output.push_back("ON_BOTTOM_ROW_MSK" + std::string(LABEL_TRUNC_SIZE - strlen("ON_BOTTOM_ROW_MSK"), ' ') + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(ON_BOTTOM_ROW_MSK) + "\n");
        _output.push_back("\n");

        _output.push_back("; Internal buffers\n");
        _output.push_back("textWorkArea" + std::string(LABEL_TRUNC_SIZE - strlen("textWorkArea"), ' ')  + "EQU" + std::string(OPCODE_TRUNC_SIZE - 3, ' ') + Expression::wordToHexString(getStrWorkArea()) + "\n");
        _output.push_back("\n");
    }

    void outputIncludes(void)
    {
        _output.push_back("; Includes\n");
        _output.push_back("%includePath" + std::string(LABEL_TRUNC_SIZE - strlen("%includePath"), ' ') + "\"" + getRuntimePath() + "\"\n");
        _output.push_back("%include" + std::string(LABEL_TRUNC_SIZE - strlen("%include"), ' ') + "util.i\n");
        _output.push_back("%include" + std::string(LABEL_TRUNC_SIZE - strlen("%include"), ' ') + "gigatron.i\n");

        if(_codeRomType < Cpu::ROMv5a)
        {
            _output.push_back("%include" + std::string(LABEL_TRUNC_SIZE - strlen("%include"), ' ') + "macros.i\n");
        }
        else
        {
            _output.push_back("%include" + std::string(LABEL_TRUNC_SIZE - strlen("%include"), ' ') + "macros_ROMv5a.i\n");
        }

        _output.push_back("\n");
    }

    void outputCode(void)
    {
        std::string line;

        _output.push_back("; Code\n");

        for(int i=0; i<int(_codeLines.size()); i++)
        {
            int labelIndex = _codeLines[i]._labelIndex;

            // Valid BASIC code
            if(_codeLines[i]._code.size() >= 2  &&  _codeLines[i]._vasm.size())
            {
                // BASIC Label, (may not be owned by vasm line 0 as PAGE JUMPS may move labels)
                std::string basicLabel = (labelIndex >= 0) ? _labels[labelIndex]._output : "";

                // Vasm code
                for(int j=0; j<int(_codeLines[i]._vasm.size()); j++)
                {
                    // Skip sys init funcs that are commented out, (when not used)
                    if(_codeLines[i]._vasm[j]._opcode[0] == ';') continue;

                    uint16_t vasmAddress = _codeLines[i]._vasm[j]._address;
                    std::string vasmCode = _codeLines[i]._vasm[j]._code;
                    std::string vasmLabel = _codeLines[i]._vasm[j]._internalLabel;

                    // BASIC label has priority over internal label
                    bool useBasicLabel = (labelIndex >=0  &&  _labels[labelIndex]._address == vasmAddress);
                    std::string label = (useBasicLabel) ? basicLabel : vasmLabel + std::string(LABEL_TRUNC_SIZE - vasmLabel.size(), ' ');

                    if(j == 0)
                    {
                        line = (label.size()) ? label + vasmCode : std::string(LABEL_TRUNC_SIZE, ' ') + vasmCode;
                    }
                    else
                    {
                        line += (label.size()) ?  "\n" + label + std::string(LABEL_TRUNC_SIZE - label.size(), ' ') + vasmCode : "\n" + std::string(LABEL_TRUNC_SIZE, ' ') + vasmCode;
                    }
                }

                // Commented BASIC code, (assumes any tabs are 4 spaces)
#define TAB_SPACE_LENGTH 4
#define COMMENT_PADDING  (TAB_SPACE_LENGTH*24)
                int lineLength = Expression::tabbedStringLength(line, TAB_SPACE_LENGTH);
                line.append(COMMENT_PADDING - (lineLength % COMMENT_PADDING), ' ');
                //fprintf(stderr, "%d\n", lineLength + COMMENT_PADDING - (lineLength % COMMENT_PADDING));

                // Line spacing for parsed code and non parsed code is different
                bool dontParse = (i+1 < int(_codeLines.size())) ? _codeLines[i+1]._dontParse : false;
                std::string newLine = (_codeLines[i]._dontParse  &&  dontParse) ? "\n" : "\n\n";
                line += "; " + _codeLines[i]._text + newLine;
                _output.push_back(line);
            }
        }
        
        _output.push_back("\n");
    }


    void discardUnusedLabels(void)
    {
        for(int k=0; k<int(_equateLabels.size()); k++)
        {
            bool foundLabel = false;

            for(int i=0; i<int(_codeLines.size()); i++)
            {
                // Valid BASIC code
                if(_codeLines[i]._code.size() >= 2  &&  _codeLines[i]._vasm.size())
                {
                    // Vasm code
                    for(int j=0; j<int(_codeLines[i]._vasm.size()); j++)
                    {
                        if(_codeLines[i]._vasm[j]._code.find(_equateLabels[k]) != std::string::npos)
                        {
                            foundLabel = true;
                            break;
                        }
                    }
                }
            }

            // Found a potential unused label to discard
            if(!foundLabel)
            {
                // Can only discard internal labels
                bool foundInternal = (_equateLabels[k].find("_if_") != std::string::npos     ||  _equateLabels[k].find("_else_") != std::string::npos  ||  _equateLabels[k].find("_elseif_") != std::string::npos  ||
                                      _equateLabels[k].find("_endif_") != std::string::npos  ||  _equateLabels[k].find("_while_") != std::string::npos ||  _equateLabels[k].find("_wend_") != std::string::npos    ||
                                      _equateLabels[k].find("_repeat_") != std::string::npos ||  _equateLabels[k].find("_next_") != std::string::npos  ||  _equateLabels[k].find("_page_") != std::string::npos);

                for(int l=0; l<int(_output.size()); l++)
                {
                    // Find unused internal label in output and discard it
                    if(foundInternal  &&  _output[l].find(_equateLabels[k]) != std::string::npos)
                    {
                        _output.erase(_output.begin() + l);
                        break;
                    }
                }
            }
        }
    }


    void clearCompiler(void)
    {
        _vasmPC       = USER_CODE_START;
        _tempVarStart = TEMP_VAR_START;
        _userVarStart = USER_VAR_START;
        _userVarsAddr = _userVarStart;
        _runtimeEnd   = 0x7FFF;
        _runtimeStart = 0x7FFF;
        _strWorkArea  = 0x0000;

        _codeOptimiseType = CodeSpeed;
        _codeRomType = Cpu::ROMv3;

        _codeIsAsm = false;
        _compilingError = false;
        _arrayIndiciesOne = false;
        _createNumericLabelLut = false;
        _createTimeData = false;

        _currentLabelIndex = -1;
        _currentCodeLineIndex = 0;
        _jumpFalseUniqueId = 0;

        _runtimePath = ".";
        _tempVarStartStr = "";
        _nextInternalLabel = "";

        _input.clear();
        _output.clear();
        _runtime.clear();

        _labels.clear();
        _gosubLabels.clear();
        _equateLabels.clear();
        _internalLabels.clear();
        _discardedLabels.clear();

        _codeLines.clear();
        _constants.clear();
        _integerVars.clear();
        _stringVars.clear();
        _defDataBytes.clear();
        _defDataWords.clear();
        _defDataImages.clear();
        _defDataSprites.clear();
        _defDataFonts.clear();
        _defFunctions.clear();
        _dataObjects.clear();

        _spritesAddrLut._address = 0x0000;
        _spritesAddrLut._spriteAddrs.clear();

        _fontsAddrLut._address = 0x0000;
        _fontsAddrLut._fontAddrs.clear();

        Linker::resetIncludeFiles();
        Linker::resetInternalSubs();
        Linker::disableFontLinking();

        Memory::initialise();
        Operators::initialise();

        Expression::setExprFunc(expression);

        while(!_forNextDataStack.empty())     _forNextDataStack.pop();
        while(!_elseIfDataStack.empty())      _elseIfDataStack.pop();
        while(!_endIfDataStack.empty())       _endIfDataStack.pop();
        while(!_whileWendDataStack.empty())   _whileWendDataStack.pop();
        while(!_repeatUntilDataStack.empty()) _repeatUntilDataStack.pop();

        // Allocate default string work area, (for string functions like LEFT$, MID$, etc), the +2 is for the length and delimiter bytes
        Memory::getFreeRAM(Memory::FitDescending, USER_STR_SIZE + 2, USER_CODE_START, _runtimeStart, _strWorkArea);
    }

    bool compile(const std::string& inputFilename, const std::string& outputFilename)
    {
        Assembler::clearAssembler();
        clearCompiler();

        // Read .gbas file
        int numLines = 0;
        std::ifstream infile(inputFilename);
        if(!readInputFile(infile, inputFilename, numLines)) return false;

        fprintf(stderr, "\n\n****************************************************************************************************\n");
        fprintf(stderr, "* Compiling file '%s'\n", inputFilename.c_str());
        fprintf(stderr, "****************************************************************************************************\n");

        // Pragmas
        if(!parsePragmas(numLines)) return false;

        // Initialise
        if(!initialiseCode()) return false;

        // Labels
        if(!parseLabels(numLines)) return false;

        // Includes
        if(!Linker::parseIncludes()) return false;

        // Code
        if(!parseCode()) return false;

        // Check for compiler errors
        if(_compilingError)
        {
            //_PAUSE_;
            return false;
        }

        // Optimise
        if(!Optimiser::optimiseCode()) return false;

        // Check for code relocations
        if(!Validater::checkForRelocations()) return false;

        // Check keywords that form statement blocks
        if(!Validater::checkStatementBlocks()) return false;

        // Only link runtime subroutines that are referenced
        if(!Linker::linkInternalSubs()) return false;

        // Output
        outputReservedWords();
        outputInternalEquates();
        outputIncludes();
        outputLabels();
        outputVars();
        outputArrs();
        outputStrs();
        outputDATA();
        outputTIME();
        outputDefs();
        outputLuts();
        outputCode();

        // Discard
        discardUnusedLabels();

        // Re-linking is needed here as collectInternalRuntime() can find new subs that need to be linked
        Linker::collectInternalRuntime();
        Linker::relinkInternalSubs();
        Linker::outputInternalSubs();

        Validater::checkBranchLabels();

        // Check for validation errors
        if(_compilingError)
        {
            //_PAUSE_;
            return false;
        }

        //Memory::printFreeRamList(Memory::SizeDescending);

        // Write .vasm file
        std::ofstream outfile(outputFilename, std::ios::binary | std::ios::out);
        if(!writeOutputFile(outfile, outputFilename)) return false;

        return true;
    }
}