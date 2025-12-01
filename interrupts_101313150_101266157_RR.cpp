/**
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 */

#include"interrupts_101313150_101266157.hpp"

void FCFS(std::vector<PCB> &ready_queue) {
    std::sort( 
                ready_queue.begin(),
                ready_queue.end(),
                []( const PCB &first, const PCB &second ){
                    return (first.arrival_time > second.arrival_time); 
                } 
            );
}

std::tuple<std::string > run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   //The ready queue of processes
    std::vector<PCB> wait_queue;    //The wait queue of processes
    std::vector<PCB> job_list = list_processes;   //A list to keep track of all the processes.

    unsigned int current_time = 0;
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;

    //make the output table (the header row)
    execution_status = print_exec_header();

    // return empty table if there are no processes 
    if (job_list.empty()) {
        execution_status += print_exec_footer();
        return std::make_tuple(execution_status);
    }

    // BONUS: open memory usage log
    std::ofstream mem_out("memory.txt");

    // log memory state when a new process is admitted
    auto log_memory = [&](unsigned int time, int pid, const char* event) {
        const unsigned int partition_sizes[6] = {40, 25, 15, 10, 8, 2};
        bool partition_used[6] = {false, false, false, false, false, false};

        for (const PCB &proc : job_list) {
            if (proc.partition_number >= 0 && proc.state != TERMINATED) {
                int idx = proc.partition_number;
                if (idx >= 0 && idx < 6) {
                    partition_used[idx] = true;
                }
            }
        }

        unsigned int total_used = 0;
        unsigned int total_usable_free = 0;

        for (int i = 0; i < 6; ++i) {
            if (partition_used[i]) {
                total_used += partition_sizes[i];
            } else {
                total_usable_free += partition_sizes[i];
            }
        }

        mem_out << "t=" << time << "ms"
                << " PID=" << pid
                << " event=" << event
                << " used=" << total_used
                << " free=" << total_usable_free
                << " partitions=[";
        for (int i = 0; i < 6; ++i) {
            mem_out << (partition_used[i] ? 'X' : '_');
            if (i < 5) mem_out << ' ';
        }
        mem_out << "]\n";
    };
    

    const std::size_t n = job_list.size();
    std::vector<unsigned int> cpu_since_last_io(n, 0);
    std::vector<int> io_completion_time(n, -1);
    std::vector<unsigned int> quantum_used(n, 0);

    const unsigned int TIME_QUANTUM = 100;

    for (std::size_t i = 0; i < n; ++i) {
        job_list[i].state = NEW;
        job_list[i].remaining_time = job_list[i].processing_time;
        job_list[i].partition_number = -1;
    }

    int running_index = -1;
    std::vector<int> rr_queue;

    //Loop while till there are no ready or waiting processes.

    while(!all_process_terminated(job_list)) {

	for (std::size_t i = 0; i < n; ++i) {
            PCB &p = job_list[i];

            // if state is NEW and arrival time has been reached admit the process
            if (p.state == NEW && p.arrival_time <= current_time) {

                if (assign_memory(p)) {
                    execution_status += print_exec_status(current_time, p.PID, NEW, READY);
                    p.state = READY;

                    ready_queue.push_back(p);
                    rr_queue.push_back(static_cast<int>(i));
                }
            }
	}
	
        ///////////////////////MANAGE WAIT QUEUE/////////////////////////

	// When I/O is completed move process from WAITING to READY
	for (std::size_t i = 0; i < n; ++i) {
            PCB &p = job_list[i];
            if (p.state == WAITING && io_completion_time[i] == static_cast<int>(current_time)) {
                execution_status += print_exec_status(current_time, p.PID, WAITING, READY);
                p.state = READY;
                io_completion_time[i] = -1;

                ready_queue.push_back(p);
                rr_queue.push_back(static_cast<int>(i));

		log_memory(current_time, p.PID, "NEW->READY");
            }
        }

        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////
        
	//if CPU is idle pick next process from RR queue
        if (running_index == -1) {
            if (!rr_queue.empty()) {
                running_index = rr_queue.front();
                rr_queue.erase(rr_queue.begin());

                PCB &p = job_list[running_index];
                execution_status += print_exec_status(current_time, p.PID, READY, RUNNING);
                p.state = RUNNING;
                quantum_used[running_index] = 0;
            }
        }

        // If nothing is running advance time
        if (running_index == -1) {
            ++current_time;
            continue;
        }
	
        /////////////////////////////////////////////////////////////////

	PCB &cur = job_list[running_index];

        if (cur.remaining_time > 0) {
            --cur.remaining_time;
        }
        ++cpu_since_last_io[running_index];
        ++quantum_used[running_index];

        unsigned int next_time = current_time + 1;
        bool context_change = false;

        // CPU burst completes
        if (cur.remaining_time == 0) {
            execution_status += print_exec_status(next_time, cur.PID, RUNNING, TERMINATED);
            cur.state = TERMINATED;
            free_memory(cur);
            running_index = -1;
            context_change = true;
        }
        // I/O request
        else if (cur.io_freq > 0 && cpu_since_last_io[running_index] >= cur.io_freq) {
            cpu_since_last_io[running_index] = 0;
            io_completion_time[running_index] = static_cast<int>(next_time + cur.io_duration);
            execution_status += print_exec_status(next_time, cur.PID, RUNNING, WAITING);
            cur.state = WAITING;
            quantum_used[running_index] = 0;
            running_index = -1;
            context_change = true;
        }
        // Quantum expiry
        else if (quantum_used[running_index] >= TIME_QUANTUM) {
            execution_status += print_exec_status(next_time, cur.PID, RUNNING, READY);
            cur.state = READY;
            quantum_used[running_index] = 0;
            rr_queue.push_back(running_index);
            running_index = -1;
            context_change = true;
        }

        current_time = next_time;
        (void)context_change;
    }
    
    //Close the output table
    execution_status += print_exec_footer();

    mem_out.close();

    return std::make_tuple(execution_status);
}


int main(int argc, char** argv) {

    //Get the input file from the user
    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrutps <your_input_file.txt>" << std::endl;
        return -1;
    }

    //Open the input file
    auto file_name = argv[1];
    std::ifstream input_file;
    input_file.open(file_name);

    //Ensure that the file actually opens
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    //Parse the entire input file and populate a vector of PCBs.
    //To do so, the add_process() helper function is used (see include file).
    std::string line;
    std::vector<PCB> list_process;
    while(std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    //With the list of processes, run the simulation
    auto [exec] = run_simulation(list_process);

    write_output(exec, "execution.txt");

    return 0;
}
