long long collatz_steps(long long n) {
    long long steps = 0;
    long long cur = n;
    while (cur != 1) {
        cur = (cur % 2 == 0) ? cur / 2 : 3 * cur + 1;   // stays long long throughout
        ++steps;
    }
    return steps;
}
