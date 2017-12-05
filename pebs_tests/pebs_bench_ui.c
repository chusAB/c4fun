#include "pebs_bench_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//#define PRINT

static int is_served_by_local_memory(union perf_mem_data_src data_src) {
  if (data_src.mem_lvl & PERF_MEM_LVL_MISS) {
    if (data_src.mem_lvl & PERF_MEM_LVL_L3) {
      return 1;
    } else {
      return 0;
    }
  } else if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
    if (data_src.mem_lvl & PERF_MEM_LVL_LOC_RAM) {
      return 1;
    } else if (data_src.mem_lvl & PERF_MEM_LVL_UNC) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

static int is_served_by_remote_memory(union perf_mem_data_src data_src) {
  if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
    if (data_src.mem_lvl & PERF_MEM_LVL_REM_RAM1) {
      return 1;
    } else if (data_src.mem_lvl & PERF_MEM_LVL_REM_RAM2) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

static int is_served_by_remote_cache(union perf_mem_data_src data_src) {
  if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
    if (data_src.mem_lvl & PERF_MEM_LVL_REM_CCE1) {
      return 1;
    } else if (data_src.mem_lvl & PERF_MEM_LVL_REM_CCE2) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

static int is_served_by_local_cache(union perf_mem_data_src data_src) {
  if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
    if (data_src.mem_lvl & PERF_MEM_LVL_L1) {
      return 1;
    } else if (data_src.mem_lvl & PERF_MEM_LVL_LFB) {
      return 1;
    }  else if (data_src.mem_lvl & PERF_MEM_LVL_L2) {
      return 1;
    }  else if (data_src.mem_lvl & PERF_MEM_LVL_L3) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

char *concat(const char *s1, const char *s2) {
  char *result = malloc(strlen(s1) + strlen(s2) + 1);
  if (result == NULL) {
    return "malloc failed in concat\n";
  }
  strcpy(result, s1);
  strcat(result, s2);
  return result;
}

char *get_snoop(union perf_mem_data_src data_src) {
  if (data_src.mem_snoop & PERF_MEM_SNOOP_NA) {
    return "NA";
  }
  if (data_src.mem_snoop & PERF_MEM_SNOOP_NONE) {
    return "none";
  }
  if (data_src.mem_snoop & PERF_MEM_SNOOP_HIT) {
    return "hit";
  }
  if (data_src.mem_snoop & PERF_MEM_SNOOP_MISS) {
    return "miss";
  }
  if (data_src.mem_snoop & PERF_MEM_SNOOP_HITM) {
    return "hit modified";
  }
  return "NULL";
}

char *get_data_src_level(union perf_mem_data_src data_src) {
  char *res = concat("", "");
  char *old_res;
  if (data_src.mem_lvl & PERF_MEM_LVL_NA) {
    old_res = res;
    res = concat(res, "NA");
    free(old_res);
  }
  if (data_src.mem_lvl & PERF_MEM_LVL_L1) {
    old_res = res;
    res = concat(res, "L1");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_LFB) {
    old_res = res;
    res = concat(res, "LFB");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_L2) {
    old_res = res;
    res = concat(res, "L2");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_L3) {
    old_res = res;
    res = concat(res, "L3");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_LOC_RAM) {
    old_res = res;
    res = concat(res, "Local RAM");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_REM_RAM1) {
    old_res = res;
    res = concat(res, "Remote RAM 1 hop");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_REM_RAM2) {
    old_res = res;
    res = concat(res, "Remote RAM 2 hops");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_REM_CCE1) {
    old_res = res;
    res = concat(res, "Remote Cache 1 hop");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_REM_CCE2) {
    old_res = res;
    res = concat(res, "Remote Cache 2 hops");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_IO) {
    old_res = res;
    res = concat(res, "I/O Memory");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_UNC) {
    old_res = res;
    res = concat(res, "Uncached Memory");
    free(old_res);
  }
  if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
    old_res = res;
    res = concat(res, " Hit");
    free(old_res);
  } else if (data_src.mem_lvl & PERF_MEM_LVL_MISS) {
    old_res = res;
    res = concat(res, " Miss");
    free(old_res);
  }
  return res;
}

char *get_tlb_string(union perf_mem_data_src data_src) {
  char *res = concat("", "");
  char *old_res;
  if (data_src.mem_dtlb & PERF_MEM_TLB_NA) {
    old_res = res;
    res = concat(res, "NA");
    free(old_res);
  }
  if (data_src.mem_dtlb & PERF_MEM_TLB_L1) {
    old_res = res;
    res = concat(res, "DTLB");
    free(old_res);
  } else if (data_src.mem_dtlb & PERF_MEM_TLB_L2) {
    old_res = res;
    res = concat(res, "STLB");
    free(old_res);
  }
  if (data_src.mem_dtlb & PERF_MEM_TLB_HIT) {
    old_res = res;
    res = concat(res, " Hit");
    free(old_res);
  } else if (data_src.mem_dtlb & PERF_MEM_TLB_MISS) {
    old_res = res;
    res = concat(res, " Miss");
    free(old_res);
  }
  if (data_src.mem_dtlb & PERF_MEM_TLB_WK) {
    old_res = res;
    res = concat(res, " hardware walker");
    free(old_res);
  } else if (data_src.mem_dtlb & PERF_MEM_TLB_OS) {
    old_res = res;
    res = concat(res, " OS fault handler");
    free(old_res);
  }
  return res;
}

int compar_addr(const void* a1, const void* a2) {
  struct sample *a = (struct sample*) a1;
  struct sample *b = (struct sample*) a2;
  if (a->addr == b->addr) {
    return 0;
  } else if (a->addr < b->addr) {
    return -1;
  } else {
    return 1;
  }
}

int compar_latency(const void* a1, const void* a2) {
  struct sample *a = (struct sample*) a1;
  struct sample *b = (struct sample*) a2;
  if (a->weight == b->weight) {
    return 0;
  } else if (a->weight < b->weight) {
    return 1;
  } else {
    return -1;
  }
}

void print_samples(struct perf_event_mmap_page *metadata_page, display_order order, uint64_t start_addr, uint64_t end_addr, int nb_samples_estimated) {

  int remote_cache_count = 0;
  int local_memory_count = 0;
  int remote_memory_count = 0;
  int cache_count = 0;
  int in_malloced_count = 0;
  int in_malloced_count_4 = 0;
  int in_malloced_count_3 = 0;
  int in_malloced_count_2 = 0;
  int nb_samples = 0;
  uint64_t latency = 0;
  uint64_t latency_local_memory = 0;
  uint64_t latency_remote_memory = 0;
  int served_by_cpt;

  if (metadata_page != NULL) {
    uint64_t head = metadata_page->data_head;
    rmb();
    long page_size = sysconf(_SC_PAGESIZE);
    struct perf_event_header *header = (struct perf_event_header *)((char *)metadata_page + page_size);

    // Creates a list of samples
    uint64_t i = 0;
    while (i < head) {
      if (header -> type == PERF_RECORD_SAMPLE) {
	nb_samples++;
      }
      i = i + header -> size;
      header = (struct perf_event_header *)((char *)header + header -> size);
    }
    struct sample *samples = malloc(nb_samples * sizeof(struct sample));
    int index = 0;
    i = 0;
    header = (struct perf_event_header *)((char *)metadata_page + page_size);
    while (i < head) {
      if (header -> type == PERF_RECORD_SAMPLE) {
	struct sample *sample = (struct sample *)((char *)(header) + 8);
	memcpy(&samples[index], sample, sizeof(struct sample));
	index++;
	if (sample->addr >= start_addr && sample->addr <= start_addr + (end_addr - start_addr) / 4) {
	  in_malloced_count_4++;
	}
	if (sample->addr >= start_addr && sample->addr <= start_addr + (end_addr - start_addr) / 3) {
	  in_malloced_count_3++;
	}
	if (sample->addr >= start_addr && sample->addr <= start_addr + (end_addr - start_addr) / 2) {
	  in_malloced_count_2++;
	}
	if (sample->addr >= start_addr && sample->addr <= end_addr) {
	  in_malloced_count++;
	}

	served_by_cpt = 0;
	if (is_served_by_remote_cache(sample->data_src)) {
	  remote_cache_count++;
	  served_by_cpt++;
	}
	if (is_served_by_local_cache(sample->data_src)) {
	  cache_count++;
	  served_by_cpt++;
	}
	if (is_served_by_local_memory(sample->data_src)) {
	  local_memory_count++;
	  latency_local_memory += sample->weight;
	  served_by_cpt++;
	}
	if (is_served_by_remote_memory(sample->data_src)) {
	  remote_memory_count++;
	  latency_remote_memory += sample->weight;
	  served_by_cpt++;
	}
	latency += sample->weight;
	assert(served_by_cpt == 1);
      }
      i = i + header -> size;
      header = (struct perf_event_header *)((char *)header + header -> size);
    }

    // Sort the list if required
    qsort(samples, nb_samples, sizeof(struct sample), compar_latency);

    printf("%-80s = %15d (expected = %d)\n", "samples count", nb_samples, nb_samples_estimated);

    // Print the list of samples
#ifdef PRINT
    printf("\n");
    for (int i = 0; i < nb_samples; i++) {
      struct sample *sample = &samples[i];
      printf("%-20" PRIx64, sample -> ip);
      printf("%-20" PRIx64, sample -> addr);
      if (sample->addr >= start_addr && sample->addr <= end_addr) {
	printf("%-10s", "in");
      } else {
	printf("%-10s", "out");
      }
      printf("%-10" PRIu64, sample -> weight);
      char *level = get_data_src_level(sample -> data_src);
      printf("%-30s", level);
      free(level);
      printf("%-10s", get_snoop(sample -> data_src));
      printf("%-10s", get_tlb_string(sample -> data_src));
      printf("\n");
    }
#endif
  }

  printf("\n");

  printf("----------------- Where are the samples -----------------\n"); 
  printf("%-8d samples in                  malloced memory on %d samples (%.3f%%)\n", in_malloced_count, nb_samples, (in_malloced_count / (float) nb_samples * 100));
  printf("%-8d samples in first half    of malloced memory on %d samples (%.3f%%)\n", in_malloced_count_2, nb_samples, (in_malloced_count_2 / (float) nb_samples * 100));
  printf("%-8d samples in first third   of malloced memory on %d samples (%.3f%%)\n", in_malloced_count_3, nb_samples, (in_malloced_count_3 / (float) nb_samples * 100));
  printf("%-8d samples in first quarter of malloced memory on %d samples (%.3f%%)\n", in_malloced_count_4, nb_samples, (in_malloced_count_4 / (float) nb_samples * 100));
  printf("\n");
  printf("----------------- In which level of memory -----------------\n"); 
  printf("%-8d remote cache samples  on %d samples (%.3f%%)\n", remote_cache_count, nb_samples, (remote_cache_count / (float) nb_samples * 100));
  printf("%-8d local  cache samples  on %d samples (%.3f%%)\n", cache_count, nb_samples, (cache_count / (float) nb_samples * 100));
  printf("%-8d local memory samples  on %d samples (%.3f%%)\n", local_memory_count, nb_samples, (local_memory_count / (float) nb_samples * 100));
  printf("%-8d remote memory samples on %d samples (%.3f%%)\n", remote_memory_count, nb_samples, (remote_memory_count / (float) nb_samples * 100));
  printf("\n");
  printf("----------------- Average latencies -----------------\n"); 
  printf("Average latency               = %0.2f ns (%0.2f cycles for frequency = 2.668 Giga hertz)\n", (latency / (float) nb_samples), (latency / (float) nb_samples) * (1E9 / 266800000.0));
  printf("Average local memory latency  = %0.2f ns\n", (latency_local_memory / (float) local_memory_count));
  printf("Average remote memory latency = %0.2f ns\n", (latency_remote_memory / (float) remote_memory_count));
}
