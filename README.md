# C++ Wikipedia Game Solver
I wrote this to experiment with C++11 types like `std::condition_variable`, `std::thread`, `std::mutex`, `std::lock_guard`, and other things.
This does a simple search by checking every link on a page (and it only pulls a given link once), and queuing all links it finds on that page to be pulled. It does this until a given link that is about to be queued is the same as the destination URL. This behavior is similar to a depth first search.
