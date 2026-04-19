#include "drm_mirror.h"
#include "globals.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <cerrno>

#ifdef HAVE_LIBDRM

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static std::atomic<bool> g_running{false};
static std::thread g_thread;
static FILE* g_log = nullptr;

#define LOG(...) if (g_log) { fprintf(g_log, __VA_ARGS__); fflush(g_log); }

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

static uint32_t GetPropertyId(int fd, uint32_t object_id, uint32_t object_type, const char* prop_name) {
    uint32_t result = 0;
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, object_id, object_type);
    if (!props) return 0;

    for (uint32_t i = 0; i < props->count_props && result == 0; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[i]);
        if (prop) {
            if (strcmp(prop->name, prop_name) == 0) result = prop->prop_id;
            drmModeFreeProperty(prop);
        }
    }

    drmModeFreeObjectProperties(props);
    return result;
}

static void SetRotation(int fd, uint32_t crtc_id, int crtc_index, bool flip) {
    drmModePlaneResPtr planes = drmModeGetPlaneResources(fd);
    if (!planes) {
        LOG("drm_mirror: could not get plane resources\n");
        return;
    }

    LOG("drm_mirror: searching for plane for CRTC %u (index %d)\n", crtc_id, crtc_index);
    bool found = false;
    for (uint32_t i = 0; i < planes->count_planes && !found; i++) {
        drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[i]);
        if (plane) {
            // A plane is compatible if (possible_crtcs & (1 << crtc_index)) is non-zero
            if (plane->possible_crtcs & (1 << crtc_index)) {
                uint32_t prop_id = GetPropertyId(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE, "rotation");
                if (prop_id) {
                    uint32_t value = flip ? (1 << 2) : (1 << 0);
                    int ret = drmModeObjectSetProperty(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE, prop_id, value);
                    if (ret == 0) {
                        LOG("drm_mirror: set rotation to %u on plane %u (compatible with CRTC %u)\n", value, plane->plane_id, crtc_id);
                        found = true;
                    } else {
                        LOG("drm_mirror: failed to set rotation on plane %u: %d\n", plane->plane_id, errno);
                    }
                }
            }
            drmModeFreePlane(plane);
        }
    }
    if (!found) LOG("drm_mirror: no suitable plane with rotation property found for CRTC %u\n", crtc_id);
    drmModeFreePlaneResources(planes);
}

struct SecondaryDisplay {
    uint32_t connector_id;
    uint32_t crtc_id;
    int crtc_index;
    drmModeModeInfo mode;
};

