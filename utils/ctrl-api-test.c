#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <net/if.h>
#include <net/netmap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct TestContext {
	const char *ifname;
	const char *bdgname;
	uint32_t nr_tx_slots;   /* slots in tx rings */
	uint32_t nr_rx_slots;   /* slots in rx rings */
	uint16_t nr_tx_rings;   /* number of tx rings */
	uint16_t nr_rx_rings;   /* number of rx rings */
	uint16_t nr_mem_id;     /* id of the memory allocator */
	uint16_t nr_ringid;     /* ring(s) we care about */
	uint32_t nr_mode;       /* specify NR_REG_* modes */
	uint64_t nr_flags;      /* additional flags (see below) */
	uint32_t nr_extra_bufs; /* number of requested extra buffers */

	uint32_t nr_hdr_len; /* for PORT_HDR_SET and PORT_HDR_GET */

	uint32_t nr_first_cpu_id;     /* vale polling */
	uint32_t nr_num_polling_cpus; /* vale polling */
};

#if 0
static void
ctx_reset(struct TestContext *ctx)
{
	const char *tmp1 = ctx->ifname;
	const char *tmp2 = ctx->bdgname;
	memset(ctx, 0, sizeof(*ctx));
	ctx->ifname = tmp1;
	ctx->bdgname = tmp2;
}
#endif

typedef int (*testfunc_t)(int fd, struct TestContext *ctx);

static void
nmreq_hdr_init(struct nmreq_header *hdr, const char *ifname)
{
	memset(hdr, 0, sizeof(*hdr));
	hdr->nr_version = NETMAP_API;
	strncpy(hdr->nr_name, ifname, sizeof(hdr->nr_name));
}

/* Single NETMAP_REQ_PORT_INFO_GET. */
static int
port_info_get(int fd, struct TestContext *ctx)
{
	struct nmreq_port_info_get req;
	struct nmreq_header hdr;
	int ret;

	printf("Testing NETMAP_REQ_PORT_INFO_GET on '%s'\n", ctx->ifname);

	nmreq_hdr_init(&hdr, ctx->ifname);
	hdr.nr_reqtype = NETMAP_REQ_PORT_INFO_GET;
	hdr.nr_body    = &req;
	memset(&req, 0, sizeof(req));
	ret = ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, PORT_INFO_GET)");
		return ret;
	}
	printf("nr_offset 0x%lx\n", req.nr_offset);
	printf("nr_memsize %lu\n", req.nr_memsize);
	printf("nr_tx_slots %u\n", req.nr_tx_slots);
	printf("nr_rx_slots %u\n", req.nr_rx_slots);
	printf("nr_tx_rings %u\n", req.nr_tx_rings);
	printf("nr_rx_rings %u\n", req.nr_rx_rings);
	printf("nr_mem_id %u\n", req.nr_mem_id);

	return req.nr_memsize && req.nr_tx_slots && req.nr_rx_slots &&
			       req.nr_tx_rings && req.nr_rx_rings &&
			       req.nr_tx_rings
		       ? 0
		       : -1;
}

