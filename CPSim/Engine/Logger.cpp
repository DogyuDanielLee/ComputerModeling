#include "Logger.h"
#include "Utils.h"

#include <fstream>
#include <string>
#include <cstdlib>
#include <stdio.h>
#include <iomanip>
#include <climits>
#include <mutex>
#include <algorithm>

/**
 *  This file is the cpp file for the Logger class.
 *  @file Logger.cpp
 *  @brief cpp file for Engine-Logger
 *  @author Seonghyeon Park
 *  @date 2020-03-31
 */


/**
 * @fn Logger::Logger()
 * @brief the function of basic constructor of Logger
 * @author Seonghyeon Park
 * @date 2020-04-01
 * @details 
 *  - None
 * @param none
 * @return none
 * @bug none
 * @warning none
 * @todo none
 */
Logger::Logger()
{
    
}

/**
 * @fn Logger::~Logger()
 * @brief the function of basic destructor of Logger
 * @author Seonghyeon Park
 * @date 2020-04-01
 * @details 
 *  - None
 * @param none
 * @return none
 * @bug none
 * @warning none
 * @todo none
 */
Logger::~Logger()
{

}

/**
 * @fn void start_logging()
 * @brief this function starts the logging of simulation events
 * @author Seonghyeon Park
 * @date 2020-04-01
 * @details 
 *  - None
 * @param none
 * @return none
 * @bug none
 * @warning none
 * @todo none
 */

void Logger::set_schedule_log_info(std::vector<std::shared_ptr<Task>>& task_vector)
{
    std::ofstream scheduling_log;
    scheduling_log.open(utils::cpsim_path + "/Log/scheduling.log", std::ios::out);     
    std::string contents = "";
    for(int idx = 0; idx < task_vector.size(); idx++)
    {
        contents += "ECU" + std::to_string(task_vector.at(idx)->get_ECU()->get_ECU_id())+ ": " + task_vector.at(idx)->get_task_name();
        if(idx == task_vector.size() - 1)
            contents += "\n";
        else
        {
            contents += ", ";
        }
    }
    scheduling_log.write(contents.c_str(), contents.size());
    scheduling_log.close();
}

void Logger::start_logging()
{
    std::ofstream scheduling_log;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - utils::simulator_start_time).count()  < utils::simulation_termination_time)
    {
        scheduling_log.open(utils::cpsim_path + "/Log/scheduling.log", std::ios::app);    
        utils::mtx_data_log.lock();
        if(global_object::schedule_data.size() > 10)
        {
            int min_idx = 0;
            std::shared_ptr<ScheduleData> current_data = global_object::schedule_data.front();
            for (int idx = 0; idx < global_object::schedule_data.size(); idx ++)
            {
                if(current_data->get_time() > global_object::schedule_data.at(idx)->get_time())
                {
                    current_data = global_object::schedule_data.at(idx);
                    min_idx = idx;
                }
            }
            
            global_object::schedule_data.erase(global_object::schedule_data.begin() + min_idx);
            scheduling_log.write(current_data->get_data().c_str(), current_data->get_data().size());
        }
        scheduling_log.close();
        utils::mtx_data_log.unlock();    
    }    
}

/* ------------------------------------------------------------------ */
/* Homework #2 (Team 4)                                                */
/* ------------------------------------------------------------------ */

namespace
{
    const std::string kTeam = "Team4";

    // Pad a column to the width of its header cell, always leaving at least one space.
    std::string pad(const std::string& text, std::size_t width)
    {
        return text + std::string(text.size() + 1 < width ? width - text.size() : 1, ' ');
    }

    std::string trim(const std::string& text)
    {
        const char* ws = " \t\r\n\"";
        std::size_t begin = text.find_first_not_of(ws);
        if (begin == std::string::npos) return "";
        std::size_t end = text.find_last_not_of(ws);
        return text.substr(begin, end - begin + 1);
    }

