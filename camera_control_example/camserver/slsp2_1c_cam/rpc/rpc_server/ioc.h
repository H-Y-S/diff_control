#ifndef IOC_H
#define IOC_H

// server side routines (prototypes)

int vpgini(void);

int vme_write_a32(unsigned int, unsigned int);
int vme_read_a32(unsigned int);

int vpgini(void);
int set_clk_divider(unsigned int);
int set_clk_delay(int, int);
int write_output(unsigned int);
int read_input(void);
int loadpattern( char *);
int vme_jump(unsigned int);
int vme_clk(void);
int write_status(unsigned int);
int read_status(void);
int set_force(unsigned int);
int clk_start(void);
int clk_stop(void);
int pc_reset(void);
int clr_mem(void);
int set_default_vpg(int);

int fifoini(void);
unsigned int fifo_read_d32(unsigned int);
unsigned int fifo_read_data(void);
unsigned int fifo_read_event_cntr(void);
unsigned int fifo_read_status(void);
int fifo_write_d32(unsigned int, unsigned int);
int fifo_write_data(unsigned int);
int fifo_write_control(unsigned int);
int fifo_write_test(unsigned int);
int px_read_image(unsigned int *, int *, unsigned int *[]);
int px_set_selector(int, unsigned int []);
int fifo_write_image(char *, int, unsigned int [], unsigned int);
int fifo_write_n_images(char *, int, unsigned int [], unsigned int, unsigned int);
void wait_us(int);
int fifo_set_level(int);


// variables
extern char image_path[];
extern char config_path[];

#endif
