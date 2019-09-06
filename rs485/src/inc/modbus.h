#ifndef _MODBUS_H_
#define _MODBUS_H_


#include <stdint.h>

typedef struct {
    uint16_t addr0;
    uint16_t size;
    uint16_t reg0;
} MODBUS_REGS;

#define modbus_REGSIZE(x) (x+sizeof(MODBUS_REGS)/sizeof(uint16_t)-1)

#define modbus_reg_addr0(r)   (((MODBUS_REGS*)r)->addr0)
#define modbus_reg_size(r)   (((MODBUS_REGS*)r)->size)
#define modbus_reg_reg0(r)   (&(((MODBUS_REGS*)r)->reg0))

#define modbus_reg_check(r, a0, s) ((a0>=modbus_reg_addr0(r))&&\
    ((a0+s)<=modbus_reg_addr0(r)+modbus_reg_size(r)))

/* init reg(start=a0, size=s)
 * r:reg, a0:address0, s:size */
#define modbus_reg_init(r, a0, s) do { \
        modbus_reg_addr0(r) = a0; \
        modbus_reg_size(r) = s;  \
    } while(0)

/* write value of reg[offset]
 * r:reg, o:offset, v:value */
#define modbus_reg_write(r, o, v) do { \
        *(modbus_reg_reg0(r)+o) = v;  \
    } while(0)

/* read value of reg[offset]
 * r:reg, o:offset */
#define modbus_reg_read(r, o) (*(modbus_reg_reg0(r)+o))


#endif /* _MODBUS_H_ */
