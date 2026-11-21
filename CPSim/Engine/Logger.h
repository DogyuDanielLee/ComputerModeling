#ifndef LOGGER_H__
#define LOGGER_H__
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include "Job.h"


/** This file is engine code of CPSim-Re engine
 * @file Logger.h
 * @class Logger
 * @author Seonghyeon Park
 * @date 2020-04-30
 * @brief Codes for Engine-Logger 
*/
class Logger{
private:
    std::vector<std::shared_ptr<Job>> m_execution_order_buffer;
    std::vector<double> m_current_time_buffer;
    
public:
    /**
     * Constructors and Destructors
     */
    Logger();
    ~Logger();

    /**
     * Getter & Setter
     */

    std::vector<std::shared_ptr<Job>> get_execution_order_buffer();
    void set_execution_order_buffer(std::vector<std::shared_ptr<Job>>);
    void add_current_simulated_job(std::shared_ptr<Job>);
    void start_logging();
    void log_task_vector_status(std::vector<std::shared_ptr<Task>>&);
    void log_job_vector_of_each_ECU_status(std::vector<std::vector<std::vector<std::shared_ptr<Job>>>>&);
    void log_job_vector_of_simulator_status(std::vector<std::shared_ptr<Job>>&);
    void log_which_job_was_deadline_miss(std::shared_ptr<Job>);
    void print_job_execution_on_ECU(std::vector<std::shared_ptr<Job>>, std::vector<std::shared_ptr<Job>>, int);
    void print_job_execution_schedule();
    void print_offline_guider_status();
    void set_schedule_log_info(std::vector<std::shared_ptr<Task>>&);

    /**
     * Homework #2 (Team 4)
     *
     * Case #1: Team4_task_read_write_logger(task_name)
     *   Caller sets io_time (real-cyber-system time in ms) and io_operation ("READ"/"WRITE")
     *   first. Only tasks listed in settings.txt log_task (e.g. "LK,CC") are logged.
     *   Lines are buffered and written sorted by time by Team4_flush_read_write_log(),
     *   which the Executor calls once per hyper period.
     *   Output: Log/Team4_read_write.log
     *
     * Case #2: Team4_real_cyber_schedule_logger(time, job_id, event_type)
     *   Caller sets schedule_task_number (1-based task index) first; job_id is the job's
     *   index since the simulation started. event_type: RELEASED, STARTED, FINISHED,
     *   DEADLINE MISS. The Executor feeds events in time order.
     *   Output: Log/Team4_schedule.log
     */
    long long io_time = 0;
    std::string io_operation;
    int schedule_task_number = 0;
    int schedule_task_count = 0;   // number of tasks in the design (set by the Executor)
    void Team4_task_read_write_logger(std::string task_name);
    void Team4_flush_read_write_log();
    void Team4_real_cyber_schedule_logger(long long time, int job_id, std::string event_type);

private:
    struct ReadWriteEntry
    {
        long long time;
        long long seq;
        std::string line;
    };
    std::vector<ReadWriteEntry> m_read_write_buffer;
    long long m_read_write_seq = 0;
    std::mutex m_read_write_mutex;
};

#endif