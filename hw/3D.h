#include "hw.h"
#include "pc.h"
#include "pci.h"

#if defined (DEBUG_ACCEL)
#  define DEBUG_PRINT(x) do { printf x ; } while (0)
#else
#  define DEBUG_PRINT(x)
#endif

#define PCI_VENDOR_ID_SAMSUNG 0x144d
#define PCI_DEVICE_ID_VIRTIO_OPENGL 0x1004

#define FUNC 0x00
#define PID 0x01
#define TRS 0x02
#define IA  0x03
#define IAS 0x04

#define RUN_OPENGL 0x10

typedef struct AccelState {
    PCIDevice dev;
    int ret;
    int Accel_mmio_io_addr;
    uint32_t function_number;
    uint32_t pid;
    uint32_t target_ret_string;
    uint32_t in_args;
    uint32_t in_args_size;
} AccelState;

uint32_t fn;
uint32_t p;
uint32_t trs;
uint32_t ia;
uint32_t ias;
