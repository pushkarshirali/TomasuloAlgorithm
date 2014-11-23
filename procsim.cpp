#include "procsim.hpp"
#include<string>
#include<queue>
#include<iostream>
using namespace std;
uint64_t r,k0,k1,k2,f;
proc_inst_t dispatch;
uint32_t start=0;
int32_t tail=0;
double new_start= 0;
double new_tail=-1;
uint64_t temp_tag;
float total_fired=0;
float total_retired=0;
double counter[100000];
uint64_t counting_guy=0;
queue<uint64_t> func_tag;           // Queues used to store tags loaded on functional units over different cycles
void stage_fetch(int32_t x, proc_stats_t* p_stats);
void stage_execute(int32_t as,proc_stats_t* p_stats);
void stage_dispatch(int32_t x, proc_stats_t* p_stats);
void stage_schedule(int32_t a,proc_stats_t* p_stats);
void stage_execute_two(int32_t d);
void stage_stateupdate(int32_t w, proc_stats_t* p_stats);
void sort_schedQ();

struct Output
{
    uint64_t dispatched;
    uint64_t fetched;
    uint64_t scheduled;
    uint64_t executed;
    uint64_t updated;
};
Output* output;

struct DispQ
{
    uint32_t address;
    int func_unit;
    int src_reg_1;
    int src_reg_2;
    int dest_reg;
    
};
DispQ* dispatchq;

struct SchedQ
{
    int func_unit;
    int dest_reg;
    uint64_t dest_reg_tag;
    bool src_1_ready;
    uint64_t src_1_tag;
    bool src_2_ready;
    uint64_t src_2_tag;
    bool ready_to_fire;
    bool empty_slot;
    bool done;
    bool fired;
    bool update;
};
SchedQ* RS;

struct reg_file
{
    bool ready;
    uint64_t tag;
    
};
reg_file *registerfile;

struct Scoreboard0
{
    bool busy;
    uint64_t tag;
};
Scoreboard0 *scoreboard0;

struct Scoreboard1
{
    bool busy;
    uint64_t tag;
};
Scoreboard1 *scoreboard1;

struct Scoreboard2
{
    bool busy;
    uint64_t tag;
};
Scoreboard2 *scoreboard2;


struct CDB
{
    bool cdb_busy;
    uint64_t cdb_tag;
};
CDB *result_bus;


void setup_proc(uint64_t R, uint64_t K0, uint64_t K1, uint64_t K2, uint64_t F)
{
    r= R;
    k0 = K0;
    k1= K1;
    k2=K2;
    f=F;
    
    RS = new SchedQ[2*(k0+k1+k2)];
    dispatchq = new DispQ[100000];
    registerfile = new reg_file[128];
    result_bus = new CDB[r];
    scoreboard1 = new Scoreboard1[k1];
    scoreboard0 = new Scoreboard0[k0];
    scoreboard2 = new Scoreboard2[k2];
    output = new Output[100000];
    
    for(int i=0; i<100000; i++)
    {
        output[i].fetched = 0;
        output[i].dispatched = 0;
        output[i].executed = 0;
        output[i].scheduled =0;
        output[i].updated = 0;
    }
    
    for(uint64_t i =0; i<100; i++)
    {
        dispatchq[i].func_unit = 0;
        dispatchq[i].address = 0;
        dispatchq[i].src_reg_1 = 0;
        dispatchq[i].src_reg_2 = 0;
        dispatchq[i].dest_reg = 0;
        
        
    }
    
    for(int i=0; i<2*(k0+k1+k2); i++)
    {
        RS[i].func_unit = 0;
        RS[i].dest_reg = 0;
        RS[i].dest_reg_tag = 0;
        RS[i].src_1_ready = 1;
        RS[i].src_1_tag = 0;
        RS[i].src_2_ready = 1;
        RS[i].src_2_tag = 0;
        RS[i].ready_to_fire = 0;
        RS[i].empty_slot =1;
        RS[i].fired = 0;
        RS[i].update = 0;
        RS[i].done = 0;
    }
    
    for(int i=0; i<128;i++)
    {
        registerfile[i].ready= true;
        registerfile[i].tag=0;
    }
    
    for(int i=0;i<r;i++)
    {
        result_bus[i].cdb_busy=false;
        result_bus[i].cdb_tag=0;
    }
    for (int i=0; i<(k0); i++)
    {
        scoreboard0[i].busy = false;
    }
    
    for (int i=0; i<(k1); i++)
    {
        scoreboard1[i].busy = false;
    }
    for (int i=0; i<(k2); i++)
    {
        scoreboard2[i].busy = false;
    }
    
}