static void MirrorLoop() {
    g_log = fopen("/tmp/drm_mirror.log", "w");
    LOG("drm_mirror: starting MirrorLoop\n");

    int fd = -1;
    for (int i = 0; i < 100 && fd < 0; i++) {
        fd = FindDRMMasterFd();
        if (fd < 0) usleep(100000);
    }

    if (fd < 0) {
        LOG("drm_mirror: could not find DRM master FD.\n");
        return;
    }
    LOG("drm_mirror: using master FD %d\n", fd);

    drmModeResPtr res = drmModeGetResources(fd);
    if (!res) {
        LOG("drm_mirror: failed to get DRM resources\n");
        return;
    }

    uint32_t primary_crtc_id = 0;
    drmModeModeInfo primary_mode = {};
    uint32_t last_fb = 0;

    for (int i = 0; i < res->count_crtcs; i++) {
        drmModeCrtcPtr c = drmModeGetCrtc(fd, res->crtcs[i]);
        if (c && c->mode_valid && c->buffer_id != 0) {
            primary_crtc_id = res->crtcs[i];
            primary_mode = c->mode;
            last_fb = c->buffer_id;
            LOG("drm_mirror: primary CRTC %u, fb %u, mode %dx%d\n", primary_crtc_id, last_fb, primary_mode.hdisplay, primary_mode.vdisplay);
            drmModeFreeCrtc(c);
            break;
        }
        if (c) drmModeFreeCrtc(c);
    }

    if (!primary_crtc_id) {
        LOG("drm_mirror: no active primary CRTC found\n");
        drmModeFreeResources(res);
        return;
    }

    std::vector<SecondaryDisplay> secondary_displays;
    std::vector<uint32_t> used_crtcs;
    used_crtcs.push_back(primary_crtc_id);

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnectorPtr conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            bool is_primary = false;
            if (conn->encoder_id) {
                drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoder_id);
                if (enc) {
                    if (enc->crtc_id == primary_crtc_id) is_primary = true;
                    drmModeFreeEncoder(enc);
                }
            }

            if (!is_primary) {
                uint32_t chosen_crtc_id = 0;
                int chosen_crtc_index = -1;
                for (int e = 0; e < conn->count_encoders && !chosen_crtc_id; e++) {
                    drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoders[e]);
                    if (!enc) continue;
                    for (int c = 0; c < res->count_crtcs && !chosen_crtc_id; c++) {
                        uint32_t cand = res->crtcs[c];
                        bool used = false;
                        for (uint32_t u : used_crtcs) if (u == cand) used = true;
                        if (!used && (enc->possible_crtcs & (1 << c))) {
                            chosen_crtc_id = cand;
                            chosen_crtc_index = c;
                        }
                    }
                    drmModeFreeEncoder(enc);
                }

                if (chosen_crtc_id) {
                    drmModeModeInfo mode = conn->modes[0];
                    for (int m = 0; m < conn->count_modes; m++) {
                        if (conn->modes[m].hdisplay == primary_mode.hdisplay &&
                            conn->modes[m].vdisplay == primary_mode.vdisplay) {
                            mode = conn->modes[m];
                            break;
                        }
                    }
                    secondary_displays.push_back({res->connectors[i], chosen_crtc_id, chosen_crtc_index, mode});
                    used_crtcs.push_back(chosen_crtc_id);
                    LOG("drm_mirror: secondary connector %u -> CRTC %u (index %d), mode %dx%d\n", res->connectors[i], chosen_crtc_id, chosen_crtc_index, mode.hdisplay, mode.vdisplay);
                }
            }
        }
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);

    if (secondary_displays.empty()) {
        LOG("drm_mirror: no secondary displays found\n");
        return;
    }

    for (auto it = secondary_displays.begin(); it != secondary_displays.end(); ) {
        int ret = drmModeSetCrtc(fd, it->crtc_id, last_fb, 0, 0, &it->connector_id, 1, &it->mode);
        if (ret != 0) {
            LOG("drm_mirror: drmModeSetCrtc failed for CRTC %u: %d\n", it->crtc_id, errno);
            it = secondary_displays.erase(it);
        } else {
            LOG("drm_mirror: CRTC %u active\n", it->crtc_id);
            SetRotation(fd, it->crtc_id, it->crtc_index, !DISPLAY_FLIP);
            ++it;
        }
    }

    int flip_count = 0;
    while (g_running) {
        drmModeCrtcPtr curr = drmModeGetCrtc(fd, primary_crtc_id);
        if (curr) {
            if (curr->buffer_id != 0 && curr->buffer_id != last_fb) {
                last_fb = curr->buffer_id;
                for (auto& disp : secondary_displays) {
                    int ret = drmModePageFlip(fd, disp.crtc_id, last_fb, 0, nullptr);
                    if (ret != 0 && errno != EBUSY) {
                        LOG("drm_mirror: flip failed on CRTC %u: %d\n", disp.crtc_id, errno);
                        drmModeSetCrtc(fd, disp.crtc_id, last_fb, 0, 0, &disp.connector_id, 1, &disp.mode);
                    }
                }
                if (flip_count < 10) {
                    LOG("drm_mirror: flip %d successful (fb %u)\n", flip_count++, last_fb);
                }
            }
            drmModeFreeCrtc(curr);
        }
        usleep(8000);
    }

    if (g_log) {
        fclose(g_log);
        g_log = nullptr;
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
