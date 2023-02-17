#include<iostream>
#include<fstream>
#include<cstring>
#include<cctype>
#include<algorithm>
#include<vector>
#include<unordered_map>
#include<unordered_set>
#include<utility>
#include<regex>
#include<sys/stat.h>
#include <dirent.h>
using namespace std;

enum class Command { ADD, SUB, AND, OR, NEG, NOT, EQ, GT, LT, POP, PUSH,
        LABEL, GOTO, IF_GOTO, FUNCTION, CALL, RETURN, INVALID = -1 };

enum class Segment { LOCAL, ARGUMENT, THIS, THAT, TEMP, POINTER, STATIC, CONSTANT, INVALID = -1 };

// line data for error reports
class LineData {
    public:
        string file_name;
        int line_num;
};

// Sematic check data
typedef struct SemeticData {
    LineData line_data;
    bool clear_lbl;
    unordered_set<string> def_func;
    vector<pair<string, LineData> > call_func;
    unordered_set<string> def_lbl;
    vector<pair<string, LineData> > call_lbl;
} SemeticData;


// Translate data
typedef struct ParseData {
    ofstream *asm_output;
    string file_name;
    string function_name;
    int func_jmp;
    int global_jmp;
} ParseData;

// Initialize label counters.
int jmp_id = 0;

string file_name;

// Command parsing
Command command_type(string command) {
    if (command.compare("add") == 0) return Command::ADD;
    if (command.compare("sub") == 0) return Command::SUB;
    if (command.compare("neg") == 0) return Command::NEG;
    if (command.compare("not") == 0) return Command::NOT;
    if (command.compare("eq") == 0) return Command::EQ;
    if (command.compare("gt") == 0) return Command::GT;
    if (command.compare("lt") == 0) return Command::LT;
    if (command.compare("and") == 0) return Command::AND;
    if (command.compare("or") == 0) return Command::OR;
    if (command.compare("pop") == 0) return Command::POP;
    if (command.compare("push") == 0) return Command::PUSH;
    if (command.compare("label") == 0) return Command::LABEL;
    if (command.compare("goto") == 0) return Command::GOTO;
    if (command.compare("if-goto") == 0) return Command::IF_GOTO;
    if (command.compare("function") == 0) return Command::FUNCTION;
    if (command.compare("call") == 0) return Command::CALL;
    if (command.compare("return") == 0) return Command::RETURN;
    return Command::INVALID;
}

// Command type checking.
bool is_arithmetic_cmd(Command command) {
    return command >= Command::ADD && command <= Command::LT;
}

bool is_memory_cmd(Command command) {
    return command == Command::POP || command == Command::PUSH;
}

bool is_flow_cmd(Command command) {
    return command >= Command::LABEL && command <= Command::IF_GOTO;
}

bool is_function_cmd(Command command) {
    return command >= Command::FUNCTION && command <= Command::RETURN;
}


// Segment parsing
Segment segment_type(string segment) {
    if (segment.compare("local") == 0) return Segment::LOCAL;
    if (segment.compare("argument") == 0) return Segment::ARGUMENT;
    if (segment.compare("this") == 0) return Segment::THIS;
    if (segment.compare("that") == 0) return Segment::THAT;
    if (segment.compare("static") == 0) return Segment::STATIC;
    if (segment.compare("temp") == 0) return Segment::TEMP;
    if (segment.compare("pointer") == 0) return Segment::POINTER;
    if (segment.compare("constant") == 0) return Segment::CONSTANT;
    return Segment::INVALID;
}


// Helper function to split arguments
vector<string> splitArgument(string arguments) {
    size_t split_index;
    vector<string> args;
    if (arguments.empty()) {
        return args;
    }
    split_index = arguments.find(" ");
    while(split_index != string::npos) {
        args.push_back(arguments.substr(0, split_index));
        arguments = arguments.substr(split_index+1);
        split_index = arguments.find(" ");
    }
    args.push_back(arguments);
    return args;
}

bool IsDigit(char i) {
    return isdigit(i);
}

bool IsValidNumber(string str) {
    return !str.empty() && all_of(str.begin(), str.end(), IsDigit);
}

/////////////////////////////////////////  Parse commands /////////////////////////////////////

