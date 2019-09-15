/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <EGL/egl.h>

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

//   PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
	PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
	PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
	PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
	PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
	PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
	PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
	PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
    
static bool has_ext(const char *extension_list, const char *ext)
{
	const char *ptr = extension_list;
	int len = strlen(ext);

	if (ptr == NULL || *ptr == '\0')
		return false;

	while (true) {
		ptr = strstr(ptr, ext);
		if (!ptr)
			return false;

		if (ptr[len] == ' ' || ptr[len] == '\0')
			return true;

		ptr += len;
	}
}

static inline int __egl_check(void *ptr, const char *name)
{
	if (!ptr) {
		printf("no %s\n", name);
		return -1;
	}
	return 0;
}

#define egl_check(name) __egl_check(name, #name)

static struct drm drm= {
	.kms_out_fence_fd = -1,
};
static struct gbm gbm;

#define VOID2U64(x) ((uint64_t)(unsigned long)(x))

static int add_connector_property(drmModeAtomicReq *req, uint32_t obj_id,
					const char *name, uint64_t value)
{
	struct connector *obj = drm.connector;
	unsigned int i;
	int prop_id = 0;

	for (i = 0 ; i < obj->props->count_props ; i++) {
		if (strcmp(obj->props_info[i]->name, name) == 0) {
			prop_id = obj->props_info[i]->prop_id;
			break;
		}
	}

	if (prop_id < 0) {
		printf("no connector property: %s\n", name);
		return -EINVAL;
	}

	return drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

static int add_crtc_property(drmModeAtomicReq *req, uint32_t obj_id,
				const char *name, uint64_t value)
{
	struct crtc *obj = drm.crtc;
	unsigned int i;
	int prop_id = -1;

	for (i = 0 ; i < obj->props->count_props ; i++) {
		if (strcmp(obj->props_info[i]->name, name) == 0) {
			prop_id = obj->props_info[i]->prop_id;
			break;
		}
	}

	if (prop_id < 0) {
		printf("no crtc property: %s\n", name);
		return -EINVAL;
	}

	return drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

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

static void page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	/* suppress 'unused parameter' warnings */
	(void)fd, (void)frame, (void)sec, (void)usec;

	int *waiting_for_flip = data;
	*waiting_for_flip = 0;
}

static uint32_t frame_buffer_id = 1000;
static uint8_t * primed_framebuffer;


// A normal C function that is executed as a thread  
// when its name is specified in pthread_create() 
void *cameraThread(void *vargp) 
{ 
    while (1) {
        sleep(0.0001);
        int ret = drm_plane_commit(drm.auxplane->plane->plane_id, frame_buffer_id, 0, 0);
    }
    return NULL; 
} 

static int atomic_init_surface(EGLDisplay display, EGLSurface surface)
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
//     
        
		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */

        // And that's what I'm gonna do
        ret = drm_plane_commit(drm.auxplane->plane->plane_id, frame_buffer_id, 0, 1);
        
        if (ret) {
            printf("failed aux plane %d: %d %d\n", drm.auxplane->plane->plane_id, ret, errno);
            //return -1;
        }
        else {
            pthread_t thread_id; 
            printf("Init camera thread\n"); 
            pthread_create(&thread_id, NULL, cameraThread, NULL); 
            //pthread_join(thread_id, NULL); 
            
            printf("Framebuffer is at /proc/%d/fd/%d\n", getpid(), dma_buf_fd);
        }
		
    
	 if (X_E_DEBUG) printf("Done atomic init surface\n");

	return ret;
}


/* Pick a plane.. something that at a minimum can be connected to
 * the chosen crtc, but prefer primary plane.
 *
 * Seems like there is some room for a drmModeObjectGetNamedProperty()
 * type helper in libdrm..
 */
static void get_plane_id(int32_t *primaryPlane, int32_t *auxPlane)
{
	drmModePlaneResPtr plane_resources;
	uint32_t i, j;
    
	plane_resources = drmModeGetPlaneResources(drm.fd);
	if (!plane_resources) {
		printf("drmModeGetPlaneResources failed: %s\n", strerror(errno));
		return;
	}

    printf("drmModeGetPlane counts %u\n", plane_resources->count_planes);
    
	for (i = 0; (i < plane_resources->count_planes) && !(*primaryPlane != -EINVAL && *auxPlane != -EINVAL); i++) {
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
						(props->prop_values[j] == DRM_PLANE_TYPE_PRIMARY)) {
					/* found our primary plane, lets use that: */
					*primaryPlane = id;
                    
                    printf("drmModeGetPlane found primary plane\n");
				} else if ((strcmp(p->name, "type") == 0) &&
						(props->prop_values[j] == DRM_PLANE_TYPE_OVERLAY)) {
					/* found our overlay plane, lets use that: */
                    if (*primaryPlane != -EINVAL) {
                        *auxPlane = id;
                    } else {
                        *primaryPlane = id;
                    }
					
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

int fd_recv = -1;

//char *socket_path = "./socket";
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
  
  int remote_fd = recv_fd(fd);
  printf("%d\n", remote_fd);
  uint8_t * primed_framebuffer;
primed_framebuffer = mmap(
		0, 614400,	PROT_READ | PROT_WRITE, MAP_SHARED,
		remote_fd, 0);

	if (primed_framebuffer == NULL || primed_framebuffer == MAP_FAILED) {
		printf(
			"Could not map buffer exported through PRIME : %s (%d)\n"
			"Buffer : %p\n",
			strerror(errno), errno,
			primed_framebuffer
		);
	}
	printf("Buffer mapped !\n");
    
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
