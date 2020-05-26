#ifndef AF_UTILS_H
#define AF_UTILS_H
u_int32_t af_get_timestamp_sec(void);

int check_local_network_ip(unsigned int ip);

void dump_str(char *name, unsigned char *p, int len);

void dump_hex(char *name, unsigned char *p, int len);

int k_sscanf(const char *buf, const char *fmt, ...);

#endif

