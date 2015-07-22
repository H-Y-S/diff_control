// gslib.h - interface functions for the PSI PMC-GigaSTaR card

#ifndef GSLIB_H
#define GSLIB_H

// initializing
int gs_initialize(char *);

// reset & start
int gs_start(int);
int gs_reset(int);
int gs_reset_all(int);

// reading status
int gs_status(int);
int gs_irq_status(int);
int gs_irq_src_status(int);
int gs_interrupt_status(int);
unsigned int *gs_read_bar0_reg(int, int, int);
double gs_last_interrupt_time(int);

// (re)setting control parameters
int gs_device_count(void);
int gs_set_control(int , unsigned int);
int gs_reset_control(int, unsigned int);
int gs_set_irq_mask(int, unsigned int);
int gs_clear_irq(int);
int gs_set_rx_burstsize(int, unsigned int );
int gs_set_rx_timeout(int, unsigned int);
int gs_set_tx_burstsize(int, unsigned int);
int gs_write_bar0_reg(int, int, unsigned int);

// programmed I/O data transfer
size_t gs_rx_data(int, void *, size_t);
size_t gs_tx_data(int, void *, size_t);

// dma setup and execution
void *gs_rx_dma_setup(int, unsigned int);
void *gs_tx_dma_setup(int, unsigned int);
size_t gs_rx_dma(int);
size_t gs_tx_dma(int);

// timer functions
int gs_set_t0(double);
double gs_time(void);
char *gs_timestamp(void);


#endif
