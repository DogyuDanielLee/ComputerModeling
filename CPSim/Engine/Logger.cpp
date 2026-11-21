#include "Logger.h"
#include "Utils.h"

#include <fstream>
#include <string>
#include <cstdlib>
#include <stdio.h>
#include <iomanip>
#include <climits>
#include <mutex>

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

void Logger::Team4_task_read_write_logger(std::string task_name) {
    static std::ofstream log(utils::cpsim_path + "/Log/<Team 4>_read_write.log", std::ios::out);
    static bool header_written = false;

    if (!log) {
        std::cerr << "Cannot open read/write log" << std::endl;
        return;
    }
    if (!header_written) {
        log << "[ TIME ] [ READ/WRITE ] [ TASK NAME ] [ DATA NAME ]" << std::endl;
        header_written = true;
    }
    if (task_name.empty() || io_operation.empty()) return;

    const std::string controller = task_name == "SWC0" ? "CC" : task_name == "SWC1" ? "LK" : task_name;
    if (utils::log_task != controller && utils::log_task != task_name) return;

    std::vector<std::string> data_names;
    if (controller == "CC") {
        if (io_operation == "READ") data_names = {"ACCEL_VALUE", "TARGET_SPEED", "SPEED", "CC_TRIGGER"};
        else if (io_operation == "WRITE") data_names = {"BRAKE", "ACCEL"};
    } else if (controller == "LK") {
        if (io_operation == "READ") data_names = {"rtU.read1", "rtU.read2"};
        else if (io_operation == "WRITE") data_names = {"rtY.write3", "rtY.write4"};
    }
    for (const auto& data_name : data_names) {
        log << io_time << '\t' << io_operation << '\t' << controller << '\t' << data_name << '\n';
    }
    log.flush();
}

void Logger::Team4_real_cyber_schedule_logger(long long time, int job_id, std::string event_type) {
    static std::ofstream log(utils::cpsim_path + "/Log/<Team 4>_schedule.log", std::ios::out);
    static bool header_written = false;

    if(!log) {
        std::cerr << "Cannot open schedule log" << std::endl;
        return;
    }
    if (!header_written) {
        log << "[ TIME ] [ JOB ID ] [ EVENT TYPE ]" << std::endl;
        header_written = true;
    }
    if (time < 0 || job_id <= 0 || event_type.empty()) return;
    const int task_number = task_index + 1;

    log << time << "\tJ" << task_number << ((task_number >= 10 || job_id >= 10) ? "_" : "")
        << job_id << '\t' << event_type << std::endl;
}