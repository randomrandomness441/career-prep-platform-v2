# Web Crawler Multithreaded

## ELI5: a team exploring a huge, connected building

You've got a team of 8 explorers and one huge building full of connected rooms. Each room
has doorways leading to other rooms, and some rooms loop back to ones you've already seen.
The job is to visit every reachable room exactly once, using all 8 explorers at once to
cover ground faster, and never send two explorers into the same new room.

The tricky part is knowing when you're *actually* done. An empty "rooms left to check"
list isn't proof the exploration is finished. One of your explorers might still be
standing in a room right now, about to discover three brand-new doorways nobody's seen
yet. You have to know that nobody is still mid-discovery before you can call it complete.
And if one room turns out to have a jammed door, because the network call for that page
fails, the team doesn't give up. They just can't see what's behind that door, and they
carry on exploring everywhere else they can still reach.

## What you're actually building

```cpp
class HtmlParser {
public:
    virtual ~HtmlParser() = default;
    virtual std::vector<std::string> getUrls(const std::string& url) = 0;
};

class WebCrawler {
public:
    std::vector<std::string> crawl(const std::string& startUrl, HtmlParser& parser,
    int num_workers = 8);
};
```

`getUrls(url)` returns every URL linked from `url`. Think of it as the possibly slow
network call that fetches and parses a page, and the "doorways" out of that room. It
**may throw**, because a real network call can fail.

## Requirements

1. Every URL reachable from `startUrl` is visited exactly once, regardless of how many
   worker threads race to discover it from different pages at once.
2. **The pool must correctly detect when there's truly nothing left to do.** An empty
   queue by itself isn't enough. A worker that's mid-call inside `getUrls` might still
   return more URLs to add, the same way an explorer standing in a room might still be
   about to find new doorways.
3. **If `getUrls` throws for some URL, the crawl must still complete.** Every other URL
   that was already queued, or that's reachable through pages other than the failing one,
   must still be visited and included in the result. The failing page's own outbound
   links are simply unknown. That's the only thing allowed to be missing.
4. `crawl` must not hang, and must not crash the process, no matter which URL(s) throw.

## Why the constraints exist

**`num_workers` worker threads pull from one shared queue.** That shared queue is the
team's single "rooms left to check" list. Everyone draws from the same source, so no two
explorers duplicate work by accident.
