// pager.cc - implementation of a memory pager
// Liam Juskevice and Kevin Li

#include "vm_pager.h"
#include <iostream>
#include <vector>
#include <map>

using namespace std;

static unsigned number_pages;
static unsigned disk_block_count;
//static unsigned long ppage_counter = 0;
static map<pid_t, tuple<int, page_table_t>> processes;
static pid_t curr_pid;
static map<pid_t, unsigned int> block_recorder;
typedef struct {
    unsigned int valid: 1;
    unsigned int reference: 1;
    unsigned int resident: 1;
    unsigned int dirty: 1;
    page_table_entry_t* pte;
    unsigned int disk_block;
    unsigned int filled: 1;
    unsigned long virtual_address;
} virtual_pages;

static map<pid_t, vector<virtual_pages>> vp_storage; //data structure of vm_extend

static map<pid_t, tuple<unsigned int, vector<virtual_pages>>> second_chance;
//static //data structure second chance algorithm?

/*
 * vm_init
 *
 * Initializes the pager and any associated data structures. Called automatically
 * on pager startup. Passed the number of physical memory pages and the number of
 * disk blocks in the raw disk.
 */
void vm_init(unsigned memory_pages, unsigned disk_blocks) {
    number_pages = memory_pages;
    disk_block_count = disk_blocks;
    //cout << "disk_block_count: " << disk_block_count << endl;
    //for (int i = 0; i < int(memory_pages); i++) {
    //    page_table_t pte;
    //    second_chance.push_back(pte);
    //}
    //cout << "second chance size: " << second_chance.size() << endl;
}

/*
 * vm_create
 *
 * Notifies the pager that a new process with the given process ID has been created.
 * The new process will only run when it's switched to via vm_switch.
 */
void vm_create(pid_t pid) {
    page_table_t pt;
    /*
    for (int i = 0; i < VM_ARENA_SIZE / VM_PAGESIZE; i++) {
        pt.ptes[i].read_enable = 0;
        pt.ptes[i].write_enable = 0;
        pt.ptes[i].ppage = ppage_counter;
        ppage_counter++;
    }
    */
    processes.insert({pid, make_tuple(0, pt)});
    vector<virtual_pages> vp_vector;
    vp_storage.insert({pid,vp_vector});
    get<0>(second_chance[pid]) = 0;
    block_recorder[pid] = 0;
    // TODO
}

/*
 * vm_switch
 *
 * Notifies the pager that the kernel is switching to a new process with the
 * given pid.
 */
void vm_switch(pid_t pid) {
    page_table_base_register = &get<1>(processes.at(pid));
    curr_pid = pid;
    
    // TODO
}

/*
 * s_c_algorithm
 */
unsigned int s_c_algorithm(virtual_pages faulty) {
    unsigned int pointer = get<0>(second_chance[curr_pid]);
    //unsigned int ref = get<1>(second_chance[curr_pid])[pointer].reference;
    //unsigned s_c_size = get<1>(second_chance[curr_pid]).size(); 
    //if (s_c_size < number_pages) {
    //    cout << "size of second chance: " << s_c_size << endl;
    //    get<1>(second_chance[curr_pid])[s_c_size].pte->ppage = s_c_size;
    //}
    while (get<1>(second_chance[curr_pid])[pointer].reference != 0) {
        get<1>(second_chance[curr_pid])[pointer].reference = 0;
        //cout << get<1>(second_chance[curr_pid]).size() << endl;
        if (pointer < get<1>(second_chance[curr_pid]).size()-1) {
            pointer++;
        }
        else{
            pointer = 0;
        }
        //cout << "pointer: " << pointer << endl;
    }
    //dirty bit = 1 --> write to disk (disk_write)
    get<1>(second_chance[curr_pid])[pointer].resident = 0;
    //cout << "resident changed" << endl;
    get<1>(second_chance[curr_pid])[pointer].pte->read_enable = 0;
    get<1>(second_chance[curr_pid])[pointer].pte->write_enable = 0;
    //
    if (get<1>(second_chance[curr_pid])[pointer].dirty == 1) {
        //cout << "about to write disk" << endl;
        //cout << "faulty's ppage: " << get<1>(second_chance[curr_pid])[pointer].pte->ppage << endl;
        //cout << "evicting page's disk block: " << get<1>(second_chance[curr_pid])[pointer].disk_block << endl;
        disk_write(get<1>(second_chance[curr_pid])[pointer].disk_block, get<1>(second_chance[curr_pid])[pointer].pte->ppage);//page_table_base_register->ptes.ppage);
        //cout << "after" << endl;
    }        
    
    //evict part, insert new faulty into second chance
    //cout << "faulty's reference bit: " << faulty.reference << endl;
    get<1>(second_chance[curr_pid])[pointer] = faulty;
    get<1>(second_chance[curr_pid])[pointer].pte->ppage = pointer;
    //cout << "ppage of new inserted frame: " << get<1>(second_chance[curr_pid])[pointer].pte->ppage << endl;    
    if (faulty.filled) {
        //cout << "about to read disk" << endl;
        disk_read(get<1>(second_chance[curr_pid])[pointer].disk_block, faulty.pte->ppage);
        //cout << "after reading disk" << endl;
    } 
    get<0>(second_chance[curr_pid]) = pointer; 
    //cout << "resident is: " << faulty.resident << endl;   
    return pointer;
}




