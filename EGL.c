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

#include "com_sun_glass_ui_monocle_EGL.h"
#include "Monocle.h"
#include "kms_common.h"

// SHARED MEM
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

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

        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
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

static int drm_atomic_commit(uint32_t plane_id, uint32_t fb_id, uint32_t flags, int needsPropertySet)
{
	drmModeAtomicReq *req;
	uint32_t blob_id;
	int ret;

	req = drmModeAtomicAlloc();

	if (flags & DRM_MODE_ATOMIC_ALLOW_MODESET) {
		if (add_connector_property(req, drm.connector_id, "CRTC_ID",
						drm.crtc_id) < 0)
				return -1;

		if (drmModeCreatePropertyBlob(drm.fd, drm.mode, sizeof(*drm.mode),
					      &blob_id) != 0)
			return -1;

		if (add_crtc_property(req, drm.crtc_id, "MODE_ID", blob_id) < 0)
			return -1;

		if (add_crtc_property(req, drm.crtc_id, "ACTIVE", 1) < 0)
			return -1;
	}

	add_plane_property(req, plane_id, "FB_ID", fb_id);
	add_plane_property(req, plane_id, "CRTC_ID", drm.crtc_id);
    if (needsPropertySet) {
        add_plane_property(req, plane_id, "SRC_X", 0);
        add_plane_property(req, plane_id, "SRC_Y", 0);
        add_plane_property(req, plane_id, "SRC_W", drm.mode->hdisplay << 16);
        add_plane_property(req, plane_id, "SRC_H", drm.mode->vdisplay << 16);
        add_plane_property(req, plane_id, "CRTC_X", 0);
        add_plane_property(req, plane_id, "CRTC_Y", 0);
        add_plane_property(req, plane_id, "CRTC_W", drm.mode->hdisplay);
        add_plane_property(req, plane_id, "CRTC_H", drm.mode->vdisplay);
    }

	if (drm.kms_in_fence_fd != -1) {
		add_crtc_property(req, drm.crtc_id, "OUT_FENCE_PTR",
				VOID2U64(&drm.kms_out_fence_fd));
		add_plane_property(req, plane_id, "IN_FENCE_FD", drm.kms_in_fence_fd);
	}

	ret = drmModeAtomicCommit(drm.fd, req, flags, NULL);
	if (ret)
		goto out;

	if (drm.kms_in_fence_fd != -1) {
		close(drm.kms_in_fence_fd);
		drm.kms_in_fence_fd = -1;
	}

out:
	drmModeAtomicFree(req);

	return ret;
}

static EGLSyncKHR create_fence(EGLDisplay display, int fd)
{
	EGLint attrib_list[] = {
		EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd,
		EGL_NONE,
	};
	EGLSyncKHR fence = eglCreateSyncKHR(display,
			EGL_SYNC_NATIVE_FENCE_ANDROID, attrib_list);
	assert(fence);
	return fence;
}

void setEGLAttrs(jint *attrs, int *eglAttrs) {
    int index = 0;

    eglAttrs[index++] = EGL_SURFACE_TYPE;
    if (attrs[6] != 0) {
        eglAttrs[index++] = (EGL_WINDOW_BIT);
    } else {
        eglAttrs[index++] = EGL_PBUFFER_BIT;
    }

    // TODO:  We are depending on the order of attributes defined in
    // GLPixelFormat - we need a better way to manage this

    if (attrs[0] == 5 && attrs[1] == 6
            && attrs[2] == 5 && attrs[3] == 0) {
        // Optimization for Raspberry Pi model B. Even though the result
        // of setting EGL_BUFFER_SIZE to 16 should be the same as setting
        // component sizes separately, we get less per-frame overhead if we
        // only set EGL_BUFFER_SIZE.
        eglAttrs[index++] = EGL_BUFFER_SIZE;
        eglAttrs[index++] = 16;
    } else {
        eglAttrs[index++] = EGL_RED_SIZE;
        eglAttrs[index++] = attrs[0];
        eglAttrs[index++] = EGL_GREEN_SIZE;
        eglAttrs[index++] = attrs[1];
        eglAttrs[index++] = EGL_BLUE_SIZE;
        eglAttrs[index++] = attrs[2];
        eglAttrs[index++] = EGL_ALPHA_SIZE;
        eglAttrs[index++] = attrs[3];
    }

    eglAttrs[index++] = EGL_DEPTH_SIZE;
    eglAttrs[index++] = attrs[4];
    eglAttrs[index++] = EGL_RENDERABLE_TYPE;
    eglAttrs[index++] = EGL_OPENGL_ES2_BIT;
    eglAttrs[index] = EGL_NONE;
}

JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_initDRM
    (JNIEnv *env, jclass UNUSED(clazz), jstring device, jint prefW, jint prefH) {
    
    const char *device_c = (*env)->GetStringUTFChars(env, device, 0);
    if (X_E_DEBUG) printf("Init DRM %s\n", device_c);
    
    int ret = init_drm(&drm, device_c, prefW, prefH);
    
    (*env)->ReleaseStringUTFChars(env, device, device_c);
    
    if (ret) {
        if (X_E_DEBUG) printf("failed to initialize legacy DRM\n");
        return JNI_FALSE;
    }
   if (suppliment_atomic(drm, ret) == NULL) ret = 1;
    
    if (ret) {
        if (X_E_DEBUG) printf("failed to initialize atomic DRM\n");
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_initGBM
    (JNIEnv *env, jclass UNUSED(clazz)) {
    
    static const struct gbm *gbm_ptr;

	gbm_ptr = init_gbm(&gbm, drm.fd, (drm.mode)->hdisplay, (drm.mode)->vdisplay,
			DRM_FORMAT_MOD_LINEAR);
    if (!gbm_ptr) {
        if (X_E_DEBUG) printf("failed to initialize GBM\n");
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL Java_com_sun_glass_ui_monocle_EGL_eglGetGBMDisplay
    (JNIEnv *UNUSED(env), jclass UNUSED(clazz), jlongArray eglWindowRef) {
    // EGLNativeDisplayType is defined differently on different systems; can be an int or a ptr so cast with care

    //EGLDisplay dpy = eglGetDisplay(((EGLNativeDisplayType) (unsigned long)(display)));
 
    #define get_proc_client(ext, name) do { \
		if (has_ext(egl_exts_client, #ext)) \
			name = (void *)eglGetProcAddress(#name); \
	} while (0)

    const char *egl_exts_client;
    egl_exts_client = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    get_proc_client(EGL_EXT_platform_base, eglGetPlatformDisplayEXT);
    
    EGLDisplay dpy;

    if (eglGetPlatformDisplayEXT) {
	printf("TRACE: valid ptr to eglGetPlatformDisplayEXT\n");
        dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm.dev, NULL);
    } else {
	printf("TRACE: INvalid ptr to eglGetPlatformDisplayEXT\n");
        dpy = eglGetDisplay((void *)(gbm.dev));
    }

    jlong castWindow = asJLong(gbm.surface);
    (*env)->SetLongArrayRegion(env, eglWindowRef, 0, 1, &castWindow);
    return asJLong(dpy);
}

//////////////////

WEAK uint64_t
gbm_bo_get_modifier(struct gbm_bo *bo);

WEAK int
gbm_bo_get_plane_count(struct gbm_bo *bo);

WEAK uint32_t
gbm_bo_get_stride_for_plane(struct gbm_bo *bo, int plane);

WEAK uint32_t
gbm_bo_get_offset(struct gbm_bo *bo, int plane);

static void
drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
	int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
	struct drm_fb *fb = data;

	if (fb->fb_id)
		drmModeRmFB(drm_fd, fb->fb_id);

	free(fb);
}

struct drm_fb * drm_fb_get_from_bo(struct gbm_bo *bo)
{
	int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
	struct drm_fb *fb = gbm_bo_get_user_data(bo);
	uint32_t width, height, format,
		 strides[4] = {0}, handles[4] = {0},
		 offsets[4] = {0}, flags = 0;
	int ret = -1;

	if (fb)
		return fb;

	fb = calloc(1, sizeof *fb);
	fb->bo = bo;

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	format = gbm_bo_get_format(bo);

    const int num_planes = gbm_bo_get_plane_count(bo);
	if (gbm_bo_get_modifier && gbm_bo_get_plane_count &&
	    gbm_bo_get_stride_for_plane && gbm_bo_get_offset) {

		uint64_t modifiers[4] = {0};
		modifiers[0] = gbm_bo_get_modifier(bo);
		const int num_planes = gbm_bo_get_plane_count(bo);
		for (int i = 0; i < num_planes; i++) {
			strides[i] = gbm_bo_get_stride_for_plane(bo, i);
			handles[i] = gbm_bo_get_handle(bo).u32;
			offsets[i] = gbm_bo_get_offset(bo, i);
			modifiers[i] = modifiers[0];
		}

		if (modifiers[0]) {
			flags = DRM_MODE_FB_MODIFIERS;
			if (X_E_DEBUG) printf("Using modifier %" PRIx64 "\n", modifiers[0]);
		}

		ret = drmModeAddFB2WithModifiers(drm_fd, width, height,
				format, handles, strides, offsets,
				modifiers, &fb->fb_id, flags);
	}

	if (ret) {
		if (flags)
			if (X_E_DEBUG) fprintf(stderr, "Modifiers failed!\n");

		memcpy(handles, (uint32_t [4]){gbm_bo_get_handle(bo).u32,0,0,0}, 16);
		memcpy(strides, (uint32_t [4]){gbm_bo_get_stride(bo),0,0,0}, 16);
		memset(offsets, 0, 16);
		ret = drmModeAddFB2(drm_fd, width, height, format,
				handles, strides, offsets, &fb->fb_id, 0);
	}

	if (ret) {
		if (X_E_DEBUG) printf("failed to create fb: %s\n", strerror(errno));
		free(fb);
		return NULL;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;
}

static uint32_t find_crtc_for_encoder(const drmModeRes *resources,
		const drmModeEncoder *encoder) {
	int i;

	for (i = 0; i < resources->count_crtcs; i++) {
		/* possible_crtcs is a bitmask as described here:
		 * https://dvdhrm.wordpress.com/2012/09/13/linux-drm-mode-setting-api
		 */
		const uint32_t crtc_mask = 1 << i;
		const uint32_t crtc_id = resources->crtcs[i];
		if (encoder->possible_crtcs & crtc_mask) {
			return crtc_id;
		}
	}

	/* no match found */
	return -1;
}

static uint32_t find_crtc_for_connector(const struct drm *drm, const drmModeRes *resources,
		const drmModeConnector *connector) {
	int i;

	for (i = 0; i < connector->count_encoders; i++) {
		const uint32_t encoder_id = connector->encoders[i];
		drmModeEncoder *encoder = drmModeGetEncoder(drm->fd, encoder_id);

		if (encoder) {
			const uint32_t crtc_id = find_crtc_for_encoder(resources, encoder);

			drmModeFreeEncoder(encoder);
			if (crtc_id != 0) {
				return crtc_id;
			}
		}
	}

	/* no match found */
	return -1;
}

static void page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	/* suppress 'unused parameter' warnings */
	(void)fd, (void)frame, (void)sec, (void)usec;

	int *waiting_for_flip = data;
	*waiting_for_flip = 0;
}

static struct gbm_bo *bo;
static struct drm_fb *fb;
static fd_set fds;
static drmEventContext evctx = {
        .version = 2,
        .page_flip_handler = page_flip_handler,
};
static int legacy_init_surface(const struct gbm *gbm, EGLDisplay display, EGLSurface surface)
{
	
	//uint32_t i = 0;
	int ret;

	eglSwapBuffers(display, surface);
	bo = gbm_surface_lock_front_buffer(gbm->surface);
	fb = drm_fb_get_from_bo(bo);
	if (!fb) {
		if (X_E_DEBUG) fprintf(stderr, "Failed to get a new framebuffer BO\n");
		return -1;
	}

	/* set mode: */
	ret = drmModeSetCrtc(drm.fd, drm.crtc_id, fb->fb_id, 0, 0,
			&drm.connector_id, 1, drm.mode);
	if (ret) {
		if (X_E_DEBUG) printf("failed to set mode: %s\n", strerror(errno));
	}
    return ret;
}

static EGLSyncKHR gpu_fence = NULL;   /* out-fence from gpu, in-fence to kms */
static EGLSyncKHR kms_fence = NULL;   /* in-fence to gpu, out-fence from kms */

static uint32_t frame_buffer_id = 1000;
static uint8_t * primed_framebuffer;
static int dma_buf_fd ;

static void send_fd(int socket, int fd)  // send fd by socket
{
    struct msghdr msg = { 0 };
    char buf[CMSG_SPACE(sizeof(fd))];
    memset(buf, '\0', sizeof(buf));
    struct iovec io = { .iov_base = "ABC", .iov_len = 3 };

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

    *((int *) CMSG_DATA(cmsg)) = fd;

    msg.msg_controllen = CMSG_SPACE(sizeof(fd));

    if (sendmsg(socket, &msg, 0) < 0)
        fprintf(stderr, "Failed to send message\n");
}

char *socket_path = "\0jfxSocket0";
void *cameraThread(void *vargp) 
{ 
    
  struct sockaddr_un addr;
  char buf[100];
  int fd,cl,rc;

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
    unlink(socket_path);
  }

  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("bind error");
    exit(-1);
  }

  if (listen(fd, 5) == -1) {
    perror("listen error");
    exit(-1);
  }

  while (1) {
    if ( (cl = accept(fd, NULL, NULL)) == -1) {
      perror("accept erron");
      continue;
    } else {
        
      printf("accept\n");
    }
      // Send DRM struct
      int sent_bytes_drm = write(cl,&drm,sizeof(drm));
      printf("Sent DRM %d\n", sent_bytes_drm);
      // Send FD here
      send_fd(cl, drm.fd);

    while ( (rc=read(cl,buf,sizeof(buf))) > 0) {
      printf("read %u bytes: %.*s\n", rc, rc, buf);
        sleep(0.0001);
        //int ret = drm_plane_commit(drm.fd, frame_buffer_id, 0, 0);
    }
    if (rc == -1) {
      perror("read");
      exit(-1);
    }
    else if (rc == 0) {
      printf("EOF\n");
      close(cl);
    }
    
  }
    
    
    return NULL; 
} 

static int atomic_init_surface(EGLDisplay display, EGLSurface surface)
{
    int ret;
    
    
	/* Allow a modeset change for the first commit only. */
	uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_ATOMIC_ALLOW_MODESET;
	
	if (egl_check(eglDupNativeFenceFDANDROID) ||
	    egl_check(eglCreateSyncKHR) ||
	    egl_check(eglDestroySyncKHR) ||
	    egl_check(eglWaitSyncKHR) ||
	    egl_check(eglClientWaitSyncKHR))
		return -1;

		struct gbm_bo *next_bo;

		if (drm.kms_out_fence_fd != -1) {
			kms_fence = create_fence(display, drm.kms_out_fence_fd);
			assert(kms_fence);

			/* driver now has ownership of the fence fd: */
			drm.kms_out_fence_fd = -1;

			/* wait "on the gpu" (ie. this won't necessarily block, but
			 * will block the rendering until fence is signaled), until
			 * the previous pageflip completes so we don't render into
			 * the buffer that is still on screen.
			 */
			eglWaitSyncKHR(display, kms_fence, 0);
		}

		// TODO
		//draw(i++);

		/* insert fence to be singled in cmdstream.. this fence will be
		 * signaled when gpu rendering done
		 */
		gpu_fence = create_fence(display, EGL_NO_NATIVE_FENCE_FD_ANDROID);
		assert(gpu_fence);

		eglSwapBuffers(display, surface);

		/* after swapbuffers, gpu_fence should be flushed, so safe
		 * to get fd:
		 */
		drm.kms_in_fence_fd = eglDupNativeFenceFDANDROID(display, gpu_fence);
		eglDestroySyncKHR(display, gpu_fence);
		assert(drm.kms_in_fence_fd != -1);

		next_bo = gbm_surface_lock_front_buffer(gbm.surface);
		if (!next_bo) {
			printf("Failed to lock frontbuffer\n");
			return -1;
		}
		fb = drm_fb_get_from_bo(next_bo);
		if (!fb) {
			printf("Failed to get a new framebuffer BO\n");
			return -1;
		}

		if (kms_fence) {
			EGLint status;

			/* Wait on the CPU side for the _previous_ commit to
			 * complete before we post the flip through KMS, as
			 * atomic will reject the commit if we post a new one
			 * whilst the previous one is still pending.
			 */
			do {
				status = eglClientWaitSyncKHR(display,
								   kms_fence,
								   0,
								   EGL_FOREVER_KHR);
			} while (status != EGL_CONDITION_SATISFIED_KHR);

			eglDestroySyncKHR(display, kms_fence);
		}

		ret = drm_atomic_commit(drm.plane->plane->plane_id, fb->fb_id, flags, 1);
		if (ret) {
			printf("failed to commit: %s\n", strerror(errno));
			return -1;
		}
		/* release last buffer to render on again: */
		if (bo)
			gbm_surface_release_buffer(gbm.surface, bo);
		bo = next_bo;
		
        
        // Start server
        pthread_t thread_id; 
            printf("Init server thread\n"); 
            pthread_create(&thread_id, NULL, cameraThread, NULL); 
        
		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */

    
	 if (X_E_DEBUG) printf("Done atomic init surface\n");

	return ret;
}
static const struct drm * suppliment_atomic() {
	int32_t plane_id = -EINVAL;
	int ret;
    
	ret = drmSetClientCap(drm.fd, DRM_CLIENT_CAP_ATOMIC, 1);
	if (ret) {
		if (X_E_DEBUG) printf("no atomic modesetting support: %s\n", strerror(errno));
		return NULL;
	}

	get_plane_id(&plane_id);
	if (plane_id == -EINVAL) {
		if (X_E_DEBUG) printf("could not find a suitable plane\n");
		return NULL;
	}

	/* We only do single plane to single crtc to single connector, no
	 * fancy multi-monitor or multi-plane stuff.  So just grab the
	 * plane/crtc/connector property info for one of each:
	 */
	drm.plane = calloc(1, sizeof(*drm.plane));
	drm.crtc = calloc(1, sizeof(*drm.crtc));
	drm.connector = calloc(1, sizeof(*drm.connector));

#define get_resource(var, type, Type, id) do { 					\
		drm.var->type = drmModeGet##Type(drm.fd, id);			\
		if (!drm.var->type) {						\
			if (X_E_DEBUG) printf("could not get %s %i: %s\n",			\
					#type, id, strerror(errno));		\
			return NULL;						\
		}								\
	} while (0)

	get_resource(plane, plane, Plane, plane_id);
	get_resource(crtc, crtc, Crtc, drm.crtc_id);
	get_resource(connector, connector, Connector, drm.connector_id);

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
	get_properties(crtc, crtc, CRTC, drm.crtc_id);
	get_properties(connector, connector, CONNECTOR, drm.connector_id);
    
    return &drm;
}

static int legacy_flip(const struct gbm *gbm, EGLDisplay display, EGLSurface surface)
{
	int ret;

	struct gbm_bo *next_bo;
	int waiting_for_flip = 1;

	//egl->draw(i++);

	//eglSwapBuffers(display, surface);
	next_bo = gbm_surface_lock_front_buffer(gbm->surface);
	fb = drm_fb_get_from_bo(next_bo);
	if (!fb) {
		if (X_E_DEBUG) fprintf(stderr, "Failed to get a new framebuffer BO\n");
		return -1;
	}

	/*
	 * Here you could also update drm plane layers if you want
	 * hw composition
	 */
    
    // Single buffered
    ret = drmModeSetCrtc(drm.fd, drm.crtc_id, fb->fb_id, 0, 0,
			&drm.connector_id, 1, drm.mode);
	if (ret) {
		if (X_E_DEBUG) printf("failed to set mode: %s\n", strerror(errno));
	}
	
	// Double buffered
/*
	ret = drmModePageFlip(drm.fd, drm.crtc_id, fb->fb_id,
			DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
	if (ret) {
		if (X_E_DEBUG) printf("failed to queue page flip: %s\n", strerror(errno));
		return -1;
	}

	while (waiting_for_flip) {
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		FD_SET(drm.fd, &fds);

		ret = select(drm.fd + 1, &fds, NULL, NULL, NULL);
		if (ret < 0) {
			if (X_E_DEBUG) printf("select err: %s\n", strerror(errno));
			return ret;
		} else if (ret == 0) {
			if (X_E_DEBUG) printf("select timeout!\n");
			return -1;
		} else if (FD_ISSET(0, &fds)) {
			if (X_E_DEBUG) printf("user interrupted!\n");
			return 0;
		}
		drmHandleEvent(drm.fd, &evctx);
	}
*/
	/* release last buffer to render on again: */
	gbm_surface_release_buffer(gbm->surface, bo);
	bo = next_bo;
    
    return 0;
}

static int swapIdx = 0, flipIdx = 0;
static int waitForDrmFence(EGLDisplay display, EGLSurface surface) {
    
    if (drm.kms_out_fence_fd != -1) {
        kms_fence = create_fence(display, drm.kms_out_fence_fd);
        assert(kms_fence);

        /* driver now has ownership of the fence fd: */
        drm.kms_out_fence_fd = -1;

        /* wait "on the gpu" (ie. this won't necessarily block, but
            * will block the rendering until fence is signaled), until
            * the previous pageflip completes so we don't render into
            * the buffer that is still on screen.
            */
        eglWaitSyncKHR(display, kms_fence, 0);
    }
    
    return 0;
}

static int doSwapBuffers(EGLDisplay display, EGLSurface surface) {
    
    // Swap buffers with fence
    gpu_fence = create_fence(display, EGL_NO_NATIVE_FENCE_FD_ANDROID);
    assert(gpu_fence);
    //printf("Swp%d\n", swapIdx++);
    int ret = eglSwapBuffers(display, surface);
    return ret;
}
		
static int colordd = 0;
static int atomic_flip(EGLDisplay display, EGLSurface surface)
{
	uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK;
	int ret;
    
		struct gbm_bo *next_bo;

   // printf("Tlp%d\n", swapIdx++);

		// TODO
		//egl->draw(i++);

		/* insert fence to be singled in cmdstream.. this fence will be
		 * signaled when gpu rendering done
		 */

        // Happens in java
		//doSwapBuffers(display, surface);

		/* after swapbuffers, gpu_fence should be flushed, so safe
		 * to get fd:
		 */
		drm.kms_in_fence_fd = eglDupNativeFenceFDANDROID(display, gpu_fence);
		eglDestroySyncKHR(display, gpu_fence);
		assert(drm.kms_in_fence_fd != -1);

		next_bo = gbm_surface_lock_front_buffer(gbm.surface);
		if (!next_bo) {
			printf("Failed to lock frontbuffer\n");
			return -1;
		}
		fb = drm_fb_get_from_bo(next_bo);
		if (!fb) {
			printf("Failed to get a new framebuffer BO\n");
			return -1;
		}

		if (kms_fence) {
			EGLint status;

			/* Wait on the CPU side for the _previous_ commit to
			 * complete before we post the flip through KMS, as
			 * atomic will reject the commit if we post a new one
			 * whilst the previous one is still pending.
			 */
			do {
				status = eglClientWaitSyncKHR(display,
								   kms_fence,
								   0,
								   EGL_FOREVER_KHR);
			} while (status != EGL_CONDITION_SATISFIED_KHR);

			eglDestroySyncKHR(display, kms_fence);
		}

		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */
        
		ret = drm_atomic_commit(drm.plane->plane->plane_id, fb->fb_id, flags, 0);
		if (ret) {
			printf("failed to commit: %s\n", strerror(errno));
			return -1;
		}
		
		/* release last buffer to render on again: */
		if (bo)
			gbm_surface_release_buffer(gbm.surface, bo);
		bo = next_bo;
        
        // AUX would go here
        
    //printf("Flp%d\n", flipIdx++);
	return ret;
}


int init_drm(struct drm *drm, const char *device, const int32_t prefW, const int32_t prefH)
{
	drmModeRes *resources;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;
	int i, area;

	drm->fd = open(device, O_RDWR);

	if (drm->fd < 0) {
		if (X_E_DEBUG) printf("could not open drm device\n");
		return -1;
	}

	resources = drmModeGetResources(drm->fd);
	if (!resources) {
		if (X_E_DEBUG) printf("drmModeGetResources failed: %s\n", strerror(errno));
		return -1;
	}

	/* find a connected connector: */
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(drm->fd, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			/* it's connected, let's use this! */
			break;
		}
		drmModeFreeConnector(connector);
		connector = NULL;
	}

	if (!connector) {
		/* we could be fancy and listen for hotplug events and wait for
		 * a connector..
		 */
		if (X_E_DEBUG) printf("no connected connector!\n");
		return -1;
	}

		if (X_E_DEBUG) printf("Pref %d,%d\n", prefW, prefH);
	/* find preferred mode or the highest resolution mode: */
	for (i = 0, area = 0; i < connector->count_modes; i++) {
		drmModeModeInfo *current_mode = &connector->modes[i];
		
		if (X_E_DEBUG) printf("Mode %d,%d: %d\n", current_mode->hdisplay, current_mode->vdisplay, current_mode->type & DRM_MODE_TYPE_PREFERRED);

		// Xact: add user preference over OS preference
		if (prefW == current_mode->hdisplay && prefH == current_mode->vdisplay) {
			drm->mode = current_mode;
			break;	
		} else if (current_mode->type & DRM_MODE_TYPE_PREFERRED) {
			drm->mode = current_mode;
		}

		int current_area = current_mode->hdisplay * current_mode->vdisplay;
		if (current_area > area) {
			drm->mode = current_mode;
			area = current_area;
		}
	}

	if (!drm->mode) {
		if (X_E_DEBUG) printf("could not find mode!\n");
		return -1;
	}

	/* find encoder: */
	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(drm->fd, resources->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id)
			break;
		drmModeFreeEncoder(encoder);
		encoder = NULL;
	}

	if (encoder) {
		drm->crtc_id = encoder->crtc_id;
	} else {
		uint32_t crtc_id = find_crtc_for_connector(drm, resources, connector);
		if (crtc_id == 0) {
			if (X_E_DEBUG) printf("no crtc found!\n");
			return -1;
		}

		drm->crtc_id = crtc_id;
	}

	for (i = 0; i < resources->count_crtcs; i++) {
		if (resources->crtcs[i] == drm->crtc_id) {
			drm->crtc_index = i;
			break;
		}
	}

	drmModeFreeResources(resources);

	drm->connector_id = connector->connector_id;

	return 0;
}


