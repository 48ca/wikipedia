#include <curl/curl.h>

#include <string>
#include <unordered_map>
#include <condition_variable>
#include <vector>
#include <thread>
#include <iostream>

#include "safequeue.hpp"
#include "logging.hpp"

#include <chrono>

const std::string WIKIPEDIA_DOMAIN = "https://en.wikipedia.org";

const unsigned num_pull_threads = 8;
const unsigned num_parse_threads = 4;

std::mutex final_notify;
bool searching = true; // does not need to be atomic -- protected by final_notify
std::condition_variable notify_main_thread;

SafeQueue<std::string> pull_queue(0);
SafeQueue<std::string> parse_queue(1);

void stop() {
    std::lock_guard<std::mutex> lk(final_notify);
    searching = false;
    notify_main_thread.notify_one();
};

void pull(int id) {
    log("Pull thread " + std::to_string(id) + " started");

    while(searching) {
        CURL* curl;
        CURLcode res;
        std::string readBuffer;

        std::string s = pull_queue.wait_for_element();
        log("Pull thread " + std::to_string(id) + " got data");
        log(s);

        curl = curl_easy_init();
        if(curl) {
            log("Pulling URL " + WIKIPEDIA_DOMAIN + s);
            curl_easy_setopt(curl, CURLOPT_URL, WIKIPEDIA_DOMAIN + s);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);
            log(std::to_string(res));
            curl_easy_cleanup(curl);
            parse_queue.push(std::move(readBuffer));
            stop();
        } else {
            log("Unable to setup cURL");
        }
    }
}

void parse(int id) {
    log("Parse thread " + std::to_string(id) + " started");

    while(searching) {
        std::string el = parse_queue.wait_for_element();
        log("Parse thread " + std::to_string(id) + " got data");
        log(el);
    }
}

int main(void) {

    std::vector<std::thread> pull_threads;
    for(unsigned i = 0; i < num_pull_threads; ++i)
        pull_threads.emplace_back(pull, i);

    std::vector<std::thread> parse_threads;
    for(unsigned i = 0; i < num_parse_threads; ++i)
        parse_threads.emplace_back(parse, i);

    std::string source_path="/wiki/Main_Page";
    std::string dest_path = "/Eggplant";

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    log("Waiting to start");
    pull_queue.push(source_path);
    log("Started");

    std::unique_lock<std::mutex> lk(final_notify);
    notify_main_thread.wait(lk, []{
        return !searching; // keep waiting if we're still searching
    });

    return 0;

}
