#include "proto_ops.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "macros.h"
#include "types/buffer.h"
#include "config.h"
#include "dump.h"
#include "log.h"
#include "system.h"

static const char *flags_totext(byte value) {
	static char text[8 * 4]; // # of flags * length with separator
	char *ptr = text;
	int has_prev = 0;
	memset(text, 0, sizeof(text));
#define FLAGS_IF(f, txt) \
	if (value & f) { \
		strcpy(ptr, has_prev ? " " # txt : "" # txt); \
		ptr += has_prev++ ? 4 : 3; \
	}
	FLAGS_IF(TH_FIN, fin)
	FLAGS_IF(TH_SYN, syn)
	FLAGS_IF(TH_RST, rst)
	FLAGS_IF(TH_PUSH, psh)
	FLAGS_IF(TH_ACK, ack)
	FLAGS_IF(TH_URG, urg)
#ifdef OS_BSD_BASED
	FLAGS_IF(TH_ECE, ece)
	FLAGS_IF(TH_CWR, cwr)
#endif
#undef FLAGS_IF
	return text;
}

int sniff_tcp_fromwire(const byte *packet, size_t length) {
	const struct tcphdr *header = (struct tcphdr *)packet;
	uint16_t header_len = header->th_off * 4;
	uint16_t sport = ntohs(header->th_sport);
	uint16_t dport = ntohs(header->th_dport);

	LOG_PRINTF(TCP, "-- TCP (%lu bytes)\n", length);
	if (length < header_len) {
		LOG_PRINTF_INDENT(IP, 2, "\tinvalid packet\n");
		return -1;
	}
	LOG_PRINTF_INDENT(TCP, 2, "\tsport: %u\n", sport); // source port
	LOG_PRINTF_INDENT(TCP, 2, "\tdport: %u\n", dport); // destination port
	LOG_PRINTF_INDENT(TCP, 2, "\tseq  : %u\n", ntohl(header->th_seq)); // sequence number
	LOG_PRINTF_INDENT(TCP, 2, "\tack  : %u\n", ntohl(header->th_ack)); // acknowledgement number
	LOG_PRINTF_INDENT(TCP, 2, "\toff  : %u\n", header->th_off); // data offset
	LOG_PRINTF_INDENT(TCP, 2, "\tflags: %u [%s]\n", header->th_flags, flags_totext(header->th_flags)); // flags
	LOG_PRINTF_INDENT(TCP, 2, "\twin  : %u\n", ntohs(header->th_win)); // window
	LOG_PRINTF_INDENT(TCP, 2, "\tsum  : %u\n", ntohs(header->th_sum)); // checksum
	LOG_PRINTF_INDENT(TCP, 2, "\turp  : %u\n", ntohs(header->th_urp)); // urgent pointer

	packet = (byte *)PTR_ADD(packet, header_len);
	length -= header_len;

//	_asm("int $3");
	if (sport == 53 || dport == 53) {
		buffer_t buffer = BUFFER_INITIALIZER;
		size_t dns_len;
		buffer_set_data(&buffer, (byte *)packet, length);
		dns_len = buffer_read_uint16(&buffer);
		dns_len = ntohs(dns_len);
		if (!buffer_has_error(&buffer)) {
			sniff_dns_fromwire(buffer_data_ptr(&buffer), dns_len);
		}
	}

#if LOG_ENABLED(TCP_DATA)
	LOG_PRINTF(TCP_DATA, "showing %lu bytes:\n", length);
	dump_hex(stdout, packet, length, 0);
#endif
	return 0;
}