void run_proc(proc_stats_t* p_stats)
{
    for(uint64_t i=0; i<140000; i++)
    {
        
        p_stats->cycle_count++;
        
        if(p_stats->cycle_count>3)              //Shouldn't happen before cycle 4
            stage_execute(start, p_stats);
        
        if(p_stats->cycle_count>2)              //Shouldn't happen before cycle 3
            stage_schedule(start, p_stats);
        
        if(p_stats->cycle_count>1 && start!=100000)     //Shouldn't happen before cycle 2
            stage_dispatch(tail, p_stats);
        
        if(p_stats->cycle_count<(100000/f)+1)           //Shouldn't happen for more than 100k instructions
            stage_fetch(tail, p_stats);
        
        counter[counting_guy] = (new_tail+1-new_start); // array that stores length of the dispatch queue every cycle
        counting_guy++;
        
        if(p_stats->cycle_count>3)                  //Shouldn't happen before cycle 4
            stage_execute_two(start);
        
        if(p_stats->cycle_count>3)                  //Shouldn't happen before cycle 4
            stage_stateupdate(start, p_stats);
        
        if(p_stats->retired_instruction == 100000)
        {
            stage_stateupdate(start, p_stats);
            break;
        }
        
        
    }
    // outputs cycle by cycle behaviour on the screen
        cout<<"INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\t"<<endl;
     for(uint64_t i =0; i<100000; i++)
     {
     cout<<i+1<<"\t"<<output[i].fetched<<"\t"<<output[i].dispatched<<"\t"<<output[i].scheduled<<"\t"<<output[i].executed<<"\t"<<output[i].updated<<endl;
     }
    
    cout<<endl; 
    
}

void stage_fetch(int32_t x, proc_stats_t* p_stats)                          // Fetches a new set of instructions from the
{                                                                           // Trace file and loads the dispatch queue accordingly
    if(tail == 100000)
        return;
    
    for(uint64_t i = 0; i<f;i++)
    {
        if(read_instruction(&dispatch))
        {
            dispatchq[tail].address = dispatch.instruction_address;
            dispatchq[tail].dest_reg = dispatch.dest_reg;
            dispatchq[tail].func_unit = dispatch.op_code;
            dispatchq[tail].src_reg_1 = dispatch.src_reg[0];
            dispatchq[tail].src_reg_2 = dispatch.src_reg[1];
            
            output[tail].fetched = p_stats->cycle_count;
            output[tail].dispatched = p_stats->cycle_count+1;
            tail++;                                                         // tail points to the next empty slot in the dispatch queue
            new_tail++;
        }
    }
}

