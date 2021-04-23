#include "linux_parser.h"

#include <dirent.h>
#include <unistd.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using std::stof;
using std::stol;
using std::string;
using std::to_string;
using std::tuple;
using std::vector;

string LinuxParser::OperatingSystem() {
    string line;
    string key;
    string value = "";
    std::ifstream fs(kOSPath);
    if (fs.is_open()) {
        while (std::getline(fs, line)) {
            std::replace(line.begin(), line.end(), ' ', '_');
            std::replace(line.begin(), line.end(), '=', ' ');
            std::replace(line.begin(), line.end(), '"', ' ');
            std::istringstream linestream(line);
            while (linestream >> key >> value) {
                if (key == "PRETTY_NAME") {
                    std::replace(value.begin(), value.end(), '_', ' ');
                    fs.close();
                    return value;
                }
            }
        }
        fs.close();
    }
    return value;
}

string LinuxParser::Kernel() {
    string os, kernel, version;
    string line;
    std::ifstream fs(kProcDirectory + kVersionFilename);
    if (fs.is_open()) {
        std::getline(fs, line);
        std::istringstream linestream(line);
        linestream >> os >> version >> kernel;
        fs.close();
    }
    return kernel;
}

vector<int> LinuxParser::Pids() {
    vector<int> pids;
    string stem;
    for (auto& p : std::filesystem::directory_iterator(kProcDirectory)) {
        stem = p.path().stem();
        stem.erase(remove(stem.begin(), stem.end(), '\"'), stem.end());
        if (std::all_of(stem.begin(), stem.end(), isdigit)) {
            pids.push_back(stoi(stem));
        }
    }
    return pids;
}

float LinuxParser::MemoryUtilization() {
    string line;
    int i{0};
    string line_split;

    float v{0.0f};
    vector<float> mem_values;
    float mem_used{0.0f};
    float mem_usage_pct = 0.0f;

    std::ifstream fs(kProcDirectory + kMeminfoFilename);
    if (fs.is_open()) {
        while ((std::getline(fs, line)) && i < 2) {
            auto pos = line.find(":");
            line_split = line.substr(pos + 1, -1);

            std::istringstream ss(line_split);
            ss >> v;
            mem_values.push_back(v);
            i++;
        }
    fs.close();
    }

    mem_used = (mem_values[kMemTotal_] - mem_values[kMemFree_]);
    mem_usage_pct = mem_used / mem_values[kMemTotal_];

    if (std::isnan(mem_usage_pct)) {
        return 0.0f;
    } else {
        return mem_usage_pct;
    }
}

long LinuxParser::UpTime() {
    string line;
    long long result{0ll};

    std::ifstream fs(kProcDirectory + kUptimeFilename);
    if (fs.is_open()) {
        std::getline(fs, line);
        std::istringstream ss(line);
        ss >> result;
        fs.close();
    }

    return result;
}

long LinuxParser::TotalJiffies() {
    string line;
    long token;
    long result{0l};

    std::ifstream fs(kProcDirectory + kStatFilename);
    if (fs.is_open()) {
        std::getline(fs, line);
        line = line.substr(5, line.back());
        std::stringstream ss(line);
        while (ss >> token)
        {
            result += token;
        }
        fs.close();
    }

    return result;
}

int LinuxParser::TotalProcesses() {
    int total_processes{0};
    string stem;

    for (auto& p : std::filesystem::directory_iterator(kProcDirectory)) {
        stem = p.path().stem();
        if (std::all_of(stem.begin(), stem.end(), isdigit)) {
            total_processes++;
        }
    }
    return total_processes;
}

int LinuxParser::RunningProcesses() {
    string line;
    string key = "procs_running";
    int running_processes{0};

    std::ifstream fs(kProcDirectory + kStatFilename);
    if (fs.is_open()) {
        while (std::getline(fs, line)) {
            if (line.find(key) != std::string::npos) {
                std::stringstream ss(line);
                ss >> key >> running_processes;
            }
        }
        fs.close();
    }
    return running_processes;
}