/* Single NETMAP_REQ_REGISTER, no use. */
static int
port_register(int fd, struct TestContext *ctx)
{
	struct nmreq_register req;
	struct nmreq_header hdr;
	int ret;

	printf("Testing NETMAP_REQ_REGISTER(mode=%d,ringid=%d,"
	       "flags=0x%lx) on '%s'\n",
	       ctx->nr_mode, ctx->nr_ringid, ctx->nr_flags, ctx->ifname);

	nmreq_hdr_init(&hdr, ctx->ifname);
	hdr.nr_reqtype = NETMAP_REQ_REGISTER;
	hdr.nr_body    = &req;
	memset(&req, 0, sizeof(req));
	req.nr_mem_id     = ctx->nr_mem_id;
	req.nr_mode       = ctx->nr_mode;
	req.nr_ringid     = ctx->nr_ringid;
	req.nr_flags      = ctx->nr_flags;
	req.nr_tx_slots   = ctx->nr_tx_slots;
	req.nr_rx_slots   = ctx->nr_rx_slots;
	req.nr_tx_rings   = ctx->nr_tx_rings;
	req.nr_rx_rings   = ctx->nr_rx_rings;
	req.nr_extra_bufs = ctx->nr_extra_bufs;
	ret		  = ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, REGISTER)");
		return ret;
	}
	printf("nr_offset 0x%lx\n", req.nr_offset);
	printf("nr_memsize %lu\n", req.nr_memsize);
	printf("nr_tx_slots %u\n", req.nr_tx_slots);
	printf("nr_rx_slots %u\n", req.nr_rx_slots);
	printf("nr_tx_rings %u\n", req.nr_tx_rings);
	printf("nr_rx_rings %u\n", req.nr_rx_rings);
	printf("nr_mem_id %u\n", req.nr_mem_id);
	printf("nr_extra_bufs %u\n", req.nr_extra_bufs);

	return req.nr_memsize && (ctx->nr_mode == req.nr_mode) &&
			       (ctx->nr_ringid == req.nr_ringid) &&
			       (ctx->nr_flags == req.nr_flags) &&
			       ((!ctx->nr_tx_slots && req.nr_tx_slots) ||
				(ctx->nr_tx_slots == req.nr_tx_slots)) &&
			       ((!ctx->nr_rx_slots && req.nr_rx_slots) ||
				(ctx->nr_rx_slots == req.nr_rx_slots)) &&
			       ((!ctx->nr_tx_rings && req.nr_tx_rings) ||
				(ctx->nr_tx_rings == req.nr_tx_rings)) &&
			       ((!ctx->nr_rx_rings && req.nr_rx_rings) ||
				(ctx->nr_rx_rings == req.nr_rx_rings)) &&
			       ((!ctx->nr_mem_id && req.nr_mem_id) ||
				(ctx->nr_mem_id == req.nr_mem_id)) &&
			       (ctx->nr_extra_bufs == req.nr_extra_bufs)
		       ? 0
		       : -1;
}

static int
port_register_hwall_host(int fd, struct TestContext *ctx)
{
	ctx->nr_mode = NR_REG_NIC_SW;
	return port_register(fd, ctx);
}

static int
port_register_host(int fd, struct TestContext *ctx)
{
	ctx->nr_mode = NR_REG_SW;
	return port_register(fd, ctx);
}

static int
port_register_hwall(int fd, struct TestContext *ctx)
{
	ctx->nr_mode = NR_REG_ALL_NIC;
	return port_register(fd, ctx);
}

static int
port_register_single_ring_couple(int fd, struct TestContext *ctx)
{
	ctx->nr_mode   = NR_REG_ONE_NIC;
	ctx->nr_ringid = 0;
	return port_register(fd, ctx);
}

/* NETMAP_REQ_VALE_ATTACH */
static int
vale_attach(int fd, struct TestContext *ctx)
{
	struct nmreq_vale_attach req;
	struct nmreq_header hdr;
	char vpname[256];
	int ret;

	snprintf(vpname, sizeof(vpname), "%s:%s", ctx->bdgname, ctx->ifname);

	printf("Testing NETMAP_REQ_VALE_ATTACH on '%s'\n", vpname);
	nmreq_hdr_init(&hdr, vpname);
	hdr.nr_reqtype = NETMAP_REQ_VALE_ATTACH;
	hdr.nr_body    = &req;
	memset(&req, 0, sizeof(req));
	req.reg.nr_mem_id = ctx->nr_mem_id;
	if (ctx->nr_mode == 0) {
		ctx->nr_mode = NR_REG_ALL_NIC; /* default */
	}
	req.reg.nr_mode = ctx->nr_mode;
	ret		= ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, VALE_ATTACH)");
		return ret;
	}
	printf("nr_mem_id %u\n", req.reg.nr_mem_id);

	return ((!ctx->nr_mem_id && req.reg.nr_mem_id > 1) ||
		(ctx->nr_mem_id == req.reg.nr_mem_id)) &&
			       (ctx->nr_flags == req.reg.nr_flags)
		       ? 0
		       : -1;
}

/* NETMAP_REQ_VALE_DETACH */
static int
vale_detach(int fd, struct TestContext *ctx)
{
	struct nmreq_header hdr;
	char vpname[256];
	int ret;

	snprintf(vpname, sizeof(vpname), "%s:%s", ctx->bdgname, ctx->ifname);

	printf("Testing NETMAP_REQ_VALE_DETACH on '%s'\n", vpname);
	nmreq_hdr_init(&hdr, vpname);
	hdr.nr_reqtype = NETMAP_REQ_VALE_DETACH;
	ret	    = ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, VALE_DETACH)");
		return ret;
	}

	return 0;
}