// Translate arithmetic command
void parse_arithmetic(Command command_type, ParseData &pdata) {
    if (command_type <= Command::OR) {
        // Binary operator
        *pdata.asm_output    << "@SP" << endl
                            << "AM=M-1" << endl
                            << "D=M" << endl
                            << "A=A-1" << endl;
        switch (command_type) {
            case Command::ADD : *pdata.asm_output << "M=D+M" << endl; break;
            case Command::SUB : *pdata.asm_output << "M=M-D" << endl; break;
            case Command::AND : *pdata.asm_output << "M=D&M" << endl; break;
            case Command::OR : *pdata.asm_output << "M=D|M" << endl; break;
        }
    }
    else if (command_type <= Command:: NOT) {
        // Unary operator
        *pdata.asm_output    << "@SP" << endl
                            << "A=M-1" << endl;
        switch (command_type) {
            case Command::NOT : *pdata.asm_output << "M=!M" << endl; break;
            case Command::NEG : *pdata.asm_output << "M=-M" << endl; break;
        }
    }
    else {
        // Compare operator
        string jmp_label = string("JUMP_") + to_string(pdata.global_jmp);
        pdata.global_jmp++;
        *pdata.asm_output    << "@SP" << endl
                            << "AM=M-1" << endl
                            << "D=M" << endl
                            << "A=A-1" << endl
                            << "D=M-D" << endl
                            << "M=-1" << endl
                            << string("@") + jmp_label << endl;
        switch (command_type) {
            case Command::EQ : *pdata.asm_output << "D;JEQ" << endl; break;
            case Command::GT : *pdata.asm_output << "D;JGT" << endl; break;
            case Command::LT : *pdata.asm_output << "D;JLT" << endl; break;
        }
        *pdata.asm_output    << "@SP" << endl
                            << "A=M-1" << endl
                            << "M=0" << endl
                            << string("(") + jmp_label + string(")") << endl;
    }
}


// Translate memory command.
void parse_memory(Command command_type, string arguments, ParseData &pdata) {
    vector<string> args = splitArgument(arguments);
    Segment segment = segment_type(args[0]);
    int idx = stoi(args[1]);
    if (command_type == Command::PUSH) {
        // push commands
        if (segment == Segment::CONSTANT) {
            *pdata.asm_output    << "@" << idx << endl
                                << "D=A" << endl
                                << "@SP" << endl
                                << "AM=M+1" << endl
                                << "A=A-1" << endl
                                << "M=D" << endl;
        }
        else if (segment <= Segment::THAT) {
            *pdata.asm_output    << "@" << idx << endl
                                << "D=A"<< endl;
            switch (segment) {
                case Segment::LOCAL : *pdata.asm_output << "@LCL" << endl; break;
                case Segment::ARGUMENT : *pdata.asm_output << "@ARG" << endl; break;
                case Segment::THIS : *pdata.asm_output << "@THIS" << endl; break;
                case Segment::THAT : *pdata.asm_output << "@THAT" << endl; break;
            }
            *pdata.asm_output    << "A=D+M" << endl
                                << "D=M" << endl
                                << "@SP" << endl
                                << "AM=M+1" << endl
                                << "A=A-1" << endl
                                << "M=D" << endl;
        }
        else if (segment <= Segment::STATIC) {
            switch (segment) {
                case Segment::TEMP : *pdata.asm_output << "@" << idx+5 << endl; break;
                case Segment::POINTER : 
                    if (idx == 0) {
                        *pdata.asm_output << "@THIS" << endl;
                    }
                    else {
                        *pdata.asm_output << "@THAT" << endl;
                    }
                    break;
                case Segment::STATIC : *pdata.asm_output << "@" << pdata.file_name << "." << idx << endl; break;
            }
            *pdata.asm_output    << "D=M" << endl
                                << "@SP" << endl
                                << "AM=M+1" << endl
                                << "A=A-1" << endl
                                << "M=D" << endl;
        }
    }
    else {
        // pop commands
        if (segment == Segment::CONSTANT) {
            *pdata.asm_output    << "@SP" << endl
                                << "M=M-1" << endl;
        }
        else if (segment <= Segment::THAT) {
            switch (segment) {
                case Segment::LOCAL : *pdata.asm_output << "@LCL" << endl; break;
                case Segment::ARGUMENT : *pdata.asm_output << "@ARG" << endl; break;
                case Segment::THIS : *pdata.asm_output << "@THIS" << endl; break;
                case Segment::THAT : *pdata.asm_output << "@THAT" << endl; break;
            }
            *pdata.asm_output    << "D=M" << endl
                                << "@" << idx << endl
                                << "D=D+A" << endl
                                << "@R13" << endl
                                << "M=D" << endl
                                << "@SP" << endl
                                << "AM=M-1" << endl
                                << "D=M" << endl
                                << "@R13" << endl
                                << "A=M" << endl
                                << "M=D" << endl;
        }
        else if (segment <= Segment::STATIC) {
            *pdata.asm_output   << "@SP" << endl
                                << "AM=M-1" << endl
                                << "D=M" << endl;
            switch (segment) {
                case Segment::TEMP : *pdata.asm_output << "@" << idx+5 << endl; break;
                case Segment::POINTER : 
                    if (idx == 0) {
                        *pdata.asm_output << "@THIS" << endl;
                    }
                    else {
                        *pdata.asm_output << "@THAT" << endl;
                    }
                    break;
                case Segment::STATIC : *pdata.asm_output << "@" << pdata.file_name << "." << idx << endl; break;
            }
            *pdata.asm_output    << "M=D" << endl;
        }
    }
}

