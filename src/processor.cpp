#include "processor.h"

#include <sys/time.h>

#include <cmath>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "linux_parser.h"

namespace lp = LinuxParser;

std::vector<float> Processor::Utilization() {
    std::time_t now = std::time(nullptr);

    if ((now - this->data_updated_ > DATA_DELTA) || 
    	(this->cpu_data_.empty())) {
        this->UpdateData();
    }
    if ((now - this->result_updated_ > RESULT_DELTA) ||
        (this->cpu_result_.empty())) {
        this->UpdateResult();
    }

    return cpu_result_;
}

void Processor::UpdateData() {
    std::string line = "";
    std::string line_elements = "";
    int state_value{0};
    int count{0};
    std::vector<int> cpu_state;
    int idle{0};
    int active{0};
    std::time_t now;

    std::ifstream fs(lp::kProcDirectory + lp::kStatFilename);
    if (fs.is_open()) {
        std::getline(fs, line);
        while (std::getline(fs, line)) {
            if (line.find("cpu") == std::string::npos) {
                break;
            }
            line_elements = line.substr(5, line.back());

            std::stringstream ss(line_elements);
            while (ss >> state_value) {
                cpu_state.push_back(state_value);
            }

            idle = cpu_state[lp::kIdle_] + cpu_state[lp::kIOwait_];
            active = cpu_state[lp::kUser_] + cpu_state[lp::kNice_] +
                     cpu_state[lp::kSystem_] + cpu_state[lp::kIRQ_] +
                     cpu_state[lp::kSoftIRQ_] + cpu_state[lp::kSteal_];

            this->AddCpuSample(count, idle, active);
            cpu_state.clear();
            count++;
        }

        fs.close();
    }

    now = std::time(nullptr);
    this->data_updated_ = now;
}

void Processor::UpdateResult() {
    float total{0.0f};
    float total_prev{0.0f};
    float totald{0.0f};
    float idled{0.0f};
    float cpu_usage_pct{0.0f};
    std::vector<float> result;
    std::time_t now;

    for (std::vector<CpuNData> cpu : this->cpu_data_) {
        total = cpu[lp::kPresent_].idle + cpu[lp::kPresent_].active;
        total_prev = cpu[lp::kPast_].idle + cpu[lp::kPast_].active;
        totald = total - total_prev;
        idled = cpu[lp::kPresent_].idle - cpu[lp::kPast_].idle;
        cpu_usage_pct = (totald - idled) / totald;
        if (std::isnan(cpu_usage_pct)) {
            result.push_back(0.0f);
        } else {
            result.push_back(cpu_usage_pct);
        }
    }

    this->cpu_result_ = result;
    now = std::time(nullptr);
    this->result_updated_ = now;
}

void Processor::AddCpuSample(int cpu_id, int idle, int active) {
    size_t count = cpu_id;
    CpuNData data = {idle, active};

    // If vector for this CPUn doesn't yet exist, we need to first create 
    // a new CPUn vector, and prime its CpuNData vector with two samples.
    if (count + 1 > cpu_data_.size()) {
        std::vector<CpuNData> new_cpu{data};
        this->cpu_data_.push_back(new_cpu);
        this->cpu_data_[cpu_id].push_back(data);
    } else {
        this->cpu_data_[cpu_id].erase(this->cpu_data_[cpu_id].begin());
        this->cpu_data_[cpu_id].push_back(data);
    }
}

void Processor::PrintData() {
    int count{0};
    for (std::vector<CpuNData> cpu : this->cpu_data_) {
        std::cout << "CPU" << std::to_string(count)
                  << " idle: " << std::to_string(cpu[1].idle)
                  << " active: " << std::to_string(cpu[1].active)
                  << "\n    prev_idle: " << std::to_string(cpu[0].idle)
                  << " prev_active: " << std::to_string(cpu[0].active) << "\n";
        count++;
    }
}

int Processor::NumCpus() {
    if (this->cpu_data_.size() == 0) {
        this->UpdateData();
    }

    return this->cpu_data_.size();
}