/* First NETMAP_REQ_VALE_ATTACH, then NETMAP_REQ_VALE_DETACH. */
static int
vale_attach_detach(int fd, struct TestContext *ctx)
{
	int ret;

	if ((ret = vale_attach(fd, ctx))) {
		return ret;
	}

	return vale_detach(fd, ctx);
}

static int
vale_attach_detach_host_rings(int fd, struct TestContext *ctx)
{
	ctx->nr_mode = NR_REG_NIC_SW;
	return vale_attach_detach(fd, ctx);
}

/* First NETMAP_REQ_PORT_HDR_SET and the NETMAP_REQ_PORT_HDR_GET
 * to check that we get the same value. */
static int
port_hdr_set_and_get(int fd, struct TestContext *ctx)
{
	struct nmreq_port_hdr req;
	struct nmreq_header hdr;
	int ret;

	printf("Testing NETMAP_REQ_PORT_HDR_SET on '%s'\n", ctx->ifname);

	nmreq_hdr_init(&hdr, ctx->ifname);
	hdr.nr_reqtype = NETMAP_REQ_PORT_HDR_SET;
	hdr.nr_body    = &req;
	memset(&req, 0, sizeof(req));
	req.nr_hdr_len = ctx->nr_hdr_len;
	ret	    = ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, PORT_HDR_SET)");
		return ret;
	}

	if (req.nr_hdr_len != ctx->nr_hdr_len) {
		return -1;
	}

	printf("Testing NETMAP_REQ_PORT_HDR_GET on '%s'\n", ctx->ifname);
	hdr.nr_reqtype = NETMAP_REQ_PORT_HDR_GET;
	req.nr_hdr_len = 0;
	ret	    = ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, PORT_HDR_SET)");
		return ret;
	}
	printf("nr_hdr_len %u\n", req.nr_hdr_len);

	return (req.nr_hdr_len == ctx->nr_hdr_len) ? 0 : -1;
}

static int
vale_ephemeral_port_hdr_manipulation(int fd, struct TestContext *ctx)
{
	int ret;

	ctx->ifname  = "vale:eph0";
	ctx->nr_mode = NR_REG_ALL_NIC;
	if ((ret = port_register(fd, ctx))) {
		return ret;
	}
	/* Try to set and get all the acceptable values. */
	ctx->nr_hdr_len = 12;
	if ((ret = port_hdr_set_and_get(fd, ctx))) {
		return ret;
	}
	ctx->nr_hdr_len = 0;
	if ((ret = port_hdr_set_and_get(fd, ctx))) {
		return ret;
	}
	ctx->nr_hdr_len = 10;
	if ((ret = port_hdr_set_and_get(fd, ctx))) {
		return ret;
	}
	return 0;
}

static int
vale_persistent_port(int fd, struct TestContext *ctx)
{
	struct nmreq_vale_newif req;
	struct nmreq_header hdr;
	int result;
	int ret;

	ctx->ifname = "per4";

	printf("Testing NETMAP_REQ_VALE_NEWIF on '%s'\n", ctx->ifname);

	nmreq_hdr_init(&hdr, ctx->ifname);
	hdr.nr_reqtype = NETMAP_REQ_VALE_NEWIF;
	hdr.nr_body    = &req;
	memset(&req, 0, sizeof(req));
	req.nr_mem_id   = ctx->nr_mem_id;
	req.nr_tx_slots = ctx->nr_tx_slots;
	req.nr_rx_slots = ctx->nr_rx_slots;
	req.nr_tx_rings = ctx->nr_tx_rings;
	req.nr_rx_rings = ctx->nr_rx_rings;
	ret		= ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, VALE_NEWIF)");
		return ret;
	}

	/* Attach the persistent VALE port to a switch and then detach. */
	result = vale_attach_detach(fd, ctx);

	printf("Testing NETMAP_REQ_VALE_DELIF on '%s'\n", ctx->ifname);
	hdr.nr_reqtype = NETMAP_REQ_VALE_DELIF;
	hdr.nr_body    = NULL;
	ret	    = ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, VALE_NEWIF)");
		if (result == 0) {
			result = ret;
		}
	}

	return result;
}

