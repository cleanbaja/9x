#include <fs/devtmpfs.h>
#include <lib/kcon.h>
#include <vm/vm.h>
#ifdef __x86_64__
#include <arch/cpuid.h> 
#endif

/* Mersenne Twister stolen from limine
 * https://github.com/limine-bootloader/limine
 */

#define n ((int)624)
#define m ((int)397)
#define matrix_a ((uint32_t)0x9908b0df)
#define msb ((uint32_t)0x80000000)
#define lsbs ((uint32_t)0x7fffffff)

static uint32_t *status = NULL;
static int ctr = 0;

void srand(uint32_t s) {
    status[0] = s;
    for (ctr = 1; ctr < n; ctr++)
        status[ctr] = (1852734613 * (status[ctr - 1] ^ (status[ctr - 1] >> 30)) + ctr);
}

uint32_t rand32(void) {
    const uint32_t mag01[2] = {0, matrix_a};

    if (ctr >= n) {
        for (int kk = 0; kk < n - m; kk++) {
            uint32_t y = (status[kk] & msb) | (status[kk + 1] & lsbs);
            status[kk] = status[kk + m] ^ (y >> 1) ^ mag01[y & 1];
        }

        for (int kk = n - m; kk < n - 1; kk++) {
            uint32_t y = (status[kk] & msb) | (status[kk + 1] & lsbs);
            status[kk] = status[kk + (m - n)] ^ (y >> 1) ^ mag01[y & 1];
        }

        uint32_t y = (status[n - 1] & msb) | (status[0] & lsbs);
        status[n - 1] = status[m - 1] ^ (y >> 1) ^ mag01[y & 1];

        ctr = 0;
    }

    uint32_t res = status[ctr++];
    res ^= (res >> 11);
    res ^= (res << 7) & 0x9d2c5680;
    res ^= (res << 15) & 0xefc60000;
    res ^= (res >> 18);

    return res;
}

ssize_t
random_read(struct backing* bck, void* buf, off_t offset, size_t count)
{
  // TODO: Find more sources of entropy, since /dev/random is supposed to be more secure
  srand((uintptr_t)buf);
  if (offset != 0)
    srand(offset);
  
  if (count % 4) {
    uint32_t* rand_buf = (uint32_t*)buf;
    for (int i = 0; i < count; i++) {
      rand_buf[i] = rand32();
    }
  } else {
    uint8_t* rand_buf = (uint8_t*)buf;
    for (int i = 0; i < count; i++) {
      rand_buf[i] = (uint8_t)rand32();
    }
  }
 
  return count;
}

static ssize_t
urandom_read(struct backing* bck, void* buf, off_t offset, size_t count)
{
  if (count % 4) {
    uint32_t* rand_buf = (uint32_t*)buf;
    for (int i = 0; i < count; i++) {
      rand_buf[i] = rand32();
    }
  } else {
    uint8_t* rand_buf = (uint8_t*)buf;
    for (int i = 0; i < count; i++) {
      rand_buf[i] = (uint8_t)rand32();
    }
  }

  return 0;
}

static ssize_t
null_write(struct backing* bck, const void* buf, off_t offset, size_t count)
{
  (void)offset;
  (void)bck;
  (void)buf;
  (void)count;
  
  return count;
}

static ssize_t 
null_resize(struct backing* bck, off_t new_size)
{
  // The null device can't be resized
  (void)bck;
  (void)new_size;
  return 0;
}

static void null_close(struct backing* bck) {
  spinlock_acquire(&bck->lock);
  bck->refcount--;
  spinlock_release(&bck->lock);
}

void setup_random_streams()
{
  uint32_t seed = ((uint32_t)0xadf8ca9f * (uint32_t)asm_rdtsc())
                * ((uint32_t)0x923c6e2d)
                ^ ((uint32_t)0x712befa9 * (uint32_t)asm_rdtsc());
#ifdef __x86_64__
    // Attempt to use the RD{SEED,RAND} instructions for randomness
  uint32_t a, b, c, d;

  cpuid_subleaf(0x07, 0, &a, &b, &c, &d);
  if (b & CPUID_EBX_RDSEED) {
    uint32_t new_seed;
    asm volatile ("rdseed %0" : "=r"(new_seed));
    seed *= (seed ^ new_seed);

    klog("urandom: using RDSEED for additional entropy");
  }

  cpuid_subleaf(0x01, 0, &a, &b, &c, &d);
  if (c & CPUID_ECX_RDRAND) {
    uint32_t new_seed;
    asm volatile ("rdrand %0" : "=r"(new_seed));
    seed *= (seed ^ new_seed);

    klog("urandom: using RDRAND for additional entropy");
  }
#endif // __x86_64__
  
  // Init the RNG (Random Number Generator) and create the VFS nodes
  status = kmalloc(n * sizeof(uint32_t));
  srand(seed);
  struct backing* urandom_bck = devtmpfs_create_device("urandom");
  struct backing* random_bck = devtmpfs_create_device("random");

  // Setup '/dev/random'
  random_bck->st.st_dev     = devtmpfs_create_id(0);
  random_bck->st.st_mode    = 0666 | S_IFCHR;
  random_bck->st.st_nlink   = 1;
  random_bck->refcount      = 1;
  random_bck->read   = random_read;
  random_bck->write  = null_write;
  random_bck->resize = null_resize;
  random_bck->close  = null_close;

  // Setup '/dev/urandom'
  urandom_bck->st.st_dev     = devtmpfs_create_id(0);
  urandom_bck->st.st_mode    = 0666 | S_IFCHR;
  urandom_bck->st.st_nlink   = 1;
  urandom_bck->refcount      = 1;
  urandom_bck->read   = urandom_read;
  urandom_bck->write  = null_write;
  urandom_bck->resize = null_resize;
  urandom_bck->close  = null_close;
}

