### How it's called

```cpp
class MyParser : public HtmlParser {
public:
    std::vector<std::string> getUrls(const std::string& url) override {
        return fetch_and_parse(url);   // may throw on a network error
    }
};

MyParser parser;
WebCrawler crawler;
std::vector<std::string> visited = crawler.crawl("https://start.example", parser, 8);
// visited contains every reachable URL exactly once, even if some pages threw
```

`crawl` is a single blocking call from the caller's point of view — it spawns and joins
its own 8 worker threads internally and returns the complete result.
