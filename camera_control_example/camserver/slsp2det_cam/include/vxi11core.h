/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _VXI11CORE_H_RPCGEN
#define _VXI11CORE_H_RPCGEN

#include <rpc/rpc.h>


typedef long Device_Link;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_Link(XDR *, Device_Link*);
#elif __STDC__ 
extern  bool_t xdr_Device_Link(XDR *, Device_Link*);
#else /* Old Style C */ 
bool_t xdr_Device_Link();
#endif /* Old Style C */ 


enum Device_AddrFamily {
	DEVICE_TCP = 0,
	DEVICE_UDP = 1,
};
typedef enum Device_AddrFamily Device_AddrFamily;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_AddrFamily(XDR *, Device_AddrFamily*);
#elif __STDC__ 
extern  bool_t xdr_Device_AddrFamily(XDR *, Device_AddrFamily*);
#else /* Old Style C */ 
bool_t xdr_Device_AddrFamily();
#endif /* Old Style C */ 


typedef long Device_Flags;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_Flags(XDR *, Device_Flags*);
#elif __STDC__ 
extern  bool_t xdr_Device_Flags(XDR *, Device_Flags*);
#else /* Old Style C */ 
bool_t xdr_Device_Flags();
#endif /* Old Style C */ 


typedef long Device_ErrorCode;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_ErrorCode(XDR *, Device_ErrorCode*);
#elif __STDC__ 
extern  bool_t xdr_Device_ErrorCode(XDR *, Device_ErrorCode*);
#else /* Old Style C */ 
bool_t xdr_Device_ErrorCode();
#endif /* Old Style C */ 


struct Device_Error {
	Device_ErrorCode error;
};
typedef struct Device_Error Device_Error;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_Error(XDR *, Device_Error*);
#elif __STDC__ 
extern  bool_t xdr_Device_Error(XDR *, Device_Error*);
#else /* Old Style C */ 
bool_t xdr_Device_Error();
#endif /* Old Style C */ 


struct Create_LinkParms {
	long clientId;
	bool_t lockDevice;
	u_long lock_timeout;
	char *device;
};
typedef struct Create_LinkParms Create_LinkParms;
#ifdef __cplusplus 
extern "C" bool_t xdr_Create_LinkParms(XDR *, Create_LinkParms*);
#elif __STDC__ 
extern  bool_t xdr_Create_LinkParms(XDR *, Create_LinkParms*);
#else /* Old Style C */ 
bool_t xdr_Create_LinkParms();
#endif /* Old Style C */ 


struct Create_LinkResp {
	Device_ErrorCode error;
	Device_Link lid;
	u_short abortPort;
	u_long maxRecvSize;
};
typedef struct Create_LinkResp Create_LinkResp;
#ifdef __cplusplus 
extern "C" bool_t xdr_Create_LinkResp(XDR *, Create_LinkResp*);
#elif __STDC__ 
extern  bool_t xdr_Create_LinkResp(XDR *, Create_LinkResp*);
#else /* Old Style C */ 
bool_t xdr_Create_LinkResp();
#endif /* Old Style C */ 