// Translate flow command.
void parse_flow(Command command_type, string arguments, ParseData &pdata) {
    vector<string> args = splitArgument(arguments);
    if (command_type == Command::LABEL) {
        *pdata.asm_output   << "(" << pdata.function_name << "$" << args[0] << ")" << endl;
    }
    else if (command_type == Command::GOTO) {
        *pdata.asm_output   << "@" + pdata.function_name << "$" << args[0] << endl
                            << "D;JMP" << endl;
    }
    else if (command_type == Command::IF_GOTO) {
        *pdata.asm_output   << "@SP" << endl
                            << "AM=M-1" << endl
                            << "D=M" << endl
                            << "@" + pdata.function_name << "$" << args[0] << endl
                            << "D;JNE" << endl;
    }
}

// Translate function command.
void parse_function(Command command_type, string arguments, ParseData &pdata) {
    vector<string> args = splitArgument(arguments);
    if (command_type == Command::CALL) {
        int narg = stoi(args[1]);
        *pdata.asm_output   << "@" << pdata.function_name << "$ret$" << pdata.func_jmp << endl
                            << "D=A" << endl
                            << "@SP" << endl
                            << "A=M" << endl
                            << "M=D" << endl
                            << "@LCL" << endl
                            << "D=M" << endl
                            << "@SP" << endl
                            << "AM=M+1" << endl
                            << "M=D" << endl
                            << "@ARG" << endl
                            << "D=M" << endl
                            << "@SP" << endl
                            << "AM=M+1" << endl
                            << "M=D" << endl
                            << "@THIS" << endl
                            << "D=M" << endl
                            << "@SP" << endl
                            << "AM=M+1" << endl
                            << "M=D" << endl
                            << "@THAT" << endl
                            << "D=M" << endl
                            << "@SP" << endl
                            << "M=M+1" << endl
                            << "AM=M+1" << endl
                            << "A=A-1" << endl
                            << "M=D" << endl
                            << "D=A" << endl
                            << "@" << (4+narg) << endl
                            << "D=D-A" << endl
                            << "@ARG" << endl
                            << "M=D" << endl
                            << "@SP" << endl
                            << "D=M" << endl
                            << "@LCL" << endl
                            << "M=D" << endl
                            << "@" << args[0] << endl
                            << "0;JMP" << endl
                            << "(" << pdata.function_name << "$ret$" << pdata.func_jmp << ")" << endl;
        pdata.func_jmp++;
    }
    else if (command_type == Command::FUNCTION) {
        int nvar = stoi(args[1]);
        *pdata.asm_output    << "(" << args[0] << ")" << endl;
        if (nvar == 1) {
            *pdata.asm_output   << "@SP" << endl
                                << "AM=M+1" << endl
                                << "A=A-1" << endl
                                << "M=0" << endl;
        } else if (nvar > 1) {
            *pdata.asm_output   << "@" << nvar << endl
                                << "D=A" << endl
                                << "@SP" << endl
                                << "AM=D+M" << endl;
            for (int i = 0; i < nvar; i++) {
                *pdata.asm_output   << "A=A-1" << endl
                                    << "M=0" << endl;
            }
        }
        pdata.func_jmp = 0;
        pdata.function_name = args[0];
    }
    else if (command_type == Command::RETURN) {
        *pdata.asm_output   << "@LCL" << endl
                            << "D=M" << endl
                            << "@FRAME" << endl
                            << "M=D" << endl
                            << "@5" << endl
                            << "A=D-A" << endl
                            << "D=M" << endl
                            << "@RET" << endl
                            << "M=D" << endl
                            << "@SP" << endl
                            << "AM=M-1" << endl
                            << "D=M" << endl
                            << "@ARG" << endl
                            << "A=M" << endl
                            << "M=D" << endl
                            << "D=A+1" << endl
                            << "@SP" << endl
                            << "M=D" << endl
                            << "@FRAME" << endl
                            << "AM=M-1" << endl
                            << "D=M" << endl
                            << "@THAT" << endl
                            << "M=D" << endl
                            << "@FRAME" << endl
                            << "AM=M-1" << endl
                            << "D=M" << endl
                            << "@THIS" << endl
                            << "M=D" << endl
                            << "@FRAME" << endl
                            << "AM=M-1" << endl
                            << "D=M" << endl
                            << "@ARG" << endl
                            << "M=D" << endl
                            << "@FRAME" << endl
                            << "AM=M-1" << endl
                            << "D=M" << endl
                            << "@LCL" << endl
                            << "M=D" << endl
                            << "@RET" << endl
                            << "A=M" << endl
                            << "0;JMP" << endl;
    }
}

