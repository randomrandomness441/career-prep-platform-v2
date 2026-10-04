### How it's called

```cpp
BatchDispatcher dispatcher([](const std::vector<Request>& reqs) {
    std::vector<Response> out;
    for (const auto& r : reqs) out.push_back(Response{r.value * 2});   // stands in for a model
    return out;
});

std::vector<std::future<Response>> futs;
std::vector<std::thread> clients;
for (int i = 0; i < 20; ++i) {
    clients.emplace_back([&, i]{
        futs.push_back(dispatcher.submit(Request{i}));   // futs written under a lock in a real test
    });
}
for (auto& t : clients) t.join();

dispatcher.flush();   // runs process_batch once on all 20 pending requests

// futs[i].get().value == i * 2 for every i -- each caller gets exactly their own response
```
