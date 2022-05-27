#include "/usr/include/SDL2/SDL.h"
#include <stdlib.h>
#include "rtweekend.h"

#include "color.h"
#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"
#include "material.h"
#include <vector>

#include <csignal>
#include <iostream>
#include <thread>

const int MAX_DEPTH = 5;
const int IMAGE_WIDTH = 400;
const double ASPECT_RATIO = 3.0 / 2.0;
const int IMAGE_HEIGHT = static_cast<int>(IMAGE_WIDTH / ASPECT_RATIO);
const int SAMPELS = 5;


color ray_color(const ray& r, const hittable& world, int depth) {
    hit_record rec;
    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return color(0,0,0);

    if (world.hit(r, 0.001, infinity, rec)) {
        ray scattered;
        color attenuation;
        if (rec.mat_ptr->scatter(r, rec, attenuation, scattered))
            return attenuation * ray_color(scattered, world, depth-1);
        return color(0,0,0);
    }
    vec3 unit_direction = unit_vector(r.direction());
    auto t = 0.5*(unit_direction.y() + 1.0);
    return (1.0-t)*color(1.0, 1.0, 1.0) + t*color(0.5, 0.7, 1.0);
}

hittable_list random_scene() {
    hittable_list world;

    auto ground_material = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0,-1000,0), 1000, ground_material));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9*random_double(), 0.2, b + 0.9*random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    sphere_material = make_shared<lambertian>(albedo);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = make_shared<metal>(albedo, fuzz);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                } else {
                    // glass
                    sphere_material = make_shared<dielectric>(1.5);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

    auto material2 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

    auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, material3));

    return world;
}

void signal_handler(int signal_num)
{
    std::cerr << "The interrupt signal is (" << signal_num
         << "). \n";
  
    // It terminates the  program
    exit(signal_num);
}

void draw(int start, int end, camera cam, SDL_Renderer *renderer, hittable_list world, bool is_renderer) {
    for (int j = start; j < end; j++) {
        for (int i = 0; i < IMAGE_WIDTH; i++) {
            color pixel_color(0, 0, 0);
            for (int s = 0; s < SAMPELS; ++s) {
                auto u = (i + random_double()) / (IMAGE_WIDTH-1);
                auto v = (j + random_double()) / (IMAGE_HEIGHT-1);
                ray r = cam.get_ray(u, v);
                pixel_color += ray_color(r, world, MAX_DEPTH);
            }
            write_color(renderer, pixel_color, SAMPELS);
            SDL_RenderDrawPoint(renderer, i, IMAGE_HEIGHT - j);
        }
        if(is_renderer) {
            SDL_RenderPresent(renderer);
        }
    }
}


int main() {
    // Image
    // TODO does not work
    signal(SIGABRT, signal_handler);
    const int processor_count = std::thread::hardware_concurrency() - 2;
    const int loops_per_thread = IMAGE_HEIGHT / processor_count;
    // super not optimal - eats remaining loops
    const int last_thread = IMAGE_HEIGHT - (processor_count * loops_per_thread);

    std::vector<std::thread> tys;

    SDL_Event event;
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(IMAGE_WIDTH, IMAGE_HEIGHT, 0, &window, &renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // World
    auto world = random_scene();

    // Camera
    point3 lookfrom(13,2,3);
    point3 lookat(0,0,0);
    vec3 vup(0,1,0);
    auto dist_to_focus = 10.0;
    auto aperture = 0.1;

    camera cam(lookfrom, lookat, vup, 20, ASPECT_RATIO, aperture, dist_to_focus);

    // Render

    for (int i = 0; i < 10; i++) {
        int start = 0 ;
        int end = loops_per_thread;
        bool is_renderer = true;
        if (i > 0) {
            start = loops_per_thread * i;
            end = loops_per_thread * (i + 1);
            is_renderer = false;
        }

        std::thread t(draw, start, end, cam, renderer, world, is_renderer);
        tys.push_back(std::move(t));
    }
    std::thread t(draw, IMAGE_HEIGHT - last_thread, IMAGE_HEIGHT, cam, renderer, world, false);

    for (auto &th : tys) {
        th.join();
    }
    t.join();


    std::cerr << "\nDone.\n";
    // SDL_RenderPresent(renderer);
    while (1) {
        if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
            break;
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}