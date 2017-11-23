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

SafeQueue<std::vector<std::string>> pull_queue(0);
SafeQueue<std::pair<std::vector<std::string>, std::string>> parse_queue(1);
SafeSet<std::string> url_set;

std::string source_path = "/wiki/GitHub";
std::string dest_path =   "/wiki/Linus_Torvalds";

void print_vec(std::vector<std::string> v) {
    std::string s = "[ ";
    for(const auto& el : v) {
        s += el + " ";
    }
    log(s + "]");
}

void stop() {
    if(searching.load(std::memory_order_acquire) == false)
        return;
    std::lock_guard<std::mutex> lk(final_notify);
    searching.store(false, std::memory_order_release);
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
void pull() {
    while(searching) {
        std::ostringstream stream;
        CURL* curl;
        CURLcode res;

        std::vector<std::string> vec = pull_queue.wait_for_element();
        auto& s = vec.back();

retry:
        if(!searching)
            return;
        // log("Pull thread " + std::to_string(id) + " got data");
        // log(s);

        curl = curl_easy_init();
        if(!curl) {
            log("Unable to setup cURL");
            continue;
        }
        // log(std::to_string(id) + " Pulling URL " + WIKIPEDIA_DOMAIN + s);
        log(WIKIPEDIA_DOMAIN + s);
        curl_easy_setopt(curl, CURLOPT_URL, (WIKIPEDIA_DOMAIN + s).c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if(res) {
            /*
            log(std::to_string(id) + " While attempting to pull: " + WIKIPEDIA_DOMAIN + s);
            log(std::to_string(id) + " Code: " + std::to_string(res));
            log(std::to_string(id) + " Error: " + std::string(curl_easy_strerror(res)));
            */
            goto retry;
        } else {
            parse_queue.push(std::pair<std::vector<std::string>, std::string>(vec, stream.str()));
        }
    }
}

const std::regex ANCHOR_REGEX("<a[^>]*href=[\"'](/[^/][\\w%?/\\(\\)\\.=&:]+)[\"'][^>]*>", std::regex_constants::icase | std::regex_constants::ECMAScript);
void parse() {
    const auto matches_end = std::sregex_iterator();
    while(searching) {
        auto [ from, body ] = parse_queue.wait_for_element();
        if(!searching)
            return;
        // log("Parse thread " + std::to_string(id) + " got data");
        auto matches_begin  = std::sregex_iterator(body.begin(), body.end(), ANCHOR_REGEX);
        for(std::sregex_iterator i = matches_begin; i != matches_end; ++i) {
            std::smatch sm = *i;
            std::smatch::iterator it = sm.begin();
            for(std::advance(it, 1); it != sm.end(); advance(it, 1)) {
                auto from_copy = from;
                // log(std::to_string(id) + " Pushing: " + std::string(*it));
                if(*it == dest_path) {
                    // log(std::to_string(id) + " FOUND DESTINATION: " + dest_path + " WHILE PARSING " + from.back());
                    from_copy.emplace_back(*it);
                    print_vec(from_copy);
                    stop();
                    return;
                }
                if(url_set.if_not_contains_add(*it)) {
                    from_copy.emplace_back(*it);
                    pull_queue.push(from_copy);
                }
            }
        }
    }
}

int handle(int argc, char** argv) {
    switch(argc) {
        case 3:
            source_path = std::string(argv[1]);
            dest_path = std::string(argv[2]);
            std::cout << "Source path:      " << source_path << std::endl;
            std::cout << "Destination path: " << dest_path << std::endl;
        case 1:
            return 0;
        case 2:
            std::cout << "Not enough arguments specified" << std::endl;
            return 1;
        default:
            std::cout << "Too many arguments specified" << std::endl;
            return 1;
    }
}

int main(int argc, char** argv) {

    if(handle(argc, argv))
        return 1;

    searching.store(true, std::memory_order_release);

    std::vector<std::thread> pull_threads;
    for(unsigned i = 0; i < num_pull_threads; ++i)
        pull_threads.emplace_back(pull);

    std::vector<std::thread> parse_threads;
    for(unsigned i = 0; i < num_parse_threads; ++i)
        parse_threads.emplace_back(parse);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    log("Waiting to start");
    pull_queue.push(std::vector<std::string>{source_path});
    log("Started");

    std::unique_lock<std::mutex> lk(final_notify);
    notify_main_thread.wait(lk, []{
        return !searching; // keep waiting if we're still searching
    });

    for(unsigned i = 0; i < num_pull_threads; ++i) {
        pull_threads[i].join();
        // log("Pull thread " + std::to_string(i) + " stopped");
    }
    for(unsigned i = 0; i < num_parse_threads; ++i) {
        parse_threads[i].join();
        // log("Parse thread " + std::to_string(i) + " stopped");
    }

    return 0;

}