/* Single NETMAP_REQ_POOLS_INFO_GET. */
static int
pools_info_get(int fd, struct TestContext *ctx)
{
	struct nmreq_pools_info_get req;
	struct nmreq_header hdr;
	int ret;

	printf("Testing NETMAP_REQ_POOLS_INFO_GET on '%s'\n", ctx->ifname);

	nmreq_hdr_init(&hdr, ctx->ifname);
	hdr.nr_reqtype = NETMAP_REQ_POOLS_INFO_GET;
	hdr.nr_body    = &req;
	memset(&req, 0, sizeof(req));
	ret = ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, POOLS_INFO_GET)");
		return ret;
	}
	printf("nr_memsize %lu\n", req.nr_memsize);
	printf("nr_mem_id %u\n", req.nr_mem_id);
	printf("nr_if_pool_offset 0x%lx\n", req.nr_if_pool_offset);
	printf("nr_if_pool_objtotal %u\n", req.nr_if_pool_objtotal);
	printf("nr_if_pool_objsize %u\n", req.nr_if_pool_objsize);
	printf("nr_ring_pool_offset 0x%lx\n", req.nr_if_pool_offset);
	printf("nr_ring_pool_objtotal %u\n", req.nr_ring_pool_objtotal);
	printf("nr_ring_pool_objsize %u\n", req.nr_ring_pool_objsize);
	printf("nr_buf_pool_offset 0x%lx\n", req.nr_buf_pool_offset);
	printf("nr_buf_pool_objtotal %u\n", req.nr_buf_pool_objtotal);
	printf("nr_buf_pool_objsize %u\n", req.nr_buf_pool_objsize);

	return req.nr_memsize && req.nr_if_pool_objtotal &&
			       req.nr_if_pool_objsize &&
			       req.nr_ring_pool_objtotal &&
			       req.nr_ring_pool_objsize &&
			       req.nr_buf_pool_objtotal &&
			       req.nr_buf_pool_objsize
		       ? 0
		       : -1;
}

static int
register_and_pools_info_get(int fd, struct TestContext *ctx)
{
	int ret;

	ret = pools_info_get(fd, ctx);
	if (ret == 0) {
		printf("Failed: POOLS_INFO_GET didn't fail on unbound "
		       "netmap device\n");
		return -1;
	}

	ctx->nr_mode = NR_REG_ONE_NIC;
	ret	  = port_register(fd, ctx);
	if (ret) {
		return ret;
	}
	ctx->nr_mem_id = 1;

	return pools_info_get(fd, ctx);
}

static int
pipe_master(int fd, struct TestContext *ctx)
{
	char pipe_name[128];

	snprintf(pipe_name, sizeof(pipe_name), "%s{%s", ctx->ifname, "pipeid1");
	ctx->ifname  = pipe_name;
	ctx->nr_mode = NR_REG_ONE_NIC;

	if (port_register(fd, ctx) == 0) {
		printf("pipes should not accept NR_REG_ONE_NIC");
		return -1;
	}
	ctx->nr_mode = NR_REG_ALL_NIC;

	return port_register(fd, ctx);
}

static int
pipe_slave(int fd, struct TestContext *ctx)
{
	char pipe_name[128];

	snprintf(pipe_name, sizeof(pipe_name), "%s}%s", ctx->ifname, "pipeid2");
	ctx->ifname  = pipe_name;
	ctx->nr_mode = NR_REG_ALL_NIC;

	return port_register(fd, ctx);
}

/* NETMAP_REQ_VALE_POLLING_ENABLE */
static int
vale_polling_enable(int fd, struct TestContext *ctx)
{
	struct nmreq_vale_polling req;
	struct nmreq_header hdr;
	char vpname[256];
	int ret;

	snprintf(vpname, sizeof(vpname), "%s:%s", ctx->bdgname, ctx->ifname);
	printf("Testing NETMAP_REQ_VALE_POLLING_ENABLE on '%s'\n", vpname);

	nmreq_hdr_init(&hdr, vpname);
	hdr.nr_reqtype = NETMAP_REQ_VALE_POLLING_ENABLE;
	hdr.nr_body    = &req;
	memset(&req, 0, sizeof(req));
	req.nr_mode		= ctx->nr_mode;
	req.nr_first_cpu_id     = ctx->nr_first_cpu_id;
	req.nr_num_polling_cpus = ctx->nr_num_polling_cpus;
	ret			= ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, VALE_POLLING_ENABLE)");
		return ret;
	}

	return (req.nr_mode == ctx->nr_mode &&
		req.nr_first_cpu_id == ctx->nr_first_cpu_id &&
		req.nr_num_polling_cpus == ctx->nr_num_polling_cpus)
		       ? 0
		       : -1;
}

