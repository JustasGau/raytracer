#ifndef PROVIDER_H
#define PROVIDER_H

#include <mutex>

struct render_package {
    bool finished;
    int height_start;
    int height_finish;
    int width_start;
    int width_finish;
};

class provider {
    public:
        int h_start = 0;
        int h_end;
        int w_start = 0;
        int w_end;
        int threads;
        std::mutex m_lock;

        provider() {}
        // provider(int h, int w, int t) : h_end(h), w_end(w), threads(t) {}

        render_package get_package() {
            const std::lock_guard<std::mutex> lock(m_lock);
            render_package pac = calc_pixels();
            return pac;
        }
    private:
        virtual render_package calc_pixels();
};

render_package provider::calc_pixels() {
    if (h_end <= 0) return render_package{true, 0, 0 ,0, 0};
    int amount_h = static_cast<int>(h_end / threads);
    // int amount_h = 20;
    bool finished = false;
    // if there are lines to render but the calculation for distribution returns less than 1 line
    // the amount is capped at one line
    // this will have a negative efect of constant line requests at the end of render, but all threads will be busy
    if (amount_h < 1 && h_end > 0) amount_h = 1;
    int start = h_start;
    h_start += amount_h;
    h_end -= amount_h;
    const int end = h_start + amount_h;
    render_package pac = {finished, start, end ,w_start, w_end};
    std::cout << pac.height_start << ":" << pac.height_finish << "\n";
    std::cout << pac.height_finish - pac.height_start << "\n";
    return pac;
}


#endif