// Parse command and translate it.
void parse_command(string vm_command, ParseData &pdata) {
    size_t split_index = vm_command.find(" ");
    string command;
    Command type;
    if (split_index != string::npos) {
        command = vm_command.substr(0, split_index);
        vm_command = vm_command.substr(split_index+1);
    }
    else {
        command = vm_command;
        vm_command = string("");
    }
    type = command_type(command);
    if (is_arithmetic_cmd(type)) {
        return parse_arithmetic(type, pdata);
    }
    else if (is_memory_cmd(type)) {
        return parse_memory(type, vm_command, pdata);
    } 
    else if (is_flow_cmd(type)) {
        return parse_flow(type, vm_command, pdata);
    }
    else if (is_function_cmd(type)) {
        return parse_function(type, vm_command, pdata);
    }
}

/////////////////////////////////////////  Sematic Analysis /////////////////////////////////////

// Check arithmetic command syntax.
string check_arithmetic(Command type, vector<string> args) {
    if (!args.empty()) {
        return string("Too many arguments for arithmetic command.");
    }
    return string("");
}

// Check memory command syntax.
string check_memory(Command type, vector<string> args) {
    if (args.size() < 2) {
        return string("Too few arguments for memory command.");
    }
    if (args.size() > 2) {
        return string("Too many arguments for memory command.");
    }
    Segment segment = segment_type(args[0]);
    if (segment == Segment::INVALID) {
        return string("No segment called \"" + string(args[0]) + "\"");
    }
    if (!IsValidNumber(args[1])) {
        return string("The index can only be non negative integer");
    }
    int idx = stoi(args[1]);
    if (segment == Segment::TEMP && idx >= 8) {
        return string("Invalid index for temp (idx < 8)");
    }
    if (segment == Segment::POINTER && idx >= 2) {
        return string("Invalid index for pointer (idx = 0,1)");
    }
    return string("");
}


// Check flow command syntax.
string check_flow(Command type, vector<string> &args, SemeticData &sdata) {
    if (args.size() < 1) {
        return string("Too few argument for flow command.");
    }
    if (args.size() > 1) {
        return string("Too many arguments for flow command.");
    }
    if (type == Command::LABEL) {
        if (sdata.def_lbl.find(args[0]) != sdata.def_func.end()) {
            return string("Duplicated label ") + args[0];
        }
        sdata.def_lbl.insert(args[0]);
        return string("");
    }
    else {
        sdata.call_lbl.push_back(make_pair(args[0], sdata.line_data));
    }
    return string("");
}

// Check function command syntax.
string check_function(Command type, vector<string> &args, SemeticData &sdata) {
    if (type == Command::FUNCTION) {
        if (args.size() < 2) {
            return string("Too few argument for function definition.");
        }
        if (args.size() > 2) {
            return string("Too many arguments for function definition.");
        }
        if (sdata.def_func.find(args[0]) != sdata.def_func.end()) {
            return string("Duplicated function ") + args[0];
        }
        if (!IsValidNumber(args[1])) {
            return string("The number of variables can only be non negative integer");
        }
        sdata.def_func.insert(args[0]);
        sdata.clear_lbl = true;
    } else if (type == Command::CALL) {
        if (args.size() < 2) {
            return string("Too few argument for function definition.");
        }
        if (args.size() > 2) {
            return string("Too many arguments for function definition.");
        }
        if (!IsValidNumber(args[1])) {
            return string("The number of arguments can only be non negative integer");
        }
        sdata.call_func.push_back(make_pair(args[0], sdata.line_data));
    } else {
        if (!args.empty()) {
            return string("Too many arguments for return.");
        }
    }
    return string("");
}

