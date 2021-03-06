#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <numa.h>
#include <numaif.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>
#include <err.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sched.h>

#include "mem_alloc.h"
#include "pebs_bench.h"
#include "pebs_bench_ui.h"

#define CPU 2
#define NUMA_NODE 0
#define NUMA_ALLOC 1 /* Set to one to use numa_alloc */

/* Used to control what we count */
// #define CORE_COUNT_INST
// #define CORE_COUNT_LOADS
/* #define CORE_MEM_UNCORE_RETIRED_LOCAL_DRAM_AND_REMOTE_CACHE_HIT*/
// #define CORE_OFFCORE_COUNT_REMOTE_CACHE
// #define CORE_OFFCORE_COUNT_LOCAL_DRAM
// #define CORE_OFFCORE_COUNT_REMOTE_DRAM
#define CORE_PEBS_SAMPLING
// #define UNCORE_COUNT_READS

#define ELEM_TYPE uint64_t

static long perf_event_open(struct perf_event_attr *hw_event,
			    pid_t pid,
			    int cpu,
			    int group_fd,
			    unsigned long flags) {
  int ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
		    group_fd, flags);
  return ret;
}

#define ONE addr = (uint64_t *)*addr;
#define FOUR ONE ONE ONE ONE
#define SIXTEEN FOUR FOUR FOUR FOUR
#define THIRTY_TWO SIXTEEN SIXTEEN
#define SIXTY_FOUR THIRTY_TWO THIRTY_TWO

/**
 * Read the given memory region. Several accesses are done in each
 * loop iteration to limit the number of accesses caused by the loop
 * test.
 */
void read_memory(uint64_t *memory, size_t size) {

  // register allocation to avoid having memory loads generated by
  // stack accesses. If addr is in the stack, the line addr =
  // (uint64_t *)*addr is compiled (in pseudo anssembler) to r1 =
  // mem[addr]; r2 = mem[r1]; mem[addr] = r2; When doing the register
  // allocation, the code is compiled to: r2 = mem[r1]; r1 = r2; and
  // we see that in that case we only have one memory load.
  register uint64_t *addr = memory;

  // Also register allocation for the same reason
  register int64_t nb_elem_remaining = size / sizeof(ELEM_TYPE);

  while (nb_elem_remaining > 0) {
    THIRTY_TWO
      nb_elem_remaining -= 32;
  }
}