/* Pick a plane.. something that at a minimum can be connected to
 * the chosen crtc, but prefer primary plane.
 *
 * Seems like there is some room for a drmModeObjectGetNamedProperty()
 * type helper in libdrm..
 */
static void get_plane_id(int32_t *primaryPlane)
{
	drmModePlaneResPtr plane_resources;
	uint32_t i, j;
    
	plane_resources = drmModeGetPlaneResources(drm.fd);
	if (!plane_resources) {
		printf("drmModeGetPlaneResources failed: %s\n", strerror(errno));
		return;
	}

    printf("drmModeGetPlane counts %u\n", plane_resources->count_planes);
    
	for (i = 0; (i < plane_resources->count_planes) && *primaryPlane == -EINVAL; i++) {
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


WEAK struct gbm_surface *
gbm_surface_create_with_modifiers(struct gbm_device *gbm,
                                  uint32_t width, uint32_t height,
                                  uint32_t format,
                                  const uint64_t *modifiers,
                                  const unsigned int count);

const struct gbm * init_gbm(struct gbm * gbm, int drm_fd, int w, int h, uint64_t modifier)
{
    return init_gbm2(gbm_create_device(drm_fd), gbm, drm_fd, w, h, modifier);
}
const struct gbm * init_gbm2(struct gbm_device *devv, struct gbm * gbm, int drm_fd, int w, int h, uint64_t modifier)
{
	gbm->dev = devv;
	gbm->format = GBM_FORMAT_XRGB8888;
	gbm->surface = NULL;

	if (gbm_surface_create_with_modifiers) {
		gbm->surface = gbm_surface_create_with_modifiers(gbm->dev, w, h,
								gbm->format,
								&modifier, 1);

	}

	if (!gbm->surface) {
		if (modifier != DRM_FORMAT_MOD_LINEAR) {
			if (X_E_DEBUG) fprintf(stderr, "Modifiers requested but support isn't available\n");
			return NULL;
		}
		gbm->surface = gbm_surface_create(gbm->dev, w, h,
						gbm->format,
						GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

	}

	if (!gbm->surface) {
		if (X_E_DEBUG) printf("failed to create gbm surface\n");
		return NULL;
	}

	gbm->width = w;
	gbm->height = h;

	return gbm;
}

//////////////////
JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_eglInitialize
    (JNIEnv *env, jclass UNUSED(clazz), jlong eglDisplay, jintArray majorArray,
     jintArray minorArray){

       EGLint major, minor;

    if (eglInitialize(asPtr(eglDisplay), &major, &minor)) {
         (*env)->SetIntArrayRegion(env, majorArray, 0, 1, &major);
         (*env)->SetIntArrayRegion(env, minorArray, 0, 1, &minor);
    
    const char *egl_exts_dpy;
	egl_exts_dpy = eglQueryString(asPtr(eglDisplay), EGL_EXTENSIONS);
    #define get_proc_dpy(ext, name) do { \
		if (has_ext(egl_exts_dpy, #ext)) \
			name = (void *)eglGetProcAddress(#name); \
	} while (0)
    get_proc_dpy(EGL_KHR_image_base, eglCreateImageKHR);
	get_proc_dpy(EGL_KHR_image_base, eglDestroyImageKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglCreateSyncKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglDestroySyncKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglWaitSyncKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglClientWaitSyncKHR);
	get_proc_dpy(EGL_ANDROID_native_fence_sync, eglDupNativeFenceFDANDROID);

        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_eglBindAPI
    (JNIEnv *UNUSED(env), jclass UNUSED(clazz), jint api) {

    if (eglBindAPI(api)) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_eglChooseConfig
    (JNIEnv *env, jclass UNUSED(clazz), jlong eglDisplay, jintArray attribs,
     jlongArray configs, jint configSize, jintArray numConfigs) {

    int i=0;

    int eglAttrs[50]; /* value, attr pair plus a None */
    jint *attrArray;

    attrArray = (*env)->GetIntArrayElements(env, attribs, JNI_FALSE);
    setEGLAttrs(attrArray, eglAttrs);
    (*env)->ReleaseIntArrayElements(env, attribs, attrArray, JNI_ABORT);
    EGLConfig *configArray = malloc(sizeof(EGLConfig) * configSize);
    jlong *longConfigArray = malloc(sizeof(long) * configSize);
    EGLint numConfigPtr=0;
    jboolean retval;

    if (!eglChooseConfig(asPtr(eglDisplay), eglAttrs, configArray, configSize,
                               &numConfigPtr)) {
        retval = JNI_FALSE;
    } else {
        retval = JNI_TRUE;
        (*env)->SetIntArrayRegion(env, numConfigs, 0, 1, &numConfigPtr);
        for (i = 0; i < numConfigPtr; i++) {
            longConfigArray[i] = asJLong(configArray[i]);
        }

        (*env)->SetLongArrayRegion(env, configs, 0, configSize, longConfigArray);
    }
    free(configArray);
    free(longConfigArray);
    return retval;
}

JNIEXPORT jlong JNICALL Java_com_sun_glass_ui_monocle_EGL__1eglCreateWindowSurface
    (JNIEnv *UNUSED(env), jclass UNUSED(clazz), jlong eglDisplay, jlong config,
     jlong nativeWindow, jintArray attribs) {

    EGLSurface eglSurface;
    EGLint *attrArray = NULL;

    if (attribs != NULL)
        attrArray = (*env)->GetIntArrayElements(env, attribs, JNI_FALSE);

    eglSurface =  eglCreateWindowSurface(asPtr(eglDisplay), asPtr(config),
                                         (EGLNativeWindowType) asPtr(nativeWindow),
                                         (EGLint *) NULL);
    if (attrArray != NULL) {
        (*env)->ReleaseIntArrayElements(env, attribs, attrArray, JNI_ABORT);
    }
    return asJLong(eglSurface);
}

JNIEXPORT jlong JNICALL Java_com_sun_glass_ui_monocle_EGL_eglCreateContext
    (JNIEnv *UNUSED(env), jclass UNUSED(clazz), jlong eglDisplay, jlong config,
      jlong UNUSED(shareContext), jintArray UNUSED(attribs)){

    // we don't support any passed-in context attributes presently
    // we don't support any share context presently
    EGLint contextAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext context = eglCreateContext(asPtr(eglDisplay), asPtr(config),
                                          NULL, contextAttrs);
    return asJLong(context);
}

JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_eglMakeCurrent
   (JNIEnv *UNUSED(env), jclass UNUSED(clazz), jlong eglDisplay, jlong drawSurface,
    jlong readSurface, jlong eglContext) {

    if (eglMakeCurrent(asPtr(eglDisplay), asPtr(drawSurface), asPtr(readSurface),
                   asPtr(eglContext))) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_eglSwapBuffers
    (JNIEnv *UNUSED(env), jclass UNUSED(clazz), jlong eglDisplay, jlong eglSurface) {
    if (doSwapBuffers(asPtr(eglDisplay), asPtr(eglSurface))) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_waitForDrmFence
    (JNIEnv *UNUSED(env), jclass UNUSED(clazz), jlong eglDisplay, jlong eglSurface) {
    if (waitForDrmFence(asPtr(eglDisplay), asPtr(eglSurface))) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}


JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_drmInitBuffers
    (JNIEnv *UNUSED(env), jclass UNUSED(clazz), jlong eglDisplay, jlong eglSurface) {
    if (atomic_init_surface(asPtr(eglDisplay), asPtr(eglSurface)) == 0) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}


JNIEXPORT jboolean JNICALL Java_com_sun_glass_ui_monocle_EGL_drmSwapBuffers
    (JNIEnv *UNUSED(env), jclass UNUSED(clazz), jlong eglDisplay, jlong eglSurface) {
    if (atomic_flip(asPtr(eglDisplay), asPtr(eglSurface)) == 0) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

JNIEXPORT jint  JNICALL Java_com_sun_glass_ui_monocle_EGL_eglGetError
    (JNIEnv *UNUSED(env), jclass UNUSED(clazz)) {
    return (jint)eglGetError();
}


