#include<iostream>
#include<fstream>
#include<cstring>
#include<cctype>
#include<algorithm>
#include<vector>
#include<unordered_map>
#include<utility>
#include<regex>
using namespace std;

enum class Command { ADD, SUB, AND, OR, NEG, NOT, EQ, GT, LT, POP, PUSH,
        LABEL, GOTO, IF_GOTO, INVALID = -1 };

enum class Segment { LOCAL, ARGUMENT, THIS, THAT, TEMP, POINTER, STATIC, CONSTANT, INVALID = -1 };

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
    return Command::INVALID;
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

// Translate arithmetic command
string parse_arithmetic(Command command_type, vector<string> &asm_commands) {
    if (command_type <= Command::OR) {
        // Binary operator
        asm_commands.push_back("@SP");
        asm_commands.push_back("AM=M-1");
        asm_commands.push_back("D=M");
        asm_commands.push_back("A=A-1");
        switch (command_type) {
            case Command::ADD : asm_commands.push_back("M=D+M"); break;
            case Command::SUB : asm_commands.push_back("M=M-D"); break;
            case Command::AND : asm_commands.push_back("M=D&M"); break;
            case Command::OR : asm_commands.push_back("M=D|M"); break;
        }
    }
    else if (command_type <= Command:: NOT) {
        // Unary operator
        asm_commands.push_back("@SP");
        asm_commands.push_back("A=M-1");
        switch (command_type) {
            case Command::NOT : asm_commands.push_back("M=!M"); break;
            case Command::NEG : asm_commands.push_back("M=-M"); break;
        }
    }
    else {
        // Compare operator
        string jmp_label = string("JUMP_") + to_string(jmp_id);
        jmp_id++;
        asm_commands.push_back("@SP");
        asm_commands.push_back("AM=M-1");
        asm_commands.push_back("D=M");
        asm_commands.push_back("A=A-1");
        asm_commands.push_back("D=M-D");
        asm_commands.push_back("M=-1");
        asm_commands.push_back(string("@") + jmp_label);
        switch (command_type) {
            case Command::EQ : asm_commands.push_back("D;JEQ"); break;
            case Command::GT : asm_commands.push_back("D;JGT"); break;
            case Command::LT : asm_commands.push_back("D;JLT"); break;
        }
        asm_commands.push_back("@SP");
        asm_commands.push_back("A=M-1");
        asm_commands.push_back("M=0");
        asm_commands.push_back(string("(") + jmp_label + string(")"));
    }
    return string("");
}