    // "CC" or "LK" for the cruise-control / lane-keeping SWCs, whatever the SWC is called in the
    // design (SWC0, CC, ...). Decided from the source file the SWC runs (CC.cpp, Team4_LK.cpp, ...).
    std::string controller_of(const std::string& task_name)
    {
        auto it = utils::swc_code_name.find(task_name);
        std::string code = (it != utils::swc_code_name.end()) ? it->second : task_name;
        auto ends_with = [&](const std::string& suffix) {
            return code.size() >= suffix.size() && code.compare(code.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        if (ends_with("CC")) return "CC";
        if (ends_with("LK")) return "LK";
        return task_name;
    }

    // log_task in settings.txt is a comma-separated list such as "LK,CC". An entry matches
    // either the SWC name in the design (SWC1) or the controller it runs (LK).
    bool is_logged_task(const std::string& task_name, const std::string& controller)
    {
        std::string list = utils::log_task_name;
        std::size_t begin = 0;
        while (begin <= list.size())
        {
            std::size_t end = list.find(',', begin);
            if (end == std::string::npos) end = list.size();
            std::string entry = trim(list.substr(begin, end - begin));
            if (!entry.empty() && (entry == task_name || entry == controller)) return true;
            begin = end + 1;
        }
        return false;
    }

    // The data each controller reads at job start and writes at job finish (see CC.cpp / LK.cpp)
    std::vector<std::string> data_names_of(const std::string& controller, const std::string& operation)
    {
        if (controller == "CC")
        {
            if (operation == "READ") return {"ACCEL_VALUE", "TARGET_SPEED", "SPEED", "CC_TRIGGER"};
            if (operation == "WRITE") return {"BRAKE", "ACCEL"};
        }
        else if (controller == "LK")
        {
            if (operation == "READ") return {"rtU.read1", "rtU.read2"};
            if (operation == "WRITE") return {"rtY.write3", "rtY.write4"};
        }
        return {};
    }

    std::ofstream& read_write_log()
    {
        static std::ofstream log(utils::cpsim_path + "/Log/" + kTeam + "_read_write.log", std::ios::out | std::ios::trunc);
        static bool header_written = false;
        if (log && !header_written)
        {
            log << "[ TIME ] [ READ/WRITE ] [ TASK NAME ] [ DATA NAME ]" << std::endl;
            header_written = true;
        }
        return log;
    }

    std::ofstream& schedule_log()
    {
        static std::ofstream log(utils::cpsim_path + "/Log/" + kTeam + "_schedule.log", std::ios::out | std::ios::trunc);
        static bool header_written = false;
        if (log && !header_written)
        {
            log << "[ TIME ] [ JOB ID ] [ EVENT TYPE ]" << std::endl;
            header_written = true;
        }
        return log;
    }
}

void Logger::Team4_task_read_write_logger(std::string task_name)
{
    if (task_name.empty() || (io_operation != "READ" && io_operation != "WRITE")) return;
    // A negative time means the job never started/finished on the real cyber system (deadline
    // miss), so the real task never read or wrote anything: nothing to log.
    if (io_time < 0) return;

    const std::string controller = controller_of(task_name);
    if (!is_logged_task(task_name, controller)) return;

    std::lock_guard<std::mutex> lock(m_read_write_mutex);
    for (const auto& data_name : data_names_of(controller, io_operation))
    {
        std::string line = pad(std::to_string(io_time), 9) + pad(io_operation, 15) + pad(controller, 14) + data_name;
        m_read_write_buffer.push_back({io_time, m_read_write_seq++, line});
    }
}

void Logger::Team4_flush_read_write_log()
{
    std::ofstream& log = read_write_log();   // creates the file (with header) even if nothing was logged
    std::lock_guard<std::mutex> lock(m_read_write_mutex);
    if (!log)
    {
        std::cerr << "[Logger] Cannot open " << kTeam << "_read_write.log" << std::endl;
        m_read_write_buffer.clear();
        return;
    }
    // Jobs are simulated in EDF order on the simulation PC, not in real-time order, so sort.
    std::sort(m_read_write_buffer.begin(), m_read_write_buffer.end(),
              [](const ReadWriteEntry& a, const ReadWriteEntry& b) {
                  return a.time != b.time ? a.time < b.time : a.seq < b.seq;
              });
    for (const auto& entry : m_read_write_buffer)
        log << entry.line << '\n';
    log.flush();
    m_read_write_buffer.clear();
}

void Logger::Team4_real_cyber_schedule_logger(long long time, int job_id, std::string event_type)
{
    std::ofstream& log = schedule_log();
    if (!log)
    {
        std::cerr << "[Logger] Cannot open " << kTeam << "_schedule.log" << std::endl;
        return;
    }
    if (time < 0 || job_id <= 0 || schedule_task_number <= 0 || event_type.empty()) return;

    // J<task><job> as in the slides (J11, J12, J21...). With 10+ tasks that would be ambiguous,
    // so then a separator is used: J10_3.
    std::string job_label = "J" + std::to_string(schedule_task_number)
                          + (schedule_task_count >= 10 ? "_" : "")
                          + std::to_string(job_id);
    log << pad(std::to_string(time), 9) << pad(job_label, 11) << event_type << std::endl;
}
