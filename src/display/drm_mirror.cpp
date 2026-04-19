#include "drm_mirror.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

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

struct SecondaryDisplay {
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
};

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

    std::vector<SecondaryDisplay> secondary_displays;
    std::vector<uint32_t> used_crtcs;
    used_crtcs.push_back(primary_crtc_id);

    // Find all secondary connected connectors not driving the primary CRTC
    for (int i = 0; i < res->count_connectors; i++) {
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

        drmModeModeInfo best_mode = {};
        // Prefer a mode matching the primary's resolution; fall back to first mode.
        for (int m = 0; m < conn->count_modes; m++) {
            if (conn->modes[m].hdisplay == primary_mode.hdisplay &&
                conn->modes[m].vdisplay == primary_mode.vdisplay) {
                best_mode = conn->modes[m];
                break;
            }
        }
        if (best_mode.hdisplay == 0) {
            best_mode = conn->modes[0];
            fprintf(stderr, "drm_mirror: no exact mode match for connector %u, using %dx%d\n",
                    res->connectors[i], best_mode.hdisplay, best_mode.vdisplay);
        }

        uint32_t chosen_crtc_id = 0;
        for (int e = 0; e < conn->count_encoders && !chosen_crtc_id; e++) {
            drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoders[e]);
            if (!enc) continue;
            for (int c = 0; c < res->count_crtcs && !chosen_crtc_id; c++) {
                uint32_t candidate_crtc_id = res->crtcs[c];
                bool already_used = false;
                for (uint32_t used : used_crtcs) if (used == candidate_crtc_id) already_used = true;

                if (!already_used && (enc->possible_crtcs & (1 << c))) {
                    chosen_crtc_id = candidate_crtc_id;
                }
            }
            drmModeFreeEncoder(enc);
        }

        if (chosen_crtc_id) {
            secondary_displays.push_back({res->connectors[i], chosen_crtc_id, best_mode});
            used_crtcs.push_back(chosen_crtc_id);
            fprintf(stderr, "drm_mirror: adding secondary display connector=%u crtc=%u mode=%dx%d\n",
                    res->connectors[i], chosen_crtc_id, best_mode.hdisplay, best_mode.vdisplay);
        } else {
            fprintf(stderr, "drm_mirror: could not find suitable CRTC for connector %u\n", res->connectors[i]);
        }
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);

    if (secondary_displays.empty()) {
        fprintf(stderr, "drm_mirror: no secondary displays found\n");
        return;
    }

    // Initialize all secondary displays with the current framebuffer
    for (auto it = secondary_displays.begin(); it != secondary_displays.end(); ) {
        int ret = drmModeSetCrtc(fd, it->crtc_id, last_fb, 0, 0,
                                 &it->connector_id, 1, &it->mode);
        if (ret != 0) {
            fprintf(stderr, "drm_mirror: drmModeSetCrtc failed for crtc %u: %d (errno %d)\n", it->crtc_id, ret, errno);
            it = secondary_displays.erase(it);
        } else {
            fprintf(stderr, "drm_mirror: secondary crtc %u initialized\n", it->crtc_id);
            ++it;
        }
    }

    while (g_running) {
        drmModeCrtcPtr curr = drmModeGetCrtc(fd, primary_crtc_id);
        if (curr) {
            if (curr->buffer_id && curr->buffer_id != last_fb) {
                last_fb = curr->buffer_id;
                for (auto& disp : secondary_displays) {
                    // EBUSY means previous flip still pending - safe to ignore
                    drmModePageFlip(fd, disp.crtc_id, last_fb, 0, nullptr);
                }
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
