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
#include <sys/ioctl.h>
#include <linux/kd.h>
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

static void LogPlaneProperties(int fd, uint32_t plane_id) {
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!props) return;
    LOG("drm_mirror: properties for plane %u:\n", plane_id);
    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[i]);
        if (prop) {
            LOG("  - %s (id %u, value %llu)\n", prop->name, prop->prop_id, props->prop_values[i]);
            if (prop->flags & DRM_MODE_PROP_ENUM) {
                for (int j = 0; j < prop->count_enums; j++) {
                    LOG("    * enum %d: %s (value %llu)\n", j, prop->enums[j].name, prop->enums[j].value);
                }
            }
            if (prop->flags & DRM_MODE_PROP_BITMASK) {
                for (int j = 0; j < prop->count_enums; j++) {
                    LOG("    * bit %d: %s (value 0x%llx)\n", j, prop->enums[j].name, (1ULL << prop->enums[j].value));
                }
            }
            drmModeFreeProperty(prop);
        }
    }
    drmModeFreeObjectProperties(props);
}

struct SecondaryDisplay {
    uint32_t connector_id;
    uint32_t crtc_id;
    int crtc_index;
    uint32_t plane_id;
    drmModeModeInfo mode;
};

static void MirrorLoop() {
    g_log = fopen("/tmp/drm_mirror.log", "w");
    LOG("--- drm_mirror: Diagnostic & Stability Session ---\n");
    
    sleep(2);
    int fd = -1;
    for (int i = 0; i < 50 && fd < 0; i++) {
        fd = FindDRMMasterFd();
        if (fd < 0) usleep(100000);
    }
    if (fd < 0) { LOG("drm_mirror: FATAL: Master FD not found\n"); return; }
    
    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmModeResPtr res = drmModeGetResources(fd);
    if (!res) return;
    
    uint32_t primary_crtc_id = 0, last_fb = 0;
    uint32_t fb_w = 0, fb_h = 0;
    for (int i = 0; i < res->count_crtcs; i++) {
        drmModeCrtcPtr c = drmModeGetCrtc(fd, res->crtcs[i]);
        if (c && c->buffer_id != 0) {
            primary_crtc_id = res->crtcs[i];
            last_fb = c->buffer_id;
            drmModeFBPtr fb = drmModeGetFB(fd, last_fb);
            if (fb) { fb_w = fb->width; fb_h = fb->height; drmModeFreeFB(fb); }
            LOG("drm_mirror: primary CRTC %u active (fb %u, %dx%d)\n", primary_crtc_id, last_fb, fb_w, fb_h);
            drmModeFreeCrtc(c);
            break;
        }
        if (c) drmModeFreeCrtc(c);
    }

    std::vector<SecondaryDisplay> secondary_displays;
    drmModePlaneResPtr planes = drmModeGetPlaneResources(fd);

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnectorPtr conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn || conn->connection != DRM_MODE_CONNECTED) { if (conn) drmModeFreeConnector(conn); continue; }
        
        bool is_primary = false;
        if (conn->encoder_id) {
            drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoder_id);
            if (enc && enc->crtc_id == primary_crtc_id) is_primary = true;
            if (enc) drmModeFreeEncoder(enc);
        }

        if (!is_primary) {
            uint32_t chosen_crtc = 0;
            int chosen_idx = -1;
            for (int c = 0; c < res->count_crtcs && !chosen_crtc; c++) {
                if (res->crtcs[c] == primary_crtc_id) continue;
                for (int e = 0; e < conn->count_encoders && !chosen_crtc; e++) {
                    drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoders[e]);
                    if (enc && (enc->possible_crtcs & (1 << c))) { chosen_crtc = res->crtcs[c]; chosen_idx = c; }
                    if (enc) drmModeFreeEncoder(enc);
                }
            }

            if (chosen_crtc) {
                uint32_t pplane = 0;
                for (uint32_t p = 0; p < planes->count_planes && pplane == 0; p++) {
                    drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[p]);
                    if (plane && (plane->possible_crtcs & (1 << chosen_idx))) {
                        uint32_t tid = GetPropertyId(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE, "type");
                        drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
                        for (uint32_t j = 0; j < props->count_props; j++) 
                            if (props->props[j] == tid && props->prop_values[j] == 1) pplane = plane->plane_id;
                        drmModeFreeObjectProperties(props);
                    }
                    if (plane) drmModeFreePlane(plane);
                }
                secondary_displays.push_back({res->connectors[i], chosen_crtc, chosen_idx, pplane, conn->modes[0]});
            }
        }
        drmModeFreeConnector(conn);
    }

    for (auto& disp : secondary_displays) {
        LOG("drm_mirror: setting up secondary display (CRTC %u, Plane %u)\n", disp.crtc_id, disp.plane_id);
        LogPlaneProperties(fd, disp.plane_id);
        
        // Initial force
        drmModeSetCrtc(fd, disp.crtc_id, last_fb, 0, 0, &disp.connector_id, 1, &disp.mode);
        
        if (disp.plane_id) {
            uint32_t rot_id = GetPropertyId(fd, disp.plane_id, DRM_MODE_OBJECT_PLANE, "rotation");
            if (rot_id) {
                // We'll set this once and leave it alone to avoid flickering
                // Try 4 (Rotate 180) as it's the most common hardware flip
                int r = drmModeObjectSetProperty(fd, disp.plane_id, DRM_MODE_OBJECT_PLANE, rot_id, 4);
                LOG("drm_mirror: set rotation (4) res %d\n", r);
            }
            drmModeSetPlane(fd, disp.plane_id, disp.crtc_id, last_fb, 0,
                            0, 0, disp.mode.hdisplay, disp.mode.vdisplay,
                            0, 0, fb_w << 16, fb_h << 16);
        }
        
        // Disable overlay planes on THIS CRTC only to stop bleeding
        if (planes) {
            for (uint32_t p = 0; p < planes->count_planes; p++) {
                drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[p]);
                if (plane) {
                    if (plane->crtc_id == disp.crtc_id && plane->plane_id != disp.plane_id) {
                        drmModeSetPlane(fd, plane->plane_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                        LOG("drm_mirror: disabled overlay plane %u on secondary CRTC\n", plane->plane_id);
                    }
                    drmModeFreePlane(plane);
                }
            }
        }
    }

    LOG("drm_mirror: main loop started (PageFlip updates)\n");
    while (g_running) {
        drmModeCrtcPtr curr = drmModeGetCrtc(fd, primary_crtc_id);
        if (curr) {
            if (curr->buffer_id != 0 && curr->buffer_id != last_fb) {
                last_fb = curr->buffer_id;
                for (auto& disp : secondary_displays) {
                    // PageFlip is hardware-synced and shouldn't flicker
                    if (drmModePageFlip(fd, disp.crtc_id, last_fb, 0, nullptr) != 0) {
                        // Fallback only if PageFlip is totally broken
                        if (disp.plane_id) {
                            drmModeSetPlane(fd, disp.plane_id, disp.crtc_id, last_fb, 0,
                                            0, 0, disp.mode.hdisplay, disp.mode.vdisplay,
                                            0, 0, fb_w << 16, fb_h << 16);
                        }
                    }
                }
            }
            drmModeFreeCrtc(curr);
        }
        // Poll slower to let the GPU breathe
        usleep(10000); 
    }
    
    if (planes) drmModeFreePlaneResources(planes);
    if (g_log) fclose(g_log);
}

void StartDRMMirror() { g_running = true; g_thread = std::thread(MirrorLoop); }
void StopDRMMirror() { g_running = false; if (g_thread.joinable()) g_thread.join(); }
#else
void StartDRMMirror() {}
void StopDRMMirror() {}
#endif
