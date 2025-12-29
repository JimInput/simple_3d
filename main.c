#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define TRUE 1
#define FALSE 0

#define WIDTH 900
#define HEIGHT 600

#define FPS 60

#define BLACK 0x000000
#define WHITE 0xFFFFFF
#define RED 0xFF0000
#define GREEN 0x00FF00
#define BLUE 0x0000FF

#define VERTEX_SIZE 1

void swap(int *x, int *y) {
    *x ^= *y;
    *y ^= *x;
    *x ^= *y;
}

void draw_pixel(SDL_Surface *surface, int x, int y, uint32_t color) {
    int bpp = SDL_BYTESPERPIXEL(surface->format);
    uint8_t *p = (uint8_t *)surface->pixels + y * surface->pitch + x * bpp;
    switch (bpp) {
        case 1:
            *p = color;
            break;
        case 2:
            *(uint16_t *)p = color;
            break;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                p[0] = (color >> 16) & 0xFF;
                p[1] = (color >> 8) & 0xFF;
                p[2] = color & 0xFF;
            } else {
                p[0] = color & 0xFF;
                p[1] = (color >> 8) & 0xFF;
                p[2] = (color >> 16) & 0xFF;
            }
            break;
        case 4:
            *(uint32_t *)p = color;
            break;
    }
}

typedef struct Point3 {
    double x, y, z;
} Point3;

typedef struct Point2 {
    int x, y;
} Point2;

typedef struct FPoint2 {
    double x, y;
} FPoint2;

typedef struct Polygon {
    Point3* vs;
    Point2* wire_order;
    Point2* screen_vs;
    SDL_Rect* rect_vs;
    size_t num_vs;
    size_t num_wires;
} Polygon;

void compute_screen_pos(Point2* p2, Point3* p3) {
    double x = p3->x;
    double y = p3->y;
    double z = p3->z;
    double proj_x = x / z;
    double proj_y = y / z;

    p2->x = (int) (((proj_x+1)/2)*WIDTH);
    p2->y = (int) (((proj_y+1)/2)*HEIGHT);
}

void rotate_xz(Point3 *p, Point3 center, double angle) {
    double cx = center.x;
    double cz = center.z;

    double x = p->x - cx;
    double z = p->z - cz;

    double xr = x * cos(angle) - z * sin(angle);
    double zr = x * sin(angle) + z * cos(angle);

    p->x = xr + cx;
    p->z = zr + cz;
}

void rotate_xy(Point3 *p, Point3 center, double angle) {
    double cx = center.x;
    double cy = center.y;

    double x = p->x - cx;
    double y = p->y - cy;

    double xr = x * cos(angle) - y * sin(angle);
    double yr = x * sin(angle) + y * cos(angle);

    p->x = xr + cx;
    p->y = yr + cy;
}

void rotate_yz(Point3 *p, Point3 center, double angle) {
    double cy = center.y;
    double cz = center.z;

    double y = p->y - cy;
    double z = p->z - cz;

    double yr = y * cos(angle) - z * sin(angle);
    double zr = y * sin(angle) + z * cos(angle);

    p->y = yr + cy;
    p->z = zr + cz;
}

void draw_low_line(SDL_Surface* psurface, Point2 start, Point2 end, uint32_t color) {
    int dx = end.x - start.x;
    int dy = end.y - start.y;
    int yi = 1;
    if (dy < 0) {
        yi = -1;
        dy = -dy;
    }
    int D = 2*dy - dx;
    int y = start.y;

    for (int x = start.x; x < end.x; x++) {
        draw_pixel(psurface, x, y, color);
        if (D > 0) {
            y += yi;
            D = D + (2*(dy-dx));
        } else {
            D = D + 2*dy;
        }
    }
}

void draw_high_line(SDL_Surface* psurface, Point2 start, Point2 end, uint32_t color) {
    int dx = end.x - start.x;
    int dy = end.y - start.y;
    int xi = 1;
    if (dx < 0) {
        xi = -1;
        dx = -dx;
    }
    int D = 2*dx - dy;
    int x = start.x;

    for (int y = start.y; y < end.y; y++) {
        draw_pixel(psurface, x, y, color);
        if (D > 0) {
            x += xi;
            D = D + (2 * (dx - dy));
        } else {
            D = D + 2*dx;
        }
    }
}

void draw_line(SDL_Surface* psurface, Point2 start, Point2 end, uint32_t color) {
    if (abs(end.y-start.y) < abs(end.x-start.x)) {
        if (start.x > end.x) {
            draw_low_line(psurface, end, start, color);
        } else {
            draw_low_line(psurface, start, end, color);
        }
    } else {
        if (start.y > end.y) {
            draw_high_line(psurface, end, start, color);
        } else {
            draw_high_line(psurface, start, end, color);
        }
    }
}

int main() {
    SDL_Window *pwindow = SDL_CreateWindow("3D Shape", WIDTH, HEIGHT, 0);
    SDL_SetWindowPosition(pwindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_Surface *psurface = SDL_GetWindowSurface(pwindow);
    SDL_Rect clear_rect = (SDL_Rect) {0,0,WIDTH,HEIGHT};

    const Point3 center = (Point3) {0, 0, 2.5};

    const int num_vs = 8;
    const int num_wires = 12;
    Point3 vertices[] = {
        {-0.5,-0.5,2},
        {0.5,-0.5,2},
        {0.5,0.5,2},
        {-0.5,0.5,2},
        {-0.5,-0.5,3},
        {0.5,-0.5,3},
        {0.5,0.5,3},
        {-0.5,0.5,3}
    };

    Point2 wire_order[] = {
        {0,1},{1,2},{2,3},{3,0},
        {0,4},{1,5},{3,7},{2,6},
        {4,5},{5,6},{6,7},{7,4}
    };

    Point2 screen_vertices[num_vs];
    SDL_Rect rects[num_vs];

    Polygon square = (Polygon) {vertices, wire_order, screen_vertices, rects, num_vs, num_wires};

    for (size_t i = 0; i < square.num_vs; i++) {
        compute_screen_pos(&square.screen_vs[i], &square.vs[i]);
        square.rect_vs[i] = (SDL_Rect) {square.screen_vs[i].x, square.screen_vs[i].y, VERTEX_SIZE, VERTEX_SIZE};
    }

    int app_running = TRUE;
    while (app_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                app_running = FALSE;
            }
        }
        SDL_FillSurfaceRect(psurface, &clear_rect, 0);

        for (size_t i = 0; i < square.num_vs; i++) {
            rotate_xz(&square.vs[i], center, 0.01);
            rotate_xy(&square.vs[i], center, 0.01);
            rotate_yz(&square.vs[i], center, 0.01);
            compute_screen_pos(&square.screen_vs[i], &square.vs[i]);
            square.rect_vs[i].x = square.screen_vs[i].x;
            square.rect_vs[i].y = square.screen_vs[i].y;
            SDL_FillSurfaceRect(psurface, &square.rect_vs[i], BLUE);
        }

        for (size_t i = 0; i < square.num_wires; i++) {
            int i1 = square.wire_order[i].x;
            int i2 = square.wire_order[i].y;
            draw_line(psurface, square.screen_vs[i1], square.screen_vs[i2], BLUE);
        }

        SDL_UpdateWindowSurface(pwindow);
        SDL_Delay(1000 / FPS);
    }

    return 0;
}