/*
 * vm_fault
 *
 * Handle a fault that occurred at the given virtual address. The write flag
 * is 1 if the faulting access was a write or 0 if the faulting access was a
 * read. Returns -1 if the faulting address corresponds to an invalid page
 * or 0 otherwise (having handled the fault appropriately).
 */
int vm_fault(void* addr, bool write_flag) {
  unsigned int fault_position;
  bool found = false;
  for (unsigned int i = 0; i < vp_storage[curr_pid].size(); i++) {
  	//cout << (void*) vp_storage[curr_pid][i].virtual_address << endl;
	if (addr == (void*) vp_storage[curr_pid][i].virtual_address) {
		fault_position = i;
		found = true;
		break;
	}
  }
  if (!found) {
  	return -1;
  }
  virtual_pages& faulty = vp_storage[curr_pid][fault_position];
  if (!faulty.resident) {
	unsigned int page_pos;
    faulty.resident = 1;
    faulty.reference = 1;
    faulty.pte->read_enable = 1;
    if (write_flag) {
        faulty.pte->write_enable = 1;
        faulty.dirty = 1; 
    }
    
    //cout << "faulty's read enable: " << faulty.pte->read_enable << endl;
	
    if (get<1>(second_chance[curr_pid]).size() < number_pages) {
		//faulty.resident = 1;
		//faulty.reference = 1;
		//faulty.pte->read_enable = 1;
		//if (write_flag) {
		//	faulty.pte->write_enable = 1;
		//	faulty.dirty = 1; 
		//}
		page_pos = get<1>(second_chance[curr_pid]).size();
        //get<1>(second_chance[curr_pid])[page_pos].pte->ppage = page_pos;
        get<1>(second_chance[curr_pid]).push_back(faulty);
        get<1>(second_chance[curr_pid])[page_pos].pte->ppage = page_pos;
	}

    // else call second chance algorithm helper function thing

    else {
        page_pos = s_c_algorithm(faulty);
    }
	

	if (!faulty.filled) {
		faulty.filled = 1;
		((char*) pm_physmem)[page_pos] = 0x00000000;	// zero filling
	}
    //cout << "original vp_storage slot filled: " << vp_storage[curr_pid][fault_position].filled << endl;    
  }
  //for (auto frame: get<1>(second_chance[curr_pid])) {
      //cout << "second chance frame's address: " << frame.virtual_address << endl;
  //}
  //cout << page_table_base_register->ptes[0].write_enable << endl;
  //cout << page_table_base_register->ptes[0].read_enable << endl;
  //cout << vp_storage[curr_pid][fault_position].pte.write_enable << endl;
  //cout << vp_storage[curr_pid][0].pte->read_enable<< endl;
  //cout << vp_storage[curr_pid][0].pte->write_enable << endl;
  //cout << (void*) vp_storage[curr_pid][0].virtual_address;
  // TODO
  //return -1;
  return 0;
}

/*
 * vm_destroy
 *
 * Notifies the pager that the current process has exited and should be
 * deallocated.
 */
void vm_destroy() {
  vp_storage[curr_pid].clear();
  vp_storage.erase(curr_pid);
  second_chance.erase(curr_pid);
  processes.erase(curr_pid);
}

/*
 * vm_extend
 *
 * Declare as valid the lowest invalid virtual page in the current process's
 * arena. Returns the lowest-numbered byte of the newly valid virtual page.
 * For example, if the valid part of the arena before calling vm_extend is
 * 0x60000000-0x60003FFF, vm_extend will return 0x60004000 and the resulting
 * valid part of the arena will be 0x60000000-0x60005FFF. The newly-allocated
 * page is allocated a disk block in swap space and should present a zero-filled
 * view to the application. Returns null if the new page cannot be allocated.
 */
void* vm_extend() {
    //have data structure --> [VM_ARENA_BASEADDR, the last valid byte] tuple?
    //newest lowest-numbered byte of the newly valid vp is tuple[1] + 1 --> store and return
    
    int lowest_invalid = get<0>(processes.at(curr_pid));
    page_table_entry_t& low_invalid_virtual_page = page_table_base_register->ptes[lowest_invalid];
    virtual_pages vp;
    vp.pte = &low_invalid_virtual_page;
    vp.valid = 1;
    vp.reference = 0;
    vp.resident = 0;
    vp.dirty = 0;
    vp.filled = 0;

    vp.disk_block = block_recorder[curr_pid];
    if (block_recorder[curr_pid] != disk_block_count) {
        block_recorder[curr_pid]++;
    }
    else{
        block_recorder[curr_pid] = 0;
    }

    unsigned long lowest_invalid_address = (unsigned long) VM_ARENA_BASEADDR + ((int) VM_PAGESIZE * lowest_invalid);
    vp.virtual_address = lowest_invalid_address;
    //if (second_chance.at(curr_pid).size() <= number_pages) {
    //   vp.reference = 1;
    //}
     
    get<0>(processes.at(curr_pid))++;
    vp_storage[curr_pid].push_back(vp);
    //cout << vp_storage[curr_pid].size() << endl;
    //page_table_entry_t* lowest_invalid_address = &get<1>(processes.at(curr_pid)).ptes[lowest_invalid];
    
    return (void*) lowest_invalid_address;
}

/*
 * vm_syslog
 *
 * Log (i.e., print) a message in the arena at the given address with the
 * given nonzero length. Returns 0 on success or -1 if the specified message
 * address or length is invalid.
 */
int vm_syslog(void* message, unsigned len) {
  // TODO
  return -1;
}

