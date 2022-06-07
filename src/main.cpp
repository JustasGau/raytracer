#include "/usr/include/SDL2/SDL.h"
#include <stdlib.h>
#include "rtweekend.h"

#include "color.h"
#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"
#include "material.h"
#include "pixel_provider.h"
#include "moving_sphere.h"

#include <unistd.h>
#include <vector>
#include <csignal>
#include <iostream>
#include <thread>
#include <mutex>

// seems like an abuse of the globals
int MAX_DEPTH = 1;
int IMAGE_WIDTH = 400;
const double ASPECT_RATIO = 3.0 / 2.0;
int IMAGE_HEIGHT = static_cast<int>(IMAGE_WIDTH / ASPECT_RATIO);
int SAMPLES = 1;
std::mutex draw_lock;
std::mutex counterlock;
int tcounter = 0;
provider singleton_povider;

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

    auto checker = make_shared<checker_texture>(color(0.2, 0.3, 0.1), color(0.9, 0.9, 0.9));
    world.add(make_shared<sphere>(point3(0,-1000,0), 1000, make_shared<lambertian>(checker)));

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
                    auto center2 = center + vec3(0, random_double(0,.5), 0);
                    world.add(make_shared<moving_sphere>(
                        center, center2, 0.0, 1.0, 0.2, sphere_material));
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

hittable_list simple_scene() {
    hittable_list world;

    auto ground_material = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0,-1000,0), 1000, ground_material));

    auto material1 = make_shared<dielectric>(1.5);
    // left
    world.add(make_shared<sphere>(point3(5, 1, 3), 1.0, material1));

    auto material2 = make_shared<metal>(color(0.4, 0.2, 0.1), 0.0);
    // center
    world.add(make_shared<sphere>(point3(5, 1, 1), 1.0, material2));

    auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    // right
    world.add(make_shared<sphere>(point3(5, 1, -1.1), 1.0, material3));

    return world;
}

hittable_list two_perlin_spheres() {
    hittable_list objects;

    auto pertext = make_shared<noise_texture>(4);
    objects.add(make_shared<sphere>(point3(0,-1000,0), 1000, make_shared<lambertian>(pertext)));
    objects.add(make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));

    return objects;
}

void signal_handler(int signal_num)
{
    std::cerr << "The interrupt signal is (" << signal_num
         << "). \n";
  
    // It terminates the  program
    exit(signal_num);
}

void draw(camera cam, SDL_Renderer *renderer, hittable_list world) {
    render_package pak = singleton_povider.get_package();
    while (!pak.finished) {
        for (int j = pak.height_start; j < pak.height_finish; j++) {
            for (int i = pak.width_start; i < pak.width_finish; i++) {
                color pixel_color(0, 0, 0);
                for (int s = 0; s < SAMPLES; ++s) {
                    auto u = (i + random_double()) / (IMAGE_WIDTH-1);
                    auto v = (j + random_double()) / (IMAGE_HEIGHT-1);
                    ray r = cam.get_ray(u, v);
                    pixel_color += ray_color(r, world, MAX_DEPTH);
                }
                color calculated = write_color(pixel_color, SAMPLES);
                draw_lock.lock();
                SDL_SetRenderDrawColor(renderer, calculated.x(), calculated.y(), calculated.z(), 255);
                SDL_RenderDrawPoint(renderer, i, IMAGE_HEIGHT - j);
                draw_lock.unlock();
            }
        }
        pak = singleton_povider.get_package();
    }
    counterlock.lock();
    tcounter++;
    counterlock.unlock();
}

void render(SDL_Renderer *renderer, const int processor_count) {
    while(tcounter < processor_count) {
        std::cout << "Finished threads: " << tcounter << "\n";
        sleep(1);
        SDL_RenderPresent(renderer);
    }
}

void check_parameter(int val) {
    if (val == 0) {
        std::cout << "Argument must be a valid number \n";
        exit(1);
    }
}

int main(int argc, char *argv[]) {

    if (argc < 4 ) {
        std::cout << "Please provide: scene width samples depth \n";
        return EXIT_FAILURE;
    }
    const int scene = std::strtol(argv[1], nullptr, 10);
    check_parameter(scene);

    IMAGE_WIDTH = std::strtol(argv[2], nullptr, 10);
    check_parameter(IMAGE_WIDTH);

    SAMPLES = std::strtol(argv[3], nullptr, 10);
    check_parameter(SAMPLES);

    MAX_DEPTH = std::strtol(argv[4], nullptr, 10);
    check_parameter(MAX_DEPTH);

    IMAGE_HEIGHT = static_cast<int>(IMAGE_WIDTH / ASPECT_RATIO);

    // Image
    signal(SIGINT, signal_handler);
    const int processor_count = std::thread::hardware_concurrency() - 2;
    // FIXME bad
    singleton_povider.threads = processor_count;
    singleton_povider.h_end = IMAGE_HEIGHT;
    singleton_povider.w_end = IMAGE_WIDTH;

    std::vector<std::thread> tys;

    SDL_Event event;
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(IMAGE_WIDTH, IMAGE_HEIGHT, 0, &window, &renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // World
    hittable_list world;
    auto vfov = 40.0;
    auto aperture = 0.0;
    point3 lookfrom;
    point3 lookat;

    switch (scene)
    {
    case 1:
        world = random_scene();
        lookfrom = point3(13,2,3);
        lookat = point3(0,0,0);
        vfov = 20.0;
        aperture = 0.1;
        break;
    case 2:
        world = simple_scene();
        lookfrom = point3(13,2,3);
        lookat = point3(0,0,0);
        vfov = 20.0;
        aperture = 0.1;
        break;
    case 3:
        world = two_perlin_spheres();
        lookfrom = point3(13,2,3);
        lookat = point3(0,0,0);
        vfov = 20.0;
        break;
    default:
        world = two_perlin_spheres();
        lookfrom = point3(13,2,3);
        lookat = point3(0,0,0);
        vfov = 20.0;
        break;
    }

    // Camera
    vec3 vup(0,1,0);
    auto dist_to_focus = 10.0;

    camera cam(lookfrom, lookat, vup, vfov, ASPECT_RATIO, aperture, dist_to_focus, 0.0, 1.0);

    // Render

    for (int i = 0; i < processor_count; i++) {
        std::thread t(draw, cam, renderer, world);
        tys.push_back(std::move(t));
    }

    std::thread render_thread(render, renderer, processor_count);

    for (auto &th : tys) {
        th.join();
    }

    render_thread.join();


    std::cerr << "\nDone.\n";
    SDL_Surface *sshot = SDL_CreateRGBSurface(0, IMAGE_WIDTH, IMAGE_HEIGHT, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, sshot->pixels, sshot->pitch);
    char img_name [100];
    snprintf(img_name, 100, "./images/sc%d_w%d_s%d_d%d.bmp",scene, IMAGE_WIDTH, SAMPLES, MAX_DEPTH);
    SDL_SaveBMP(sshot, img_name);
    SDL_FreeSurface(sshot);
    SDL_RenderPresent(renderer);
    while (1) {
        if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
            break;
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}