void stage_dispatch(int32_t x, proc_stats_t* p_stats)               // If there is an empty spot in the scheduling queue
{                                                                   // an instruction is loaded there and deleted from dispatch
    int32_t sched_limiter = start;
    for (uint64_t i=0; i<2*(k0+k1+k2);i++)
    {
        if(RS[i].empty_slot == true && start<(sched_limiter+f))
        {
            RS[i].func_unit = dispatchq[start].func_unit;
            if(dispatchq[start].src_reg_1 == -1)
                RS[i].src_1_ready = true;
            if(dispatchq[start].src_reg_1 != -1 )
            {
                if(registerfile[dispatchq[start].src_reg_1].ready == true)
                    RS[i].src_1_ready = true;
                else
                {
                    RS[i].src_1_tag = registerfile[dispatchq[start].src_reg_1].tag;
                    RS[i].src_1_ready = false;
                }
            }                                                   //Checking ready bits of source registers from the registerfile
            
            if(dispatchq[start].src_reg_2 == -1)
                RS[i].src_2_ready = true;
            if(dispatchq[start].src_reg_2 != -1)
            {
                if(registerfile[dispatchq[start].src_reg_2].ready == true)
                    RS[i].src_2_ready = true;
                else
                {
                    RS[i].src_2_tag = registerfile[dispatchq[start].src_reg_2].tag;
                    RS[i].src_2_ready = false;
                    
                }
            }
            
            if(dispatchq[start].dest_reg == -1)                     //Assigning tags to destination registers
            {
                RS[i].dest_reg_tag = start;
                RS[i].dest_reg = dispatchq[start].dest_reg;
                
            }
            else
            {
                RS[i].dest_reg = dispatchq[start].dest_reg;
                registerfile[RS[i].dest_reg].tag = start;
                registerfile[RS[i].dest_reg].ready = false;
                RS[i].dest_reg_tag = start;
            }
            
            RS[i].empty_slot = false;
            output[start].scheduled = p_stats->cycle_count+1;
            start++;
            new_start++;
        }
    }
    
}


void stage_schedule(int32_t a, proc_stats_t* p_stats)               // Instructions whose source bits are ready are marked to fire
{                                                                   // and sent to functional units in tag order
    sort_schedQ();
    for(uint64_t i = 0; i<2*(k0+k1+k2); i++)
    {
        if(RS[i].src_1_ready == true && RS[i].src_2_ready == true && RS[i].empty_slot == false && RS[i].fired == false)
        {
            
            if(RS[i].func_unit == 0)
            {
                for(uint64_t j=0; j<k0;j++)
                {
                    if(scoreboard0[j].busy == false)
                    {
                        scoreboard0[j].busy = true;
                        scoreboard0[j].tag = RS[i].dest_reg_tag;
                        RS[i].fired = true;                         // so as to avoid refiring of the same instruction
                        func_tag.push(RS[i].dest_reg_tag);             // in the next cycle
                        output[RS[i].dest_reg_tag].executed = p_stats->cycle_count+1;
                        total_fired++;
                        break;
                        
                    }
                    
                }
            }
            
            if(RS[i].func_unit == 1 || RS[i].func_unit == -1)       // since -1 also corresponds to FU k1
            {
                
                for(uint64_t j=0; j<k1; j++)
                {
                    
                    if(scoreboard1[j].busy == false)
                    {
                        scoreboard1[j].busy = true;
                        scoreboard1[j].tag = RS[i].dest_reg_tag;
                        RS[i].fired = true;
                        func_tag.push(RS[i].dest_reg_tag);
                        output[RS[i].dest_reg_tag].executed = p_stats->cycle_count+1;
                        total_fired++;
                        break;
                    }
                }
            }
            
            if(RS[i].func_unit ==2)
            {
                for(uint64_t j=0; j<k2; j++)
                {
                    if(scoreboard2[j].busy == false)
                    {
                        scoreboard2[j].busy = true;
                        scoreboard2[j].tag = RS[i].dest_reg_tag;
                        RS[i].fired = true;
                        func_tag.push(RS[i].dest_reg_tag);
                        output[RS[i].dest_reg_tag].executed = p_stats->cycle_count+1;
                        total_fired++;
                        break;
                    }
                }
            }
        }
    }
    
}

void sort_schedQ()                          // Bubble Sorts elements of the schedule queue in tag order
{
    SchedQ temp;
    for(uint64_t i = 0; i<2*(k0+k1+k2) ; i++)
    {
        for(uint64_t j=0; j<2*(k0+k1+k2)-1; j++)
        {
            if(RS[j].dest_reg_tag > RS[j+1].dest_reg_tag)
            {
                temp = RS[j];
                RS[j] = RS[j+1];
                RS[j+1] = temp;
            }
        }
    }
}

