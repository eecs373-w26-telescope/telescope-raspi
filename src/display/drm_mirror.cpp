#include "drm_mirror.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef HAVE_LIBDRM

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static std::atomic<bool> g_running{false};
static std::thread g_thread;

// Reuse Raylib's DRM master fd rather than opening a fresh one.
// A fresh open of /dev/dri/card0 is never DRM master, so modesetting ioctls fail with EACCES.
static int FindDRMMasterFd() {
    DIR* dir = opendir("/proc/self/fd");
    if (!dir) return -1;

    int result = -1;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        int fd = atoi(entry->d_name);
        if (fd <= 2) continue;

        char proc_path[64], link_target[256];
        snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
        ssize_t len = readlink(proc_path, link_target, sizeof(link_target) - 1);
        if (len <= 0) continue;
        link_target[len] = '\0';

        if (!strstr(link_target, "/dev/dri/card")) continue;
        if (drmIsMaster(fd)) { result = fd; break; }
    }

    closedir(dir);
    return result;
}

static void MirrorLoop() {
    int fd = FindDRMMasterFd();
    if (fd < 0) {
        fprintf(stderr, "drm_mirror: no DRM master fd found\n");
        return;
    }
    fprintf(stderr, "drm_mirror: using fd %d\n", fd);

    drmModeResPtr res = drmModeGetResources(fd);
    if (!res) {
        fprintf(stderr, "drm_mirror: drmModeGetResources failed\n");
        return;
    }
    fprintf(stderr, "drm_mirror: %d CRTCs, %d connectors\n", res->count_crtcs, res->count_connectors);

    // Find the active primary CRTC set up by Raylib
    uint32_t primary_crtc_id = 0;
    drmModeModeInfo primary_mode = {};
    uint32_t last_fb = 0;

    for (int i = 0; i < res->count_crtcs; i++) {
        drmModeCrtcPtr c = drmModeGetCrtc(fd, res->crtcs[i]);
        if (c && c->mode_valid && c->buffer_id != 0) {
            primary_crtc_id = res->crtcs[i];
            primary_mode = c->mode;
            last_fb = c->buffer_id;
            fprintf(stderr, "drm_mirror: primary CRTC id=%u fb=%u mode=%dx%d\n",
                    primary_crtc_id, last_fb, primary_mode.hdisplay, primary_mode.vdisplay);
            drmModeFreeCrtc(c);
            break;
        }
        if (c) drmModeFreeCrtc(c);
    }

    if (!primary_crtc_id) {
        fprintf(stderr, "drm_mirror: no active primary CRTC found\n");
        drmModeFreeResources(res);
        return;
    }

    // Find a secondary connected connector not driving the primary CRTC
    uint32_t secondary_conn_id = 0;
    uint32_t secondary_crtc_id = 0;
    drmModeModeInfo secondary_mode = {};

    for (int i = 0; i < res->count_connectors && !secondary_conn_id; i++) {
        drmModeConnectorPtr conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;

        fprintf(stderr, "drm_mirror: connector id=%u status=%d modes=%d\n",
                res->connectors[i], conn->connection, conn->count_modes);

        if (conn->connection != DRM_MODE_CONNECTED || !conn->count_modes) {
            drmModeFreeConnector(conn);
            continue;
        }

        bool is_primary = false;
        if (conn->encoder_id) {
            drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoder_id);
            if (enc) {
                is_primary = (enc->crtc_id == primary_crtc_id);
                drmModeFreeEncoder(enc);
            }
        }
        if (is_primary) {
            fprintf(stderr, "drm_mirror: connector id=%u is primary, skipping\n", res->connectors[i]);
            drmModeFreeConnector(conn);
            continue;
        }

        // Prefer a mode matching the primary's resolution; fall back to first mode.
        // Raylib's double-buffered FBOs are both the same size, so either mode entry works for page flips.
        for (int m = 0; m < conn->count_modes; m++) {
            if (conn->modes[m].hdisplay == primary_mode.hdisplay &&
                conn->modes[m].vdisplay == primary_mode.vdisplay) {
                secondary_mode = conn->modes[m];
                break;
            }
        }
        if (secondary_mode.hdisplay == 0) {
            secondary_mode = conn->modes[0];
            fprintf(stderr, "drm_mirror: no exact mode match, using %dx%d\n",
                    secondary_mode.hdisplay, secondary_mode.vdisplay);
        }

        for (int e = 0; e < conn->count_encoders && !secondary_crtc_id; e++) {
            drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoders[e]);
            if (!enc) continue;
            for (int c = 0; c < res->count_crtcs && !secondary_crtc_id; c++) {
                if (res->crtcs[c] != primary_crtc_id && (enc->possible_crtcs & (1 << c)))
                    secondary_crtc_id = res->crtcs[c];
            }
            drmModeFreeEncoder(enc);
        }

        if (secondary_crtc_id) secondary_conn_id = res->connectors[i];
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);

    if (!secondary_conn_id || !secondary_crtc_id) {
        fprintf(stderr, "drm_mirror: no secondary connector/CRTC available\n");
        return;
    }
    fprintf(stderr, "drm_mirror: secondary CRTC id=%u connector id=%u mode=%dx%d\n",
            secondary_crtc_id, secondary_conn_id, secondary_mode.hdisplay, secondary_mode.vdisplay);

    int ret = drmModeSetCrtc(fd, secondary_crtc_id, last_fb, 0, 0,
                             &secondary_conn_id, 1, &secondary_mode);
    if (ret != 0) {
        fprintf(stderr, "drm_mirror: drmModeSetCrtc failed: %d (errno %d)\n", ret, errno);
        return;
    }
    fprintf(stderr, "drm_mirror: secondary display initialized\n");

    while (g_running) {
        drmModeCrtcPtr curr = drmModeGetCrtc(fd, primary_crtc_id);
        if (curr) {
            if (curr->buffer_id && curr->buffer_id != last_fb) {
                last_fb = curr->buffer_id;
                // EBUSY means previous flip still pending - safe to ignore
                drmModePageFlip(fd, secondary_crtc_id, last_fb, 0, nullptr);
            }
            drmModeFreeCrtc(curr);
        }
        usleep(8000); // 125Hz poll, well above 60Hz display rate
    }
}

void StartDRMMirror() {
    g_running = true;
    g_thread = std::thread(MirrorLoop);
}

void StopDRMMirror() {
    g_running = false;
    if (g_thread.joinable()) g_thread.join();
}

#else

void StartDRMMirror() {}
void StopDRMMirror() {}

#endif
