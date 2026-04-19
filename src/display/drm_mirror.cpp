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

struct SecondaryDisplay {
    uint32_t connector_id;
    uint32_t crtc_id;
    int crtc_index;
    uint32_t primary_plane_id;
    drmModeModeInfo mode;
};

static void MirrorLoop() {
    g_log = fopen("/tmp/drm_mirror.log", "w");
    LOG("--- drm_mirror: Stable Mirror Session ---\n");
    
    // 1. Wait for system to settle
    sleep(3);

    // 2. Aggressively unbind console from kernel
    system("echo 0 > /sys/class/vtconsole/vtcon1/bind 2>/dev/null");
    system("echo 0 > /sys/class/vtconsole/vtcon0/bind 2>/dev/null");

    int fd = -1;
    for (int i = 0; i < 100 && fd < 0; i++) {
        fd = FindDRMMasterFd();
        if (fd < 0) usleep(100000);
    }
    if (fd < 0) { LOG("drm_mirror: FATAL: master FD not found\n"); return; }
    
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
            LOG("drm_mirror: primary CRTC %u active (fb %u)\n", primary_crtc_id, last_fb);
            drmModeFreeCrtc(c);
            break;
        }
        if (c) drmModeFreeCrtc(c);
    }
    if (!primary_crtc_id) return;

    std::vector<SecondaryDisplay> secondary_displays;
    std::vector<uint32_t> used_crtcs;
    used_crtcs.push_back(primary_crtc_id);
    drmModePlaneResPtr planes = drmModeGetPlaneResources(fd);

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
                    uint32_t primary_plane = 0;
                    if (planes) {
                        for (uint32_t p = 0; p < planes->count_planes; p++) {
                            drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[p]);
                            if (plane && (plane->possible_crtcs & (1 << chosen_index))) {
                                uint32_t type_id = GetPropertyId(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE, "type");
                                drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
                                if (props) {
                                    for (uint32_t j = 0; j < props->count_props; j++) {
                                        if (props->props[j] == type_id && props->prop_values[j] == 1) primary_plane = plane->plane_id;
                                    }
                                    drmModeFreeObjectProperties(props);
                                }
                            }
                            if (plane) drmModeFreePlane(plane);
                        }
                    }
                    secondary_displays.push_back({res->connectors[i], chosen_crtc_id, chosen_index, primary_plane, conn->modes[0]});
                    used_crtcs.push_back(chosen_crtc_id);
                    LOG("drm_mirror: secondary CRTC %u added\n", chosen_crtc_id);
                }
            }
        }
        drmModeFreeConnector(conn);
    }

    // --- INITIAL SETUP ONLY (Do not repeat in loop) ---
    for (auto& disp : secondary_displays) {
        // Set the mode once
        drmModeSetCrtc(fd, disp.crtc_id, last_fb, 0, 0, &disp.connector_id, 1, &disp.mode);
        
        if (disp.primary_plane_id) {
            // Apply rotation once
            uint32_t rot_prop = GetPropertyId(fd, disp.primary_plane_id, DRM_MODE_OBJECT_PLANE, "rotation");
            if (rot_prop) {
                // Bitmask: bit 0=0deg, bit 1=90deg, bit 2=180deg, bit 3=270deg, bit 4=reflect-x, bit 5=reflect-y
                // If DISPLAY_FLIP is 1 (primary is flipped), secondary should be 0 (identity) to match?
                // Actually, if they are mirrored physically, they should both be the same? 
                // Let's try 180 degrees (bit 2 = 4) if DISPLAY_FLIP is NOT set.
                uint32_t val = (DISPLAY_FLIP == 0) ? 4 : 1; 
                drmModeObjectSetProperty(fd, disp.primary_plane_id, DRM_MODE_OBJECT_PLANE, rot_prop, val);
                LOG("drm_mirror: applied rotation %u to plane %u\n", val, disp.primary_plane_id);
            }
            
            // Set initial scaling
            drmModeSetPlane(fd, disp.primary_plane_id, disp.crtc_id, last_fb, 0,
                            0, 0, disp.mode.hdisplay, disp.mode.vdisplay,
                            0, 0, fb_w << 16, fb_h << 16);
        }
        
        // Wipe other planes on this CRTC ONLY
        if (planes) {
            for (uint32_t p = 0; p < planes->count_planes; p++) {
                drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[p]);
                if (plane) {
                    if ((plane->possible_crtcs & (1 << disp.crtc_index)) && plane->plane_id != disp.primary_plane_id) {
                        drmModeSetPlane(fd, plane->plane_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                    }
                    drmModeFreePlane(plane);
                }
            }
        }
    }

    LOG("drm_mirror: stabilized main loop started\n");
    while (g_running) {
        drmModeCrtcPtr curr = drmModeGetCrtc(fd, primary_crtc_id);
        if (curr) {
            if (curr->buffer_id != 0 && curr->buffer_id != last_fb) {
                last_fb = curr->buffer_id;
                for (auto& disp : secondary_displays) {
                    // Use PageFlip for smooth sync
                    if (drmModePageFlip(fd, disp.crtc_id, last_fb, 0, nullptr) != 0) {
                        // Fallback to SetPlane if Flip fails (e.g. format issues)
                        if (disp.primary_plane_id) {
                            drmModeSetPlane(fd, disp.primary_plane_id, disp.crtc_id, last_fb, 0,
                                            0, 0, disp.mode.hdisplay, disp.mode.vdisplay,
                                            0, 0, fb_w << 16, fb_h << 16);
                        }
                    }
                }
            }
            drmModeFreeCrtc(curr);
        }
        // Poll slower to reduce CPU and bus contention
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