// Parse command, and check if command exist or not. 
// Then check for command syntax.
string check_command(string vm_command, SemeticData &sdata) {
    size_t split_index = vm_command.find(" ");
    string command;
    Command type;
    if (split_index != string::npos) {
        command = vm_command.substr(0, split_index);
        vm_command = vm_command.substr(split_index+1);
    }
    else {
        command = vm_command;
        vm_command = string("");
    }
    vector<string> args = splitArgument(vm_command);
    type = command_type(command);
    if (type == Command::INVALID) {
        return string("Unrecognized command ") + command;
    }
    if (is_arithmetic_cmd(type)) {
        return check_arithmetic(type, args);
    }
    else if (is_memory_cmd(type)) {
        return check_memory(type, args);
    } 
    else if (is_flow_cmd(type)) {
        return check_flow(type, args, sdata);
    }
    else if (is_function_cmd(type)) {
        return check_function(type, args, sdata);
    }
    return string("");
}


// Check if functions throughout all files consistent or not.
bool check_function_error(SemeticData &sdata) {
    bool has_error = false;
    for (auto name : sdata.call_func) {
        if (sdata.def_func.find(name.first) == sdata.def_func.end()) {
            cout << endl << "No declaration of function " << name.first
                        << " (" << name.second.file_name 
                        << "::" << name.second.line_num << ")" << endl;
            has_error = true;
        }
    }
    sdata.call_func.clear();
    sdata.def_func.clear();
    return has_error;
}


// Check if label in the file/function is consistent or not.
bool check_label_error(SemeticData &sdata) {
    bool has_error = false;
    for (auto name : sdata.call_lbl) {
        if (sdata.def_lbl.find(name.first) == sdata.def_lbl.end()) {
            cout << endl << "No declaration of label " << name.first
                        << " (" << name.second.file_name 
                        << "::" << name.second.line_num << ")" << endl;
            has_error = true;
        }
    }
    sdata.clear_lbl = false;
    sdata.call_lbl.clear();
    sdata.def_lbl.clear();
    return has_error;
}


// Samantic check for a file.
bool check_file(string &vmFileName, SemeticData &sdata) {
    ifstream vmFile;
    vmFile.open(vmFileName, ios::in);
    if (vmFile.fail()) {
        cout << "Fail to open file " << vmFileName << endl;
        return false;
    }

    sdata.line_data.file_name = vmFileName;

    string line;
    size_t comment_start;
    pair<string,int> line_data;
    int line_num = 1;
    string error;
    bool has_error = false;
    while (!vmFile.eof()) {
        getline(vmFile, line);

        // Remove comments and white space.
        comment_start = line.find("//");
        if (line.find("//") != string::npos) {
            line.erase(comment_start);
        }
        replace( line.begin(), line.end(), '\t', ' ');
        line = std::regex_replace(line, std::regex("^ +| +$|( ) +"), "$1");

        // If not an empty line, run sematic check.
        if (!line.empty()) {
            sdata.line_data.line_num = line_num;

            // Sematic check the command.
            error = check_command(line, sdata);

            // If there's an error, print out and mark error.
            if (!error.empty()) {
                has_error = true;
                cout << endl << error << " (" << sdata.line_data.file_name 
                        << "::" << sdata.line_data.line_num << ")" << endl;
            }

            // If there's a need to clear label, do it
            if (sdata.clear_lbl) {
                has_error |= check_label_error(sdata);
            }
        }
        line_num++;
    }

    // Check if all label exists.
    has_error |= check_label_error(sdata);
    vmFile.close();
    return has_error;
}


/////////////////////////////////////////  Files management /////////////////////////////////////