struct Device_WriteParms {
	Device_Link lid;
	u_long io_timeout;
	u_long lock_timeout;
	Device_Flags flags;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct Device_WriteParms Device_WriteParms;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_WriteParms(XDR *, Device_WriteParms*);
#elif __STDC__ 
extern  bool_t xdr_Device_WriteParms(XDR *, Device_WriteParms*);
#else /* Old Style C */ 
bool_t xdr_Device_WriteParms();
#endif /* Old Style C */ 


struct Device_WriteResp {
	Device_ErrorCode error;
	u_long size;
};
typedef struct Device_WriteResp Device_WriteResp;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_WriteResp(XDR *, Device_WriteResp*);
#elif __STDC__ 
extern  bool_t xdr_Device_WriteResp(XDR *, Device_WriteResp*);
#else /* Old Style C */ 
bool_t xdr_Device_WriteResp();
#endif /* Old Style C */ 


struct Device_ReadParms {
	Device_Link lid;
	u_long requestSize;
	u_long io_timeout;
	u_long lock_timeout;
	Device_Flags flags;
	char termChar;
};
typedef struct Device_ReadParms Device_ReadParms;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_ReadParms(XDR *, Device_ReadParms*);
#elif __STDC__ 
extern  bool_t xdr_Device_ReadParms(XDR *, Device_ReadParms*);
#else /* Old Style C */ 
bool_t xdr_Device_ReadParms();
#endif /* Old Style C */ 


struct Device_ReadResp {
	Device_ErrorCode error;
	long reason;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct Device_ReadResp Device_ReadResp;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_ReadResp(XDR *, Device_ReadResp*);
#elif __STDC__ 
extern  bool_t xdr_Device_ReadResp(XDR *, Device_ReadResp*);
#else /* Old Style C */ 
bool_t xdr_Device_ReadResp();
#endif /* Old Style C */ 


struct Device_ReadStbResp {
	Device_ErrorCode error;
	u_char stb;
};
typedef struct Device_ReadStbResp Device_ReadStbResp;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_ReadStbResp(XDR *, Device_ReadStbResp*);
#elif __STDC__ 
extern  bool_t xdr_Device_ReadStbResp(XDR *, Device_ReadStbResp*);
#else /* Old Style C */ 
bool_t xdr_Device_ReadStbResp();
#endif /* Old Style C */ 


struct Device_GenericParms {
	Device_Link lid;
	Device_Flags flags;
	u_long lock_timeout;
	u_long io_timeout;
};
typedef struct Device_GenericParms Device_GenericParms;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_GenericParms(XDR *, Device_GenericParms*);
#elif __STDC__ 
extern  bool_t xdr_Device_GenericParms(XDR *, Device_GenericParms*);
#else /* Old Style C */ 
bool_t xdr_Device_GenericParms();
#endif /* Old Style C */ 


struct Device_RemoteFunc {
	u_long hostAddr;
	u_long hostPort;
	u_long progNum;
	u_long progVers;
	Device_AddrFamily progFamily;
};
typedef struct Device_RemoteFunc Device_RemoteFunc;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_RemoteFunc(XDR *, Device_RemoteFunc*);
#elif __STDC__ 
extern  bool_t xdr_Device_RemoteFunc(XDR *, Device_RemoteFunc*);
#else /* Old Style C */ 
bool_t xdr_Device_RemoteFunc();
#endif /* Old Style C */ 


struct Device_EnableSrqParms {
	Device_Link lid;
	bool_t enable;
	struct {
		u_int handle_len;
		char *handle_val;
	} handle;
};
typedef struct Device_EnableSrqParms Device_EnableSrqParms;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_EnableSrqParms(XDR *, Device_EnableSrqParms*);
#elif __STDC__ 
extern  bool_t xdr_Device_EnableSrqParms(XDR *, Device_EnableSrqParms*);
#else /* Old Style C */ 
bool_t xdr_Device_EnableSrqParms();
#endif /* Old Style C */ 


struct Device_LockParms {
	Device_Link lid;
	Device_Flags flags;
	u_long lock_timeout;
};
typedef struct Device_LockParms Device_LockParms;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_LockParms(XDR *, Device_LockParms*);
#elif __STDC__ 
extern  bool_t xdr_Device_LockParms(XDR *, Device_LockParms*);
#else /* Old Style C */ 
bool_t xdr_Device_LockParms();
#endif /* Old Style C */ 


struct Device_DocmdParms {
	Device_Link lid;
	Device_Flags flags;
	u_long io_timeout;
	u_long lock_timeout;
	long cmd;
	bool_t network_order;
	long datasize;
	struct {
		u_int data_in_len;
		char *data_in_val;
	} data_in;
};
typedef struct Device_DocmdParms Device_DocmdParms;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_DocmdParms(XDR *, Device_DocmdParms*);
#elif __STDC__ 
extern  bool_t xdr_Device_DocmdParms(XDR *, Device_DocmdParms*);
#else /* Old Style C */ 
bool_t xdr_Device_DocmdParms();
#endif /* Old Style C */ 


struct Device_DocmdResp {
	Device_ErrorCode error;
	struct {
		u_int data_out_len;
		char *data_out_val;
	} data_out;
};
typedef struct Device_DocmdResp Device_DocmdResp;
#ifdef __cplusplus 
extern "C" bool_t xdr_Device_DocmdResp(XDR *, Device_DocmdResp*);
#elif __STDC__ 
extern  bool_t xdr_Device_DocmdResp(XDR *, Device_DocmdResp*);
#else /* Old Style C */ 
bool_t xdr_Device_DocmdResp();
#endif /* Old Style C */ 


#define DEVICE_ASYNC ((u_long)0x0607B0)
#define DEVICE_ASYNC_VERSION ((u_long)1)

#ifdef __cplusplus
#define device_abort ((u_long)1)
extern "C" Device_Error * device_abort_1(Device_Link *, CLIENT *);
extern "C" Device_Error * device_abort_1_svc(Device_Link *, struct svc_req *);

#elif __STDC__
#define device_abort ((u_long)1)
extern  Device_Error * device_abort_1(Device_Link *, CLIENT *);
extern  Device_Error * device_abort_1_svc(Device_Link *, struct svc_req *);

#else /* Old Style C */ 
#define device_abort ((u_long)1)
extern  Device_Error * device_abort_1();
extern  Device_Error * device_abort_1_svc();
#endif /* Old Style C */ 

#define DEVICE_CORE ((u_long)0x0607AF)
#define DEVICE_CORE_VERSION ((u_long)1)

#ifdef __cplusplus
#define create_link ((u_long)10)
extern "C" Create_LinkResp * create_link_1(Create_LinkParms *, CLIENT *);
extern "C" Create_LinkResp * create_link_1_svc(Create_LinkParms *, struct svc_req *);
#define device_write ((u_long)11)
extern "C" Device_WriteResp * device_write_1(Device_WriteParms *, CLIENT *);
extern "C" Device_WriteResp * device_write_1_svc(Device_WriteParms *, struct svc_req *);
#define device_read ((u_long)12)
extern "C" Device_ReadResp * device_read_1(Device_ReadParms *, CLIENT *);
extern "C" Device_ReadResp * device_read_1_svc(Device_ReadParms *, struct svc_req *);
#define device_readstb ((u_long)13)
extern "C" Device_ReadStbResp * device_readstb_1(Device_GenericParms *, CLIENT *);
extern "C" Device_ReadStbResp * device_readstb_1_svc(Device_GenericParms *, struct svc_req *);
#define device_trigger ((u_long)14)
extern "C" Device_Error * device_trigger_1(Device_GenericParms *, CLIENT *);
extern "C" Device_Error * device_trigger_1_svc(Device_GenericParms *, struct svc_req *);
#define device_clear ((u_long)15)
extern "C" Device_Error * device_clear_1(Device_GenericParms *, CLIENT *);
extern "C" Device_Error * device_clear_1_svc(Device_GenericParms *, struct svc_req *);
#define device_remote ((u_long)16)
extern "C" Device_Error * device_remote_1(Device_GenericParms *, CLIENT *);
extern "C" Device_Error * device_remote_1_svc(Device_GenericParms *, struct svc_req *);
#define device_local ((u_long)17)
extern "C" Device_Error * device_local_1(Device_GenericParms *, CLIENT *);
extern "C" Device_Error * device_local_1_svc(Device_GenericParms *, struct svc_req *);
#define device_lock ((u_long)18)
extern "C" Device_Error * device_lock_1(Device_LockParms *, CLIENT *);
extern "C" Device_Error * device_lock_1_svc(Device_LockParms *, struct svc_req *);
#define device_unlock ((u_long)19)
extern "C" Device_Error * device_unlock_1(Device_Link *, CLIENT *);
extern "C" Device_Error * device_unlock_1_svc(Device_Link *, struct svc_req *);
#define device_enable_srq ((u_long)20)
extern "C" Device_Error * device_enable_srq_1(Device_EnableSrqParms *, CLIENT *);
extern "C" Device_Error * device_enable_srq_1_svc(Device_EnableSrqParms *, struct svc_req *);
#define device_docmd ((u_long)22)
extern "C" Device_DocmdResp * device_docmd_1(Device_DocmdParms *, CLIENT *);
extern "C" Device_DocmdResp * device_docmd_1_svc(Device_DocmdParms *, struct svc_req *);
#define destroy_link ((u_long)23)
extern "C" Device_Error * destroy_link_1(Device_Link *, CLIENT *);
extern "C" Device_Error * destroy_link_1_svc(Device_Link *, struct svc_req *);
#define create_intr_chan ((u_long)25)
extern "C" Device_Error * create_intr_chan_1(Device_RemoteFunc *, CLIENT *);
extern "C" Device_Error * create_intr_chan_1_svc(Device_RemoteFunc *, struct svc_req *);
#define destroy_intr_chan ((u_long)26)
extern "C" Device_Error * destroy_intr_chan_1(void *, CLIENT *);
extern "C" Device_Error * destroy_intr_chan_1_svc(void *, struct svc_req *);

#elif __STDC__
#define create_link ((u_long)10)
extern  Create_LinkResp * create_link_1(Create_LinkParms *, CLIENT *);
extern  Create_LinkResp * create_link_1_svc(Create_LinkParms *, struct svc_req *);
#define device_write ((u_long)11)
extern  Device_WriteResp * device_write_1(Device_WriteParms *, CLIENT *);
extern  Device_WriteResp * device_write_1_svc(Device_WriteParms *, struct svc_req *);
#define device_read ((u_long)12)
extern  Device_ReadResp * device_read_1(Device_ReadParms *, CLIENT *);
extern  Device_ReadResp * device_read_1_svc(Device_ReadParms *, struct svc_req *);
#define device_readstb ((u_long)13)
extern  Device_ReadStbResp * device_readstb_1(Device_GenericParms *, CLIENT *);
extern  Device_ReadStbResp * device_readstb_1_svc(Device_GenericParms *, struct svc_req *);
#define device_trigger ((u_long)14)
extern  Device_Error * device_trigger_1(Device_GenericParms *, CLIENT *);
extern  Device_Error * device_trigger_1_svc(Device_GenericParms *, struct svc_req *);
#define device_clear ((u_long)15)
extern  Device_Error * device_clear_1(Device_GenericParms *, CLIENT *);
extern  Device_Error * device_clear_1_svc(Device_GenericParms *, struct svc_req *);
#define device_remote ((u_long)16)
extern  Device_Error * device_remote_1(Device_GenericParms *, CLIENT *);
extern  Device_Error * device_remote_1_svc(Device_GenericParms *, struct svc_req *);
#define device_local ((u_long)17)
extern  Device_Error * device_local_1(Device_GenericParms *, CLIENT *);
extern  Device_Error * device_local_1_svc(Device_GenericParms *, struct svc_req *);
#define device_lock ((u_long)18)
extern  Device_Error * device_lock_1(Device_LockParms *, CLIENT *);
extern  Device_Error * device_lock_1_svc(Device_LockParms *, struct svc_req *);
#define device_unlock ((u_long)19)
extern  Device_Error * device_unlock_1(Device_Link *, CLIENT *);
extern  Device_Error * device_unlock_1_svc(Device_Link *, struct svc_req *);
#define device_enable_srq ((u_long)20)
extern  Device_Error * device_enable_srq_1(Device_EnableSrqParms *, CLIENT *);
extern  Device_Error * device_enable_srq_1_svc(Device_EnableSrqParms *, struct svc_req *);
#define device_docmd ((u_long)22)
extern  Device_DocmdResp * device_docmd_1(Device_DocmdParms *, CLIENT *);
extern  Device_DocmdResp * device_docmd_1_svc(Device_DocmdParms *, struct svc_req *);
#define destroy_link ((u_long)23)
extern  Device_Error * destroy_link_1(Device_Link *, CLIENT *);
extern  Device_Error * destroy_link_1_svc(Device_Link *, struct svc_req *);
#define create_intr_chan ((u_long)25)
extern  Device_Error * create_intr_chan_1(Device_RemoteFunc *, CLIENT *);
extern  Device_Error * create_intr_chan_1_svc(Device_RemoteFunc *, struct svc_req *);
#define destroy_intr_chan ((u_long)26)
extern  Device_Error * destroy_intr_chan_1(void *, CLIENT *);
extern  Device_Error * destroy_intr_chan_1_svc(void *, struct svc_req *);

#else /* Old Style C */ 
#define create_link ((u_long)10)
extern  Create_LinkResp * create_link_1();
extern  Create_LinkResp * create_link_1_svc();
#define device_write ((u_long)11)
extern  Device_WriteResp * device_write_1();
extern  Device_WriteResp * device_write_1_svc();
#define device_read ((u_long)12)
extern  Device_ReadResp * device_read_1();
extern  Device_ReadResp * device_read_1_svc();
#define device_readstb ((u_long)13)
extern  Device_ReadStbResp * device_readstb_1();
extern  Device_ReadStbResp * device_readstb_1_svc();
#define device_trigger ((u_long)14)
extern  Device_Error * device_trigger_1();
extern  Device_Error * device_trigger_1_svc();
#define device_clear ((u_long)15)
extern  Device_Error * device_clear_1();
extern  Device_Error * device_clear_1_svc();
#define device_remote ((u_long)16)
extern  Device_Error * device_remote_1();
extern  Device_Error * device_remote_1_svc();
#define device_local ((u_long)17)
extern  Device_Error * device_local_1();
extern  Device_Error * device_local_1_svc();
#define device_lock ((u_long)18)
extern  Device_Error * device_lock_1();
extern  Device_Error * device_lock_1_svc();
#define device_unlock ((u_long)19)
extern  Device_Error * device_unlock_1();
extern  Device_Error * device_unlock_1_svc();
#define device_enable_srq ((u_long)20)
extern  Device_Error * device_enable_srq_1();
extern  Device_Error * device_enable_srq_1_svc();
#define device_docmd ((u_long)22)
extern  Device_DocmdResp * device_docmd_1();
extern  Device_DocmdResp * device_docmd_1_svc();
#define destroy_link ((u_long)23)
extern  Device_Error * destroy_link_1();
extern  Device_Error * destroy_link_1_svc();
#define create_intr_chan ((u_long)25)
extern  Device_Error * create_intr_chan_1();
extern  Device_Error * create_intr_chan_1_svc();
#define destroy_intr_chan ((u_long)26)
extern  Device_Error * destroy_intr_chan_1();
extern  Device_Error * destroy_intr_chan_1_svc();
#endif /* Old Style C */ 

#endif /* !_VXI11CORE_H_RPCGEN */
