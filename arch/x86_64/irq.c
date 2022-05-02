#include <arch/asm.h>
#include <lib/kcon.h>
#include <arch/ic.h>
#include <arch/irq.h>
#include <stddef.h>

static struct irq_handler handlers[256] = { 0 };
static int last_vector = 32; 
 
int alloc_irq_vec() {
  // Make sure we don't give out reserved vectors, 
  // which are from 250-255 and 0-31
  if (last_vector > 249 || last_vector < 32) {
    klog("sys/irq: (WARN) Unable to find any free vectors!");
    return -1;
  } else {
    return last_vector++;
  }
}

struct irq_handler* request_irq(char* desc, int* slot_ptr) {
  int slot = alloc_irq_vec();
  struct irq_handler* res = get_handler(slot);
  if (res == NULL)
    return NULL;
  res->name = desc;

  if (slot_ptr)
    *slot_ptr = slot;
  return res;
}

struct irq_handler* get_handler(int slot) {
  if (slot < 0)
    return NULL;
  else
    return &handlers[slot];
}

cpu_ctx_t* sys_dispatch_isr(cpu_ctx_t* context)
{
  uint32_t vec = context->int_no;

  // Check to see if a handler is missing, then take appropriate action
  if (!handlers[vec].hnd) {
    if (vec < 32) {
      PANIC(context, NULL); // PANIC for unhandled CPU exceptions
    } else {
      klog("x86/irq: Unhandled IRQ #%d, which maps to IO-APIC GSI #%d", vec, handlers[vec].gsi);
      ic_mask_irq(handlers[vec].gsi, true); // Otherwise, mask the IRQ
    }

    return context;
  }

  // Call the associated handler
  handlers[vec].hnd(context, handlers[context->int_no].extra_arg);

  // Finally, finish handling of IRQ, based on type
  if (handlers[vec].is_irq)
    ic_eoi();
  if (!handlers[vec].should_return) {
    asm ("cli");
    for (;;) { asm volatile ("hlt"); }
  }

  return context;
}