// Translating a file.
void parse_file(string &vmFileName, ParseData &pdata) {
    ifstream vmFile;
    vmFile.open(vmFileName, ios::in);
    
    string line;
    size_t comment_start;
    while (!vmFile.eof()) {
        getline(vmFile, line);

        // Remove comments and white space.
        comment_start = line.find("//");
        if (line.find("//") != string::npos) {
            line.erase(comment_start);
        }
        replace( line.begin(), line.end(), '\t', ' ');
        line = std::regex_replace(line, std::regex("^ +| +$|( ) +"), "$1");

        // If not an empty line, translate the code.
        if (!line.empty()) {
            parse_command(line, pdata);
        }
    }

    vmFile.close();
}

void process_file(string &path) {
    // Step 1: Run sematic check
    bool has_error = false;
    if (path.length() < 4 || 
            path.find(".vm", path.length()-3)==string::npos) {
        cout << "The input file is not an 'vm' extension file." << endl;
        return;
    }
    string prog_name;
    prog_name = path.substr(path.find_last_of("/\\") + 1);
    prog_name = prog_name.substr(0, prog_name.length()-3);
    SemeticData sdata;
    sdata.clear_lbl = false;
    has_error |= check_file(path, sdata);
    has_error |= check_function_error(sdata);

    if (has_error) {
        // Code invalid. End parsing immedietely!
        return;
    }

    // Step 2: Translate to asm
    string asmFileName;
    asmFileName = path.substr(0, path.length()-3) + string(".asm");
    ofstream asmFile;
    ParseData pdata;
    asmFile.open(asmFileName, ios::out);
    pdata.asm_output = &asmFile;
    pdata.function_name = prog_name;
    pdata.file_name = prog_name;
    pdata.func_jmp = 0;
    pdata.global_jmp = 0;
    *pdata.asm_output   << "@256" << endl
                        << "D=A" << endl
                        << "@SP" << endl
                        << "M=D" << endl;
    parse_file(path, pdata);
    asmFile.close();

    return;
}

void process_folder(string &path) {
    // Check if all function exists.
    DIR *dr;
    struct dirent *en;
    dr = opendir(path.c_str()); //open all directory
    bool has_sys = false;
    bool has_error = false;
    SemeticData sdata;
    string name;
    vector<string> vm_list;
    string file_path;
    if (dr) {
        while ((en = readdir(dr)) != NULL) {
            name = en->d_name;
            if (name.length() >= 4 && 
                    name.find(".vm", name.length()-3)!=string::npos) {
                if (name.compare("Sys.vm") == 0) {
                    has_sys = true;
                }
                sdata.clear_lbl = false;
                file_path = path + "/" + name;
                has_error |= check_file(file_path, sdata);
                vm_list.push_back(name);
            }
        }
        closedir(dr); //close all directory
    }

    if (!has_sys) {
        has_error = true;
        cout << "No file name Sys.vm in the directory." << endl;
        return;
    }

    if (sdata.def_func.find("Sys.init") == sdata.def_func.end()) {
        cout << endl << "No declaration of function Sys.init" << endl;
        has_error = true;
    }

    has_error |= check_function_error(sdata);

    if (has_error) {
        // Code invalid. End parsing immedietely!
        return;
    }


    string asmFileName;
    ofstream asmFile;
    string prog_name;
    prog_name = path.substr(path.find_last_of("/\\") + 1);
    asmFileName = path + "/" + prog_name + string(".asm");
    asmFile.open(asmFileName, ios::out);
    ParseData pdata;
    pdata.asm_output = &asmFile;
    pdata.function_name = "Sys";
    pdata.file_name = "Sys";
    pdata.func_jmp = 0;
    pdata.global_jmp = 0;
    *pdata.asm_output   << "@256" << endl
                        << "D=A" << endl
                        << "@SP" << endl
                        << "M=D" << endl;
    parse_command("call Sys.init 0", pdata);
    for (auto vm_file : vm_list) {
        pdata.function_name = vm_file.substr(0, vm_file.length()-3);
        pdata.file_name = pdata.function_name;
        pdata.func_jmp = 0;
        string vm_path = path + "/" + vm_file;
        parse_file(vm_path, pdata);
    }
    asmFile.close();
    return;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "Missing input file or directory." << endl;
        return 0;
    }

    // Step 1: Check if it's file or directory
    struct stat sb;
    if (!stat(argv[1], &sb) == 0) {
        cout << "The path is neither file nor directory." << endl;
        return 0;
    }

    string path = string(argv[1]);

    if (sb.st_mode & S_IFDIR) {
        process_folder(path);
    }
    else {
        process_file(path);
    }

    return 0;
}