void stage_execute(int32_t as, proc_stats_t* p_stats)          //Executed instructions are loaded on to the cdb and FU's are freed
{
    for(uint64_t i=0; i<r; i++)
    {
        if(result_bus[i].cdb_busy == false)
        {
            result_bus[i].cdb_tag = func_tag.front();           // Preference is given to instructions that have
            result_bus[i].cdb_busy = true ;                     // spent more time in the functional units
            
            for(uint64_t j=0; j<k0; j++)
            {
                if(scoreboard0[j].busy == true)
                {
                    if(scoreboard0[j].tag == result_bus[i].cdb_tag)
                    {
                        scoreboard0[j].busy = false;
                        func_tag.pop();
                        
                    }
                }
            }
            
            for(uint64_t j=0; j<k1; j++)
            {
                
                if(scoreboard1[j].busy == true)
                {
                    if(scoreboard1[j].tag == result_bus[i].cdb_tag)
                    {
                        scoreboard1[j].busy = false;
                        func_tag.pop();
                    }
                }
            }
            
            for(uint64_t j=0; j<k2; j++)
            {
                if(scoreboard2[j].busy == true)
                {
                    if(scoreboard2[j].tag == result_bus[i].cdb_tag)
                    {
                        scoreboard2[j].busy = false;
                        func_tag.pop();
                        
                    }
                }
            }
        }
        
        for(uint64_t j=0; j<128; j++)                   // Register files are updated using tag values loaded on the cdb
        {
            if(registerfile[j].tag == result_bus[i].cdb_tag && registerfile[j].ready == false)
            {
                registerfile[j].ready = true;
            }
        }
    }
}

void stage_execute_two(int32_t d)                   // Scheduling queue is updated using tag values on the cdb
{
    for(uint64_t j=0; j<r; j++)
    {
        for(uint64_t i=0; i<2*(k0+k1+k2); i++)
        {
            if(RS[i].src_1_tag == result_bus[j].cdb_tag && RS[i].empty_slot == false)
            {
                RS[i].src_1_ready = true;
            }
            
            if(RS[i].src_2_tag == result_bus[j].cdb_tag && RS[i].empty_slot == false)
                RS[i].src_2_ready = true;
            if(RS[i].dest_reg_tag == result_bus[j].cdb_tag)
            {
                RS[i].done = true;
            }
            
            
            
        }
        
        result_bus[j].cdb_busy = false;
    }
    
}

void stage_stateupdate(int32_t w, proc_stats_t* p_stats)
{
    for(uint64_t i=0; i<2*(k0+k1+k2); i++)
    {
        if(RS[i].update == true && RS[i].done == true)
        {
            output[RS[i].dest_reg_tag].updated = p_stats->cycle_count;
            RS[i].empty_slot = true;
            RS[i].fired = false;
            RS[i].update = false;
            RS[i].done = false;
            total_retired++;
            if(p_stats->retired_instruction<100000)
                p_stats->retired_instruction++;
        }
        
        if(RS[i].done == true)             // this is done to avoid deletion of an instruction in the same
        {                                  // cycle that it executes in
            RS[i].update = true;
        }
        
    }
}


void complete_proc(proc_stats_t *p_stats)
{
    double sum =0;
    uint64_t max;
    
    for(uint64_t i =0; i<counting_guy; i++)
    {
        sum = sum+ counter[i];
    }
    max= counter[0];
    for(uint64_t i =0; i<counting_guy-1; i++)
    {
        if(counter[i+1]>counter[i])
            max = counter[i+1];
    }
    p_stats->max_disp_size = max;
    p_stats->avg_disp_size = (sum)/(p_stats->cycle_count);
    p_stats->avg_inst_fired = total_fired/(p_stats->cycle_count);
    p_stats->avg_inst_retired = total_retired/(p_stats->cycle_count);
    
    delete [] RS;
    delete [] scoreboard0;
    delete [] scoreboard1;
    delete [] scoreboard2;
    delete [] dispatchq;
    
}
