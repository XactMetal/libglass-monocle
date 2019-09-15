#include "kms_common.h"

// SHARED MEM
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#include <sys/ioctl.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <string.h>
#include <sys/select.h>
#include <sys/mman.h>

#define X_E_DEBUG 1

static struct drm drm= {
	.kms_out_fence_fd = -1,
};

#define VOID2U64(x) ((uint64_t)(unsigned long)(x))

static int add_plane_property(drmModeAtomicReq *req, uint32_t obj_id,
				const char *name, uint64_t value)
{
	struct plane *obj = drm.plane;
	unsigned int i;
	int prop_id = -1;

	for (i = 0 ; i < obj->props->count_props ; i++) {
		if (strcmp(obj->props_info[i]->name, name) == 0) {
			prop_id = obj->props_info[i]->prop_id;
			break;
		}
	}


	if (prop_id < 0) {
		printf("no plane property: %s\n", name);
		return -EINVAL;
	}

	return drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}


static void get_aux_plane_id(int32_t *auxPlane)
{
	drmModePlaneResPtr plane_resources;
	uint32_t i, j;
    
	plane_resources = drmModeGetPlaneResources(drm.fd);
	if (!plane_resources) {
		printf("drmModeGetPlaneResources failed: %s\n", strerror(errno));
		return;
	}

    printf("drmModeGetPlane counts %u\n", plane_resources->count_planes);
    
	for (i = 0; (i < plane_resources->count_planes) && *auxPlane == -EINVAL; i++) {
		uint32_t id = plane_resources->planes[i];
        printf("drmModeGetPlane(%u)\n", id);
		drmModePlanePtr plane = drmModeGetPlane(drm.fd, id);
		if (!plane) {
			printf("drmModeGetPlane(%u) failed: %s\n", id, strerror(errno));
			continue;
		}

		if (plane->possible_crtcs & (1 << drm.crtc_index)) {
			drmModeObjectPropertiesPtr props =
				drmModeObjectGetProperties(drm.fd, id, DRM_MODE_OBJECT_PLANE);

			for (j = 0; j < props->count_props; j++) {
				drmModePropertyPtr p =
					drmModeGetProperty(drm.fd, props->props[j]);
                
                    printf("%u->%s = %d\n", id,p->name,(int32_t) props->prop_values[j]);
				if ((strcmp(p->name, "type") == 0) &&
						(props->prop_values[j] == DRM_PLANE_TYPE_OVERLAY)) {
					/* found our primary plane, lets use that: */
					*auxPlane = id;
                    printf("drmModeGetPlane found aux plane\n");
				}

				drmModeFreeProperty(p);
			}

			drmModeFreeObjectProperties(props);
		}

		drmModeFreePlane(plane);
	}

	drmModeFreePlaneResources(plane_resources);

	return;
}


