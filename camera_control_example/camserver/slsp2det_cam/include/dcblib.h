// dcblib.h

#ifndef DCBLIB_H
#define DCBLIB_H

#include "detsys.h"

#ifndef max
  #define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
  #define min(a,b) (((a) < (b)) ? (a) : (b))
#endif


int dcb_initialize(char *);
int dcb_prog_i2c(int, int, unsigned int, unsigned int);
int dcb_start_read_image(void);
int dcb_expose(void);
int dcb_check_status(char *);
double dcb_wait_read_image(char **);
int dcb_set_readout_bits(int);
int dcb_get_readout_bits(void);

void dcb_set_autoframe(void);
void dcb_reset_autoframe(void);

int dcb_reset_exposure_mode(void);

int dcb_set_external_enb(int);
int dcb_set_external_trigger(int);
int dcb_set_multi_trigger(int);
int dcb_external_enb(void);
int dcb_external_trig(void);
int dcb_external_multi_trig(void);

int dcb_set_exposure_time(double);
int dcb_set_exposure_period(double);
int dcb_set_exp_trigger_delay(double, int);
int dcb_set_exposure_count_limit(unsigned int);
int dcb_set_tx_frame_limit(int);
int dcb_set_debounce_time(double);

int dcb_get_gsdevno(int, int);
int dcb_set_bank_module_address(int, int);
int dcb_set_bank_address(int, int);
int dcb_set_module_address(int, int);
int dcb_set_bank_enable_pattern(int, unsigned int);
int dcb_set_module_enable_pattern(int, unsigned int);

double dcb_get_exposure_time(void);
double dcb_get_exposure_period(void);
double dcb_calc_remaining_time(void);
double dcb_get_remaining_time(void);
double dcb_get_exp_trigger_delay(int);
double dcb_get_measured_exposure_time(void);
double dcb_get_ref_counter_time(void);
double bcb_get_ref_counter_time(void);
double dcb_get_debounce_time(void);

unsigned int dcb_read_register(int, unsigned int);
int dcb_write_register(int, unsigned int, unsigned int);
int dcb_set_register_bits(int, unsigned int, unsigned int);
int dcb_clear_register_bits(int, unsigned int, unsigned int);
int dcb_write_command(int, unsigned int, unsigned int);
int dcb_write_command_no_ack(int, unsigned int);

int dcb_stop(void);
unsigned int dcb_get_exposure_count_limit(void);
int dcb_get_tx_frame_limit(void);
int dcb_get_tx_frame_count(void);
unsigned int dcb_get_exposures_in_frame(void);

int dcb_write_data_modpix(int);
int dcb_write_data_allpix(int);
int dcb_write_data_allpix_slow(int);
int dcb_send_n_calibrates(int);
int dcb_set_delay_setting_all(int);

int dcb_set_roi(int, int, int, int);
int dcb_get_roi(int *, int *, int *, int *);

//----- trivial (low level) functions --------
int dcb_insert_col_token(int);
int dcb_insert_row_token(int);
int dcb_insert_row_col_token(int);
int dcb_advance_col_token(int);
int dcb_advance_row_token(int);
int dcb_advance_row_col_token(int);
int dcb_reset_row_col(int);
int dcb_reset_image_fifo(int);
int dcb_image_fifo_enable(int);
int dcb_image_fifo_disable(int);
int dcb_trigger_psel(int);
int dcb_trigger_enable(int);
int dcb_trigger_dcal(int, int);
int dcb_start_calpix(int);
int dcb_start_calpix_x(int);
int dcb_apply_din_value(int, int);
int dcb_trim_allpix(int);
int dcb_trim_pixels(int);
int dcb_get_enable_state(int);

int dcb_get_bank_address(int);
int dcb_get_module_address(int);
int dcb_get_delay_setting(int);
int dcb_set_delay_setting(int, int);
int dcb_get_dip_switch_setting(int);
int dcb_tx_frame_count_reset(int);
int dcb_trim_fifo_reset(int);
int dcb_trim_fifo_ld(int, int);
int dcb_enable_image_tx(int);
double dcb_overhead_time(void);


// ----- temperature and hummidity -----
double dcb_read_temp(int);
double dcb_read_humidity(int);
int dcb_sensor_heater_control(int, int);
double dcb_read_temp_high_limit(int);
double dcb_read_temp_low_limit(int);
double dcb_read_humidity_high_limit(int);
double dcb_read_humidity_low_limit(int);
int dcb_set_temp_high_limit(int, double);
int dcb_set_temp_low_limit(int, double);
int dcb_set_humidity_high_limit(int, double);
int dcb_set_humidity_low_limit(int, double);


//---------------------------------
void dcb_open_log(void);
void dcb_close_log(void);



//---------------------------------
// For "Compile date:" message

#define YEAR ((+ (__DATE__ [9] - '0')) * 10 + (__DATE__ [10] - '0'))

#define MONTH (__DATE__ [2] == 'n' ? (__DATE__ [1] == 'a' ? 0 : 5) \
: __DATE__ [2] == 'b' ? 1 \
: __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ? 2 : 3) \
: __DATE__ [2] == 'y' ? 4 \
: __DATE__ [2] == 'l' ? 6 \
: __DATE__ [2] == 'g' ? 7 \
: __DATE__ [2] == 'p' ? 8 \
: __DATE__ [2] == 't' ? 9 \
: __DATE__ [2] == 'v' ? 10 : 11) + 1

#define DAY ((__DATE__ [4] == ' ' ? 0 : __DATE__ [4] - '0') * 10 \
+ (__DATE__ [5] - '0'))


#endif
