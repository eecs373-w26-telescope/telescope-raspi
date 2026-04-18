#include "drm_mirror.h"

#ifdef HAVE_LIBDRM

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <thread>
#include <atomic>

static std::atomic<bool> g_running{false};
static std::thread g_thread;

static void MirrorLoop() {
    int fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "drm_mirror: failed to open /dev/dri/card0\n");
        return;
    }

    drmModeResPtr res = drmModeGetResources(fd);
    if (!res) { close(fd); return; }

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
            drmModeFreeCrtc(c);
            break;
        }
        if (c) drmModeFreeCrtc(c);
    }

    if (!primary_crtc_id) {
        fprintf(stderr, "drm_mirror: no active primary CRTC found\n");
        drmModeFreeResources(res);
        close(fd);
        return;
    }

    // Find a secondary connected connector not driving the primary CRTC
    uint32_t secondary_conn_id = 0;
    uint32_t secondary_crtc_id = 0;
    drmModeModeInfo secondary_mode = {};
    bool mode_matches = false;

    for (int i = 0; i < res->count_connectors && !secondary_conn_id; i++) {
        drmModeConnectorPtr conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn || conn->connection != DRM_MODE_CONNECTED || !conn->count_modes) {
            if (conn) drmModeFreeConnector(conn);
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
        if (is_primary) { drmModeFreeConnector(conn); continue; }

        // Prefer a mode matching the primary's resolution for framebuffer compatibility
        for (int m = 0; m < conn->count_modes; m++) {
            if (conn->modes[m].hdisplay == primary_mode.hdisplay &&
                conn->modes[m].vdisplay == primary_mode.vdisplay) {
                secondary_mode = conn->modes[m];
                mode_matches = true;
                break;
            }
        }
        if (!mode_matches) secondary_mode = conn->modes[0];

        // Find a CRTC this connector's encoder can drive (not primary_crtc_id)
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
        close(fd);
        return;
    }

    if (drmModeSetCrtc(fd, secondary_crtc_id, last_fb, 0, 0,
                       &secondary_conn_id, 1, &secondary_mode) != 0) {
        fprintf(stderr, "drm_mirror: initial drmModeSetCrtc failed\n");
        close(fd);
        return;
    }

    if (!mode_matches) {
        // Secondary mode resolution differs from primary's framebuffer - static first frame only
        fprintf(stderr, "drm_mirror: secondary mode resolution mismatch, static mirror only\n");
        close(fd);
        return;
    }

    // Poll primary CRTC for framebuffer swaps and mirror them to secondary
    while (g_running) {
        drmModeCrtcPtr curr = drmModeGetCrtc(fd, primary_crtc_id);
        if (curr) {
            if (curr->buffer_id && curr->buffer_id != last_fb) {
                last_fb = curr->buffer_id;
                // EBUSY means a previous flip is still pending - safe to skip
                drmModePageFlip(fd, secondary_crtc_id, last_fb, 0, nullptr);
            }
            drmModeFreeCrtc(curr);
        }
        usleep(8000); // 125Hz poll, well above 60Hz display rate
    }

    close(fd);
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
