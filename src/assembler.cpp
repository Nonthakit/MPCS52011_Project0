#include<iostream>
#include<fstream>
#include<cstring>
#include<cctype>
#include<algorithm>
#include<vector>
#include<unordered_map>
#include<utility>
#include<bitset>
using namespace std;

bool IsSpace(char i) {
    return isspace(i);
}

bool IsAlnum(char i) {
    return isalnum(i) || i == '_';
}

bool IsDigit(char i) {
    return isdigit(i);
}

bool IsValidNumber(string str) {
    return !str.empty() && all_of(str.begin(), str.end(), IsDigit);
}

bool IsValidLabel(string str) {
    return !str.empty() /*&& (isalpha(str[0]) || str[0] == '_') && 
            all_of(str.begin(), str.end(), IsAlnum)*/;
}


int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "Missing input file." << endl;
        return 0;
    }

    string asmFileName = string(argv[1]);
    if (asmFileName.length() < 5 || 
            asmFileName.find(".asm", asmFileName.length()-4)==string::npos) {
        cout << "The input file is not an 'asm' extension file." << endl;
        return 0;
    }

    ifstream asmFile;
    asmFile.open(asmFileName, ios::in);
    if (asmFile.fail()) {
        cout << "Fail to open the file." << endl;
        return 0;
    }


    // Step 1: Cleaning space and comments.
    string line;
    size_t comment_start;
    vector<pair<string,int> > lines;
    pair<string,int> line_data;
    int line_num = 1;
    while (!asmFile.eof()) {
        getline(asmFile, line);
        comment_start = line.find("//");
        if (line.find("//") != string::npos) {
            line.erase(comment_start);
        }
        line.erase(remove_if(line.begin(), line.end(), IsSpace), line.end());
        if (!line.empty()) {
            line_data.first = line;
            line_data.second = line_num;
            lines.push_back(line_data);
        }
        line_num++;
    }
    asmFile.close();

    // Initialize symbol table.
    unordered_map<string, int> symbol_table;
    symbol_table["SP"] = 0;
    symbol_table["LCL"] = 1;
    symbol_table["ARG"] = 2;
    symbol_table["THIS"] = 3;
    symbol_table["THAT"] = 4;
    for (int i = 0; i < 16; i++) {
        symbol_table["R"+to_string(i)] = i;
    }
    symbol_table["SCREEN"] = 16384;
    symbol_table["KBD"] = 24576;
    int memory_stack = 16;

    // Step 2: Find labels.
    vector<pair<string,int>> command_lines;
    string label;
    for (auto line_data: lines) {
        line = line_data.first;
        if (line[0] == '(') {
            if (line[line.length()-1] != ')') {
                cout << "Missing ')' in line " << line_data.second << endl;
                return 0;
            }
            label = line.substr(1, line.length()-2);
            if (!IsValidLabel(label)) {
                cout << "Invalid label '" << label << "' in line " << line_data.second << endl;
                return 0;
            }
            if (symbol_table.find(label) != symbol_table.end()) {
                cout << "Dublicated label '" << label << "' in line " << line_data.second << endl;
                return 0;
            }
            symbol_table[label] = command_lines.size();
        }
        else {
            command_lines.push_back(line_data);
        }
    }

    // Step 3: Translate commands.
    int num;
    int command;
    string comp, dest, jmp;
    size_t eq_pos, semicolon_pos;
    vector<bitset<16> > codes;
    for (auto command_data: command_lines) {
        line = command_data.first;

        if (line[0]=='@') {
            // A-command
            label = line.substr(1);
            if (isdigit(label[0])) {
                // integer
                if (!IsValidNumber(label)) {
                    cout << "Invalid value '" << label 
                            << "' for A-command in line " << command_data.second << endl;
                    return 0;
                }
                num = stoi(label);
                if (num >= 1<<15) {
                    cout << "Value overflow '" << label 
                            << "' for A-command in line " << command_data.second << endl;
                    return 0;
                }
                command = num;
            }
            else {
                // label or variable
                if (!IsValidLabel(label)) {
                    cout << "Invalid label '" << label 
                            << "' for A-command in line " << command_data.second << endl;
                    return 0;
                }
                if (symbol_table.find(label) != symbol_table.end()) {
                    command = symbol_table[label];
                }
                else {
                    if (memory_stack >= 1<<15) {
                        cout << "Stack overflow for '" << label 
                                << "' for A-command in line " << command_data.second << endl;
                        return 0;
                    }
                    symbol_table[label] = memory_stack;
                    command = memory_stack;
                    memory_stack++;
                }
            }
        }
        else {
            // C-command
            command = 0b111 << 13;
            eq_pos = line.find('=');
            if (eq_pos != string::npos) {
                dest = line.substr(0, eq_pos);
                line = line.substr(eq_pos+1);
                // Parse dest
                if (!dest.empty() && dest[0] == 'A') {
                    command |= 0b100000;
                    dest = dest.substr(1);
                }
                if (!dest.empty() && dest[0] == 'M') {
                    command |= 0b1000;
                    dest = dest.substr(1);
                }
                if (!dest.empty() && dest[0] == 'D') {
                    command |= 0b10000;
                    dest = dest.substr(1);
                }
                if (!dest.empty()) {
                    cout << "Invalid destination for C-command in line " << command_data.second << endl;
                    return 0;
                }
            }
            semicolon_pos = line.find(';');
            if (semicolon_pos != string::npos) {
                jmp = line.substr(semicolon_pos+1);
                line = line.substr(0, semicolon_pos);
                // Parse jmp
                if (jmp.empty()) {
                    // null
                } else if (jmp.compare("JGT") == 0) {
                    command |= 0b1;
                } else if (jmp.compare("JEQ") == 0) {
                    command |= 0b10;
                } else if (jmp.compare("JGE") == 0) {
                    command |= 0b11;
                } else if (jmp.compare("JLT") == 0) {
                    command |= 0b100;
                } else if (jmp.compare("JNE") == 0) {
                    command |= 0b101;
                } else if (jmp.compare("JLE") == 0) {
                    command |= 0b110;
                } else if (jmp.compare("JMP") == 0) {
                    command |= 0b111;
                } else {
                    cout << "Invalid jump for C-command in line " << command_data.second << endl;
                    return 0;
                }
            }
            comp = line;
            // Parse comp
            if (comp.compare("0") == 0) {
                command |= 0b101010000000;
            } else if (comp.compare("1") == 0) {
                command |= 0b111111000000;
            } else if (comp.compare("-1") == 0) {
                command |= 0b111010000000;
            } else if (comp.compare("D") == 0) {
                command |= 0b001100000000;
            } else if (comp.compare("A") == 0) {
                command |= 0b110000000000;
            } else if (comp.compare("!D") == 0) {
                command |= 0b001101000000;
            } else if (comp.compare("!A") == 0) {
                command |= 0b110001000000;
            } else if (comp.compare("-D") == 0) {
                command |= 0b001111000000;
            } else if (comp.compare("-A") == 0) {
                command |= 0b110011000000;
            } else if (comp.compare("D+1") == 0) {
                command |= 0b011111000000;
            } else if (comp.compare("A+1") == 0) {
                command |= 0b110111000000;
            } else if (comp.compare("D-1") == 0) {
                command |= 0b001110000000;
            } else if (comp.compare("A-1") == 0) {
                command |= 0b110010000000;
            } else if (comp.compare("D+A") == 0) {
                command |= 0b000010000000;
            } else if (comp.compare("D-A") == 0) {
                command |= 0b010011000000;
            } else if (comp.compare("A-D") == 0) {
                command |= 0b000111000000;
            } else if (comp.compare("D&A") == 0) {
                command |= 0b000000000000;
            } else if (comp.compare("D|A") == 0) {
                command |= 0b010101000000;
            } else if (comp.compare("M") == 0) {
                command |= 0b1110000000000;
            } else if (comp.compare("!M") == 0) {
                command |= 0b1110001000000;
            } else if (comp.compare("-M") == 0) {
                command |= 0b1110011000000;
            } else if (comp.compare("M+1") == 0) {
                command |= 0b1110111000000;
            } else if (comp.compare("M-1") == 0) {
                command |= 0b1110010000000;
            } else if (comp.compare("D+M") == 0) {
                command |= 0b1000010000000;
            } else if (comp.compare("D-M") == 0) {
                command |= 0b1010011000000;
            } else if (comp.compare("M-D") == 0) {
                command |= 0b1000111000000;
            } else if (comp.compare("D&M") == 0) {
                command |= 0b1000000000000;
            } else if (comp.compare("D|M") == 0) {
                command |= 0b1010101000000;
            } else {
                cout << "Invalid comp for C-command in line " << command_data.second << endl;
                return 0;
            }
        }
        bitset<16> command_code(command);
        codes.push_back(command_code);
    }

    // Print to hack file.
    string hackFileName;
    hackFileName = asmFileName.substr(0, asmFileName.length()-4) + string(".hack");
    ofstream hackFile;
    hackFile.open(hackFileName, ios::out);
    for (auto command_code: codes) {
        hackFile << command_code << endl;
    }
    hackFile.close();
    return 0;
}