static const struct drm * init_plane() {
	int32_t plane_id = -EINVAL;
	int ret;
    
	get_aux_plane_id(&plane_id);
	if (plane_id == -EINVAL) {
		if (X_E_DEBUG) printf("could not find a suitable plane\n");
		return NULL;
	}

	drm.plane = calloc(1, sizeof(*drm.plane));

#define get_resource(var, type, Type, id) do { 					\
		drm.var->type = drmModeGet##Type(drm.fd, id);			\
		if (!drm.var->type) {						\
			if (X_E_DEBUG) printf("could not get %s %i: %s\n",			\
					#type, id, strerror(errno));		\
			return NULL;						\
		}								\
	} while (0)

	get_resource(plane, plane, Plane, plane_id);

#define get_properties(var, type, TYPE, id) do {					\
		uint32_t i;							\
		drm.var->props = drmModeObjectGetProperties(drm.fd,		\
				id, DRM_MODE_OBJECT_##TYPE);			\
		if (!drm.var->props) {						\
			if (X_E_DEBUG) printf("could not get %s %u properties: %s\n", 		\
					#type, id, strerror(errno));		\
			return NULL;						\
		}								\
		drm.var->props_info = calloc(drm.var->props->count_props,	\
				sizeof(drm.var->props_info));			\
		for (i = 0; i < drm.var->props->count_props; i++) {		\
			drm.var->props_info[i] = drmModeGetProperty(drm.fd,	\
					drm.var->props->props[i]);		\
		}								\
	} while (0)

	get_properties(plane, plane, PLANE, plane_id);
    
    return &drm;
}






static int px = 0, py = 0;
static int pxs = 2, pys = 2;
static int drm_plane_commit(uint32_t plane_id, uint32_t fb_id, uint32_t flags, int needsPropertySet)
{
	drmModeAtomicReq *req;
	int ret;

	req = drmModeAtomicAlloc();

    px+=pxs;
    py+=pys;
    if (px >= 512 || px <= 0) pxs *= -1;
    if (py >= 300 || py <= 0) pys *= -1;
    px+=pxs;
    py+=pys;
    
    if (needsPropertySet) {
        add_plane_property(req, plane_id, "FB_ID", fb_id);
        add_plane_property(req, plane_id, "CRTC_ID", drm.crtc_id);
        add_plane_property(req, plane_id, "SRC_X", 0);
        add_plane_property(req, plane_id, "SRC_Y", 0);
        add_plane_property(req, plane_id, "SRC_W", ((uint32_t)512) << 16);
        add_plane_property(req, plane_id, "SRC_H", ((uint32_t)300) << 16);
        add_plane_property(req, plane_id, "CRTC_W", 512); // TODO
        add_plane_property(req, plane_id, "CRTC_H", 300);
    }
// TODO
        add_plane_property(req, plane_id, "CRTC_X", px);
        add_plane_property(req, plane_id, "CRTC_Y", py);
        
	ret = drmModeAtomicCommit(drm.fd, req, flags, NULL);

	drmModeAtomicFree(req);

	return ret;
}

//////////////////

static uint32_t frame_buffer_id = 1000;
static uint8_t * primed_framebuffer;

static int atomic_init_surface()
{
    int ret;
    
    // AUX FRAMEBUFFER
    /* Request a dumb buffer */
	struct drm_mode_create_dumb create_request = {
		.width  = 512,
		.height = 300,
		.bpp    = 32
	};
	ret = ioctl(drm.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_request);

	/* Bail out if we could not allocate a dumb buffer */
	if (ret) {
		printf("DBA failed\n");
	}
	/* Create a framebuffer, using the old method.
	 */
	ret = drmModeAddFB(
		drm.fd,
		512, 300,
		24, 32, create_request.pitch,
		create_request.handle, &frame_buffer_id
	);
	/* Without framebuffer, we won't do anything so bail out ! */
	if (ret) {
		printf("Could not add a framebuffer using drmModeAddFB\n");
	} else {
        printf("Aux fb id = %u\n", frame_buffer_id);
    }
    
    struct drm_prime_handle prime_request = {
		.handle = create_request.handle,
		.flags  = DRM_CLOEXEC | DRM_RDWR,
		.fd     = -1
	};

	ret = ioctl(drm.fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime_request);
	int const dma_buf_fd = prime_request.fd;
    /* If we could not export the buffer, bail out since that's the
	 * purpose of our test */
	if (ret || dma_buf_fd < 0) {
		printf(
			"Could not export buffer : %s (%d) - FD : %d\n",
			strerror(ret), ret,
			dma_buf_fd
 		);
	}

	/* Map the exported buffer, using the PRIME File descriptor */
	/* That ONLY works if the DRM driver implements gem_prime_mmap.
	 * This function is not implemented in most of the DRM drivers for 
	 * GPU with discrete memory. Meaning that it will surely fail with
	 * Radeon, AMDGPU and Nouveau drivers for desktop cards ! */
	primed_framebuffer = mmap(
		0, create_request.size,	PROT_READ | PROT_WRITE, MAP_SHARED,
		dma_buf_fd, 0);
	ret = errno;

	if (primed_framebuffer == NULL || primed_framebuffer == MAP_FAILED) {
		printf(
			"Could not map buffer exported through PRIME : %s (%d)\n"
			"Buffer : %p\n",
			strerror(ret), ret,
			primed_framebuffer
		);
	}
	
    for (uint_fast32_t p = 0; p < 30000; p++)
			((uint32_t *) primed_framebuffer)[p] = 120;
	printf("Buffer mapped !\n");
    
    // END AUX FRAMEBUFFER
    
        ret = drm_plane_commit(drm.plane->plane->plane_id, frame_buffer_id, 0, 1);
        
        if (ret) {
            printf("failed aux plane %d: %d %d\n", drm.plane->plane->plane_id, ret, errno);
            //return -1;
        }
		
    
	 if (X_E_DEBUG) printf("Done atomic init surface\n");

	return ret;
}


int fd_recv = -1;

char *socket_path = "\0jfxSocket0";

static int recv_fd(int socket)  // receive fd from socket
{
    struct msghdr msg = {0};

    char m_buffer[256];
    struct iovec io = { .iov_base = m_buffer, .iov_len = sizeof(m_buffer) };
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char c_buffer[256];
    msg.msg_control = c_buffer;
    msg.msg_controllen = sizeof(c_buffer);

    if (recvmsg(socket, &msg, 0) < 0)
        perror("Failed to receive message\n");

    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);

    unsigned char * data = CMSG_DATA(cmsg);

    printf("About to extract fd\n");
    int fd = *((int*) data);
    printf("Extracted fd %d\n", fd);

    return fd;
}

