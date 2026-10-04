// A counter meant to be incremented extremely often from many threads at
// once, and read back only occasionally. No increment may ever be lost.
class StreamCounter {
public:
    explicit StreamCounter(int num_shards) {
        // TODO: implement
        (void)num_shards;
    }

    void increment() {
        // TODO: implement
    }

    long total() const {
        // TODO: implement
        return 0;
    }
};