// Returns: a vector containing CPU usage information for a process ID.
vector<long> LinuxParser::PidStat(int pid) { 
    string line;
    std::size_t op, cp;
    vector<int> token_pos{14, 15, 16, 17, 22};
    string token;
    vector<long> result(5, 0);

    std::ifstream fs(kProcDirectory + to_string(pid) + kStatFilename);
    if (fs.is_open()) {
        std::getline(fs, line);
        op = line.find("(");
        cp = line.find(")");
        line = line.replace(op + 1, cp - op - 1, "");
        result.clear();

        for (auto pos : token_pos) {
            token = LinuxParser::NthToken(line, pos);
            if (!std::all_of(token.begin(), token.end(), isdigit))
            {
                token = "0";
            }
            result.push_back(stol(token));
        }
        fs.close();
    }
    return result; 
}

string LinuxParser::Command(int pid) { 
    string line;
    std::size_t op, cp;
    string chop;
    string command = "";

    std::ifstream fs(kProcDirectory + to_string(pid) + kCmdlineFilename);
    if (fs.is_open()) {
        if (std::getline(fs, line)) {
            command = line;
        } else {
            std::ifstream fs2(kProcDirectory + to_string(pid) + kStatFilename);
            if (fs2.is_open()) {
                if (std::getline(fs2, line)) {
                    cp = line.find(")");
                    line = line.substr(0, cp + 1);
                    op = line.find("(");
                    line = line.substr(op, line.size());
                    command = line;
                }
                fs2.close();
            }
        }
        fs.close();
    }
    return command;
}

string LinuxParser::Ram(int pid) { 
    string line;
    string ram = "-------";
    string token = "";
    float value{0.0f};
    vector<tuple<int,string>> units_in_kb{ { 1048576, "G" },
            { 1024, "M" },
            { 1, "K"} };
    string pretty_value;
    
    std::ifstream fs(kProcDirectory + to_string(pid) + kStatusFilename);
    if (fs.is_open()){
        for (int i = 0; i < 18; i++) {
            std::getline(fs, line);
        }
        std::stringstream ss(line);
        ss >> token >> token;
        fs.close();
    }

    for ( auto unit : units_in_kb ) {
        value = stoi(token);
        value = value / std::get<0>(unit);
        if ((int)value != 0 ) {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << value;
            pretty_value  = ss.str();
            ram = pretty_value + std::get<1>(unit);
            break;
        }
    }
    if (ram.size() < 7) {
        ram.insert(0, 7 - ram.size(), ' ');
    }

    return ram; 
}

int LinuxParser::Uid(int pid) {
    int uid{0};
    string line;
    string value;
    
    std::ifstream fs(kProcDirectory + to_string(pid) + kStatusFilename);
    if (fs.is_open()) {
        for (int i = 0; i < 9; i++) {
            std::getline(fs, line);
        }
        std::stringstream ss(line);
        ss >> value >> value;
        if (std::all_of(value.begin(),value.end(), isdigit)) {
            uid = stoi(value);
        }
        fs.close();
    }
    return uid;
}

string LinuxParser::User(int uid) { 
    string name = "";
    string line;
    string user;
    string id;
    
    std::ifstream fs(kPasswordPath);
    if(fs.is_open()) {
        while (std::getline(fs, line)) {
            if (line.find(to_string(uid)) != std::string::npos) {
                std::replace(line.begin(), line.end(), ':', ' ');
                std::stringstream ss(line);
                ss >> user >> id >> id;
                if (uid == stoi(id)) {
                    name = user;
                    break;
                }
            }
        }
        fs.close();
    }
    return name;
}

long LinuxParser::StartTime(int pid) {
    int start{0};
    vector<long> pid_stat = LinuxParser::PidStat(pid);
    int clock_ticks{1};

    clock_ticks = LinuxParser::SysClk();
    start = pid_stat[kStartTime_] / clock_ticks;
    return start;
}

string LinuxParser::NthToken(string line, int token_pos)
{
    string token = "";
    std::stringstream ss(line);

    for (int i = 1; i <= token_pos; i++) {
        ss >> token;
    }
    return token;
}

int LinuxParser::SysClk() {
    int clock_ticks{0};
    clock_ticks = sysconf(_SC_CLK_TCK);
    return clock_ticks;
};