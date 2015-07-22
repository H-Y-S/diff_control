/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include <memory.h> /* for memset */
#include "ioc_rpc.h"

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

int *
rpc_vpg_write_a32_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_write_a32, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_read_a32_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_read_a32, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_set_clk_divider_2(vpg *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_set_clk_divider, xdr_vpg, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_set_clk_delay_2(vpg *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_set_clk_delay, xdr_vpg, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_write_output_2(vpg *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_write_output, xdr_vpg, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_read_input_2(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_read_input, xdr_void, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_loadpattern_2(vpg *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_loadpattern, xdr_vpg, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_jump_2(vpg *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_jump, xdr_vpg, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_clk_2(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_clk, xdr_void, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_write_status_2(vpg *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_write_status, xdr_vpg, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_read_status_2(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_read_status, xdr_void, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_set_force_2(vpg *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_set_force, xdr_vpg, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_clk_start_2(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_clk_start, xdr_void, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_clk_stop_2(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_clk_stop, xdr_void, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_pc_reset_2(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_pc_reset, xdr_void, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_clr_mem_2(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_clr_mem, xdr_void, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_vpgini_2(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_vpgini, xdr_void, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_read_d32_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_read_d32, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_write_d32_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_write_d32, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_fifoini_2(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_fifoini, xdr_void, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_read_data_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_read_data, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_read_event_cntr_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_read_event_cntr, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_read_status_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_read_status, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_write_data_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_write_data, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_write_control_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_write_control, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_write_test_2(address *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_write_test, xdr_address, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_selectmodule_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_selectmodule, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_selectchip_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_selectchip, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_selectstrip_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_selectstrip, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_setdac_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_setdac, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_reset_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_reset, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_readmodule_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_readmodule, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_readdet_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_readdet, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_testfpga_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_testFPGA, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_teststshift_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_teststshift, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_testdtshift_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_testdtshift, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_settrimbits_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_settrimbits, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_initdet_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_initdet, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_writest_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_writest, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_readst_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_readst, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_writedt_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_writedt, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_readdt_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_readdt, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_writemodfpga_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_writemodFPGA, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_readmodfpga_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_readmodFPGA, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_writedetfpga_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_writedetFPGA, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_sd_readdetfpga_2(detector *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_sd_readdetFPGA, xdr_detector, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

image *
rpc_px_read_image_2(void *argp, CLIENT *clnt)
{
	static image clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_px_read_image, xdr_void, argp, xdr_image, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_px_set_selector_2(arry *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_px_set_selector, xdr_arry, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_vpg_set_default_2(int *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_vpg_set_default, xdr_int, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_write_image_2(image_spec *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_write_image, xdr_image_spec, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_write_n_images_2(n_image_spec *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_write_n_images, xdr_n_image_spec, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
rpc_fifo_set_level_2(int *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, rpc_fifo_set_level, xdr_int, argp, xdr_int, &clnt_res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}