int run_benchs(size_t size_in_bytes,
	       enum access_mode_t access_mode,
	       uint64_t period) {

  /**
   * Allocates and fills memory. Because the memory is filled, all its
   * pages are touched and as a consequence, no page faults will occur
   * during the measurement.
   */
  numa_available();
  uint64_t *memory;
  if (numa_available() != -1 && NUMA_ALLOC) {
    memory = numa_alloc_onnode(size_in_bytes, NUMA_NODE);
  } else {
    memory = malloc(size_in_bytes);
  }
  assert(memory);
  //memory = mmap(NULL, size_in_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
  memset(memory, -1, size_in_bytes);
  fill_memory(memory, size_in_bytes, access_mode);

  // Check where is located the memory
  void *to_chk = memory;
  int status[1];
  int ret_code;
  status[0] = -1;
  ret_code = move_pages(0 /*self memory */, 1, &to_chk, NULL, status, 0);
  if (ret_code == -1) {
    fprintf(stderr, "failed to check where is memeory with move_pages: %s\n", strerror(errno));
    return -1;
  }
  if (NUMA_NODE != status[0]) {
    fprintf(stderr, "memory on the wrong node: expected %d vs current = %d\n", NUMA_NODE, status[0]);
    return -1;
  }

  mlockall(MCL_CURRENT | MCL_FUTURE); // Ensure pages are not swapped

  fprintf(stderr, "Running test on core %d\n", CPU);
  fprintf(stderr, "Running test with memory on node %d (%s)\n", NUMA_NODE, (numa_node_of_cpu(CPU) == NUMA_NODE ? "local" : "remote"));

  /**
   * Profile memory and other things
   */
  struct perf_event_attr pe_attr_page_faults;
  memset(&pe_attr_page_faults, 0, sizeof(pe_attr_page_faults));
  pe_attr_page_faults.size = sizeof(pe_attr_page_faults);
  pe_attr_page_faults.type =   PERF_TYPE_SOFTWARE;
  pe_attr_page_faults.config = PERF_COUNT_SW_PAGE_FAULTS;
  pe_attr_page_faults.disabled = 1;
  pe_attr_page_faults.exclude_kernel = 1;
  int page_faults_fd = perf_event_open(&pe_attr_page_faults, 0, CPU, -1, 0);
  if (page_faults_fd == -1) {
    printf("perf_event_open failed for page faults: %s\n", strerror(errno));
    return -1;
  }

#ifdef UNCORE_COUNT_READS
  struct perf_event_attr pe_attr_unc_memory;
  memset(&pe_attr_unc_memory, 0, sizeof(pe_attr_unc_memory));
  pe_attr_unc_memory.size = sizeof(pe_attr_unc_memory);
  pe_attr_unc_memory.type = 6; // /sys/bus/event_source/devices/uncore/type
  pe_attr_unc_memory.config = 0x072c; // QMC_NORMAL_READS.ANY
  pe_attr_unc_memory.disabled = 1;
  int memory_reads_fd = perf_event_open(&pe_attr_unc_memory, -1, NUMA_NODE, -1, 0);
  if (memory_reads_fd == -1) {
    printf("perf_event_open failed for uncore memory: %s\n", strerror(errno));
    return -1;
  }
#endif

#ifdef CORE_MEM_UNCORE_RETIRED_LOCAL_DRAM_AND_REMOTE_CACHE_HIT
  struct perf_event_attr pe_attr_mem_uncore_loc_rem;
  memset(&pe_attr_mem_uncore_loc_rem, 0, sizeof(pe_attr_mem_uncore_loc_rem));
  pe_attr_mem_uncore_loc_rem.size = sizeof(pe_attr_mem_uncore_loc_rem);
  pe_attr_mem_uncore_loc_rem.type = PERF_TYPE_RAW;
  pe_attr_mem_uncore_loc_rem.config = 0x53080f; // MEM_UNCORE_RETIRED.LOCAL_DRAM_AND_REMOTE_CACHE_HIT
  pe_attr_mem_uncore_loc_rem.disabled = 1;
  pe_attr_mem_uncore_loc_rem.exclude_kernel = 1;
  pe_attr_mem_uncore_loc_rem.exclude_hv = 1;
  int mem_uncore_loc_rem_fd = perf_event_open(&pe_attr_mem_uncore_loc_rem, 0, CPU, -1, 0);
  if (mem_uncore_loc_rem_fd == -1) {
    printf("perf_event_open failed for core MEM_UNCORE_LOC_REM_FD: %s\n", strerror(errno));
    return -1;
  }
#endif

#ifdef CORE_COUNT_LOADS
  struct perf_event_attr pe_attr_loads;
  memset(&pe_attr_loads, 0, sizeof(pe_attr_loads));
  pe_attr_loads.size = sizeof(pe_attr_loads);
  pe_attr_loads.type = PERF_TYPE_RAW;
  pe_attr_loads.config = 0x010b; // MEM_INST_RETIRED.LOADS
  pe_attr_loads.disabled = 1;
  pe_attr_loads.exclude_kernel = 1;
  pe_attr_loads.exclude_hv = 1;
  int loads_fd = perf_event_open(&pe_attr_loads, 0, CPU, -1, 0);
  if (loads_fd == -1) {
    printf("perf_event_open failed for core loads: %s\n", strerror(errno));
    return -1;
  }
#endif

#ifdef CORE_COUNT_INST
  struct perf_event_attr pe_attr_inst;
  memset(&pe_attr_inst, 0, sizeof(pe_attr_inst));
  pe_attr_inst.size = sizeof(pe_attr_inst);
  pe_attr_inst.type = PERF_TYPE_RAW;
  pe_attr_inst.config = 0x00c0; // INST_RETIRED.ANY
  pe_attr_inst.disabled = 1;
  pe_attr_inst.exclude_kernel = 1;
  pe_attr_inst.exclude_hv = 1;
  int inst_fd = perf_event_open(&pe_attr_inst, 0, CPU, -1, 0);
  if (inst_fd == -1) {
    printf("perf_event_open failed for instructions: %s\n", strerror(errno));
    return -1;
  }
#endif

#ifdef CORE_OFFCORE_COUNT_REMOTE_CACHE
  struct perf_event_attr pe_attr_remote_cache;
  memset(&pe_attr_remote_cache, 0, sizeof(pe_attr_remote_cache));
  pe_attr_remote_cache.size = sizeof(pe_attr_remote_cache);
  pe_attr_remote_cache.type = PERF_TYPE_RAW;
  pe_attr_remote_cache.config = 0x5301b7; // OFF_CORE_RESPONSE_0
  pe_attr_remote_cache.config1 = 0x1011; // REMOTE_CACHE_FWD
  pe_attr_remote_cache.disabled = 1;
  pe_attr_remote_cache.exclude_kernel = 1;
  pe_attr_remote_cache.exclude_hv = 1;
  int remote_cache_fd = perf_event_open(&pe_attr_remote_cache, 0, CPU, -1, 0);
  if (remote_cache_fd == -1) {
    printf("perf_event_open failed for remote cache: %s\n", strerror(errno));
    return -1;
  }
#endif

#ifdef CORE_OFFCORE_COUNT_LOCAL_DRAM
  struct perf_event_attr pe_attr_local_ram;
  memset(&pe_attr_local_ram, 0, sizeof(pe_attr_local_ram));
  pe_attr_local_ram.size = sizeof(pe_attr_local_ram);
  pe_attr_local_ram.type = PERF_TYPE_RAW;
  pe_attr_local_ram.config = 0x5301bb; // OFF_CORE_RESPONSE_1
  pe_attr_local_ram.config1 = 0x4033; // LOCAL_DRAM
  pe_attr_local_ram.disabled = 1;
  pe_attr_local_ram.exclude_kernel = 1;
  pe_attr_local_ram.exclude_hv = 1;
  int local_ram_fd = perf_event_open(&pe_attr_local_ram, 0, CPU, -1, 0);
  if (local_ram_fd == -1) {
    printf("perf_event_open failed for local dram: %s\n", strerror(errno));
    return -1;
  }
#endif

#ifdef CORE_OFFCORE_COUNT_REMOTE_DRAM
  struct perf_event_attr pe_attr_remote_ram;
  memset(&pe_attr_remote_ram, 0, sizeof(pe_attr_remote_ram));
  pe_attr_remote_ram.size = sizeof(pe_attr_remote_ram);
  pe_attr_remote_ram.type = PERF_TYPE_RAW;
  pe_attr_remote_ram.config = 0x5301bb; // OFF_CORE_RESPONSE_1
  pe_attr_remote_ram.config1 = 0x2033; // REMOTE_DRAM
  pe_attr_remote_ram.disabled = 1;
  pe_attr_remote_ram.exclude_kernel = 1;
  pe_attr_remote_ram.exclude_hv = 1;
  int remote_ram_fd = perf_event_open(&pe_attr_remote_ram, 0, CPU, -1, 0);
  if (remote_ram_fd == -1) {
    printf("perf_event_open failed for remote dram: %s\n", strerror(errno));
    return -1;
  }
#endif

#ifdef CORE_PEBS_SAMPLING
  struct perf_event_attr pe_attr_sampling;
  memset(&pe_attr_sampling, 0, sizeof(pe_attr_sampling));
  pe_attr_sampling.size = sizeof(pe_attr_sampling);
  pe_attr_sampling.type = PERF_TYPE_RAW;
  pe_attr_sampling.config = 0x100b; // MEM_INST_RETIRED.LATENCY_ABOVE_THRESHOLD

  pe_attr_sampling.config1 = 3; // latency threshold
  pe_attr_sampling.sample_period = period;
  pe_attr_sampling.precise_ip = 2;

  pe_attr_sampling.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_ADDR | PERF_SAMPLE_WEIGHT | PERF_SAMPLE_DATA_SRC;

  pe_attr_sampling.read_format = 0;
  pe_attr_sampling.disabled = 1;
  pe_attr_sampling.pinned = 1;
  pe_attr_sampling.exclude_kernel = 1;
  pe_attr_sampling.exclude_hv = 1;
  pe_attr_sampling.wakeup_events = 1;

  int memory_sampling_fd = perf_event_open(&pe_attr_sampling, 0, -1, -1, 0);
  if (memory_sampling_fd == -1) {
    printf("perf_event_open failed for sampling: %s\n", strerror(errno));
    return -1;
  }
  long page_size = sysconf(_SC_PAGESIZE);
  int nb_pages = 8;
  size_t mmap_len = (1 + nb_pages) * page_size;
  struct perf_event_mmap_page *metadata_page = mmap(NULL, mmap_len, PROT_WRITE, MAP_SHARED, memory_sampling_fd, 0);
  if (metadata_page == MAP_FAILED) {
    fprintf (stderr, "Couldn't mmap file descriptor: %s - errno = %d\n",
  	     strerror (errno), errno);
    return -1;
  }
#endif

  // Starts measuring
  struct timeval t1, t2;
  double elapsedTime;
  gettimeofday(&t1, NULL);
#ifdef CORE_MEM_UNCORE_RETIRED_LOCAL_DRAM_AND_REMOTE_CACHE_HIT
  ioctl(mem_uncore_loc_rem_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(mem_uncore_loc_rem_fd, PERF_EVENT_IOC_ENABLE, 0);
#endif
#ifdef UNCORE_COUNT_READS
  ioctl(memory_reads_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(memory_reads_fd, PERF_EVENT_IOC_ENABLE, 0);
#endif
#ifdef CORE_COUNT_LOADS
  ioctl(loads_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(loads_fd, PERF_EVENT_IOC_ENABLE, 0);
#endif
#ifdef CORE_COUNT_INST
  ioctl(inst_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(inst_fd, PERF_EVENT_IOC_ENABLE, 0);
#endif
  ioctl(page_faults_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(page_faults_fd, PERF_EVENT_IOC_ENABLE, 0);
#ifdef CORE_PEBS_SAMPLING
  ioctl(memory_sampling_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(memory_sampling_fd, PERF_EVENT_IOC_ENABLE, 0);
#endif
#ifdef CORE_OFFCORE_COUNT_REMOTE_CACHE
  ioctl(remote_cache_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(remote_cache_fd, PERF_EVENT_IOC_ENABLE, 0);
#endif
#ifdef CORE_OFFCORE_COUNT_LOCAL_DRAM
  ioctl(local_ram_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(local_ram_fd, PERF_EVENT_IOC_ENABLE, 0);
#endif
#ifdef CORE_OFFCORE_COUNT_REMOTE_DRAM
  ioctl(remote_ram_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(remote_ram_fd, PERF_EVENT_IOC_ENABLE, 0);
#endif

  // Access memory
  read_memory(memory, size_in_bytes);

  // Stop measuring
#ifdef CORE_OFFCORE_COUNT_REMOTE_DRAM
  ioctl(remote_ram_fd, PERF_EVENT_IOC_DISABLE, 0);
#endif
#ifdef CORE_MEM_UNCORE_RETIRED_LOCAL_DRAM_AND_REMOTE_CACHE_HIT
  ioctl(mem_uncore_loc_rem_fd, PERF_EVENT_IOC_DISABLE, 0);
#endif
#ifdef UNCORE_COUNT_READS
  ioctl(memory_reads_fd, PERF_EVENT_IOC_DISABLE, 0);
#endif
#ifdef CORE_COUNT_LOADS
  ioctl(loads_fd, PERF_EVENT_IOC_DISABLE, 0);
#endif
#ifdef CORE_COUNT_INST
  ioctl(inst_fd, PERF_EVENT_IOC_DISABLE, 0);
#endif
  ioctl(page_faults_fd, PERF_EVENT_IOC_DISABLE, 0);
#ifdef CORE_PEBS_SAMPLING
  ioctl(memory_sampling_fd, PERF_EVENT_IOC_DISABLE, 0);
#endif
#ifdef CORE_OFFCORE_COUNT_REMOTE_CACHE
  ioctl(remote_cache_fd, PERF_EVENT_IOC_DISABLE, 0);
#endif
#ifdef CORE_OFFCORE_COUNT_LOCAL_DRAM
  ioctl(local_ram_fd, PERF_EVENT_IOC_DISABLE, 0);
#endif
  gettimeofday(&t2, NULL);
  elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;
  elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;

  // Print results
#ifdef CORE_MEM_UNCORE_RETIRED_LOCAL_DRAM_AND_REMOTE_CACHE_HIT
  uint64_t mem_uncore_loc_rem_count;
  read(mem_uncore_loc_rem_fd, &mem_uncore_loc_rem_count, sizeof(mem_uncore_loc_rem_count));
#endif
#ifdef CORE_OFFCORE_COUNT_REMOTE_DRAM
  uint64_t remote_ram_count;
  read(remote_ram_fd, &remote_ram_count, sizeof(remote_ram_count));
#endif
#ifdef CORE_OFFCORE_COUNT_LOCAL_DRAM
  uint64_t local_ram_count;
  read(local_ram_fd, &local_ram_count, sizeof(local_ram_count));
#endif
#ifdef CORE_OFFCORE_COUNT_REMOTE_CACHE
  uint64_t remote_cache_count;
  read(remote_cache_fd, &remote_cache_count, sizeof(remote_cache_count));
#endif
#ifdef UNCORE_COUNT_READS
  uint64_t memory_reads_count;
  read(memory_reads_fd, &memory_reads_count, sizeof(memory_reads_count));
#endif
#ifdef CORE_COUNT_LOADS
  uint64_t loads_count;
  read(loads_fd, &loads_count, sizeof(loads_count));
#endif
#ifdef CORE_COUNT_INST
  uint64_t insts_count;
  read(inst_fd, &insts_count, sizeof(insts_count));
#endif
  uint64_t page_faults_count;
  read(page_faults_fd, &page_faults_count, sizeof(page_faults_count));
  printf("\n");
  printf("%-80s = %15.3f \n",       "time (milliseconds)", elapsedTime);
  printf("%-80s = %15" PRIu64 "\n", "Page faults count (software event)", page_faults_count);
#ifdef CORE_COUNT_INST
  printf("%-80s = %15" PRId64 "\n", "instructions count (core event: INST_RETIRED.ANY)", insts_count);
#endif
#ifdef CORE_COUNT_LOADS
  int nb_elems = size_in_bytes / sizeof(ELEM_TYPE);
  printf("%-80s = %15" PRId64 " (expected = %d)\n", "loads count (core event: MEM_INST_RETIRED.LOADS)", loads_count, nb_elems);
#endif
#ifdef UNCORE_COUNT_READS
  if (access_mode == access_seq) {
    printf("%-80s = %15" PRId64 " (expected = %" PRId64 " considering each cache line is loaded once only)\n", "64 bytes cache line reads from RAM count (uncore event: QMC_NORMAL_READS.ANY)", memory_reads_count, (size_in_bytes / 64));
  } else {
    printf("%-80s = %15" PRId64 "\n", "64 bytes cache line reads from RAM count (uncore event: QMC_NORMAL_READS.ANY)", memory_reads_count);
  }
#endif
#ifdef CORE_MEM_UNCORE_RETIRED_LOCAL_DRAM_AND_REMOTE_CACHE_HIT
  printf("%-80s = %15" PRId64 "\n", "local ram & remote cache count (core event: MEM_UNCORE_RETIRED:L_DRAM_REM_CACHE)", mem_uncore_loc_rem_count);
#endif
#ifdef CORE_OFFCORE_COUNT_REMOTE_CACHE
  printf("%-80s = %15" PRId64 "\n", "remote cache count (core event: OFF_CORE_RESPONSE_0:REMOTE_CACHE_FWD)", remote_cache_count);
#endif
#ifdef CORE_OFFCORE_COUNT_LOCAL_DRAM
  printf("%-80s = %15" PRId64 "\n", "local memory count (core event: OFF_CORE_RESPONSE_1:LOCAL_DRAM)", local_ram_count);
#endif
#ifdef CORE_OFFCORE_COUNT_REMOTE_DRAM
  printf("%-80s = %15" PRId64 "\n", "remote memory count (core event: OFF_CORE_RESPONSE_1:REMOTE_DRAM)", remote_ram_count);
#endif

#ifdef CORE_PEBS_SAMPLING
  if (metadata_page->data_head > mmap_len) {
    printf("more samples than space in mmap allocated ring buffer\n");
    return -1;
  }
  int nb_elems2 = size_in_bytes / sizeof(ELEM_TYPE);
  print_samples(metadata_page, ADDR, (uint64_t)memory, (uint64_t)memory + size_in_bytes, nb_elems2 / period);
#endif

  if (numa_available() == -1 && NUMA_ALLOC) {
    free(memory);
  } else {
    numa_free(memory, size_in_bytes);
  }
  return 0;
}

void usage(const char *prog_name) {
  printf ("Usage %s size access_mode period\n\trun benchmarks where:\n\t\tsize is the size of the allocated and accessed memory in mega bytes\n\t\taccess_mode is either seq for sequential accesses or rand for random accesses\n\t\tperiod the sampling period in number of events\n", prog_name);
}

int main(int argc, char **argv) {

  /**
   * Pin process on core CPU
   */
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(CPU, &mask);
  if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
    printf("sched_setaffinity failed: %s\n", strerror(errno));
    return -1;
  }

  /**
   * Check and get arguments.
   */
  if (argc <= 3) {
    usage(argv[0]);
    return -1;
  }
  size_t size_in_bytes = atol(argv[1]) * 1000000;
  enum access_mode_t access_mode;
  if (!strcmp(argv[2], "seq")) {
    access_mode = access_seq;
  } else if (!strcmp(argv[2], "rand")) {
    access_mode = access_rand;
  } else {
    printf("Unknown access_mode %s\n", argv[2]);
    usage(argv[0]);
    return -1;
  }
  uint64_t period = atol(argv[3]);
  return run_benchs(size_in_bytes, access_mode, period);
}
