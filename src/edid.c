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
#include <inttypes.h>

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
static Bool                 verbose = False;

const unsigned char edid_v1_header[] = {0x00, 0xff, 0x0ff, 0xff, 0xff, 0xff, 0xff, 0x00};

typedef struct {
    uint8_t                 header[8];
    uint8_t                 vendor[2];
    uint8_t                 product[2];
    uint8_t                 serial[4];
    uint8_t                 manufactured_week;
    uint8_t                 manufactured_year;
    uint8_t                 edid_version;
    uint8_t                 edid_revision;
    uint8_t                 display_params[5];
    uint8_t                 color_characteristics[10];
    uint8_t                 established_timings[3];
    uint16_t                standard_timings[8];
} edid_t;

static char*
parse_vendor(edid_t *edid) {
    static char buff[4];
    unsigned short h = COMBINE_HI_8LO(edid->vendor[0], edid->vendor[1]);
    buff[0] = ((h >> 0x0a) & 0x1f) + 'A' - 1;
    buff[1] = ((h >> 0x05) & 0x1f) + 'A' - 1;
    buff[2] = ((h >> 0x00) & 0x1f) + 'A' - 1;
    buff[3] = 0;
    return buff;
}

static char*
parse_product(edid_t *edid) {
    static char buff[5];
    uint16_t product =
          (edid->product[1] << 0x08)
        | (edid->product[0] << 0x00);
    snprintf(buff, sizeof(buff), "%04x", product);
    return buff;
}

static char*
parse_serial(edid_t *edid) {
    static char buff[9];
    uint32_t serial =
          (edid->serial[3] << 0x18)
        | (edid->serial[2] << 0x10)
        | (edid->serial[1] << 0x08)
        | (edid->serial[0] << 0x00);
    snprintf(buff, sizeof(buff), "%08x", serial);
    return buff;
}

static void
parse_edid(unsigned char *buffer, unsigned long size) {
    edid_t *edid = calloc(1, sizeof(edid_t));
    if (!edid) {
        fprintf(stderr, "OOM!\n");
        exit(1);
    }

    memcpy(edid, buffer, sizeof(edid_t));
    if (strncmp(edid->header, edid_v1_header, sizeof(edid_v1_header))) {
        printf("<corrupt EDID v1 header>");
    } else {
        if (verbose) {
            printf("\n");
            printf("  vendor : %s\n", parse_vendor(edid));
            printf("  product: %s\n", parse_product(edid));
            printf("  serial : %s\n", parse_serial(edid));
        } else {
            printf(":%s", parse_vendor(edid));
            printf(":%s", parse_product(edid));
            printf(":%s", parse_serial(edid));
        }
    }
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
                for (k = EDID_HEADER_END; k < nitems; ++k) {
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

    if (argc > 1) {
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            fprintf(stderr, "edid version 0.1\n");
            fprintf(stderr, "usage:     %s [-h|--help|-v|--verbose]\n", argv[0]);
            fprintf(stderr, "where:\n");
            fprintf(stderr, "   -h:     help\n");
            fprintf(stderr, "   -v:     be verbose\n");
            exit(0);
        } else if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--verbose")) {
            verbose = 1;
        } else {
            fprintf(stderr, "%s: invalid switch\n", argv[0]);
            exit(1);
        }
    }

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