int main(int argc, char *argv[]) {
  struct sockaddr_un addr;
  char buf[100];
  int getFDCmdLen = 5;
  char * getFDCmd = "getFD";
  int fd,rc;
  

  if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket error");
    exit(-1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (*socket_path == '\0') {
    *addr.sun_path = '\0';
    strncpy(addr.sun_path+1, socket_path+1, sizeof(addr.sun_path)-2);
  } else {
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
  }

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("connect error");
    exit(-1);
  }
  
  int bytes_to_receive = sizeof(drm);
  int received_bytes = 0;
  do {
      received_bytes +=read(fd, ((uint8_t*)(&drm)) + received_bytes, bytes_to_receive - received_bytes);
  } while (received_bytes != bytes_to_receive);
  
  printf("Recv %d/%d for DRM\n", received_bytes, bytes_to_receive);
  
  drm.fd = recv_fd(fd);
  printf("%d\n", drm.fd);
  init_plane();
  
  atomic_init_surface();
  int ret = drm_plane_commit(drm.fd, frame_buffer_id, 0, 0);
    
    const uint32_t WIDTH = 512;
    const uint32_t HEIGHT = 300;
    
    uint8_t xrev = 0, yrev = 0;
    uint_fast32_t x,y;
    uint_fast32_t p;
    struct timeval timee;
	while (1) {
        gettimeofday(&timee, NULL);
        xrev = (timee.tv_usec / 1000 / 10 + timee.tv_sec * 100);
        yrev = (timee.tv_usec / 1000 / 100 + timee.tv_sec * 10);
        for (p = 0; p < 614400>>2; p++) {
            x = p & 511;
            y = p >> 9;
            ((uint32_t*)primed_framebuffer)[p] = (((x*256/WIDTH)+xrev)<<8) + (((y*256/HEIGHT)+yrev)<<16);
        }
    }
	
  
  rc = write(fd, getFDCmd, getFDCmdLen);
  if (rc != getFDCmdLen) {
      perror("write error");
      exit(-1);
  }
  
  
  
  while( (rc=read(STDIN_FILENO, &fd_recv, sizeof(fd_recv))) > 0) {
    fprintf(stderr,"Read %d bytes: %d", rc, fd_recv);
    sleep(0.1);
  }

  return 0;
}
