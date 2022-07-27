#ifndef NINEX_IRQ_H
#define NINEX_IRQ_H

#include <arch/irqchip.h>

// Represents a IRQ handler, along with all the information needed to
// dispatch/respond NOTE: 9x's IRQ handling is inspired by the fine
// implmentation in the linux kernel!
#define IRQ_MASKED 0xF0
#define IRQ_PENDING 0xF1
#define IRQ_INPROGRESS 0xF2
#define IRQ_DISABLED 0xF3
struct irq_resource {
  void (*HandlerFunc)(cpu_ctx_t *);
  enum {
    EOI_MODE_UNKNOWN,
    EOI_MODE_LEVEL = 10,
    EOI_MODE_EDGE,
    EOI_MODE_TIMER
  } eoi_strategy;
  volatile int status;
  volatile int lock;

  // Extra information (for debugging and tracking)
  char *procfs_name;
};

struct irq_resource *get_irq_handler(int irq_num);
struct irq_resource *alloc_irq_handler(int *result);
void respond_irq(cpu_ctx_t *context, int irq_num);

// Various IPIs that are used within the kernel.
#define IPI_HALT 254
#define IPI_INVL_TLB 253
#define IPI_SCHED_YIELD 252

// Macros that aid in dealing with IRQs
#define disable_irq(i) get_irq_handler(i)->status |= IRQ_DISABLED;
#define unmask_irq(i)                          \
  ({                                           \
    get_irq_handler(i)->status &= ~IRQ_MASKED; \
    ic_mask_irq(i, false);                     \
  })
#define mask_ack_irq(i)                       \
  ({                                          \
    get_irq_handler(i)->status |= IRQ_MASKED; \
    ic_mask_irq(i, true);                     \
    ic_eoi(i);                                \
  })

#endif  // NINEX_IRQ_H
