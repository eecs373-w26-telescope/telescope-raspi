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

static void LogFBInfo(int fd, uint32_t fb_id) {
    drmModeFBPtr fb = drmModeGetFB(fd, fb_id);
    if (fb) {
        LOG("drm_mirror: FB %u info: %dx%d, bpp %d, depth %d, pitch %d\n", fb_id, fb->width, fb->height, fb->bpp, fb->depth, fb->pitch);
        drmModeFreeFB(fb);
    } else {
        LOG("drm_mirror: could not get info for FB %u (errno %d)\n", fb_id, errno);
    }
}

static uint32_t FindPrimaryPlane(int fd, int crtc_index) {
    drmModePlaneResPtr planes = drmModeGetPlaneResources(fd);
    uint32_t result = 0;
    if (!planes) return 0;
    for (uint32_t i = 0; i < planes->count_planes && result == 0; i++) {
        drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[i]);
        if (plane) {
            if (plane->possible_crtcs & (1 << crtc_index)) {
                uint32_t type_id = GetPropertyId(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE, "type");
                drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
                if (props) {
                    for (uint32_t j = 0; j < props->count_props; j++) {
                        if (props->props[j] == type_id && props->prop_values[j] == 1) result = plane->plane_id;
                    }
                    drmModeFreeObjectProperties(props);
                }
            }
            drmModeFreePlane(plane);
        }
    }
    drmModeFreePlaneResources(planes);
    return result;
}

struct SecondaryDisplay {
    uint32_t connector_id;
    uint32_t crtc_id;
    int crtc_index;
    uint32_t primary_plane_id;
    drmModeModeInfo mode;
};

static void MirrorLoop() {
    g_log = fopen("/tmp/drm_mirror.log", "w");
    LOG("--- drm_mirror: session start ---\n");
    int fd = -1;
    for (int i = 0; i < 100 && fd < 0; i++) {
        fd = FindDRMMasterFd();
        if (fd < 0) usleep(100000);
    }
    if (fd < 0) { LOG("drm_mirror: FATAL: master FD not found\n"); return; }
    
    drmModeResPtr res = drmModeGetResources(fd);
    if (!res) { LOG("drm_mirror: FATAL: get resources failed\n"); return; }
    
    uint32_t primary_crtc_id = 0;
    uint32_t last_fb = 0;
    drmModeModeInfo primary_mode = {};
    for (int i = 0; i < res->count_crtcs; i++) {
        drmModeCrtcPtr c = drmModeGetCrtc(fd, res->crtcs[i]);
        if (c && c->buffer_id != 0) {
            primary_crtc_id = res->crtcs[i];
            last_fb = c->buffer_id;
            primary_mode = c->mode;
            LOG("drm_mirror: primary CRTC %u, fb %u\n", primary_crtc_id, last_fb);
            LogFBInfo(fd, last_fb);
            drmModeFreeCrtc(c);
            break;
        }
        if (c) drmModeFreeCrtc(c);
    }
    
    if (!primary_crtc_id) { LOG("drm_mirror: FATAL: no primary CRTC\n"); drmModeFreeResources(res); return; }

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
                if (enc && enc->crtc_id == primary_crtc_id) is_primary = true;
                if (enc) drmModeFreeEncoder(enc);
            }
            if (!is_primary) {
                uint32_t chosen_crtc_id = 0;
                int chosen_index = -1;
                for (int e = 0; e < conn->count_encoders && !chosen_crtc_id; e++) {
                    drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoders[e]);
                    if (!enc) continue;
                    for (int c = 0; c < res->count_crtcs && !chosen_crtc_id; c++) {
                        uint32_t cand = res->crtcs[c];
                        bool used = false;
                        for (uint32_t u : used_crtcs) if (u == cand) used = true;
                        if (!used && (enc->possible_crtcs & (1 << c))) { chosen_crtc_id = cand; chosen_index = c; }
                    }
                    drmModeFreeEncoder(enc);
                }
                if (chosen_crtc_id) {
                    uint32_t plane_id = FindPrimaryPlane(fd, chosen_index);
                    drmModeModeInfo mode = conn->modes[0];
                    for (int m = 0; m < conn->count_modes; m++) {
                        if (conn->modes[m].hdisplay == primary_mode.hdisplay &&
                            conn->modes[m].vdisplay == primary_mode.vdisplay) { mode = conn->modes[m]; break; }
                    }
                    secondary_displays.push_back({res->connectors[i], chosen_crtc_id, chosen_index, plane_id, mode});
                    used_crtcs.push_back(chosen_crtc_id);
                    LOG("drm_mirror: added secondary CRTC %u (index %d, plane %u)\n", chosen_crtc_id, chosen_index, plane_id);
                }
            }
        }
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(res);

    for (auto& disp : secondary_displays) {
        // Full reset
        drmModeSetCrtc(fd, disp.crtc_id, 0, 0, 0, nullptr, 0, nullptr);
        int ret = drmModeSetCrtc(fd, disp.crtc_id, last_fb, 0, 0, &disp.connector_id, 1, &disp.mode);
        LOG("drm_mirror: SetCrtc %u result %d\n", disp.crtc_id, ret);
        
        if (disp.primary_plane_id) {
            // Explicitly set the plane to the framebuffer
            ret = drmModeSetPlane(fd, disp.primary_plane_id, disp.crtc_id, last_fb, 0,
                                  0, 0, disp.mode.hdisplay, disp.mode.vdisplay,
                                  0, 0, disp.mode.hdisplay << 16, disp.mode.vdisplay << 16);
            LOG("drm_mirror: SetPlane %u result %d\n", disp.primary_plane_id, ret);
            
            // Try rotation only if Plane is active
            if (ret == 0) {
                uint32_t prop_id = GetPropertyId(fd, disp.primary_plane_id, DRM_MODE_OBJECT_PLANE, "rotation");
                if (prop_id) {
                    uint32_t val = (!DISPLAY_FLIP) ? (1 << 2) : (1 << 0);
                    drmModeObjectSetProperty(fd, disp.primary_plane_id, DRM_MODE_OBJECT_PLANE, prop_id, val);
                }
            }
        }
    }

    while (g_running) {
        drmModeCrtcPtr curr = drmModeGetCrtc(fd, primary_crtc_id);
        if (curr) {
            if (curr->buffer_id != 0 && curr->buffer_id != last_fb) {
                last_fb = curr->buffer_id;
                for (auto& disp : secondary_displays) {
                    int ret = drmModePageFlip(fd, disp.crtc_id, last_fb, 0, nullptr);
                    if (ret != 0 && errno != EBUSY) {
                         drmModeSetCrtc(fd, disp.crtc_id, last_fb, 0, 0, &disp.connector_id, 1, &disp.mode);
                    }
                }
            }
            drmModeFreeCrtc(curr);
        }
        usleep(8000);
    }
    if (g_log) fclose(g_log);
}

void StartDRMMirror() { g_running = true; g_thread = std::thread(MirrorLoop); }
void StopDRMMirror() { g_running = false; if (g_thread.joinable()) g_thread.join(); }
#else
void StartDRMMirror() {}
void StopDRMMirror() {}
#endif