/* NETMAP_REQ_VALE_POLLING_DISABLE */
static int
vale_polling_disable(int fd, struct TestContext *ctx)
{
	struct nmreq_vale_polling req;
	struct nmreq_header hdr;
	char vpname[256];
	int ret;

	snprintf(vpname, sizeof(vpname), "%s:%s", ctx->bdgname, ctx->ifname);
	printf("Testing NETMAP_REQ_VALE_POLLING_DISABLE on '%s'\n", vpname);

	nmreq_hdr_init(&hdr, vpname);
	hdr.nr_reqtype = NETMAP_REQ_VALE_POLLING_DISABLE;
	hdr.nr_body    = &req;
	memset(&req, 0, sizeof(req));
	ret = ioctl(fd, NIOCCTRL, &hdr);
	if (ret) {
		perror("ioctl(/dev/netmap, NIOCCTRL, VALE_POLLING_DISABLE)");
		return ret;
	}

	return 0;
}

static int
vale_polling_enable_disable(int fd, struct TestContext *ctx)
{
	int ret = 0;

	if ((ret = vale_attach(fd, ctx))) {
		return ret;
	}

	ctx->nr_mode		 = NETMAP_POLLING_MODE_SINGLE_CPU;
	ctx->nr_num_polling_cpus = 1;
	ctx->nr_first_cpu_id     = 0;
	if ((ret = vale_polling_enable(fd, ctx))) {
		vale_detach(fd, ctx);
		return ret;
	}

	if ((ret = vale_polling_disable(fd, ctx))) {
		vale_detach(fd, ctx);
		return ret;
	}

	return vale_detach(fd, ctx);
}

static void
usage(const char *prog)
{
	printf("%s -i IFNAME [-j TESTCASE]\n", prog);
}

static testfunc_t tests[] = {port_info_get,
			     port_register_hwall_host,
			     port_register_hwall,
			     port_register_host,
			     port_register_single_ring_couple,
			     vale_attach_detach,
			     vale_attach_detach_host_rings,
			     vale_ephemeral_port_hdr_manipulation,
			     vale_persistent_port,
			     register_and_pools_info_get,
			     pipe_master,
			     pipe_slave,
			     vale_polling_enable_disable};

int
main(int argc, char **argv)
{
	struct TestContext ctx;
	unsigned int i;
	int j = -1;
	int opt;

	memset(&ctx, 0, sizeof(ctx));
	ctx.ifname  = "lo";
	ctx.bdgname = "vale1x2";

	while ((opt = getopt(argc, argv, "hi:j:")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return 0;

		case 'i':
			ctx.ifname = optarg;
			break;

		case 'j':
			j = atoi(optarg);
			break;

		default:
			printf("    Unrecognized option %c\n", opt);
			usage(argv[0]);
			return -1;
		}
	}

	if (j >= 0) {
		j--; /* one-based --> zero-based */
		if (j >= (int)(sizeof(tests) / sizeof(tests[0]))) {
			printf("Error: Test not in range\n");
			return -1;
		}
	}
	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		struct TestContext ctxcopy;
		int fd;
		int ret;
		if (j >= 0 && (unsigned)j != i) {
			continue;
		}
		fd = open("/dev/netmap", O_RDWR);
		if (fd < 0) {
			perror("open(/dev/netmap)");
			return fd;
		}
		memcpy(&ctxcopy, &ctx, sizeof(ctxcopy));
		ret = tests[i](fd, &ctxcopy);
		if (ret) {
			printf("Test #%d failed\n", i + 1);
			return ret;
		}
		printf("Test #%d successful\n", i + 1);
		close(fd);
	}

	return 0;
}
