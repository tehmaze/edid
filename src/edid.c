#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#define COMBINE_HI_8LO(a,b) ((((unsigned)a) << 8) | (unsigned)b)
#define COMBINE_HI_4LO(a,b) ((((unsigned)a) << 4) | (unsigned)b)

#define EDID_HEADER         0x00
#define EDID_HEADER_END     0x07
#define MANUFACTURER        0x08
#define MANUFACTURER_END    0x09
#define MODEL               0x0a
#define SERIAL              0x0c

static Display              *display;
static Window               root;
static int                  screen = -1;
static XRRScreenResources   *res;
typedef unsigned char       byte;

const byte edid_v1_header[] = {0x00, 0xff, 0x0ff, 0xff, 0xff, 0xff, 0xff, 0x00};

static char*
parse_vendor(byte const* vendorid) {
    static char sign[4];
    unsigned short h;

    h = COMBINE_HI_8LO(vendorid[0], vendorid[1]);
    sign[0] = ((h >> 0x0a) & 0x1f) + 'A' - 1;
    sign[1] = ((h >> 0x05) & 0x1f) + 'A' - 1;
    sign[2] = (h   & 0x1f) + 'A' - 1;
    sign[3] = 0;
    return sign;
}

static void
parse_edid(unsigned char *edid, unsigned long size) {
    if (strncmp(edid + EDID_HEADER, edid_v1_header, EDID_HEADER_END + 1)) {
        fprintf(stderr, "<corrupt EDID v1 header>");
        return;
    }

    printf(";%s", parse_vendor(edid + MANUFACTURER));
}

static void
get_outputs(void) {
    int i, j, np;
    Atom *p;

    for (i = 0; i < res->noutput; ++i) {
        XRROutputInfo *output_info = XRRGetOutputInfo(display, res,
            res->outputs[i]);
        if (!output_info) {
            continue;
        }
        p = XRRListOutputProperties(display, res->outputs[i], &np);
        for (j = 0; j < np; ++j) {
            unsigned char *prop;
            int actual_format;
            unsigned long nitems, bytes_after;
            Atom actual_type;
            XRRPropertyInfo *propinfo;

            XRRGetOutputProperty(display, res->outputs[i], p[j], 0, 100,
                False, False, AnyPropertyType, &actual_type, &actual_format,
                &nitems, &bytes_after, &prop);
            propinfo = XRRQueryOutputProperty(display, res->outputs[i],
                p[j]);

            if (!strcmp(XGetAtomName(display, p[j]), "EDID")) {
                printf("%s:", output_info->name);
                int k;
                for (k = 0; k < nitems; ++k) {
                    printf("%02x", (unsigned char) prop[k]);
                }
                parse_edid(prop, nitems);
                printf("\n");
            }
        }
    }
}

int
main(int argc, char **argv) {
    char *display_name = NULL;
    int event_base, error_base;
    int major, minor;

    display = XOpenDisplay(display_name);
    if (display == NULL) {
        fprintf(stderr, "Could not open display %s\n",
        XDisplayName(display_name));
        exit(1);
    }

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    if (!XRRQueryExtension(display, &event_base, &error_base) ||
        !XRRQueryVersion(display, &major, &minor)) {
        fprintf(stderr, "RandR extension missing\n");
        exit(1);
    }
    if (major < 1 || (major == 1 )&& minor < 3) {
        fprintf(stderr, "RandR version %d.%d too old", major, minor);
        exit(1);
    }

    res = XRRGetScreenResources(display, root);
    if (!res) {
        fprintf(stderr, "Could not get screen resources\n");
        exit(1);
    }
    get_outputs();

    return 0;
}
