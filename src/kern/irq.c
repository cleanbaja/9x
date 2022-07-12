#include <lib/kcon.h>
#include <lib/lock.h>
#include <ninex/irq.h>
#include <stddef.h>

static struct irq_resource irq_table[ARCH_NUM_IRQS] = {0};
static int last_vector = ARCH_LOWEST_IRQ;

static int alloc_irq_vector() {
  // Make sure we don't give out reserved vectors
  if (last_vector > (ARCH_NUM_IRQS + ARCH_LOWEST_IRQ) ||
      last_vector < ARCH_LOWEST_IRQ) {
    klog("sys/irq: Unable to find any free vectors!");
    return -1;
  } else if (irq_table[last_vector].HandlerFunc) {
    // Loop until we find a open handler
    struct irq_resource* cur_irq = &irq_table[++last_vector];
    while (cur_irq->HandlerFunc)
      cur_irq = &irq_table[++last_vector];
  }

  return last_vector++;
}

struct irq_resource* get_irq_handler(int irq_num) {
  if ((irq_num - ARCH_LOWEST_IRQ) > ARCH_NUM_IRQS)
    return NULL;

  return &irq_table[irq_num - ARCH_LOWEST_IRQ];
}

struct irq_resource* alloc_irq_handler(int* result) {
  if (!result) {
    klog("irq: can't allocate a handler without providing a slot pointer!");
    return NULL;
  } else {
    *result = alloc_irq_vector();
    return &irq_table[*result - ARCH_LOWEST_IRQ];
  }
}

void dispatch_level_irq(cpu_ctx_t* c, struct irq_resource* res, int irq) {
  mask_ack_irq(irq);
  res->status |= IRQ_INPROGRESS;

  // Actually dispatch the IRQ...
  res->HandlerFunc(c);

  res->status &= ~IRQ_INPROGRESS;
  if (!(res->status & IRQ_DISABLED)) {
    unmask_irq(irq);
  }
  return;
}

void dispatch_edge_irq(cpu_ctx_t* c, struct irq_resource* res, int irq) {
  ic_eoi(irq);
  res->status |= IRQ_INPROGRESS;

  // Unmask the IRQ, if it was pending from a earlier interrupt
  if (res->status & (IRQ_PENDING | IRQ_MASKED)) {
    unmask_irq(irq);
    res->status &= ~IRQ_PENDING;
  }

  res->HandlerFunc(c);
  res->status &= ~IRQ_INPROGRESS;
  return;
}

void respond_irq(cpu_ctx_t* context, int irq_num) {
  struct irq_resource* cur_irq = get_irq_handler(irq_num);
  if (cur_irq == NULL) {
    klog("spurrious IRQ #%d has no resource???", irq_num);
    return;
  } else {
    spinlock(&cur_irq->lock);
  }

  // Check if we can't respond to the IRQ
  if ((cur_irq->status & IRQ_DISABLED) || (cur_irq->status & IRQ_INPROGRESS) ||
      (!cur_irq->HandlerFunc)) {
    mask_ack_irq(irq_num);
    if (cur_irq->eoi_strategy == EOI_MODE_EDGE) {
      cur_irq->status |=
          IRQ_PENDING;  // Mark it as pending, so that we can deal with it later
    }

    spinrelease(&cur_irq->lock);
    return;
  }

  switch (cur_irq->eoi_strategy) {
    case EOI_MODE_UNKNOWN:
      // For unknown IRQs, MASK'ing, ACK'ing and disabling the IRQ is good
      // enough
      mask_ack_irq(irq_num);
      cur_irq->status |= IRQ_DISABLED;
      break;

    case EOI_MODE_LEVEL:
      dispatch_level_irq(context, cur_irq, irq_num);
      break;

    case EOI_MODE_EDGE:
      dispatch_edge_irq(context, cur_irq, irq_num);
      break;

     case EOI_MODE_TIMER:
      // Timer IRQs are diffrent, since they return their own way...
      ic_eoi(irq_num);
      spinrelease(&cur_irq->lock);
      cur_irq->HandlerFunc(context);
      break;
  }

  spinrelease(&cur_irq->lock);
  return;
}
