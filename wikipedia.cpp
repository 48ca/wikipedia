#include <curl/curl.h>
#include <stdlib.h>
#include <cstring>

#include <string>
#include <unordered_map>
#include <condition_variable>
#include <vector>
#include <thread>
#include <iostream>
#include <sstream>
#include <regex>

#include "safetypes.hpp"
#include "logging.hpp"

#include <chrono>
#include <atomic>

const std::string WIKIPEDIA_DOMAIN = "https://en.wikipedia.org";

const unsigned num_pull_threads = 64;
const unsigned num_parse_threads = 4;

std::mutex final_notify;
std::atomic<bool> searching;
std::condition_variable notify_main_thread;

SafeQueue<std::string> pull_queue(0);
SafeQueue<std::string> parse_queue(1);

void stop() {
    std::lock_guard<std::mutex> lk(final_notify);
    searching = false;
    notify_main_thread.notify_one();
    pull_queue.notify_all();
    parse_queue.notify_all();
    log("WARNING: Waiting for all threads to stop");
}

size_t writedata(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::ostringstream *stream = (std::ostringstream*)userdata;
    size_t count = size*nmemb;
    stream->write(ptr, count);
    return count;
}
void pull(int id) {
    log("Pull thread " + std::to_string(id) + " started");

    while(searching) {
        std::ostringstream stream;
        CURL* curl;
        CURLcode res;

        std::string s = pull_queue.wait_for_element();
        if(!searching)
            return;
        // log("Pull thread " + std::to_string(id) + " got data");
        // log(s);

retry:
        curl = curl_easy_init();
        if(!curl) {
            log("Unable to setup cURL");
            continue;
        }
        log(std::to_string(id) + " Pulling URL " + WIKIPEDIA_DOMAIN + s);
        curl_easy_setopt(curl, CURLOPT_URL, (WIKIPEDIA_DOMAIN + s).c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if(res) {
            log(std::to_string(id) + " While attempting to pull: " + WIKIPEDIA_DOMAIN + s);
            log(std::to_string(id) + " Code: " + std::to_string(res));
            log(std::to_string(id) + " Error: " + std::string(curl_easy_strerror(res)));
            goto retry;
        } else {
            parse_queue.push(stream.str());
        }
    }
}

const std::regex ANCHOR_REGEX("<a[^>]*href=[\"'](/[^/][\\w?/\\.=&:]+)[\"'][^>]*>", std::regex_constants::icase | std::regex_constants::ECMAScript);
void parse(int id) {
    log("Parse thread " + std::to_string(id) + " started");

    const auto matches_end = std::sregex_iterator();
    while(searching) {
        std::string el = parse_queue.wait_for_element();
        if(!searching)
            return;
        // log("Parse thread " + std::to_string(id) + " got data");
        auto matches_begin  = std::sregex_iterator(el.begin(), el.end(), ANCHOR_REGEX);
        for(std::sregex_iterator i = matches_begin; i != matches_end; ++i) {
            std::smatch sm = *i;
            std::smatch::iterator it = sm.begin();
            for(std::advance(it, 1); it != sm.end(); advance(it, 1)) {
                // log(std::to_string(id) + " Pushing: " + std::string(*it));
                pull_queue.push(*it);
            }
        }
    }
}

int main(void) {

    searching.store(true, std::memory_order_release);

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

    for(unsigned i = 0; i < num_pull_threads; ++i) {
        pull_threads[i].join();
        log("Pull thread " + std::to_string(i) + " stopped");
    }
    for(unsigned i = 0; i < num_parse_threads; ++i) {
        parse_threads[i].join();
        log("Parse thread " + std::to_string(i) + " stopped");
    }

    return 0;

}
