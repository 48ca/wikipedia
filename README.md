# C++ Wikipedia Game Solver
I wrote this to experiment with C++11 types like `std::condition_variable`, `std::thread`, `std::mutex`, `std::lock_guard`, and other things.
This does a simple search by checking every link on a page (and it only pulls a given link once), and queuing all links it finds on that page to be pulled. It does this until a given link that is about to be queued is the same as the destination URL. This behavior is similar to a breadth first search.
Since the middle of Sophomore year in high school, the same thought has been in my head to write a Wikipedia game solver, ever since my CS teacher Mr. Rudwick posed the challenge to one of my classmates.
I didn't know how to do it back then, but I thought that sort of challenge would be fun to try, especially as it would help me hone my skills with the newer C++ standards.
