#include "stm8s.h"
#include "eeprom.h"

static int eeprom_init_config(void);


int eeprom_init(void) {
#ifdef _COSMIC_
/* Call the _fctcpy() function with the first segment character as parameter
   "_fctcpy('F');"  for a manual copy of the declared moveable code segment
   (FLASH_CODE) in RAM before execution*/
  _fctcpy('F');
#endif /*_COSMIC_*/

#ifdef _RAISONANCE_
/* Call the standard C library: memcpy() or fmemcpy() functions available through
   the <string.h> to copy the inram function to the RAM destination address */
  MEMCPY(FLASH_EraseBlock,
         (void PointerAttr*)&__address__FLASH_EraseBlock,
         (int)&__size__FLASH_EraseBlock);
  MEMCPY(FLASH_ProgramBlock,
         (void PointerAttr*)&__address__FLASH_ProgramBlock,
         (int)&__size__FLASH_ProgramBlock);
#endif /*_RAISONANCE_*/

    /* Define flash programming Time*/
    FLASH_SetProgrammingTime(FLASH_PROGRAMTIME_STANDARD);

    /* Unlock flash data eeprom memory */
    FLASH_Unlock(FLASH_MEMTYPE_DATA);
    /* Wait until Data EEPROM area unlocked flag is set*/
    while (FLASH_GetFlagStatus(FLASH_FLAG_DUL) == RESET) {}

    return eeprom_init_config();
}

/*
 * block: [head] [idx] [size] <data...> [chksum]
 */
typedef struct {
    uint8_t head;
    uint8_t idx[4];
    uint8_t size;
    uint8_t data[FLASH_BLOCK_SIZE-1-4-1-1];
    uint8_t chksum;
} EECFG;

#define MAGIC_CODE   0x3B
#define UID32(p)     (*((uint32_t *)(p)))

int eBlockId = -1;
EECFG eBlock;

static int eeprom_init_config(void) {
    uint32_t maxIdx = 0;
    for(int i=0; i<FLASH_DATA_BLOCKS_NUMBER; i++) {
        uint32_t addr = FLASH_DATA_START_PHYSICAL_ADDRESS + i*FLASH_BLOCK_SIZE;
        if(FLASH_ReadByte(addr) == MAGIC_CODE) {
            uint32_t idx = 0;
            idx |= ((uint32_t)FLASH_ReadByte(addr+1)<<24);
            idx |= ((uint32_t)FLASH_ReadByte(addr+2)<<16);
            idx |= ((uint32_t)FLASH_ReadByte(addr+3)<<8);
            idx |= ((uint32_t)FLASH_ReadByte(addr+4)<<0);
            if(idx > maxIdx) {
                maxIdx = idx;
                eBlockId = i;
            }
        }
    }
    maxIdx = 0;
    while(eBlockId >= 0) {
        /* read&chkeck eBlock */
        uint8_t chksum = 0x00;
        uint32_t addr = FLASH_DATA_START_PHYSICAL_ADDRESS + eBlockId*FLASH_BLOCK_SIZE;
        uint8_t *pBlock = (uint8_t *)&eBlock;
        for(int i=0; i<FLASH_BLOCK_SIZE; i++) {
            pBlock[i] = FLASH_ReadByte(addr+i);
            chksum += pBlock[i];
        }
        if(chksum == 0xFF) break;
        eBlockId = (eBlockId-1+FLASH_DATA_BLOCKS_NUMBER)%FLASH_DATA_BLOCKS_NUMBER;
        maxIdx += 1;
        if(maxIdx >= FLASH_DATA_BLOCKS_NUMBER) eBlockId=-1;
    }
    if(eBlockId < 0) {
        /* init eBlock */
        eBlock.head = MAGIC_CODE;
        UID32(eBlock.idx) = 0;
        eBlock.size = 0;
        eBlock.chksum = 0xFF-MAGIC_CODE;
    }
    return 0;
}

int eeprom_write_config(void *cfg, int size) {
    uint8_t *pcfg = (uint8_t *)cfg;
    if((size>0) && (size<=sizeof(eBlock.data))) {
        uint8_t chksum = 0;
        int blk_id = (eBlockId+1)%FLASH_DATA_BLOCKS_NUMBER;
        eBlock.head = MAGIC_CODE;
        UID32(eBlock.idx) = UID32(eBlock.idx) + 1;
        eBlock.size = size;
        for(int i=0; i<size; i++) {
            eBlock.data[i] = pcfg[i];
            chksum += pcfg[i];
        }
        chksum += MAGIC_CODE + eBlock.idx[0]+eBlock.idx[1]+eBlock.idx[2]+eBlock.idx[3] + eBlock.size;
        eBlock.chksum = 0xFF-chksum;
        /* This function is executed from RAM */
        FLASH_ProgramBlock(blk_id, FLASH_MEMTYPE_DATA, FLASH_PROGRAMMODE_STANDARD, (uint8_t *)(&eBlock));
        eBlockId = blk_id;
        return 0;
    }
    return -1;
}

int eeprom_read_config(void *cfg) {
    uint8_t *pcfg = (uint8_t *)cfg;
    if(eBlockId >= 0) {
        for(int i=0; i<eBlock.size; i++) {
            pcfg[i] = eBlock.data[i];
        }
        return eBlock.size;
    }
    return -1;
}
