#ifndef _EEPROM_H_
#define _EEPROM_H_


extern int eeprom_init(void);
extern int eeprom_read_config(void *cfg);
extern int eeprom_write_config(void *cfg, int size);


#endif /* _EEPROM_H_ */