// Parse and translate memory command
string parse_memory(Command command_type, string arguments, vector<string> &asm_commands) {
    vector<string> args = splitArgument(arguments);
    if (args.size() < 2) {
        return string("Too few argument");
    }
    if (args.size() > 2) {
        return string("Too many arguments");
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
    if (command_type == Command::PUSH) {
        // push commands
        if (segment == Segment::CONSTANT) {
            asm_commands.push_back("@" + to_string(idx));
            asm_commands.push_back("D=A");
            asm_commands.push_back("@SP");
            asm_commands.push_back("AM=M+1");
            asm_commands.push_back("A=A-1");
            asm_commands.push_back("M=D");
        }
        else if (segment <= Segment::THAT) {
            asm_commands.push_back("@" + to_string(idx));
            asm_commands.push_back("D=A");
            switch (segment) {
                case Segment::LOCAL : asm_commands.push_back("@LCL"); break;
                case Segment::ARGUMENT : asm_commands.push_back("@ARG"); break;
                case Segment::THIS : asm_commands.push_back("@THIS"); break;
                case Segment::THAT : asm_commands.push_back("@THAT"); break;
            }
            asm_commands.push_back("A=D+M");
            asm_commands.push_back("D=M");
            asm_commands.push_back("@SP");
            asm_commands.push_back("AM=M+1");
            asm_commands.push_back("A=A-1");
            asm_commands.push_back("M=D");
        }
        else if (segment <= Segment::STATIC) {
            switch (segment) {
                case Segment::TEMP : asm_commands.push_back("@" + to_string(idx+5)); break;
                case Segment::POINTER : 
                    if (idx == 0) {
                        asm_commands.push_back("@THIS");
                    }
                    else {
                        asm_commands.push_back("@THAT");
                    }
                    break;
                case Segment::STATIC : asm_commands.push_back("@st_" + file_name + "." + to_string(idx)); break;
            }
            asm_commands.push_back("D=M");
            asm_commands.push_back("@SP");
            asm_commands.push_back("AM=M+1");
            asm_commands.push_back("A=A-1");
            asm_commands.push_back("M=D");
        }
    }
    else {
        // pop commands
        if (segment == Segment::CONSTANT) {
            asm_commands.push_back("@SP");
            asm_commands.push_back("M=M-1");
        }
        else if (segment <= Segment::THAT) {
            switch (segment) {
                case Segment::LOCAL : asm_commands.push_back("@LCL"); break;
                case Segment::ARGUMENT : asm_commands.push_back("@ARG"); break;
                case Segment::THIS : asm_commands.push_back("@THIS"); break;
                case Segment::THAT : asm_commands.push_back("@THAT"); break;
            }
            asm_commands.push_back("D=M");
            asm_commands.push_back("@" + to_string(idx));
            asm_commands.push_back("D=D+A");
            asm_commands.push_back("@R13");
            asm_commands.push_back("M=D");
            asm_commands.push_back("@SP");
            asm_commands.push_back("AM=M-1");
            asm_commands.push_back("D=M");
            asm_commands.push_back("@R13");
            asm_commands.push_back("A=M");
            asm_commands.push_back("M=D");
        }
        else if (segment <= Segment::STATIC) {
            asm_commands.push_back("@SP");
            asm_commands.push_back("AM=M-1");
            asm_commands.push_back("D=M");
            switch (segment) {
                case Segment::TEMP : asm_commands.push_back("@" + to_string(idx+5)); break;
                case Segment::POINTER : 
                    if (idx == 0) {
                        asm_commands.push_back("@THIS");
                    }
                    else {
                        asm_commands.push_back("@THAT");
                    }
                    break;
                case Segment::STATIC : asm_commands.push_back("@st_" + file_name + "." + to_string(idx)); break;
            }
            asm_commands.push_back("M=D");
        }
    }
    return string("");
}


string parse_flow(Command command_type, string arguments, vector<string> &asm_commands){
    return string("Program flow not yet implemented.");
}

// Parse and identify vm command's type.
string parse_command(string vm_command, vector<string> &asm_commands) {
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
    if (type == Command::INVALID) {
        return string("Unrecognized command ") + command;
    }
    if (type <= Command::LT && !vm_command.empty()) {
        return string("Too many arguments for command ") + command;
    }
    if (type <= Command::LT) {
        return parse_arithmetic(type, asm_commands);
    }
    else {
        return parse_memory(type, vm_command, asm_commands);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "Missing input file." << endl;
        return 0;
    }

    string vmFileName = string(argv[1]);
    if (vmFileName.length() < 5 || 
            vmFileName.find(".vm", vmFileName.length()-3)==string::npos) {
        cout << "The input file is not an 'vm' extension file." << endl;
        return 0;
    }

    file_name = vmFileName.substr(vmFileName.find_last_of("/\\") + 1);
    file_name = file_name.substr(0, file_name.length()-3);

    ifstream vmFile;
    vmFile.open(vmFileName, ios::in);
    if (vmFile.fail()) {
        cout << "Fail to open the file." << endl;
        return 0;
    }


    // Step 1: Cleaning space and comments.
    string line;
    size_t comment_start;
    vector<pair<string,int> > lines;
    pair<string,int> line_data;
    int line_num = 1;
    while (!vmFile.eof()) {
        getline(vmFile, line);
        comment_start = line.find("//");
        if (line.find("//") != string::npos) {
            line.erase(comment_start);
        }
        line = std::regex_replace(line, std::regex("^ +| +$|( ) +"), "$1");
        if (!line.empty()) {
            line_data.first = line;
            line_data.second = line_num;
            lines.push_back(line_data);
        }
        line_num++;
    }
    vmFile.close();

    // Step 2: Parse all commands
    string error;
    bool has_error = false;
    vector<string> asm_commands;
    for (auto line_data : lines) {
        line = line_data.first;
        error = parse_command(line, asm_commands);
        if (!error.empty()) {
            has_error = true;
            cout << error << " in line " << line_data.second << endl;
        }
    }

    if (has_error) return 0;


    // Step 3: If there's no error, output the translated asm code.
    string asmFileName;
    asmFileName = vmFileName.substr(0, vmFileName.length()-3) + string(".asm");
    ofstream asmFile;
    asmFile.open(asmFileName, ios::out);
    for (auto asm_code: asm_commands) {
        asmFile << asm_code << endl;
    }
    asmFile.close();
    return 0;
}